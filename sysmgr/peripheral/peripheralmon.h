// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_
#define BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_

#include "bruno/constructormagic.h"
#include "bruno/time.h"
#include "mailbox.h"

#define OVERHEATING_VALUE   125.0
#define OVERHEATING_COUNT   3
#define OVERHEATING_FILE    "/user/rw/overheating"

#define OVERHEATING_LED_ON  "1 0 1 0 1 0"
#define OVERHEATING_LED_OFF "0"

namespace bruno_platform_peripheral {

class GpIoFanSpeed;
class PeripheralMon : public bruno_base::MessageHandler, public Mailbox {
 public:
  PeripheralMon(FanControl* fan_control, unsigned int interval = 5000)
      : fan_control_(fan_control), interval_(interval),
        last_time_(0), mgr_thread_(NULL), gpio_mailbox_ready(false) {
  }
  virtual ~PeripheralMon();

  enum EventType {
    EVENT_TIMEOUT
  };

  void Init(bruno_base::Thread* mgr_thread, unsigned int interval);
  void Terminate(void) {}

  void OnMessage(bruno_base::Message* msg);

 private:
  void Probe(void);
  void Overheating(float soc_temperature);

  bruno_base::scoped_ptr<FanControl> fan_control_;
  unsigned int interval_;
  unsigned int overheating_;
  bruno_base::TimeStamp last_time_;
  bruno_base::Thread* mgr_thread_;
  bool  gpio_mailbox_ready;
  DISALLOW_COPY_AND_ASSIGN(PeripheralMon);
};

}  // namespace bruno_platform_peripheral

#endif // BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_
