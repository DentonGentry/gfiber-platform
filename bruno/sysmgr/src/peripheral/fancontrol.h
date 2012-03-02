// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_
#define BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_

#include "base/constructormagic.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class FanControl {
 public:
  enum StateType {
    OFF,
    VAR_SPEED,
    FULL_SPEED
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

  explicit FanControl(uint32_t channel)
      : pwm_channel_(channel),
        pwm_handle_(NULL),
        state_(OFF),
        auto_mode_(true),
        var_speed_on_(false),
        lut_enabled_(true),
        self_start_enabled_(false),
        duty_cycle_min_(0x59),
        duty_cycle_max_(0xbf),
        duty_cycle_regulated_(0x00),
        duty_cycle_pwm_(0x00),
        duty_cycle_startup_(0x87),
        duty_cycle_slope_(0x16),
        duty_cycle_intercept_(0x0a),
        period_(0xfe),
        diff_max_(0x02),
        diff_min_(0x05) {}

  virtual ~FanControl();

  bool Init(void);
  void Terminate(void);
  bool SelfStart(void);
  bool AdjustSpeed(uint32_t avg_temp);
  bool DrivePwm(uint16_t duty_cycle);

  void set_self_start_enabled(bool enabled) {
    self_start_enabled_ = enabled;
  }

  bool self_start_enabled(void) const {
    return self_start_enabled_;
  }

 private:

  bool InitPwm(void);
  void ComputeDutyCycle(uint32_t avg_temp, uint16_t *new_duty_cycle_pwm);

  uint32_t pwm_channel_;
  NEXUS_PwmChannelHandle pwm_handle_;
  StateType state_;
  bool auto_mode_;
  bool var_speed_on_;
  bool lut_enabled_;
  bool self_start_enabled_;
  uint16_t duty_cycle_min_;
  uint16_t duty_cycle_max_;
  uint16_t duty_cycle_regulated_;
  uint16_t duty_cycle_pwm_;
  uint16_t duty_cycle_startup_;
  uint16_t duty_cycle_slope_;
  uint16_t duty_cycle_intercept_;
  /*
   * Period = period_ + 1 where period_ is the register value in chip.
   * (I have no idea why BRCM need it to be one short...), in this class, the
   * period_ value is the value you will set in register. But the real Period is
   * period_+1 mathmatically.
   * To bump up the CPU with full duty cycle, the On register needs to be set as
   * Period a.k.a period_+1.
   */
  uint16_t period_;
  uint16_t diff_max_;
  uint16_t diff_min_;

  DISALLOW_COPY_AND_ASSIGN(FanControl);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_
