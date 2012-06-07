// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#ifndef BRUNO_PLATFORM_PERIPHERAL_FACTORYRESETBUTTON_H_
#define BRUNO_PLATFORM_PERIPHERAL_FACTORYRESETBUTTON_H_

#include "bruno/constructormagic.h"
#include "bruno/scoped_ptr.h"
#include "bruno/sigslot.h"
#include "bruno/thread.h"
#include "bruno/time.h"
#include "platformnexus.h"

namespace bruno_platform_peripheral {

class GpIoFactoryReset;
class FactoryResetButton : public sigslot::has_slots<>, public bruno_base::MessageHandler {
 public:
  static const bruno_base::TimeStamp kDefaultInterval;
  static const bruno_base::TimeStamp kDefaultHoldTime;

  enum EventType {
    EVENT_PRESSED,
    EVENT_RELEASED,
    EVENT_TIMEOUT
  };

  FactoryResetButton();
  virtual ~FactoryResetButton();

  // The object which does the real factory reset procedures need to subscribe
  // this event.
  sigslot::signal0<> SignalResetEvent;

  void Init(bruno_base::Thread* mgr_thread);
  void Terminate(void);

  void OnMessage(bruno_base::Message* msg);
  void OnButtonEvent(NEXUS_GpioValue value);
  void SendReminder(void);

 private:
  bruno_base::scoped_ptr<GpIoFactoryReset> gpio_;
  EventType value_;
  bruno_base::TimeStamp last_toggle_;
  // The manager thread is the thread which handles all message dispatching.
  // e.g. it could be sysmgr thread.
  bruno_base::Thread* mgr_thread_;

  DISALLOW_COPY_AND_ASSIGN(FactoryResetButton);
};

}  // namespace bruno_platform_peripheral

#endif // BRUNO_PLATFORM_PERIPHERAL_FACTORYRESETBUTTON_H_
