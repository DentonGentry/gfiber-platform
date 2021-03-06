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

#define MAX(a,b)        (((a) > (b) ? (a) : (b)))

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
                          pwm_step      : 1,
                          temp_overheat : 120,
                        };

const FanControlParams FanControl::kGFMS100FanCtrlHddDefaults = {
                          temp_setpt    : 56,
                          temp_max      : 60,
                          temp_step     : 2,
                          duty_cycle_min: 25,
                          duty_cycle_max: 100,
                          pwm_step      : 1,
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
                          pwm_step      : 1,
                          temp_overheat : 105,
                        };

const FanControlParams FanControl::kGFRG210FanCtrlSocDefaults = {
                          temp_setpt    : 86,   // fan on @ 89 (cpu =~ 93)
                          temp_max      : 94,   // cpu =~ 100
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 1,
                          temp_overheat : 105,
                        };

const FanControlParams FanControl::kGFRG210FanCtrlHddDefaults = {
                          temp_setpt    : 56,
                          temp_max      : 60,
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 1,
                          temp_overheat : 105,
                        };
/*
 * Defaults of Fan control parameters for GFRG250 (Optimus Prime)
 * There is no direct SOC temp input, so we use the remote sensor.
 * Thermal policy can be found at b/23119698
 */

const FanControlParams FanControl::kGFRG250FanCtrlSocDefaults = {
                          temp_setpt    : 76,
                          temp_max      : 88,
                          temp_step     : 3,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 105,
                        };

const FanControlParams FanControl::kGFRG250FanCtrlHddDefaults = {
                          temp_setpt    : 55,
                          temp_max      : 60,
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 1,
                          temp_overheat : 105,
                        };

/*
 * On Optimus Prime, AUX1 refers to the temperature sensor in the Quantenna SoC
 * which controls the 11ac wifi interface. The granularity of the temperature
 * readings are very coarse: increments of 5C.
 */
const FanControlParams FanControl::kGFRG250FanCtrlAux1Defaults = {
                          temp_setpt    : 90,
                          temp_max      : 109, /* fan speed is set to max when
                                                  temperatures reaches 110C */
                          temp_step     : 9,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 120,
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
                          pwm_step      : 1,
                          temp_overheat : 105,
                        };

const FanControlParams FanControl::kGFSC100FanCtrlHddDefaults = {
                          temp_setpt    : 56,
                          temp_max      : 60,
                          temp_step     : 2,
                          duty_cycle_min: 30,
                          duty_cycle_max: 100,
                          pwm_step      : 1,
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

const FanControlParams FanControl::kGFHD200FanCtrlSocDefaults = {
                          temp_setpt    : 0,  /* No fan */
                          temp_max      : 0,
                          temp_step     : 0,
                          duty_cycle_min: 0,
                          duty_cycle_max: 0,
                          pwm_step      : 0,
                          temp_overheat : 120,
                        };

const FanControlParams FanControl::kGFHD254FanCtrlSocDefaults = {
                          temp_setpt    : 88,
                          temp_max      : 105,
                          temp_step     : 3,
                          duty_cycle_min: 25,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 120,
                        };

/*
 * AUX1 refers to the temperature sensor in the Quantenna SoC
 * which controls the 11ac wifi interface. The granularity of the temperature
 * readings are very coarse: increments of 5C.
 */
const FanControlParams FanControl::kGFHD254FanCtrlAux1Defaults = {
                          temp_setpt    : 94,
                          temp_max      : 110,
                          temp_step     : 3,
                          duty_cycle_min: 25,
                          duty_cycle_max: 100,
                          pwm_step      : 2,
                          temp_overheat : 120,
                        };

const FanControlParams FanControl::kGFLT110FanCtrlSocDefaults = {
                          temp_setpt    : 0,  /* No fan */
                          temp_max      : 0,
                          temp_step     : 0,
                          duty_cycle_min: 0,
                          duty_cycle_max: 0,
                          pwm_step      : 0,
                          temp_overheat : 97,
                        };

const FanControlParams FanControl::kGFLT300FanCtrlSocDefaults = {
                          temp_setpt    : 0,  /* No fan */
                          temp_max      : 0,
                          temp_step     : 0,
                          duty_cycle_min: 0,
                          duty_cycle_max: 0,
                          pwm_step      : 0,
                          temp_overheat : 97,
                        };

const FanControlParams FanControl::kGFLT400FanCtrlSocDefaults = {
                          temp_setpt    : 0,  /* No fan */
                          temp_max      : 0,
                          temp_step     : 0,
                          duty_cycle_min: 0,
                          duty_cycle_max: 0,
                          pwm_step      : 0,
                          temp_overheat : 97,
                        };

const FanControlParams FanControl::kGFCH100FanCtrlSocDefaults = {
                          temp_setpt    : 0,  /* No fan */
                          temp_max      : 0,
                          temp_step     : 0,
                          duty_cycle_min: 0,
                          duty_cycle_max: 0,
                          pwm_step      : 0,
                          temp_overheat : 125,
                        };

FanControl::~FanControl() {
  Terminate();
}

bool FanControl::Init(bool *gpio_mailbox_ready) {

  /* Check if the platform instance has been initialized
   * 1) If run sysmgr,  the platform_ would be initalized in
   *    platformperipheral module.
   * 2) If run test_fan test util, the platformperipheral module won't be used.
   */

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
}

void FanControl::InitParams() {
  pfan_ctrl_params_ = new FanControlParams[BRUNO_PARAMS_TYPES_MAX];

  switch (platform_->PlatformType()) {
    case BRUNO_GFMS100:
      /* Set thermal fan policy parameters of GFMS100 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFMS100FanCtrlSocDefaults;
      pfan_ctrl_params_[BRUNO_IS_HDD] = kGFMS100FanCtrlHddDefaults;
      break;
    case BRUNO_GFHD100:
      /* Set thermal fan policy parameters of GFHD100 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFHD100FanCtrlSocDefaults;
      break;
    case BRUNO_GFHD200:
      pfan_ctrl_params_[BRUNO_SOC] = kGFHD200FanCtrlSocDefaults;
      break;
    case BRUNO_GFHD254:
      pfan_ctrl_params_[BRUNO_SOC] = kGFHD254FanCtrlSocDefaults;
      pfan_ctrl_params_[BRUNO_AUX1] = kGFHD254FanCtrlAux1Defaults;
      break;
    case BRUNO_GFRG200:
      /* Set thermal fan policy parameters of GFRG200 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFRG200FanCtrlSocDefaults;
      break;
    case BRUNO_GFRG210:
      /* Set thermal fan policy parameters of GFRG210 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFRG210FanCtrlSocDefaults;
      pfan_ctrl_params_[BRUNO_IS_HDD] = kGFRG210FanCtrlHddDefaults;
      break;
    case BRUNO_GFRG250:
      /* Set thermal fan policy parameters of GFRG250 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFRG250FanCtrlSocDefaults;
      pfan_ctrl_params_[BRUNO_IS_HDD] = kGFRG250FanCtrlHddDefaults;
      pfan_ctrl_params_[BRUNO_AUX1] = kGFRG250FanCtrlAux1Defaults;
      break;
    case BRUNO_GFSC100:
      /* Set thermal fan policy parameters of GFSC100 */
      pfan_ctrl_params_[BRUNO_SOC] = kGFSC100FanCtrlSocDefaults;
      pfan_ctrl_params_[BRUNO_IS_HDD] = kGFSC100FanCtrlHddDefaults;
      break;
    case BRUNO_GFLT110:
      pfan_ctrl_params_[BRUNO_SOC] = kGFLT110FanCtrlSocDefaults;
      break;
    case BRUNO_GFLT300:
      pfan_ctrl_params_[BRUNO_SOC] = kGFLT300FanCtrlSocDefaults;
      break;
    case BRUNO_GFLT400:
      pfan_ctrl_params_[BRUNO_SOC] = kGFLT400FanCtrlSocDefaults;
      break;
    case BRUNO_GFCH100:
      pfan_ctrl_params_[BRUNO_SOC] = kGFCH100FanCtrlSocDefaults;
      break;
    case BRUNO_UNKNOWN:
      LOG(LS_ERROR) << "Invalid platform type, ignore ... " << platform_;
      break;
  }

  /* Check if an external fan control parameter table existing */
  dbgUpdateFanControlParams();

  /* Adjust the fan control parameters for calculation. */
  for (int i = 0; i < BRUNO_PARAMS_TYPES_MAX; i++) {
    const char *suffix;
    switch(i) {
      case BRUNO_SOC:
        suffix = "_SOC";
        break;

      case BRUNO_IS_HDD:
        suffix = "_HDD";
        if (!platform_->has_hdd()) {
          LOG(LS_INFO) << "platform does not have hdd.";
          continue;
        }
        break;

      case BRUNO_AUX1:
        suffix = "_AUX1";
        if (!platform_->has_aux1()) {
          LOG(LS_INFO) << "platform does not have aux1.";
          continue;
        }
        break;

      default:
        suffix = "_UNKNOWN";
        LOG(LS_ERROR) << "Unknown type in fan param array";
        continue;
    }

    LOG(LS_INFO)
        << platform_->PlatformName()
        << suffix << std::endl
        << " Tsetpt: "    << pfan_ctrl_params_[i].temp_setpt << std::endl
        << " Tmax: "      << pfan_ctrl_params_[i].temp_max << std::endl
        << " Tstep: "     << pfan_ctrl_params_[i].temp_step << std::endl
        << " Dmin: "      << pfan_ctrl_params_[i].duty_cycle_min << std::endl
        << " Dmax: "      << pfan_ctrl_params_[i].duty_cycle_max << std::endl
        << " PWMstep: "   << pfan_ctrl_params_[i].pwm_step << std::endl
        << " Toverheat: " << pfan_ctrl_params_[i].temp_overheat << std::endl;
  }
}


bool FanControl::AdjustSpeed(
      uint16_t soc_temp, uint16_t hdd_temp, uint16_t aux1_temp,
      uint16_t fan_speed) {
  bool ret = true;
  uint16_t new_duty_cycle_pwm;

  LOG(LS_VERBOSE) << __func__ << ": soc_temp=" << soc_temp
                  << " hdd_temp=" << hdd_temp << " aux1_temp=" << aux1_temp
                  << " fan_speed=" << fan_speed;

  do {
    /* Get new SOC PWM per the current SOC and HDD temperatures */

    /* Get new duty cycle per SOC and HDD temperatures */
    ComputeDutyCycle(soc_temp, hdd_temp, aux1_temp, fan_speed,
        &new_duty_cycle_pwm);

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

  if (platform_->has_hdd() == true) {
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

uint16_t FanControl::__ComputeDutyCycle(
  uint16_t temp,
  uint16_t fan_speed,
  const FanControlParams &params) {

  uint16_t  compute_duty_cycle = duty_cycle_pwm_;
  if (temp > params.temp_max) {
    compute_duty_cycle = params.duty_cycle_max;
  }
  else if (temp > (params.temp_setpt + params.temp_step)) {
    if (fan_speed == kFanSpeedNotSpinning) {
      compute_duty_cycle = params.duty_cycle_min;
    }
    else if (duty_cycle_pwm_ < params.duty_cycle_max) {
      /* 1. Possibly, the fan still stops due to duty_cycle_pwm_ is not large
       *    enough. Continue increase the duty cycle.
       * 2. Or the fan is running, but it's not fast enough to cool down
       *    the unit.
       */
      compute_duty_cycle = duty_cycle_pwm_ + params.pwm_step;
      if (compute_duty_cycle > params.duty_cycle_max)
        compute_duty_cycle = params.duty_cycle_max;
    }
  }
  else if (temp < (params.temp_setpt - params.temp_step)) {
    if ((fan_speed == kFanSpeedNotSpinning) ||
        (duty_cycle_pwm_ < params.pwm_step)) {
      compute_duty_cycle = kPwmMinValue;
    }
    else {
      /* Reduce fan pwm if temp is lower than
       * the (temp_setpt - temp_step) and plus fan is still spinning
       */
      compute_duty_cycle = duty_cycle_pwm_ - params.pwm_step;
    }
  }
  return compute_duty_cycle;
}

void FanControl::ComputeDutyCycle(
  uint16_t soc_temp,
  uint16_t hdd_temp,
  uint16_t aux1_temp,
  uint16_t fan_speed,
  uint16_t *new_duty_cycle_pwm) {

  uint16_t  soc_compute_duty_cycle = 0;
  uint16_t  hdd_compute_duty_cycle = 0;
  uint16_t  aux1_compute_duty_cycle = 0;
  FanControlParams  *psoc = &pfan_ctrl_params_[BRUNO_SOC];
  FanControlParams  *phdd = get_hdd_fan_ctrl_parms();
  FanControlParams  *paux1 = get_aux1_fan_ctrl_parms();

  LOG(LS_VERBOSE) << __func__ << " - duty_cycle_pwm_ = " << duty_cycle_pwm_
               << " i/p soc_temp=" << soc_temp
               << " hdd_temp="     << hdd_temp
               << " aux1_temp="    << aux1_temp
               << " fan_speed="    << fan_speed;

  /* check SOC temps */
  if (psoc) {
    soc_compute_duty_cycle = __ComputeDutyCycle(soc_temp, fan_speed, *psoc);
  }

  /* check HDD temps */
  if (phdd) {
    hdd_compute_duty_cycle = __ComputeDutyCycle(hdd_temp, fan_speed, *phdd);
  }

  /* check HDD temps */
  if (paux1) {
    aux1_compute_duty_cycle = __ComputeDutyCycle(aux1_temp, fan_speed, *paux1);
  }

  LOG(LS_INFO) << "soc_duty_cycle_pwm = " << soc_compute_duty_cycle << " "
               << "hdd_duty_cycle_pwm = " << hdd_compute_duty_cycle << " "
               << "aux1_duty_cycle_pwm = " << aux1_compute_duty_cycle;

  *new_duty_cycle_pwm = MAX(soc_compute_duty_cycle, hdd_compute_duty_cycle);
  *new_duty_cycle_pwm = MAX(*new_duty_cycle_pwm, aux1_compute_duty_cycle);

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
  if (platform_->has_hdd() == true) {
    return &pfan_ctrl_params_[BRUNO_IS_HDD];
  }
  return NULL;
}

FanControlParams *FanControl::get_aux1_fan_ctrl_parms() {
  if (platform_->has_aux1() == true) {
    return &pfan_ctrl_params_[BRUNO_AUX1];
  }
  return NULL;
}


void FanControl::dbgUpdateFanControlParams(void) {
  /* Check if the external fan control parameter table existing */
  std::ifstream params_table_file (FAN_CONTROL_PARAMS_FILE);
  if (params_table_file.is_open()) {
    LOG(LS_INFO) << FAN_CONTROL_PARAMS_FILE << " existing...";
    dbgGetFanControlParamsFromParamsFile(BRUNO_SOC);
    if (platform_->has_hdd() == true) {
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
  std::string buf = platform_->PlatformName();
  switch (fc_idx) {
    case BRUNO_SOC:
      buf += "_SOC";
      break;
    case BRUNO_IS_HDD:
      buf += "_HDD";
      break;
    case BRUNO_AUX1:
      buf += "_AUX1";
      break;
    default:
      buf += "_UNKNOWN";
      LOG(LS_WARNING) << "Invalid fc_index: " << fc_idx << std::endl;
      break;
  }

  LOG(LS_INFO) << buf << std::endl;

  std::string result = platform_->GetLine((char *)FAN_CONTROL_PARAMS_FILE, &buf);
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
