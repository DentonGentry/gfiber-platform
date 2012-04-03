// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_
#define BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_

#include "base/constructormagic.h"
#include "platformnexus.h"
#include "platform.h"

namespace bruno_platform_peripheral {

class Platform;

#define DUTY_CYCLE_MIN_VALUE      0
#define DUTY_CYCLE_MAX_VALUE      100

#define DUTY_CYCLE_PWM_MIN_VALUE  0
#define DUTY_CYCLE_PWM_MAX_VALUE  255


#define SOC_MULTI_VALUE_IN_FLOAT  1000.0
#define HDD_MULTI_VALUE_IN_FLOAT  100.0

#define MULTI_VALUE               100
/* Adjust the value back = x /(MULTI_VALUE)*/
#define ADJUST_VALUE(x)         ((x) / MULTI_VALUE)
#define ADJUST_VALUE_TWICE(x)   ((x) / (MULTI_VALUE * MULTI_VALUE))

/* Adjust the value back = x * (MULTI_VALUE)*/
#define TIMES_VALUE(x)          ((x) * MULTI_VALUE)

/* (MULTIPLE_VALUE) time of 1 PWM on per 1% duty cycle */
#define ONE_PWM_ON_PER_PCT      (TIMES_VALUE(DUTY_CYCLE_PWM_MAX_VALUE)/DUTY_CYCLE_MAX_VALUE)

/* alpha is MULTI_VALUE * real alpha */
#define ALPHA(Dmax, Dmin, Tmax, Tmin)   ((Dmax - Dmin)/(Tmax - Tmin))

/* Due to the temperature is an unsigned value,
 * 1. temp_min won't be less than 0 degC
 * 2. threshold can't be larger than temp_min
 */
#define GET_THRESHOLD(Tmin, Th) ((Th > Tmin)? Tmin : Th)

typedef struct FanControlParams {
  uint16_t  temp_min;
  uint16_t  temp_max;
  uint16_t  duty_cycle_min;
  uint16_t  duty_cycle_max;
  uint16_t  threshold;
  uint16_t  alpha;              /* alpha = delta(duty_cycle)/delta(temp) */

  FanControlParams& operator = (const FanControlParams& param) {
    temp_min = param.temp_min;
    temp_max = param.temp_max;
    duty_cycle_min = param.duty_cycle_min;
    duty_cycle_max = param.duty_cycle_max;
    threshold = param.threshold;
    alpha = param.alpha;
    return *this;
  }

}FanControlParams;


class FanControl {
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


  /*
   * Set the control word to clock rate.
   * a unit is 411.7 Hz.
   * PWM Freq = clock_freq/period
   *
   * Unit   | Clock rate | Period | PWM Freq
   * =======================================
   * 0x7900 |  12.75Mhz  | 255    | 50Khz
   * 0x4000 |  6.75Mhz   | 255    | 26.47Khz
   * 0x0080 |  52.7Khz   | 255    | 206.6Hz
   */
  static const unsigned int kPwmFreq50Khz;
  static const unsigned int kPwmFreq26Khz;
  static const unsigned int kPwmFreq206hz;
  static const unsigned int kPwmDefaultTemperatureScale;
  static const unsigned int kPwmDefaultDutyCycleScale;

  static const FanControlParams kGFMS100FanCtrlSocDefaults;
  static const FanControlParams kGFMS100FanCtrlHddDefaults;
  static const FanControlParams kGFHD100FanCtrlSocDefaults;

  explicit FanControl(uint32_t channel, Platform *platform)
      : pwm_channel_(channel),
        pwm_handle_(NULL),
        state_(OFF),
        auto_mode_(true),
        var_speed_on_(false),
        lut_enabled_(true),
        self_start_enabled_(false),
        duty_cycle_scale_(kPwmDefaultDutyCycleScale),
        duty_cycle_min_(0x5A),
        duty_cycle_max_(0x5A),
        duty_cycle_regulated_(0x00),
        duty_cycle_pwm_(0x5A),
        duty_cycle_startup_(0x87),
        temperature_scale_(kPwmDefaultTemperatureScale),
        temperature_min_(0x33),
        temperature_max_(0xcc),
        period_(0xfe),
        step_(0x02),
        threshold_(0x05),
        platform_(BRUNO_GFHD100),
        pfan_ctrl_params_(NULL),
        allocatedPlatformInstanceLocal_(false),
        platformInstance_(platform) {}

  virtual ~FanControl();

  bool Init(uint8_t min_temp=0, uint8_t max_temp=0, uint8_t n_levels=0);
  void Terminate(void);
  bool SelfStart(void);
  bool AdjustSpeed(uint32_t avg_temp);
  bool DrivePwm(uint16_t duty_cycle);
  bool AdjustSpeed_PControl(uint16_t soc_temp, uint16_t hdd_temp);
  void GetHddTemperature(uint16_t *phdd_temp);

  void set_self_start_enabled(bool enabled) {
    self_start_enabled_ = enabled;
  }

  bool self_start_enabled(void) const {
    return self_start_enabled_;
  }

 private:

  bool InitPwm(void);
  void ComputeDutyCycle(uint32_t avg_temp, uint16_t *new_duty_cycle_pwm);
  void InitParams(void);
  std::string ExecCmd(char* cmd);
  void ComputeDutyCycle_PControl(uint16_t temp, uint16_t *new_duty_cycle_pwm, uint8_t idx);
  void dbgUpdateFanControlParams(void);
  bool dbgGetFanControlParamsFromParamsFile(uint8_t fc_idx);

  uint32_t pwm_channel_;
  NEXUS_PwmChannelHandle pwm_handle_;
  StateType state_;
  bool auto_mode_;
  bool var_speed_on_;
  bool lut_enabled_;
  bool self_start_enabled_;
  uint16_t duty_cycle_scale_;  /* duty cycle scale */
  uint16_t duty_cycle_min_;  /* minimum duty cycle */
  uint16_t duty_cycle_max_;  /* maximum duty cycle */
  uint16_t duty_cycle_regulated_;  /* current regulated duty cycle */
  uint16_t duty_cycle_pwm_;  /* current pwm duty cycle */
  uint16_t duty_cycle_startup_;  /* initial duty cycle */
  uint16_t temperature_scale_;  /* temperature scale */
  uint16_t temperature_min_;  /* minimum temperature */
  uint16_t temperature_max_;  /* maximum temperature */
  /*
   * Period = period_ + 1 where period_ is the register value in chip.
   * (I have no idea why BRCM need it to be one short...), in this class, the
   * period_ value is the value you will set in register. But the real Period is
   * period_+1 mathmatically.
   * To bump up the CPU with full duty cycle, the On register needs to be set as
   * Period a.k.a period_+1.
   */
  uint16_t period_;
  uint16_t step_; /* The amount of change to increment/decrement per duty cycle change */
  uint16_t threshold_; /* The threshold to affect duty cycle change */

  /* Fan control parameters table
   * idx BRUNO_SOC: depending upon GFMS100 (Bruno-IS) or GFHD100 (Thin Bruno);
   * idx BRUNO_IS_HDD: use by HDD GFMS100
   * */
  enum BrunoPlatformTypes platform_;
  FanControlParams *pfan_ctrl_params_;
  bool allocatedPlatformInstanceLocal_;
  Platform *platformInstance_;

  DISALLOW_COPY_AND_ASSIGN(FanControl);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_
