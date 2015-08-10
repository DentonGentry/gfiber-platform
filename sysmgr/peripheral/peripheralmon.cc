// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "bruno/logging.h"
#include "bruno/thread.h"
#include "fancontrol.h"
#include "peripheralmon.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace bruno_platform_peripheral {
extern Platform* platformInstance_;

PeripheralMon::~PeripheralMon() {
}

void PeripheralMon::Probe(void) {
  bruno_base::TimeStamp now = bruno_base::Time();
  float soc_temperature;
  uint16_t  fan_speed = 0;
  std::string soc_voltage;

  if (platformInstance_->PlatformHasHdd() &&
      bruno_base::TimeIsLaterOrEqual(next_time_hdd_temp_check_, now)) {
    fan_control_->GetHddTemperature(&hdd_temp_);
    LOG(LS_INFO) << "hdd_temperature (new):" << hdd_temp_;
    next_time_hdd_temp_check_ = bruno_base::TimeAfter(hdd_temp_interval_);
  }

  if (gpio_mailbox_ready == false)
    gpio_mailbox_ready = CheckIfMailBoxIsReady();

  if (gpio_mailbox_ready == true) {
    bool read_soc_temperature = ReadSocTemperature(&soc_temperature);
    ReadSocVoltage(&soc_voltage);

    if (platformInstance_->PlatformHasFan()) {
      ReadFanSpeed(&fan_speed);
    }

    LOG(LS_INFO) << "voltage:" << soc_voltage
                 << "  soc_temperature:" << soc_temperature
                 << "  hdd_temperature:" << hdd_temp_
                 << "  fanspeed:" << fan_speed;

    if (read_soc_temperature) {
      Overheating(soc_temperature);
    }

    if (platformInstance_->PlatformHasFan()) {
      /* If failed to read soc_temperature, don't change PWM */
      if (read_soc_temperature) {
        fan_control_->AdjustSpeed(
                      static_cast<uint16_t>(soc_temperature),
                      hdd_temp_,
                      fan_speed);
      } else {
        LOG(LS_INFO) << "Not change PWM due to fail to read soc_temperature";
      }
    }
  } else {
    LOG(LS_INFO) << "gpio_mailbox is not ready";
  }

  last_time_ = now;
  mgr_thread_->PostDelayed(interval_, this, static_cast<uint32>(EVENT_TIMEOUT));
}

void PeripheralMon::Overheating(float soc_temperature)
{
  std::ostringstream message;
  uint16_t  overheat_value;

  fan_control_->GetOverheatTemperature(&overheat_value);

  if (soc_temperature < overheat_value) {
    overheating_ = 0;
    Common::ClrLED(Common::OVERHEATING, "");
  }
  else {
    overheating_ ++;

    if (overheating_ >= OVERHEATING_COUNT) {
      message << "System power off: SOC overheating " << overheating_;
      LOG(LS_ERROR) << message.str();
      Common::ClrLED(Common::OVERHEATING, "");

      overheating_ = 0;
      Common::Poweroff();
    }
    else {
      message << "SOC overheating detected " << overheating_;
      LOG(LS_ERROR) << message.str();
      Common::SetLED(Common::OVERHEATING, message.str());
    }
  }
}

void PeripheralMon::Init(bruno_base::Thread* mgr_thread, unsigned int interval,
                         unsigned int hdd_temp_interval) {
  interval_ = interval;
  hdd_temp_interval_ = hdd_temp_interval;
  next_time_hdd_temp_check_ = bruno_base::Time(); // = now
  overheating_ = 0;
  mgr_thread_ = mgr_thread;
  fan_control_->Init(&gpio_mailbox_ready);
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
