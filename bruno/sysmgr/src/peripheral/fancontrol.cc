// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "fancontrol.h"

namespace bruno_platform_peripheral {

const unsigned int FanControl::kPwmFreq50Khz = 0x7900;
const unsigned int FanControl::kPwmFreq26Khz = 0x4000;
const unsigned int FanControl::kPwmFreq206hz = 0x0080;

FanControl::~FanControl() {
  Terminate();
}

bool FanControl::Init() {
  NEXUS_PwmChannelSettings pwm_channel_settings;
  NEXUS_Pwm_GetDefaultChannelSettings(&pwm_channel_settings);
  pwm_channel_settings.eFreqMode = NEXUS_PwmFreqModeType_eConstant;
  pwm_handle_ = NEXUS_Pwm_OpenChannel(pwm_channel_, &pwm_channel_settings);
  if (NULL == pwm_handle_) {
    LOG(LS_ERROR) << "NEXUS_Pwm_OpenChannel failed";
    return false;
  }
  return InitPwm();
}

void FanControl::Terminate(void) {
  if (pwm_handle_) {
    NEXUS_Pwm_CloseChannel(pwm_handle_);
    pwm_handle_ = NULL;
  }
}

bool FanControl::InitPwm() {
  NEXUS_Error ret_code = NEXUS_SUCCESS;

  /*
   * Use constant frequency mode
   */
  ret_code = NEXUS_Pwm_SetFreqMode(pwm_handle_,NEXUS_PwmFreqModeType_eConstant);
  if (NEXUS_SUCCESS != ret_code) {
    LOG(LS_ERROR) << "NEXUS_Pwm_SetFreqMode failed - " << ret_code;
    return false;
  }

  ret_code = NEXUS_Pwm_SetControlWord(pwm_handle_,kPwmFreq50Khz);
  if (NEXUS_SUCCESS != ret_code) {
    LOG(LS_ERROR) << "NEXUS_Pwm_SetControlWord failed - " << ret_code;
    return false;
  }

  ret_code = NEXUS_Pwm_SetPeriodInterval(pwm_handle_, period_);
  if (NEXUS_SUCCESS != ret_code) {
    LOG(LS_ERROR) << "NEXUS_Pwm_SetPeriodInterval failed - " << ret_code;
    return false;
  }

  ret_code = NEXUS_Pwm_Start(pwm_handle_);
  if (NEXUS_SUCCESS != ret_code) {
    LOG(LS_ERROR) << "NEXUS_Pwm_Start failed - " << ret_code;
    return false;
  }

  if (!DrivePwm(0)) {
    LOG(LS_ERROR) << "FanControl::DrivePwm failed";
    return false;
  }

  return true;
}

bool FanControl::SelfStart() {
  bool ret = true;
  if (self_start_enabled_){
    /*
     * Drive the fan with fancontrol_dutyCycleStartup for 1 second
     */
    ret = DrivePwm(duty_cycle_startup_);
    if (!ret) {
      LOG(LS_ERROR) << "FanControl::DrivePwm failed";
      return false;
    }
    state_ = VAR_SPEED;

    BKNI_Sleep (1000);                  /* sleep for 1 second */
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

bool FanControl::DrivePwm(uint16_t duty_cycle) {
  NEXUS_Error ret_code=NEXUS_SUCCESS;

  LOG(LS_INFO) << "DrivePwm 0x" << std::hex << duty_cycle;
  duty_cycle_pwm_ = duty_cycle;
  ret_code = NEXUS_Pwm_SetOnInterval(pwm_handle_, duty_cycle);/* period is already set to 255 */
  if (NEXUS_SUCCESS != ret_code) {
    LOG(LS_ERROR) << "NEXUS_Pwm_SetOnInterval failed - " << ret_code;
    return false;
  }
  if (duty_cycle == 0) {
    state_ = OFF;
  } else if (duty_cycle == period_) {
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

  /*
   * Compute duty cyle: y = mx + b
   */
  /* TODO: Multiply the slope, integer and fraction parts */

  compute_duty_cycle = avg_temp * duty_cycle_slope_;
  compute_duty_cycle += duty_cycle_intercept_;

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
       * Difference must be greater than the min diff to affect a change
       * in duty cycle
       */
      if (diff >= diff_min_) {
        duty_cycle_regulated_ += diff_max_;    /* maximum step change */
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
       * Difference must be greater than the min diff to affect a change
       * in duty cycle
       */
      if (diff >= diff_min_) {
        duty_cycle_regulated_ -= diff_max_;    /* maximum step change */
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

  *new_duty_cycle_pwm = duty_cycle_regulated_;       /* do this for now until we get the linearization LUT */

  LOG(LS_INFO) << "new_duty_cycle_pwm = 0x" << std::hex << *new_duty_cycle_pwm;

  return;
}

}  // namespace bruno_platform_peripheral
