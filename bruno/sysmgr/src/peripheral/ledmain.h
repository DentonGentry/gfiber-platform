// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_LEDMAIN_H_
#define BRUNO_PLATFORM_PERIPHERAL_LEDMAIN_H_

#include "base/constructormagic.h"
#include "platformnexus.h"
#include "ledctrl.h"

namespace bruno_platform_peripheral {

class LedMain : public LedCtrl {
 public:
  LedMain () {
    AddLed(new GpIo(GpIoConfig::kTable[GpIoConfig::GPIO_LED_BLUE]));
  }

  virtual ~LedMain();

 private:
  DISALLOW_COPY_AND_ASSIGN(LedMain);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_LEDMAIN_H_
