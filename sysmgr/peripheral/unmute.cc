// Copyright 2012 Google Inc. All Rights Reserved.
// Author: roylou@google.com (Roy Lou)

#include "bruno/logging.h"
#include "bruno/sigslot.h"
#include "gpioconfig.h"
#include "gpio.h"
#include "unmute.h"

namespace bruno_platform_peripheral {

bool Unmute::Init() {
  if (!unmute_->Init()) {
    LOG(LS_WARNING) << "Failed to open " << unmute_->GetConfig().name_;
    return false;
  }
  return true;
}

void Unmute::Terminate() {
  unmute_->Terminate();
}

}  // namespace bruno_platform_peripheral
