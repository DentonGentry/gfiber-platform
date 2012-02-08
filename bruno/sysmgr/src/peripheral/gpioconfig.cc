// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "gpioconfig.h"

namespace bruno_platform_peripheral {

const GpIoConfig GpIoConfig::kTable[] = {
  // GPIO_LED_STANDBY0
  GpIoConfig("LED Standby 0",
             NEXUS_GpioType_eAonStandard, 10, NEXUS_GpioMode_eOutputPushPull,
             NEXUS_GpioInterrupt_eDisabled, NEXUS_GpioValue_eHigh),
  // GPIO_LED_STANDBY1
  GpIoConfig("LED Standby 1",
             NEXUS_GpioType_eAonStandard, 11, NEXUS_GpioMode_eOutputPushPull,
             NEXUS_GpioInterrupt_eDisabled, NEXUS_GpioValue_eHigh),
  // GPIO_LED_RED
  GpIoConfig("LED Red",
             NEXUS_GpioType_eAonStandard, 13, NEXUS_GpioMode_eOutputPushPull,
             NEXUS_GpioInterrupt_eDisabled, NEXUS_GpioValue_eMax),
  // GPIO_LED_GREEN
  GpIoConfig("LED Green",
             NEXUS_GpioType_eAonStandard, 17, NEXUS_GpioMode_eOutputPushPull,
             NEXUS_GpioInterrupt_eDisabled, NEXUS_GpioValue_eMax),
  // GPIO_LED_BLUE
  GpIoConfig("LED Blue",
             NEXUS_GpioType_eAonStandard, 12, NEXUS_GpioMode_eOutputPushPull,
             NEXUS_GpioInterrupt_eDisabled, NEXUS_GpioValue_eMax),
  // GPIO_FAN_CONTROL
  GpIoConfig("LED Fan Control",
             NEXUS_GpioType_eStandard, 89, NEXUS_GpioMode_eOutputPushPull,
             NEXUS_GpioInterrupt_eDisabled, NEXUS_GpioValue_eLow),
  // GPIO_FAN_SPEED
  GpIoConfig("LED Fan Speed",
             NEXUS_GpioType_eStandard, 98, NEXUS_GpioMode_eInput,
             NEXUS_GpioInterrupt_eFallingEdge, NEXUS_GpioValue_eMax),
  // GPIO_UNMUTE
  GpIoConfig("Unmute",
             NEXUS_GpioType_eStandard, 74, NEXUS_GpioMode_eOutputPushPull,
             NEXUS_GpioInterrupt_eDisabled, NEXUS_GpioValue_eHigh),
  // GPIO_FACTORY_RESET
  GpIoConfig("LED Factory Reset",
             NEXUS_GpioType_eAonStandard, 4, NEXUS_GpioMode_eInput,
             NEXUS_GpioInterrupt_eEdge, NEXUS_GpioValue_eMax),
};

}  // namespace bruno_platform_peripheral
