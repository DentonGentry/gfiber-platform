// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#ifndef BRUNO_PLATFORM_PERIPHERAL_UBIFSMON_H_
#define BRUNO_PLATFORM_PERIPHERAL_UBIFSMON_H_

#include "bruno/logging.h"
#include "bruno/thread.h"
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include "bruno/constructormagic.h"
#include "bruno/sigslot.h"
#include "bruno/time.h"


namespace bruno_platform_peripheral {

typedef void (*sig_fptr)(int, siginfo_t*, void*);

#define SYSMGR_PROCFS   "/proc/sysmgr_pid"

class Platform;
class UbifsMon : public bruno_base::MessageHandler {
 public:
  UbifsMon(Platform *platform, unsigned int interval = 5000)
      : platformInstance_(platform),
        current_pid_(0),
        interval_(interval),
        mgr_thread_(NULL) {}
  virtual ~UbifsMon() {}

  enum EventType {
    EVENT_TIMEOUT_UBIMON
  };

  void Terminate();
  void Init(bruno_base::Thread* mgr_thread, unsigned int interval);
  void UbifsErrorHandler();
  static void SignalHandler(int n, siginfo_t *info, void *context);
  void OnMessage(bruno_base::Message* msg);

  sigslot::signal0<> SignalRecvRoUbiFsEvent;

 private:
  Platform *platformInstance_;
  pid_t current_pid_;
  unsigned int interval_;
  bruno_base::Thread* mgr_thread_;
  static bool ubifs_err_occurred_;
  static int  ubifs_err_reason_;

  void SetPid(void);
  void UbiProbe(void);

  DISALLOW_COPY_AND_ASSIGN(UbifsMon);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_UBIFSMON_H_
