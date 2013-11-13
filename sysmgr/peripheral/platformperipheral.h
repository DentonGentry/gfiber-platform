// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_H_
#define BRUNO_PLATFORM_PERIPHERAL_H_

namespace bruno_platform_peripheral {

class FanControl;
class TempMonitor;
class PeripheralMon;
class Platform;

class PlatformPeripheral {
 public:
  static bool Init(unsigned int monitor_interval,
                   unsigned int hdd_temp_interval);
  static void Run(void);
  static bool Terminate(void);

  PlatformPeripheral(Platform *platform);
  ~PlatformPeripheral();

 private:
  bruno_base::Thread* mgr_thread_;
  bruno_base::scoped_ptr<PeripheralMon> peripheral_mon_;

  static bruno_base::CriticalSection kCrit_;
  static PlatformPeripheral* kInstance_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformPeripheral);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_H_
