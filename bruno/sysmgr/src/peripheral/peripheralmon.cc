// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "base/thread.h"
#include "gpioconfig.h"
#include "gpio.h"
#include "gpiofanspeed.h"
#include "fancontrol.h"
#include "peripheralmon.h"
#include <stdio.h>

namespace bruno_platform_peripheral {

PeripheralMon::~PeripheralMon() {
}

void PeripheralMon::Probe(void) {
  NEXUS_AvsStatus avsStatus;
  NEXUS_GetAvsStatus(&avsStatus);
  bruno_base::TimeStamp now = bruno_base::Time();

  if (0 == last_time_) {
    LOG(LS_INFO) << "voltage:" << avsStatus.voltage/1000.0
                 << " temperature:" << avsStatus.temperature/1000.0;
  } else {
    LOG(LS_INFO) << "voltage:" << avsStatus.voltage/1000.0
                 << " temperature:" << avsStatus.temperature/1000.0
                 << " fanspeed:" << fan_speed_->ResetCounter()*1000.0/(now - last_time_);
  }
  fan_control_->DrivePwm((uint16_t)(avsStatus.temperature/1000));
  last_time_ = now;
  mgr_thread_->PostDelayed(interval_, this, static_cast<uint32>(EVENT_TIMEOUT));
}

void PeripheralMon::Init(bruno_base::Thread* mgr_thread, unsigned int interval) {
  interval_ = interval;
  mgr_thread_ = mgr_thread;
  fan_control_->Init(50, 120, 10);
  fan_speed_->Init();
  Probe();
}

void PeripheralMon::Terminate(void) {
  fan_speed_->Terminate();
}

void PeripheralMon::OnMessage(bruno_base::Message* msg) {
  LOG(LS_VERBOSE) << "Received message " << msg->message_id;
  switch (msg->message_id) {
    case EVENT_TIMEOUT:
      Probe();
      break;
    default:
      LOG(LS_WARNING) << "Invalid message type, ignore ... " << msg->message_id;
      break;
  }
}

}  // namespace bruno_platform_peripheral
