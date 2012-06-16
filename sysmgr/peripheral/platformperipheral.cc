// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "bruno/logging.h"
#include "bruno/scoped_ptr.h"
#include "bruno/criticalsection.h"
#include "fancontrol.h"
#include "flash.h"
#include "peripheralmon.h"
#include "platform_peripheral_api.h"
#include "platformperipheral.h"
#include "platform.h"
#include "ubifsmon.h"

namespace bruno_platform_peripheral {

PlatformPeripheral* PlatformPeripheral::kInstance_ = NULL;
bruno_base::CriticalSection PlatformPeripheral::kCrit_;

extern Platform* platformInstance_;

bool PlatformPeripheral::Init(unsigned int monitor_interval) {
  {
    bruno_base::CritScope lock(&kCrit_);
    if ((kInstance_ != NULL) || (platformInstance_ != NULL)) {
      LOG(LS_WARNING) << "Peripherals are already initialized...";
      return false;
    }
    LOG(LS_INFO) << "Init platformInstance_ in platformperipheral" << std::endl;
    platformInstance_ = new Platform ("Unknown Platform", BRUNO_UNKNOWN, false);
    kInstance_ = new PlatformPeripheral(platformInstance_);
  }
  /* Initialize platform */
  platformInstance_->Init();

  kInstance_->mgr_thread_ = bruno_base::Thread::Current();
  kInstance_->peripheral_mon_->Init(kInstance_->mgr_thread_, monitor_interval);
  kInstance_->ubifs_mon_->Init(kInstance_->mgr_thread_, monitor_interval);
  kInstance_->flash_->Init(kInstance_->mgr_thread_,
                           kInstance_->ubifs_mon_);
  return true;
}

void PlatformPeripheral::Run(void) {
  kInstance_->mgr_thread_->Run();
}

bool PlatformPeripheral::Terminate(void) {
  if (kInstance_ == NULL) {
    LOG(LS_WARNING) << "Peripherals are already terminated...";
    return false;
  } else {
    kInstance_->peripheral_mon_->Terminate();
    kInstance_->ubifs_mon_->Terminate();
    delete kInstance_;
    kInstance_ = NULL;
    delete platformInstance_;
    platformInstance_ = NULL;
  }
  return true;
}

PlatformPeripheral::PlatformPeripheral(Platform *platform)
  : peripheral_mon_(new PeripheralMon(new FanControl(platform))),
    ubifs_mon_(new UbifsMon(platform)),
    flash_(new Flash()) {
}

PlatformPeripheral::~PlatformPeripheral() {
}

}  // namespace bruno_platform_peripheral

#ifdef __cplusplus
extern "C" {
#endif

int platform_peripheral_init(unsigned int monitor_interval) {
  if (!bruno_platform_peripheral::PlatformPeripheral::Init(monitor_interval)) {
    return -1;
  }
  return 0;
}

void platform_peripheral_run(void) {
  bruno_platform_peripheral::PlatformPeripheral::Run();
}

int platform_peripheral_terminate(void) {
  if (!bruno_platform_peripheral::PlatformPeripheral::Terminate()) {
    return -1;
  }
  return 0;
}

#ifdef __cplusplus
}  // extern "C" {
#endif
