// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_TEMPMONITOR_H_
#define BRUNO_PLATFORM_PERIPHERAL_TEMPMONITOR_H_

#include "base/constructormagic.h"
#include "base/scoped_ptr.h"
#include "base/sigslot.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class FanControl;
class TempMonitor : public sigslot::has_slots<> {
 public:
  TempMonitor(uint32_t channel, FanControl* fan_control);
  virtual ~TempMonitor();

  static void EventHandler(void *context, int param);
  static void AlarmHandler(void *context, int param);

  sigslot::signal0<> SignalAlarm;
  sigslot::signal0<> SignalEvent;

  void OnAlarm(void);
  void OnEvent(void);

  bool Init(void);
  void Terminate(void);

 private:

  uint32_t channel_;
  bruno_base::scoped_ptr<FanControl> fan_control_;
  NEXUS_TempMonitorHandle handle_;
  NEXUS_TempMonitorOpenSettings open_settings_;

  DISALLOW_COPY_AND_ASSIGN(TempMonitor);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_TEMPMONITOR_H_
