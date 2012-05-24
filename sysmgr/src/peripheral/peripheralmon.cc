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
  uint16_t  hdd_temp;

  fan_control_->GetHddTemperature(&hdd_temp);
  if (0 == last_time_) {
    LOG(LS_INFO) << "voltage:" << avsStatus.voltage/SOC_MULTI_VALUE_IN_FLOAT
                 << "  soc_temperature:" << avsStatus.temperature/SOC_MULTI_VALUE_IN_FLOAT
                 << "  hdd_temperature:" << hdd_temp/HDD_MULTI_VALUE_IN_FLOAT;
  } else {
    LOG(LS_INFO) << "voltage:" << avsStatus.voltage/SOC_MULTI_VALUE_IN_FLOAT
                 << "  soc_temperature:" << avsStatus.temperature/SOC_MULTI_VALUE_IN_FLOAT
                 << "  hdd_temperature:" << hdd_temp/HDD_MULTI_VALUE_IN_FLOAT
                 << "  fanspeed:" << fan_speed_->ResetCounter()*SOC_MULTI_VALUE_IN_FLOAT/(now - last_time_);
  }
  fan_control_->AdjustSpeed_PControl((uint16_t)(avsStatus.temperature/10), hdd_temp);
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
