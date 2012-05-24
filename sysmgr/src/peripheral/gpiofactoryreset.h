// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BUNO_PLATFORM_PERIPHERAL_GPIOFACTORYRESET_H_
#define BUNO_PLATFORM_PERIPHERAL_GPIOFACTORYRESET_H_

#include "base/constructormagic.h"
#include "base/sigslot.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class GpIoFactoryReset : public GpIo {
 public:
  GpIoFactoryReset ()
      : GpIo(GpIoConfig::kTable[GpIoConfig::GPIO_FACTORY_RESET]) {
  }

  virtual ~GpIoFactoryReset();

  bool Init();

  sigslot::signal1<NEXUS_GpioValue> SignalButtonEvent;

 private:
  static void InterruptHandler(void *context, int param);

  DISALLOW_COPY_AND_ASSIGN(GpIoFactoryReset);
};

}  // namespace bruno_platform_peripheral

#endif // BUNO_PLATFORM_PERIPHERAL_GPIOFACTORYRESET_H_
