// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "base/sigslot.h"
#include "gpioconfig.h"
#include "gpio.h"
#include "gpiofanspeed.h"

namespace bruno_platform_peripheral {

GpIoFanSpeed::~GpIoFanSpeed() {
}

void GpIoFanSpeed::InterruptHandler(void *context, int param) {
  UNUSED(param);
  GpIoFanSpeed* gpio= reinterpret_cast<GpIoFanSpeed*>(context);
  if (NULL == gpio) {
    LOG(LS_ERROR) << "NULL gpio pointer";
    return;
  }

  gpio->PegCounter();
}

bool GpIoFanSpeed::Init() {
  if (!GpIo::Init()) {
    return false;
  }
  if (!RegisterInterrupt(
      (NEXUS_Callback)&GpIoFanSpeed::InterruptHandler, this, 0)) {
    return false;
  }
  return true;
}

}  // namespace bruno_platform_peripheral
