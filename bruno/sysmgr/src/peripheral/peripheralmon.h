// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_
#define BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_

#include "base/constructormagic.h"
#include "base/time.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class GpIoFanSpeed;
class PeripheralMon : public bruno_base::MessageHandler {
 public:
  PeripheralMon(GpIoFanSpeed* fan_speed, unsigned int interval = 5000)
      : fan_speed_(fan_speed), interval_(interval), last_time_(0), mgr_thread_(NULL) {
  }
  virtual ~PeripheralMon();

  enum EventType {
    EVENT_TIMEOUT
  };

  void Init(bruno_base::Thread* mgr_thread, unsigned int interval);
  void Terminate(void);

  void OnMessage(bruno_base::Message* msg);

 private:
  void Probe(void);

  bruno_base::scoped_ptr<GpIoFanSpeed> fan_speed_;
  unsigned int interval_;
  bruno_base::TimeStamp last_time_;
  bruno_base::Thread* mgr_thread_;
  DISALLOW_COPY_AND_ASSIGN(PeripheralMon);
};

}  // namespace bruno_platform_peripheral

#endif // BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_
