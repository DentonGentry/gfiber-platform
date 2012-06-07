// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BUNO_PLATFORM_PERIPHERAL_GPIOCONFIG_H_
#define BUNO_PLATFORM_PERIPHERAL_GPIOCONFIG_H_

#include "bruno/constructormagic.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class GpIoConfig {
 public:
  /*
   * GPIO defintions for B2 boards:
   *
   * Bruno:
   *   Front view LEDs positions
   *   D20     D4621    D21  D21
   *
   * Name              | Schematics net name | Ref Des | Front view LED position
   * ==========================================================================
   * GPIO_LED_BLUE     | BT_B_LED_N          | D20     | The most left LED
   * GPIO_LED_RED      | 7425_R_LED_N        | D4621   | The 2nd left LED (dual color)
   * GPIO_LED_ACT_BLUE | 7425_ACT_LED_N      | D4621   | The 2nd left LED (dual color)
   * GPIO_LED_STANDBY0 | STANDBY_LED_N0      | D21/D22 | The 2 right LEDs
   *
   *
   * Bruno-IS:
   * Name              | Schematics net name | Ref Des
   * ==========================================================================
   * GPIO_LED_RED      | 7425_R_LED_N        | D4621-D4623 (dual color LEDs)
   * GPIO_LED_ACT_BLUE | 7425_ACT_LED_N      | D4621-D4623 (dual color LEDs)
   *
   */
  enum {
    GPIO_LED_STANDBY0,
    GPIO_LED_RED,
    GPIO_LED_ACT_BLUE,
    GPIO_LED_BLUE,
    GPIO_FAN_CONTROL,
    GPIO_FAN_SPEED,
    GPIO_UNMUTE,
    GPIO_FACTORY_RESET,
    GPIO_MAX_NUM
  };

  GpIoConfig(const char* name, NEXUS_GpioType type, unsigned int pin,
                      NEXUS_GpioMode mode, NEXUS_GpioInterrupt interrupt_mode,
                      NEXUS_GpioValue init_value)
      : name_(name), type_(type), pin_(pin), mode_(mode),
        interrupt_mode_(interrupt_mode), init_value_(init_value) {
  }

  static const GpIoConfig kTable[];

  // Save all accessors since the config is usually used as const.
  const char* name_;
  NEXUS_GpioType type_;
  unsigned int pin_;
  NEXUS_GpioMode mode_;
  NEXUS_GpioInterrupt interrupt_mode_;
  NEXUS_GpioValue init_value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpIoConfig);
};

}  // namespace bruno_platform_peripheral

#endif // BUNO_PLATFORM_PERIPHERAL_GPIOCONFIG_H_
