// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#include <fstream>
#include "platform.h"
#include "ubifsmon.h"

namespace bruno_platform_peripheral {

bool UbifsMon::ubifs_err_occurred_ = false;
int UbifsMon::ubifs_err_reason_ = 0;

void UbifsMon::Init(bruno_base::Thread* mgr_thread, unsigned int interval) {

#ifdef ENABLE_SIGUSR2_SIGNALLING
  /* Setup the signal handler for SIGUSR2
   * SA_SIGINFO -> we want the signal handler function with 3 arguments
   */
  struct sigaction sig_act;
  sig_act.sa_sigaction = SignalHandler;
  sig_act.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR2, &sig_act, NULL);
  SetPid();
  interval_ = interval;
  mgr_thread_ = mgr_thread;
  UbiProbe();
#endif /* ENABLE_SIGUSR2_SIGNALLING */

}

void UbifsMon::Terminate() {

#ifdef ENABLE_SIGUSR2_SIGNALLING
  /* Unregister the signal handler for SIGUSR2 */
  struct sigaction sig_act;
  sig_act.sa_sigaction = reinterpret_cast<sig_fptr>(SIG_DFL);
  sig_act.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR2, &sig_act, NULL);
#endif /* ENABLE_SIGUSR2_SIGNALLING */

}

void UbifsMon::SetPid() {
  LOG(LS_VERBOSE) << "SetPid()" << std::endl;
  std::ofstream opened_file(SYSMGR_PROCFS);
  if (opened_file.is_open()) {
    std::stringstream out;
    pid_t tmp_pid = getpid();
    /* check if our pid changed */
    if (current_pid_ != tmp_pid) {
      current_pid_ = tmp_pid;
      out << current_pid_;
      std::string s = out.str() + "\n";
      LOG(LS_VERBOSE) << "Set_pid(): pid: " << s;
      opened_file << s;
    }
    opened_file.close();
  }
  else {
    LOG(LS_ERROR) << "Fail to open " << SYSMGR_PROCFS << std::endl;
    current_pid_ = 0;
  }
}

void UbifsMon::OnMessage(bruno_base::Message* msg) {
  LOG(LS_VERBOSE) << "Received message " << msg->message_id;
  switch (msg->message_id) {
    case EVENT_TIMEOUT_UBIMON:
      UbiProbe();
      break;
    default:
      LOG(LS_WARNING) << "Invalid message type, ignore ... " << msg->message_id;
      break;
  }
}

void UbifsMon::SignalHandler(int n, siginfo_t *info, void *context) {
  UNUSED(context);
  ubifs_err_occurred_ = true;
  ubifs_err_reason_ = info->si_int;
  LOG(LS_VERBOSE) << "SignalHandler: received value si_int=" << info->si_int
               << std::endl;
}

void UbifsMon::UbiProbe(void) {
  LOG(LS_VERBOSE) << "******UbiProbe()" << std::endl;
  if (ubifs_err_occurred_ == true) {
    LOG(LS_INFO) << "Taking erase read-only volume(s) action now..." << std::endl;
    SignalRecvRoUbiFsEvent();
  }

  mgr_thread_->PostDelayed(interval_, this, static_cast<uint32>(EVENT_TIMEOUT_UBIMON));
}

}  // namespace bruno_platform_peripheral
