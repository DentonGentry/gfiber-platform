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

/* same as lm96063 spinup setting in barebox. */
const unsigned int FanControl::kPwmDefaultStartup = 50;
const unsigned int FanControl::kPwmMinValue = 0;
const unsigned int FanControl::kPwmMaxValue = 100;

const unsigned int FanControl::kFanSpeedNotSpinning = 0;

/*
 * Fan will start and increase speed at temp_setpt + temp_step + 1
 * Fan will start slowing at temp_setpt - temp_step - 1
 * In between, it will not change speed.
 */

/*
 * Defaults of Fan control parameters for GFMS100 (Bruno-IS)
 * For GFMS100, Dmin and PWMsetp are used under FMS100_SOC settings.
 */
const FanControlParams FanControl::kGFMS100FanCtrlSocDefaults = {
                          temp_setpt    : 90,
                          temp_max      : 100,
                          temp_step     : 2,
                          duty_cycle_min: 25,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 120,
                        };

const FanControlParams FanControl::kGFMS100FanCtrlHddDefaults = {
                          temp_setpt    : 56,
                          temp_max      : 60,
                          temp_step     : 2,
                          duty_cycle_min: 25,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 120,
                        };

/*
 * Defaults of Fan control parameters for GFRG200/210 (optimus/optimus+hdd)
 * There is no direct SOC temp input, so we use the remote sensor.
 * Mapping between external temp sensor and actual cpu temp was determined
 * exterimentally.  See b/14666398 spreadsheet attachment.
 */
const FanControlParams FanControl::kGFRG200FanCtrlSocDefaults = {
                          temp_setpt    : 82,  // fan on @ 85 (cpu =~ 93)
                          temp_max      : 92,  // cpu =~ 100
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 105,
                        };

const FanControlParams FanControl::kGFRG210FanCtrlSocDefaults = {
                          temp_setpt    : 86,   // fan on @ 89 (cpu =~ 93)
                          temp_max      : 94,   // cpu =~ 100
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 105,
                        };

const FanControlParams FanControl::kGFRG210FanCtrlHddDefaults = {
                          temp_setpt    : 56,
                          temp_max      : 60,
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 105,
                        };

/*
 * Defaults of Fan control parameters for GFSC100 (Spacecast).
 * There is no direct SOC temp input, so we use the remote sensor.
 * Mapping between external temp sensor and actual cpu temp was determined
 * exterimentally.
 */

const FanControlParams FanControl::kGFSC100FanCtrlSocDefaults = {
                          temp_setpt    : 86,   // fan on @ 89 (cpu =~ 93)
                          temp_max      : 94,   // cpu =~ 100
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 105,
                        };

const FanControlParams FanControl::kGFSC100FanCtrlHddDefaults = {
                          temp_setpt    : 56,
                          temp_max      : 60,
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 105,
                        };
/*
 * Defaults of Fan control parameters for GFHD100 (Bruno)
 * the original duty_cycle_min value is set to 25
 * but from the measurement, pwm = 25%, fan duty-cycle
 * (or fan speed) is 45~50%.
 * the original duty_cycle_max value is set to 100
 * but from the measurement, pwm = 40% or above, fan duty-cycle
 * (or fan speed) is 99%. pwm is set to any value greater 40
 * it will only increase fan speed by less than 1%.
 * Therefore Dmax is set to 40.
 *
 * Temporary Solution (09/24/2012)
 * Since we are getting close to FCS, make the immediate change of raising
 * the Tmin for Thin Bruno from 45C to 85C. This will reduce the overall
 * fan speed and noise.
 */
const FanControlParams FanControl::kGFHD100FanCtrlSocDefaults = {
                          temp_setpt    : 90,
                          temp_max      : 100,
                          temp_step     : 2,
                          duty_cycle_min: 12,
                          duty_cycle_max: 40,
                          pwm_step      : 1,
                          temp_overheat : 120,
                        };

FanControl::~FanControl() {
  Terminate();
}

bool FanControl::Init(bool *gpio_mailbox_ready) {

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

  if (gpio_mailbox_ready != NULL) {
    for (int loopno = 4;
         (*gpio_mailbox_ready == false) && (loopno > 0); loopno--) {
      sleep(2);
      *gpio_mailbox_ready = CheckIfMailBoxIsReady();
      LOG(LS_VERBOSE) << "loopno=" << loopno;
    }
  }

  /* Get the current fan duty cycle */
  if (ReadFanDutyCycle(&duty_cycle_pwm_) == false) {
    LOG(LS_ERROR) << __func__ << ": failed to get fan duty cycle";
    duty_cycle_pwm_ = pfan_ctrl_params_[BRUNO_SOC].duty_cycle_min;
  }
  LOG(LS_VERBOSE) << "duty_cycle_pwm_=" << duty_cycle_pwm_;

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
    case BRUNO_GFRG200:
      /* Set thermal fan policy parameters of GFRG200 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFRG200FanCtrlSocDefaults;
      max = BRUNO_SOC;
      break;
    case BRUNO_GFRG210:
      /* Set thermal fan policy parameters of GFRG210 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFRG210FanCtrlSocDefaults;
      pfan_ctrl_params_[BRUNO_IS_HDD] = kGFRG210FanCtrlHddDefaults;
      max = BRUNO_IS_HDD;
      break;
    case BRUNO_GFSC100:
      /* Set thermal fan policy parameters of GFSC100 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFSC100FanCtrlSocDefaults;
      pfan_ctrl_params_[BRUNO_IS_HDD] = kGFSC100FanCtrlHddDefaults;
      max = BRUNO_IS_HDD;
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
                 << " Tsetpt: "    << pfan_ctrl->temp_setpt << std::endl
                 << " Tmax: "      << pfan_ctrl->temp_max << std::endl
                 << " Tstep: "     << pfan_ctrl->temp_step << std::endl
                 << " Dmin: "      << pfan_ctrl->duty_cycle_min << std::endl
                 << " Dmax: "      << pfan_ctrl->duty_cycle_max << std::endl
                 << " PWMstep: "   << pfan_ctrl->pwm_step << std::endl
                 << " Toverheat: " << pfan_ctrl->temp_overheat << std::endl;
  }
}


bool FanControl::AdjustSpeed(
      uint16_t soc_temp, uint16_t hdd_temp, uint16_t fan_speed) {
  bool ret = true;
  uint16_t new_duty_cycle_pwm;

  LOG(LS_VERBOSE) << __func__ << ": soc_temp=" << soc_temp
                  << " hdd_temp=" << hdd_temp << " fan_speed=" << fan_speed;

  do {
    /* Get new SOC PWM per the current SOC and HDD temperatures */

    /* Get new duty cycle per SOC and HDD temperatures */
    ComputeDutyCycle(soc_temp, hdd_temp, fan_speed, &new_duty_cycle_pwm);

    LOG(LS_INFO) << __func__ << ": duty_cycle_pwm = " << new_duty_cycle_pwm;
    if (new_duty_cycle_pwm != duty_cycle_pwm_) {
      /* When fan is not spinning and new_duty_cycle_pwm > duty_cycle_pwm_,
       * 1) Set to higher pwm kPwmDefaultStartup for a period of time to
       *    make sure the fan starts spinning
       * 2) then lower down to new_duty_cycle_pwm
       */
      if (fan_speed == kFanSpeedNotSpinning) {
        /* Fan is not rotating */
        if (new_duty_cycle_pwm > duty_cycle_pwm_) {
          LOG(LS_INFO) << "Set higher pwm=" << kPwmDefaultStartup;
          ret = DrivePwm(kPwmDefaultStartup);
          if (!ret) {
            LOG(LS_ERROR) << "DrivePwm failed" << kPwmDefaultStartup;
            break;
          }
          /* Sleep before lower pwm down to new_duty_cycle_pwm */
          sleep(2);
        }
      }

      ret = DrivePwm(new_duty_cycle_pwm);
      if (!ret) {
        LOG(LS_ERROR) << "DrivePwm failed";
        break;
      }
    }

  } while (false);

  return ret;
}

void FanControl::GetOverheatTemperature(uint16_t *poverheat_temp) {
  FanControlParams  *psoc = &pfan_ctrl_params_[BRUNO_SOC];
  *poverheat_temp = psoc->temp_overheat;
  return;
}

void FanControl::GetHddTemperature(uint16_t *phdd_temp) {
  *phdd_temp = 0;
  uint16_t hdd_temp;

  if (platformInstance_->PlatformHasHdd() == true) {
    std::string buf = "hdd-temperature /dev/sda";
    /* Create vector to hold hdd temperature words */
    std::vector<std::string> tokens;

    /* Insert the HDD temperature string into a stream */
    std::string result = ExecCmd((char *)buf.c_str(), NULL);
    if ((result == "ERROR") || (result.empty() == true)) {
      /* Failed to get HDD temperature. Exit */
      LOG(LS_ERROR) << "GetHddTemperature: Can't get HDD temperature";
      return;
    }
    std::istringstream(result) >> hdd_temp;
    /* LOG(LS_INFO) << "hdd_temp: " << hdd_temp << std::endl; */
    *phdd_temp = hdd_temp;
  }
  return;
}

bool FanControl::DrivePwm(uint16_t duty_cycle) {

  LOG(LS_INFO) << "DrivePwm = " << duty_cycle;
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


void FanControl::ComputeDutyCycle(
  uint16_t soc_temp,
  uint16_t hdd_temp,
  uint16_t fan_speed,
  uint16_t *new_duty_cycle_pwm) {

  uint16_t  compute_duty_cycle = duty_cycle_pwm_;
  FanControlParams  *psoc = &pfan_ctrl_params_[BRUNO_SOC];
  FanControlParams  *phdd = get_hdd_fan_ctrl_parms();

  LOG(LS_VERBOSE) << __func__ << " - duty_cycle_pwm_ = " << duty_cycle_pwm_
               << " i/p soc_temp=" << soc_temp
               << " hdd_temp="     << hdd_temp
               << " fan_speed="    << fan_speed;

  if ((soc_temp > psoc->temp_max) ||
      (if_hdd_temp_over_temp_max(hdd_temp, phdd) == true)) {
    compute_duty_cycle = psoc->duty_cycle_max;
  }
  else if ((soc_temp > (psoc->temp_setpt + psoc->temp_step)) ||
           (if_hdd_temp_over_temp_setpt(hdd_temp, phdd) == true)) {
    if (fan_speed == kFanSpeedNotSpinning) {
      compute_duty_cycle = psoc->duty_cycle_min;
    }
    else if (duty_cycle_pwm_ < psoc->duty_cycle_max) {
      /* 1. Possibly, the fan still stops due to duty_cycle_pwm_ is not large
       *    enough. Continue increase the duty cycle.
       * 2. Or the fan is running, but it's not fast enough to cool down
       *    the unit.
       */
      compute_duty_cycle = duty_cycle_pwm_ + psoc->pwm_step;
      if (compute_duty_cycle > psoc->duty_cycle_max)
        compute_duty_cycle = psoc->duty_cycle_max;
    }
  }
  else if ((soc_temp < (psoc->temp_setpt - psoc->temp_step)) &&
           (if_hdd_temp_lower_than_temp_setpt(hdd_temp, phdd) == true)) {
    if ((fan_speed == kFanSpeedNotSpinning) ||
        (duty_cycle_pwm_ < psoc->pwm_step)) {
      compute_duty_cycle = kPwmMinValue;
    }
    else {
      /* Reduce fan pwm if both soc_temp and hdd_temp are lower than
       * their (temp_setpt - temp_step) and plus fan is still spinning
       */
      compute_duty_cycle = duty_cycle_pwm_ - psoc->pwm_step;
    }
  }

  *new_duty_cycle_pwm = compute_duty_cycle;

  LOG(LS_INFO) << "new_duty_cycle_pwm = " << *new_duty_cycle_pwm;

  return;
}


std::string FanControl::ExecCmd(char* cmd, std::string *pattern) {
  char buffer[256];
  std::string result = "";
  FILE* pipe = popen(cmd, "r");

  if (!pipe) {
    LOG(LS_ERROR) << __func__ << ": ERROR";
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

FanControlParams *FanControl::get_hdd_fan_ctrl_parms() {
  FanControlParams  *ptr = NULL;
  if (platformInstance_->PlatformHasHdd() == true) {
    ptr = &pfan_ctrl_params_[BRUNO_IS_HDD];
  }
  return ptr;
}


bool FanControl::if_hdd_temp_over_temp_max(const uint16_t hdd_temp, const FanControlParams *phdd) const {
  bool  ret = false;  /* if no hdd params, default is false */
  if ((phdd != NULL) && (hdd_temp > phdd->temp_max)) {
    ret = true;
  }
  return ret;
}


bool FanControl::if_hdd_temp_over_temp_setpt(const uint16_t hdd_temp, const FanControlParams *phdd) const {
  bool  ret = false;  /* if no hdd params, default is false */
  if ((phdd != NULL) && (hdd_temp > (phdd->temp_setpt + phdd->temp_step))) {
    ret = true;
  }
  return ret;
}


bool FanControl::if_hdd_temp_lower_than_temp_setpt(const uint16_t hdd_temp, const FanControlParams *phdd) const {
  bool  ret = true;   /* if no hdd params, default is true */
  if (phdd != NULL) {
    if (hdd_temp < (phdd->temp_setpt - phdd->temp_step)) {
      ret = true;
    }
    else {
      ret = false;
    }
  }
  return ret;
}


void FanControl::dbgUpdateFanControlParams(void) {
  /* Check if the external fan control parameter table existing */
  std::ifstream params_table_file (FAN_CONTROL_PARAMS_FILE);
  if (params_table_file.is_open()) {
    LOG(LS_INFO) << FAN_CONTROL_PARAMS_FILE << " existing...";
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

  /* Each line in the fan control table must have 7 elements */
  if (tokens.size() < 7) {
    LOG(LS_ERROR) << __func__ << "Incorrect number of params -->" << tokens.size() ;
    return false;       /* Incorrect length. Exit. */
  }

  /* Compare Tsetpt and Tmax */
  std::istringstream(tokens.at(1)) >> min;
  std::istringstream(tokens.at(2)) >> max;
  if (min > max) {
    LOG(LS_ERROR) << __func__ << "Incorrect Tsettp: " << min << " and Tmax: " << max;
    return false;   /* Invalid. Exit */
  }

  std::istringstream(tokens.at(4)) >> min;
  std::istringstream(tokens.at(5)) >> max;
  if (min > max) {
    LOG(LS_ERROR) << __func__ << "Dmin: " << min << " and Dmax: " << max;
    return false;   /* Invalid. Exit */
  }

  std::istringstream(tokens.at(1)) >> pfan_ctrl_params_[fc_idx].temp_setpt;
  std::istringstream(tokens.at(2)) >> pfan_ctrl_params_[fc_idx].temp_max;
  std::istringstream(tokens.at(3)) >> pfan_ctrl_params_[fc_idx].temp_step;
  std::istringstream(tokens.at(4)) >> pfan_ctrl_params_[fc_idx].duty_cycle_min;
  std::istringstream(tokens.at(5)) >> pfan_ctrl_params_[fc_idx].duty_cycle_max;
  std::istringstream(tokens.at(6)) >> pfan_ctrl_params_[fc_idx].pwm_step;
  return true;
}

}  // namespace bruno_platform_peripheral
