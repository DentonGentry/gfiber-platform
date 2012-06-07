// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_LEDSTANDBY_H_
#define BRUNO_PLATFORM_PERIPHERAL_LEDSTANDBY_H_

#include "bruno/constructormagic.h"
#include "platformnexus.h"
#include "ledctrl.h"

namespace bruno_platform_peripheral {

class LedStandby : public LedCtrl {
 public:
  LedStandby () {
    AddLed(new GpIo(GpIoConfig::kTable[GpIoConfig::GPIO_LED_STANDBY0]));
  }

  virtual ~LedStandby();

 private:
  DISALLOW_COPY_AND_ASSIGN(LedStandby);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_LEDSTANDBY_H_
