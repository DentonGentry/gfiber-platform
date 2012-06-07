// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_LEDSTATUS_H_
#define BRUNO_PLATFORM_PERIPHERAL_LEDSTATUS_H_

#include "bruno/constructormagic.h"
#include "platformnexus.h"
#include "ledctrl.h"

namespace bruno_platform_peripheral {

class LedStatus : public LedCtrl {
 public:
  LedStatus () {
    AddLed(new GpIo(GpIoConfig::kTable[GpIoConfig::GPIO_LED_RED]));
    AddLed(new GpIo(GpIoConfig::kTable[GpIoConfig::GPIO_LED_ACT_BLUE]));
  }

  virtual void SetRed(void);
  virtual void SetPurple(void);
  virtual void SetBlue(void);
  virtual ~LedStatus();

 private:
  virtual void TurnOn(void);
  DISALLOW_COPY_AND_ASSIGN(LedStatus);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_LEDSTATUS_H_
