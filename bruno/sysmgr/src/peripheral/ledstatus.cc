// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "gpioconfig.h"
#include "gpio.h"
#include "ledstatus.h"

namespace bruno_platform_peripheral {

LedStatus::~LedStatus() {
}

void LedStatus::TurnOn(void) {
  return LedCtrl::TurnOn();
}

void LedStatus::SetPurple(void) {
  return TurnOn();
}

void LedStatus::SetRed(void) {
  for (std::list<GpIo*>::iterator i=led_list_.begin();
       i != led_list_.end(); ++i) {
    if ((*i)->GetConfig().pin_ == GpIoConfig::kTable[GpIoConfig::GPIO_LED_RED].pin_) {
      if (!(*i)->Write(NEXUS_GpioValue_eHigh))
        LOG(LS_WARNING) << "Failed to turn on " << (*i)->GetConfig().name_;
    } else {
      if (!(*i)->Write(NEXUS_GpioValue_eLow))
        LOG(LS_WARNING) << "Failed to turn off " << (*i)->GetConfig().name_;
    }
  }
}

void LedStatus::SetBlue(void) {
  for (std::list<GpIo*>::iterator i=led_list_.begin();
       i != led_list_.end(); ++i) {
    if ((*i)->GetConfig().pin_ == GpIoConfig::kTable[GpIoConfig::GPIO_LED_ACT_BLUE].pin_) {
      if (!(*i)->Write(NEXUS_GpioValue_eHigh))
        LOG(LS_WARNING) << "Failed to turn on " << (*i)->GetConfig().name_;
    } else {
      if (!(*i)->Write(NEXUS_GpioValue_eLow))
        LOG(LS_WARNING) << "Failed to turn off " << (*i)->GetConfig().name_;
    }
  }
}

}  // namespace bruno_platform_peripheral
