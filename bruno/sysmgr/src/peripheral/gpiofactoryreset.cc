// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "base/sigslot.h"
#include "gpioconfig.h"
#include "gpio.h"
#include "gpiofactoryreset.h"

namespace bruno_platform_peripheral {

GpIoFactoryReset::~GpIoFactoryReset() {
}

bool GpIoFactoryReset::Init() {
  if (!GpIo::Init()) {
    return false;
  }
  if (!RegisterInterrupt(
      (NEXUS_Callback)&GpIoFactoryReset::InterruptHandler, this, 0)) {
    return false;
  }
  return true;
}

void GpIoFactoryReset::InterruptHandler(void *context, int param) {
  NEXUS_GpioValue val;
  UNUSED(param);
  GpIoFactoryReset* gpio= reinterpret_cast<GpIoFactoryReset*>(context);
  if (NULL == gpio) {
    LOG(LS_ERROR) << "NULL gpio pointer";
    return;
  }

  if (!gpio->Read(&val)) {
    LOG(LS_ERROR) << "Failed to read value of factory reset button";
    return;
  }
  LOG(LS_VERBOSE) << "Signaling factory reset button event " << val;
  gpio->SignalButtonEvent(val);
}

}  // namespace bruno_platform_peripheral
