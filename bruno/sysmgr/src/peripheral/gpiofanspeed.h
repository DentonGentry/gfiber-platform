// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BUNO_PLATFORM_PERIPHERAL_GPIOFANSPEED_H_
#define BUNO_PLATFORM_PERIPHERAL_GPIOFANSPEED_H_

#include "atomic.h"
#include "base/constructormagic.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class GpIoFanSpeed : public GpIo {
 public:
  GpIoFanSpeed ()
      : GpIo(GpIoConfig::kTable[GpIoConfig::GPIO_FAN_SPEED]) {
  }

  virtual ~GpIoFanSpeed();

  bool Init();

  unsigned int GetCounter(void) const { return count_; }
  void PegCounter(void) { atomic_increment(&count_); }
  unsigned int ResetCounter(void) { return atomic_exchange_acq(&count_, 0); }

 private:
  static void InterruptHandler(void *context, int param);

  unsigned int count_;
  DISALLOW_COPY_AND_ASSIGN(GpIoFanSpeed);
};

}  // namespace bruno_platform_peripheral

#endif // BUNO_PLATFORM_PERIPHERAL_GPIOFANSPEED_H_
