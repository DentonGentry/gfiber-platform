// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "gpioconfig.h"
#include "gpio.h"
#include "factoryresetbutton.h"
#include "gpiofactoryreset.h"

namespace bruno_platform_peripheral {

// default timer intreval in milliseconds
const bruno_base::TimeStamp FactoryResetButton::kDefaultInterval = 500;
const bruno_base::TimeStamp FactoryResetButton::kDefaultHoldTime = 4000;

FactoryResetButton::FactoryResetButton()
    : value_(EVENT_RELEASED), last_toggle_(0), mgr_thread_(NULL) {
  gpio_.reset(new GpIoFactoryReset());
}

FactoryResetButton::~FactoryResetButton() {
  Terminate();
}

void FactoryResetButton::Init(bruno_base::Thread* mgr_thread) {
  mgr_thread_ = mgr_thread;
  gpio_->SignalButtonEvent.connect(this, &FactoryResetButton::OnButtonEvent);
  gpio_->Init();
}

void FactoryResetButton::Terminate(void) {
  gpio_->Terminate();
}

void FactoryResetButton::SendReminder(void) {
  mgr_thread_->PostDelayed(kDefaultInterval, this,
                           static_cast<uint32>(EVENT_TIMEOUT));
}

void FactoryResetButton::OnMessage(bruno_base::Message* msg) {
  LOG(LS_VERBOSE) << "Received message " << msg->message_id;
  switch (msg->message_id) {
    case EVENT_TIMEOUT:
      if (EVENT_PRESSED == value_) {
        int32 ms= bruno_base::TimeSince(last_toggle_);
        if (ms < kDefaultHoldTime) {
          SendReminder();
        } else {
          LOG(LS_INFO) << "Factory reset button has been held for "
                       << ms << " ms";
          LOG(LS_INFO) << "Taking reset action...";
          SignalResetEvent();
        }
      }
      break;
    case EVENT_PRESSED:
    case EVENT_RELEASED:
      value_ = static_cast<EventType>(msg->message_id);
      last_toggle_ = bruno_base::Time();
      SendReminder();
      break;
    default:
      LOG(LS_WARNING) << "Invalid message type, ignore ... " << msg->message_id;
      break;
  }
}

void FactoryResetButton::OnButtonEvent(NEXUS_GpioValue value) {
  // Low means the button is pressed.
  LOG(LS_VERBOSE) << "Received factory reset button event " << value;
  mgr_thread_->Post(this, static_cast<uint32>(value));
}

}  // namespace bruno_platform_peripheral
