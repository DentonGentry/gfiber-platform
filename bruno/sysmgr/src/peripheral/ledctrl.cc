// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "gpioconfig.h"
#include "gpio.h"
#include "ledctrl.h"

namespace bruno_platform_peripheral {

LedCtrl::~LedCtrl() {
  while (!led_list_.empty()) {
    GpIo* led = led_list_.front();
    if (led) {
      led->Terminate();
      delete led;
    }
    led_list_.pop_front();
  }
}

void LedCtrl::AddLed(GpIo* led) {
  led_list_.push_back(led);
}

void LedCtrl::Init(void) {
  for (std::list<GpIo*>::iterator i=led_list_.begin();
       i != led_list_.end(); ++i) {
    if (!(*i)->Init()) {
      LOG(LS_WARNING) << "Failed to open " << (*i)->GetConfig().name_;
    }
  }
}

void LedCtrl::Terminate(void) {
  for (std::list<GpIo*>::iterator i=led_list_.begin();
       i != led_list_.end(); ++i) {
    (*i)->Terminate();
  }
}

void LedCtrl::TurnOn(void) {
  for (std::list<GpIo*>::iterator i=led_list_.begin();
       i != led_list_.end(); ++i) {
    if (!(*i)->Write(NEXUS_GpioValue_eHigh))
      LOG(LS_WARNING) << "Failed to turn on " << (*i)->GetConfig().name_;
  }
}

void LedCtrl::TurnOff(void) {
  for (std::list<GpIo*>::iterator i=led_list_.begin();
       i != led_list_.end(); ++i) {
    if (!(*i)->Write(NEXUS_GpioValue_eLow))
      LOG(LS_WARNING) << "Failed to turn off " << (*i)->GetConfig().name_;
  }
}

}  // namespace bruno_platform_peripheral
