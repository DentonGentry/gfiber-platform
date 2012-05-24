// Copyright 2012 Google Inc. All Rights Reserved.
// Author: roylou@google.com (Roy Lou)

#ifndef BRUNO_PLATFORM_PERIPHERAL_UNMUTE_H_
#define BRUNO_PLATFORM_PERIPHERAL_UNMUTE_H_

#include "atomic.h"
#include "base/constructormagic.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class Unmute {
 public:
  Unmute() : unmute_(new GpIo(GpIoConfig::kTable[GpIoConfig::GPIO_UNMUTE])) {}

  virtual ~Unmute() {delete unmute_;}
  void Terminate();
  bool Init();


 private:
  GpIo *unmute_;
  DISALLOW_COPY_AND_ASSIGN(Unmute);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_UNMUTE_H_
