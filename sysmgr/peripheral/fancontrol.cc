// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "bruno/logging.h"
#include "platform.h"
#include "fancontrol.h"
#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <string.h>
#include <stdint.h>
#include <fstream>
#include <unistd.h>

namespace bruno_platform_peripheral {

#define FAN_CONTROL_PARAMS_FILE   "/user/sysmgr/fan_control_params.tbl"

const unsigned int FanControl::kPwmFreq50Khz = 0x7900;
const unsigned int FanControl::kPwmFreq26Khz = 0x4000;
const unsigned int FanControl::kPwmFreq206hz = 0x0080;
const unsigned int FanControl::kPwmDefaultTemperatureScale = 0x11;
const unsigned int FanControl::kPwmDefaultDutyCycleScale = 0x11;
const unsigned int FanControl::kPwmDefaultStartup = 30;

/*
 * Defaults of Fan control parameters for GFMS100 (Bruno-IS)
 */
const FanControlParams FanControl::kGFMS100FanCtrlSocDefaults = {
                          temp_min      : 70,
                          temp_max      : 100,
                          duty_cycle_min: 25,
                          duty_cycle_max: 75,
                          threshold     : 2,
                          alpha         : 0
                        };

const FanControlParams FanControl::kGFMS100FanCtrlHddDefaults = {
                          temp_min      : 45,
                          temp_max      : 70,
                          duty_cycle_min: 25,
                          duty_cycle_max: 50,
                          threshold     : 2,
                          alpha         : 0
                        };
/*
 * Defaults of Fan control parameters for GFHD100 (Bruno)
 */
const FanControlParams FanControl::kGFHD100FanCtrlSocDefaults = {
                          temp_min      : 45,
                          temp_max      : 100,
                          duty_cycle_min: 25,
                          duty_cycle_max: 100,
                          threshold     : 2,
                          alpha         : 0
                        };

FanControl::~FanControl() {
  Terminate();
}

bool FanControl::Init(uint8_t min_temp, uint8_t max_temp, uint8_t n_levels) {
  if (max_temp < min_temp) {
    LOG(LS_ERROR) << "Maximum temperature " << max_temp << " is less than"
                  << " minimum temperature" << min_temp;
    return false;
  }
  if (max_temp == min_temp || n_levels == 0) {
    duty_cycle_scale_ = 0;
    temperature_scale_ = 0;
    duty_cycle_pwm_ = duty_cycle_startup_;
  } else {
    temperature_max_ = max_temp;
    temperature_min_ = min_temp;
    temperature_scale_ = (max_temp-min_temp)/(n_levels-1);
    duty_cycle_scale_ = (duty_cycle_max_-duty_cycle_min_)/(n_levels-1);
    if ((max_temp-min_temp)%temperature_scale_) {
      LOG(LS_WARNING) << "Maximum temperature is rounded to "
                      <<  min_temp*(n_levels-1)*temperature_scale_
                      << ", original maximum temperature " << max_temp
                      << ", minimum temperature " << min_temp
                      << ", number of levels " << n_levels
                      << ". To avoid this, set the differential between "
                      << "maximum and minimum temperatures as a multiple "
                      << "of number of levels";
    }
    threshold_ = duty_cycle_scale_/4;
    threshold_ = threshold_<2?2:threshold_;
    step_ = duty_cycle_scale_/2;
    step_ = step_<4?4:step_;
  }
  LOG(LS_INFO) << "Maximum temperature " << temperature_max_
               << ", minimum temperature " << temperature_min_
               << ", threshold " << threshold_
               << ", step " << step_;

  /* Check if the platform instance has been initialized
   * 1) If run sysmgr,  the platformInstance_ would be initalized in
   *    platformperipheral module.
   * 2) If run test_fan test util, the platformperipheral module won't be used.
   */
  if (platformInstance_ == NULL) {
    /* The global platform instance is not initialized. Let's handle it. */
    LOG(LS_VERBOSE) << "Init platformInstance_ in fancontrol";
    platformInstance_ = new Platform ("Unknown Platform", BRUNO_UNKNOWN, false);
    platformInstance_->Init();
    allocatedPlatformInstanceLocal_ = true;
  }

  InitParams();

  /* Fan pwm has been initialized in nexus init script */
  return true;
}

void FanControl::Terminate(void) {
  if (pfan_ctrl_params_) {
    delete [] pfan_ctrl_params_;
    pfan_ctrl_params_ = NULL;
  }
  if ((allocatedPlatformInstanceLocal_ == true) && (platformInstance_)) {
    delete platformInstance_;
    platformInstance_ = NULL;
  }
}

bool FanControl::InitPwm() {
  if (!DrivePwm(duty_cycle_pwm_)) {
    LOG(LS_ERROR) << "FanControl::DrivePwm failed";
    return false;
  }

  return true;
}

void FanControl::InitParams() {
  pfan_ctrl_params_ = new FanControlParams[BRUNO_PARAMS_TYPES];

  uint8_t max;
  switch (platform_ = platformInstance_->PlatformType()) {
    case BRUNO_GFMS100:
      /* Set thermal fan policy parameters of GFMS100 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFMS100FanCtrlSocDefaults;
      pfan_ctrl_params_[BRUNO_IS_HDD] = kGFMS100FanCtrlHddDefaults;
      max = BRUNO_IS_HDD;
      break;
    case BRUNO_GFHD100:
      /* Set thermal fan policy parameters of GFHD100 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFHD100FanCtrlSocDefaults;
      max = BRUNO_SOC;
      break;
    default:
      LOG(LS_ERROR) << "Invalid platform type, ignore ... " << platform_;
      max = BRUNO_SOC;
      break;
  }

  /* Check if an external fan control parameter table existing */
  dbgUpdateFanControlParams();

  FanControlParams *pfan_ctrl;
  uint8_t idx;
  /* Adjust the fan control parameters for calculation. */
  for (idx = 0, pfan_ctrl = pfan_ctrl_params_; idx <= max; idx++, pfan_ctrl++) {
    LOG(LS_INFO) << platformInstance_->PlatformName()
                 << ((idx == BRUNO_SOC)? "_SOC" : "_HDD") << std::endl
                 << " Tmin: " << pfan_ctrl->temp_min << std::endl
                 << " Tmax: " << pfan_ctrl->temp_max << std::endl
                 << " Dmin: " << pfan_ctrl->duty_cycle_min << std::endl
                 << " Dmax: " << pfan_ctrl->duty_cycle_max << std::endl
                 << " Threshold: " << pfan_ctrl->threshold << std::endl
                 << " alpha: " << pfan_ctrl->alpha << std::endl;
    pfan_ctrl->duty_cycle_min = TIMES_VALUE(pfan_ctrl->duty_cycle_min);
    pfan_ctrl->duty_cycle_max = TIMES_VALUE(pfan_ctrl->duty_cycle_max);
    pfan_ctrl->threshold      = TIMES_VALUE(GET_THRESHOLD(pfan_ctrl->temp_min,
                                                          pfan_ctrl->threshold));
    pfan_ctrl->alpha = ALPHA(pfan_ctrl->duty_cycle_max, pfan_ctrl->duty_cycle_min,
                             pfan_ctrl->temp_max, pfan_ctrl->temp_min);
    pfan_ctrl->temp_min = TIMES_VALUE(pfan_ctrl->temp_min);
    pfan_ctrl->temp_max = TIMES_VALUE(pfan_ctrl->temp_max);
  }
}

bool FanControl::SelfStart() {
  bool ret = true;
  if (self_start_enabled_){
    /*
     * Drive the fan with duty_cycle_startup_ for 1 second
     */
    ret = DrivePwm(duty_cycle_startup_);
    if (!ret) {
      LOG(LS_ERROR) << "FanControl::DrivePwm failed";
      return false;
    }
    state_ = VAR_SPEED;

    sleep (1);                  /* sleep for 1 second */
  }
  return ret;
}

bool FanControl::AdjustSpeed(uint32_t avg_temp) {
  bool ret = true;
  uint16_t new_duty_cycle_pwm;

  ComputeDutyCycle(avg_temp, &new_duty_cycle_pwm);

  if (new_duty_cycle_pwm != duty_cycle_pwm_){
    ret = DrivePwm(new_duty_cycle_pwm);
    if (!ret) {
      LOG(LS_ERROR) << "FanControl::DrivePwm failed";
      return false;
    }
  }

  return true;
}

/*
 * soc_temp = MULTI_VALUE times of SOC temperature.
 * hdd_temp = MULTI_VALUE times of HDD temperature.
 */
bool FanControl::AdjustSpeed_PControl(uint16_t soc_temp, uint16_t hdd_temp) {
  bool ret = true;
  uint16_t new_duty_cycle_pwm;
  uint16_t new_hdd_duty_cycle_pwm = 0;

  LOG(LS_VERBOSE) << "AdjustSpeed_PControl: soc_temp=" << soc_temp
                  << " hdd_temp=" << hdd_temp << std::endl;
  ComputeDutyCycle_PControl(soc_temp, &new_duty_cycle_pwm, BRUNO_SOC);
  if ((platformInstance_->PlatformHasHdd() == true) &&
      (new_duty_cycle_pwm != DUTY_CYCLE_PWM_MAX_VALUE)) {
    /* GFMS100 platform, compute HDD duty cycle PWM via HDD temperature */
    ComputeDutyCycle_PControl(hdd_temp, &new_hdd_duty_cycle_pwm, BRUNO_IS_HDD);
  }

  /* duty cycle PWM = max(duty_cycle_pwm_soc(soc_temp),
   *                      duty_cycle_pwm_hdd(hdd_temp))
   */
  if (new_hdd_duty_cycle_pwm > new_duty_cycle_pwm) {
    LOG(LS_INFO) << "HDD duty cycle PWM is larger than SOC duty cycle\n";
    new_duty_cycle_pwm = new_hdd_duty_cycle_pwm;
  }

  LOG(LS_INFO) << "AdjustSpeed_PControl: duty_cycle_pwm = 0x"
               << std::hex << new_duty_cycle_pwm;
  if (new_duty_cycle_pwm != duty_cycle_pwm_){
    ret = DrivePwm(new_duty_cycle_pwm);
    if (!ret) {
      LOG(LS_ERROR) << "FanControl::DrivePwm failed";
      return false;
    }
  }

  return true;
}

/* The returned hdd temperature = real HDD temperature * TIMES_VALUE */
void FanControl::GetHddTemperature(uint16_t *phdd_temp) {
  *phdd_temp = 0;
  double  hdd_temp;

  /* TODO - Use ioctl to get SMART data if possible. */
  if (platformInstance_->PlatformHasHdd() == true) {
    std::string pattern = "Current";
    std::string buf = "smartctl -l scttempsts /dev/sda";
    /* Create vector to hold hdd temperature words */
    std::vector<std::string> tokens;

    /* Insert the HDD temperature string into a stream */
    std::string result = ExecCmd((char *)buf.c_str(), &pattern);
    if ((result == "ERROR") || (result.empty() == true)) {
      /* Failed to get HDD temperature. Exit */
      LOG(LS_ERROR) << "GetHddTemperature: Can't get HDD temperature";
      return;
    }
    std::stringstream ss(result);
    while (ss >> buf) {
      tokens.push_back(buf);
    }
    /* HDD temperature is in the 3rd element */
    std::istringstream(tokens.at(2)) >> hdd_temp;
    /* LOG(LS_INFO) << "hdd_temp: " << hdd_temp << std::endl; */
    *phdd_temp = (uint16_t)(TIMES_VALUE(hdd_temp));
  }
  return;
}

bool FanControl::DrivePwm(uint16_t duty_cycle) {

  LOG(LS_INFO) << "DrivePwm 0x" << std::hex << duty_cycle;
  duty_cycle_pwm_ = duty_cycle;

  if (WriteFanDutyCycle(duty_cycle) == false) {
    LOG(LS_ERROR) << "WriteFanDutyCycle failed";
    return false;
  }

  if (duty_cycle == 0) {
    state_ = OFF;
  } else if (duty_cycle == period_+1) {
    state_ = FULL_SPEED;
  } else {
    state_ = VAR_SPEED;
  }

  return true;
}

void FanControl::ComputeDutyCycle(uint32_t avg_temp, uint16_t *new_duty_cycle_pwm) {
  uint16_t     compute_duty_cycle, diff;

  LOG(LS_INFO) << "FanControl::ComputeDutyCycle - current dutycycle = 0x" << std::hex << duty_cycle_pwm_;

  *new_duty_cycle_pwm = duty_cycle_pwm_; /* initialize it to current value */

  if (duty_cycle_scale_ == 0 || temperature_scale_ == 0) {
    return;
  }

  if (avg_temp < temperature_min_){
    LOG(LS_INFO) << "Set dutycycle to minimum 0x" << std::hex << duty_cycle_min_;
    *new_duty_cycle_pwm = duty_cycle_min_;
    return;
  }

  compute_duty_cycle = duty_cycle_min_ + (avg_temp-temperature_min_)*duty_cycle_scale_/temperature_scale_;

  LOG(LS_INFO) << "FanControl::ComputeDutyCycle - compute_duty_cycle = 0x" << std::hex << compute_duty_cycle;

  /*
   * Apply dutyCycle limits.  Avg temp limits are already applied when we calculated the
   * avg temperature.
   */
  if (compute_duty_cycle < duty_cycle_min_){
    LOG(LS_INFO) << "Set dutycycle to minimum 0x" << std::hex << duty_cycle_min_;
    compute_duty_cycle = duty_cycle_min_;
  }

  if (compute_duty_cycle > duty_cycle_max_){
    LOG(LS_INFO) << "Set dutycycle to maximum 0x" << std::hex << duty_cycle_max_;
    compute_duty_cycle = duty_cycle_max_;
  }

  LOG(LS_INFO) << "duty_cycle_regulated_ = 0x" << std::hex << duty_cycle_regulated_;

  /*
   * Calculate regulated duty cycle by applying Diff_max limit
   */
  if (duty_cycle_regulated_){
    if (compute_duty_cycle == duty_cycle_regulated_) {
      /* no change in duty cycle */
      return;
    } else if (compute_duty_cycle > duty_cycle_regulated_) {
      /* rise in temp */
      diff = compute_duty_cycle - duty_cycle_regulated_;
      /*
       * Difference must be greater than the threshold to affect a change
       * in duty cycle
       */
      if (diff >= threshold_) {
        duty_cycle_regulated_ += step_;    /* maximum step change */
        if (duty_cycle_regulated_>duty_cycle_max_) {
          duty_cycle_regulated_ = duty_cycle_max_;
        }
      } else {
        return;
      }
    } else {
      /* decreasing temp */
      diff = duty_cycle_regulated_ - compute_duty_cycle;
      /*
       * Difference must be greater than the threshold to affect a change
       * in duty cycle
       */
      if (diff >= threshold_) {
        duty_cycle_regulated_ -= step_;    /* maximum step change */
        if (duty_cycle_regulated_<duty_cycle_min_) {
          duty_cycle_regulated_ = duty_cycle_min_;
        }
      } else {
        return;
      }
    }
  } else {
    /*
     * First time, set the regulated duty cycle to the computed duty cycle
     */
    duty_cycle_regulated_ = compute_duty_cycle;
  }

  /* do this for now until we get the linearization LUT */
  *new_duty_cycle_pwm = duty_cycle_regulated_;

  LOG(LS_INFO) << "new_duty_cycle_pwm = 0x" << std::hex << *new_duty_cycle_pwm;

  return;
}

/* To get better PWM resolution, temperature is MULTI_VALUE times of
 * temperature.
 */
void FanControl::ComputeDutyCycle_PControl(
  uint16_t temp, uint16_t *new_duty_cycle_pwm, uint8_t idx) {

  uint16_t  compute_duty_cycle;
  bool      calculate_duty_cycle_pwm = false;
  /* Get the fan control parameters based on the device type (hdd or soc) */
  uint16_t  temp_min = pfan_ctrl_params_[idx].temp_min;
  uint16_t  temp_max = pfan_ctrl_params_[idx].temp_max;
  uint16_t  threshold = pfan_ctrl_params_[idx].threshold;
  uint16_t  duty_cycle_min = pfan_ctrl_params_[idx].duty_cycle_min;

  LOG(LS_VERBOSE) << "FanCtrl::ComputeDutyCycle_PControl - current dutycycle = 0x"
               << std::hex << duty_cycle_pwm_
               << " i/p temperature = " << std::dec << temp
               << " pfan_ctrl_params_ idx = " << std::dec << (uint16_t)idx
               << std::endl;

  compute_duty_cycle = duty_cycle_pwm_; /* initialize it to current value */

  /* The thermal fan policy is including hysteresis handling */
  if (temp <= temp_min) {
    /* hysteresis handling */
    if (duty_cycle_pwm_ != DUTY_CYCLE_PWM_MIN_VALUE) {
      if ((threshold == 0) || (temp < (temp_min - threshold))) {
        compute_duty_cycle = DUTY_CYCLE_PWM_MIN_VALUE;
      } else {
        /* Set flag to calculate duty cycle PWM */
        calculate_duty_cycle_pwm = true;
      }
    } else {
      /*
       * 1. duty_cycle_pwm_ is DUTY_CYCLE_PWM_MIN_VALUE.
       * 2. *new_duty_cycle_pwm has been set to duty_cycle_pwm_.
       */
      if (temp == temp_min) {
        /* Set flag to calculate duty cycle PWM */
        calculate_duty_cycle_pwm = true;
      }
    }
  } else if (temp >= (temp_max - threshold)) {
    /* hysteresis handling */
    if (temp > temp_max) {
      compute_duty_cycle = DUTY_CYCLE_PWM_MAX_VALUE;
    } else {
      /*
       * (temp_max - threshold) <= temp <= temp_max
       */
      if (duty_cycle_pwm_ != DUTY_CYCLE_PWM_MAX_VALUE) {
        /* Set flag for calculating duty cycle PWM */
        calculate_duty_cycle_pwm = true;
      } else {
        /* duty_cycle_pwm_ is DUTY_CYCLE_PWM_MAX_VALUE.
         * While (temp_max - threshold) < temp < temp_max,
         * remain DUTY_CYCLE_PWM_MAX_VALUE.
         */
        if (temp == (temp_max - threshold)) {
          /* Set flag to calculate duty cycle PWM */
          calculate_duty_cycle_pwm = true;
        }
      }
    }
  } else {
    /* Set flag to calculate duty cycle PWM */
    calculate_duty_cycle_pwm = true;
  }

  if (calculate_duty_cycle_pwm == true) {
    /*
     * compute_duty_cycle =
     *    (duty_cycle_min + (temp - temp_min) * alpha) * steps_per_percent)
     *
     * Notes -
     * 1) For having more accurate duty cycle PWM, duty_cycle_min, temp,
     *    temp_min, alpha and ONE_PWM_ON_PER_PCT are MULTI_VALUE times
     *    of actual values.
     */
    compute_duty_cycle = duty_cycle_min +
              ADJUST_VALUE(((temp - temp_min) * pfan_ctrl_params_[idx].alpha));
    /*
     * 2) Compute_duty_cycle is (MULTI_VALUE * MULTI_VALUE) times.
     *    Convert it back.
     */
    compute_duty_cycle =
            ADJUST_VALUE_TWICE((compute_duty_cycle * ONE_PWM_ON_PER_PCT));
  }

  *new_duty_cycle_pwm = compute_duty_cycle;

  LOG(LS_INFO) << "new_duty_cycle_pwm = 0x" << std::hex
               << *new_duty_cycle_pwm << std::endl;

  return;
}

std::string FanControl::ExecCmd(char* cmd, std::string *pattern) {
  char buffer[256];
  std::string result = "";
  FILE* pipe = popen(cmd, "r");

  if (!pipe) {
    LOG(LS_ERROR) << "ExecCmd(): ERROR" << std::endl;
    return "ERROR";
  }

  while(!feof(pipe)) {
    if(fgets(buffer, sizeof(buffer), pipe) != NULL) {
      /* pattern == NULL, read and return all of lines
       * pattern != NULL, return the line if found the pattern in the line
       */
      if (pattern != NULL) {
        result = buffer;
        if (result.compare(0, pattern->size(), *pattern) == 0) {
          break;      /* Found the pattern. Exit. */
        }
        result.clear();
      }
      else {
        result += buffer;
      }
    }
  }
  pclose(pipe);

  return result;
}

void FanControl::dbgUpdateFanControlParams(void) {
  /* Check if the external fan control parameter table existing */
  std::ifstream params_table_file (FAN_CONTROL_PARAMS_FILE);
  if (params_table_file.is_open()) {
    LOG(LS_INFO) << FAN_CONTROL_PARAMS_FILE << " existing...\n";
    dbgGetFanControlParamsFromParamsFile(BRUNO_SOC);
    if (platformInstance_->PlatformHasHdd() == true) {
      dbgGetFanControlParamsFromParamsFile(BRUNO_IS_HDD);
    }
  }
}

/* A debugging function: Allow hardware engineers to tune the fan
 * control parameters
 */
bool FanControl::dbgGetFanControlParamsFromParamsFile(uint8_t fc_idx) {
  /* Create vector to hold hdd temperature words */
  std::vector<std::string> tokens;
  uint16_t max, min;

  /* TODO - Use protobuf to parse the fan control parameters. */

  /* Get the search platform keyword in the table file: GFMS100_SOC,
   * GFMS100_HDD...
   */
  std::string buf = platformInstance_->PlatformName();
  switch (fc_idx) {
    case BRUNO_SOC:
      buf += "_SOC";
      break;
    case BRUNO_IS_HDD:
      buf += "_HDD";
      break;
    default:
      buf += "_UNKNOWN";
      LOG(LS_WARNING) << "Invalid fc_index: " << fc_idx << std::endl;
      break;
  }

  LOG(LS_INFO) << buf << std::endl;

  std::string result = platformInstance_->GetLine((char *)FAN_CONTROL_PARAMS_FILE, &buf);
  if (result.empty() == true)
    return false;

  /* Insert the fan control parameters string into a stream */
  std::stringstream ss(result);
  while (ss >> buf) {
    tokens.push_back(buf);
  }

  /* LOG(LS_INFO) << "token.size = " << tokens.size() << std::endl; */

  /* Each line in the fan control table must have 6 elements */
  if (tokens.size() < 6)
    return false;       /* Incorrect length. Exit. */

  std::istringstream(tokens.at(1)) >> min;
  std::istringstream(tokens.at(2)) >> max;
  if (min > max)
    return false;   /* Invalid. Exit */

  std::istringstream(tokens.at(3)) >> min;
  std::istringstream(tokens.at(4)) >> max;
  if (min > max)
    return false;   /* Invalid. Exit */

  std::istringstream(tokens.at(1)) >> pfan_ctrl_params_[fc_idx].temp_min;
  std::istringstream(tokens.at(2)) >> pfan_ctrl_params_[fc_idx].temp_max;
  std::istringstream(tokens.at(3)) >> pfan_ctrl_params_[fc_idx].duty_cycle_min;
  std::istringstream(tokens.at(4)) >> pfan_ctrl_params_[fc_idx].duty_cycle_max;
  std::istringstream(tokens.at(5)) >> pfan_ctrl_params_[fc_idx].threshold;
  return true;
}

}  // namespace bruno_platform_peripheral
