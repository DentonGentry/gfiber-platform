// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include <stacktrace.h>
#include <stdio.h>
#include <stdlib.h>
#include "bruno/basictypes.h"
#include "bruno/criticalsection.h"
#include "bruno/flags.h"
#include "bruno/logging.h"
#include "platform_peripheral_api.h"
#include "peripheral/peripheralmon.h"
#include "peripheral/platform.h"
#include "peripheral/fancontrol.h"

using bruno_platform_peripheral::Platform;
using bruno_platform_peripheral::PeripheralMon;

int main(int argc, char** argv) {
  DEFINE_int(interval, 5000, "Monitor interval in ms (except for HDD-temp)");
  DEFINE_int(hdd_temp_interval, 300000,
             "HDD temperature monitor interval in ms"
             " (should be multiple of <interval>");
  DEFINE_bool(debug, false, "Enable debug log");
  DEFINE_bool(help, false, "Prints this message");

  // parse options
  if (0 != FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    FlagList::Print(NULL, false);
    return 0;
  }

  if (FLAG_help) {
    FlagList::Print(NULL, false);
    return 0;
  }

  stacktrace_setup();

  bruno_base::LogMessage::LogToDebug(bruno_base::LS_INFO);
  if (FLAG_debug) {
    bruno_base::LogMessage::LogToDebug(bruno_base::LS_VERBOSE);
  }

  Platform* platform = new Platform();
  platform->Init();
  PeripheralMon* pmon = new PeripheralMon(platform);
  pmon->Init(FLAG_hdd_temp_interval);

  for (;;) {
    pmon->Probe();
    usleep(FLAG_interval * 1000);
  }

  return 0;
}
