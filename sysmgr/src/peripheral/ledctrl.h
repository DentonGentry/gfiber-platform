// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_LEDCTRL_H_
#define BRUNO_PLATFORM_PERIPHERAL_LEDCTRL_H_

#include "base/constructormagic.h"
#include "platformnexus.h"
#include <list>

namespace bruno_platform_peripheral {

class LedCtrl {
 public:
  LedCtrl () {
  }

  virtual ~LedCtrl();

  void AddLed(GpIo* led);

  virtual void Init(void);
  virtual void Terminate(void);

  virtual void TurnOn(void);
  virtual void TurnOff(void);

 protected:
  std::list<GpIo*> led_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LedCtrl);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_LEDCTRL_H_
