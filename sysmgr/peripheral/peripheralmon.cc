// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "bruno/logging.h"
#include "bruno/thread.h"
#include "fancontrol.h"
#include "peripheralmon.h"
#include <stdio.h>

namespace bruno_platform_peripheral {

PeripheralMon::~PeripheralMon() {
}

void PeripheralMon::Probe(void) {
  bruno_base::TimeStamp now = bruno_base::Time();
  uint16_t  hdd_temp;
  float soc_temperature;
  std::string fan_speed;
  std::string soc_voltage;

  fan_control_->GetHddTemperature(&hdd_temp);
  if (gpio_mailbox_ready == false)
    gpio_mailbox_ready = CheckIfMailBoxIsReady();

  if (gpio_mailbox_ready == true) {
    ReadFanSpeed(&fan_speed);
    bool read_soc_temperature = ReadSocTemperature(&soc_temperature);
    ReadSocVoltage(&soc_voltage);

    if (0 == last_time_) {
      LOG(LS_INFO) << "voltage:" << soc_voltage
                   << "  soc_temperature:" << soc_temperature
                   << "  hdd_temperature:" << hdd_temp/MULTI_VALUE_IN_FLOAT;
    } else {
      LOG(LS_INFO) << "voltage:" << soc_voltage
                   << "  soc_temperature:" << soc_temperature
                   << "  hdd_temperature:" << hdd_temp/MULTI_VALUE_IN_FLOAT
                   << "  fanspeed:" << fan_speed;
    }

    /* If failed to read soc_temperature, don't change PWM
     * for both thin bruno and fat bruno
     */
    if (read_soc_temperature == true) {
      fan_control_->AdjustSpeed_PControl(
                    static_cast<uint16_t>(soc_temperature * MULTI_VALUE),
                    hdd_temp);
    } else {
      LOG(LS_INFO) << "Not change PWM due to fail to read soc_temperature";
    }
  } else {
    LOG(LS_INFO) << "gpio_mailbox is not ready";
  }

  last_time_ = now;
  mgr_thread_->PostDelayed(interval_, this, static_cast<uint32>(EVENT_TIMEOUT));
}

void PeripheralMon::Init(bruno_base::Thread* mgr_thread, unsigned int interval) {
  interval_ = interval;
  mgr_thread_ = mgr_thread;
  fan_control_->Init(50, 120, 10);
  Probe();
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
