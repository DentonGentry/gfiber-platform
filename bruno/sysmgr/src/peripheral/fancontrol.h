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
   * PWM Freq = clock_freq/255
   *
   * Unit   | Clock rate | PWM Freq
   * =================================
   * 0x7900 |  12.75Mhz  | 50Khz
   * 0x4000 |  6.75Mhz   | 26.47Khz
   * 0x0080 |  52.7Khz   | 206.6Hz
   */
  static const unsigned int kPwmFreq50Khz;

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
        sample_period_(0x02),
        diff_max_(0x02),
        diff_min_(0x05) {
      }

  virtual ~FanControl();

  bool Init(void);
  void Terminate(void);
  bool SelfStart(void);
  bool AdjustSpeed(uint8_t avg_temp);
  bool DrivePwm(uint8_t duty_cycle);

  void set_self_start_enabled(bool enabled) {
    self_start_enabled_ = enabled;
  }

  bool self_start_enabled(void) const {
    return self_start_enabled_;
  }

  void set_sample_period_(uint32_t sample_period) {
    sample_period_ = sample_period;
  }

  uint32_t get_sample_period(void) const {
    return sample_period_;
  }

 private:

  static void InterruptHandler(void *context, int param);

  bool InitPwm(void);
  void ComputeDutyCycle(uint8_t avg_temp, uint8_t *new_duty_cycle_pwm);

  uint32_t pwm_channel_;
  NEXUS_PwmChannelHandle pwm_handle_;
  StateType state_;
  bool auto_mode_;
  bool var_speed_on_;
  bool lut_enabled_;
  bool self_start_enabled_;
  uint32_t duty_cycle_min_;
  uint32_t duty_cycle_max_;
  uint32_t duty_cycle_regulated_;
  uint32_t duty_cycle_pwm_;
  uint32_t duty_cycle_startup_;
  uint32_t duty_cycle_slope_;
  uint32_t duty_cycle_intercept_;
  uint32_t sample_period_;
  uint32_t diff_max_;
  uint32_t diff_min_;

  DISALLOW_COPY_AND_ASSIGN(FanControl);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_FANCONTROL_H_
