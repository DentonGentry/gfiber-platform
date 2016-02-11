// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_
#define BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_

#include "bruno/constructormagic.h"
#include "platform.h"
#include "mailbox.h"

namespace bruno_platform_peripheral {

class Platform;

#define DUTY_CYCLE_PWM_MIN_VALUE    0
#define DUTY_CYCLE_PWM_MAX_VALUE    100


typedef struct FanControlParams {
  uint16_t  temp_setpt;
  uint16_t  temp_max;
  uint16_t  temp_step;
  uint16_t  duty_cycle_min;
  uint16_t  duty_cycle_max;
  uint16_t  pwm_step;
  uint16_t  temp_overheat;

  FanControlParams& operator = (const FanControlParams& param) {
    temp_setpt = param.temp_setpt;
    temp_max = param.temp_max;
    temp_step = param.temp_step;
    duty_cycle_min = param.duty_cycle_min;
    duty_cycle_max = param.duty_cycle_max;
    pwm_step = param.pwm_step;
    temp_overheat = param.temp_overheat;
    return *this;
  }

}FanControlParams;


class FanControl : public Mailbox {
 public:
  enum StateType {
    OFF,
    VAR_SPEED,
    FULL_SPEED
  };

  enum FanControlParamsTypes {
    BRUNO_SOC = 0,
    BRUNO_IS_HDD,
    BRUNO_PARAMS_TYPES
  };

  static const unsigned int kPwmDefaultStartup;
  static const unsigned int kPwmMinValue;
  static const unsigned int kPwmMaxValue;
  static const unsigned int kFanSpeedNotSpinning;

  static const FanControlParams kGFMS100FanCtrlSocDefaults;
  static const FanControlParams kGFMS100FanCtrlHddDefaults;

  static const FanControlParams kGFRG200FanCtrlSocDefaults;

  static const FanControlParams kGFRG210FanCtrlSocDefaults;
  static const FanControlParams kGFRG210FanCtrlHddDefaults;

  static const FanControlParams kGFRG250FanCtrlSocDefaults;
  static const FanControlParams kGFRG250FanCtrlHddDefaults;

  static const FanControlParams kGFSC100FanCtrlSocDefaults;
  static const FanControlParams kGFSC100FanCtrlHddDefaults;

  static const FanControlParams kGFHD100FanCtrlSocDefaults;
  static const FanControlParams kGFHD200FanCtrlSocDefaults;
  static const FanControlParams kGFHD254FanCtrlSocDefaults;

  static const FanControlParams kGFLT110FanCtrlSocDefaults;

  explicit FanControl(Platform *platform)
      : state_(OFF),
        auto_mode_(true),
        duty_cycle_pwm_(kPwmMinValue),
        duty_cycle_startup_(kPwmDefaultStartup),
        period_(DUTY_CYCLE_PWM_MAX_VALUE-1),
        platform_(BRUNO_GFHD100),
        pfan_ctrl_params_(NULL),
        platformInstance_(platform) {}

  virtual ~FanControl();

  bool Init(bool *gpio_mailbox_ready);
  void Terminate(void);
  bool DrivePwm(uint16_t duty_cycle);
  bool AdjustSpeed(uint16_t soc_temp, uint16_t hdd_temp, uint16_t fan_speed);
  void GetHddTemperature(uint16_t *phdd_temp);
  void GetOverheatTemperature(uint16_t *poverheat_temp);

 private:

  void InitParams(void);
  std::string ExecCmd(char* cmd, std::string *pattern);
  void ComputeDutyCycle(uint16_t soc_temp, uint16_t hdd_temp,
                        uint16_t fan_speed, uint16_t *new_duty_cycle_pwm);

  void dbgUpdateFanControlParams(void);
  bool dbgGetFanControlParamsFromParamsFile(uint8_t fc_idx);

  StateType state_;
  bool auto_mode_;
  uint16_t duty_cycle_pwm_;     /* current pwm duty cycle */
  uint16_t duty_cycle_startup_; /* initial duty cycle */
  /*
   * Period = period_ + 1 where period_ is the register value in chip.
   * (I have no idea why BRCM need it to be one short...), in this class, the
   * period_ value is the value you will set in register. But the real Period is
   * period_+1 mathmatically.
   * To bump up the CPU with full duty cycle, the On register needs to be set as
   * Period a.k.a period_+1.
   */
  uint16_t period_;

  /* Fan control parameters table
   * idx BRUNO_SOC: depending upon GFMS100 (Bruno-IS) or GFHD100 (Thin Bruno);
   * idx BRUNO_IS_HDD: use by HDD GFMS100
   * */
  enum BrunoPlatformTypes platform_;
  FanControlParams *pfan_ctrl_params_;
  Platform *platformInstance_;

  FanControlParams *get_hdd_fan_ctrl_parms();
  bool if_hdd_temp_over_temp_max(const uint16_t hdd_temp, const FanControlParams *phdd) const;
  bool if_hdd_temp_over_temp_setpt(const uint16_t hdd_temp, const FanControlParams *phdd) const;
  bool if_hdd_temp_lower_than_temp_setpt(const uint16_t hdd_temp, const FanControlParams *phdd) const;

  DISALLOW_COPY_AND_ASSIGN(FanControl);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_
