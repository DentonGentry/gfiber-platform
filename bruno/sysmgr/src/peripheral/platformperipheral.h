// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_H_
#define BRUNO_PLATFORM_PERIPHERAL_H_

namespace bruno_platform_peripheral {

class GpIoFanSpeed;
class LedMain;
class LedStandby;
class LedStatus;
class FanControl;
class TempMonitor;
class FactoryResetButton;
class PeripheralMon;

class PlatformPeripheral {
 public:
  static bool Init(unsigned int monitor_interval);
  static void Run(void);
  static bool Terminate(void);
  static void TurnOnLedMain(void);
  static void TurnOffLedMain(void);
  static void TurnOnLedStandby(void);
  static void TurnOffLedStandby(void);
  static bool SetLedStatusColor(led_status_color_e color);
  static void TurnOffLedStatus(void);

  ~PlatformPeripheral();

 private:
  bruno_base::Thread* mgr_thread_;
  bruno_base::scoped_ptr<LedMain> led_main_;
  bruno_base::scoped_ptr<LedStandby> led_standby_;
  bruno_base::scoped_ptr<LedStatus> led_status_;
  bruno_base::scoped_ptr<FactoryResetButton> factory_reset_button_;
  bruno_base::scoped_ptr<TempMonitor> temp_monitor_;
  bruno_base::scoped_ptr<PeripheralMon> peripheral_mon_;

  static bruno_base::CriticalSection kCrit_;
  static PlatformPeripheral* kInstance_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformPeripheral);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_H_
