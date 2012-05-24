// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BUNO_PLATFORM_PERIPHERAL_GPIO_H_
#define BUNO_PLATFORM_PERIPHERAL_GPIO_H_

#include "base/constructormagic.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class GpIoConfig;

class GpIo {
 public:
  explicit GpIo(const GpIoConfig& config)
      : handle_(NULL), config_(config) {
  }

  virtual ~GpIo();

  virtual bool Init();
  virtual void Terminate();
  bool Read(NEXUS_GpioValue* value);
  bool Write(NEXUS_GpioValue value);
  bool RegisterInterrupt(NEXUS_Callback isr, void *context, int param);
  const GpIoConfig& GetConfig (void) const { return config_; }

 private:
  NEXUS_GpioHandle handle_;
  const GpIoConfig& config_;
  DISALLOW_COPY_AND_ASSIGN(GpIo);
};

}  // namespace bruno_platform_peripheral

#endif // BUNO_PLATFORM_PERIPHERAL_GPIO_H_
