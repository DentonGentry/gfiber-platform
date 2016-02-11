// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_
#define BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_

#include "bruno/constructormagic.h"
#include "bruno/messagehandler.h"
#include "bruno/thread.h"
#include "bruno/time.h"
#include "fancontrol.h"
#include "mailbox.h"
#include "platform.h"

#define OVERHEATING_COUNT   3

namespace bruno_platform_peripheral {

class GpIoFanSpeed;
class PeripheralMon : public Mailbox {
 public:
  PeripheralMon(Platform *plat, unsigned int interval = 5000,
                unsigned int hdd_temp_interval = 300000)
      : platform_(plat), fan_control_(new FanControl(plat)), interval_(interval),
    hdd_temp_interval_(hdd_temp_interval), hdd_temp_(0),
    last_time_(0), next_time_hdd_temp_check_(0),
    gpio_mailbox_ready(false) {
  }
  virtual ~PeripheralMon();
  void Probe(void);

  void Init(int interval, int hdd_temp_interval);

 private:
  void Overheating(float soc_temperature);

  Platform* platform_;
  bruno_base::scoped_ptr<FanControl> fan_control_;
  int interval_;
  int hdd_temp_interval_;
  uint16_t hdd_temp_;
  unsigned int overheating_;
  bruno_base::TimeStamp last_time_;
  bruno_base::TimeStamp next_time_hdd_temp_check_;
  bool  gpio_mailbox_ready;
  DISALLOW_COPY_AND_ASSIGN(PeripheralMon);
};

}  // namespace bruno_platform_peripheral

#endif // BUNO_PLATFORM_PERIPHERAL_PERIPHERALMON_H_
