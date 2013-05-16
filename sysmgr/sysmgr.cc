// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "bruno/basictypes.h"
#include "bruno/criticalsection.h"
#include "bruno/logging.h"
#include "bruno/flags.h"
#include "platform_peripheral_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stacktrace.h>

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

  if (FLAG_debug) {
    bruno_base::LogMessage::LogToDebug(bruno_base::LS_VERBOSE);
  } else {
    bruno_base::LogMessage::LogToDebug(bruno_base::LS_INFO);
  }

  if (0 != platform_peripheral_init((unsigned int)FLAG_interval,
                                    (unsigned int)FLAG_hdd_temp_interval)) {
    fprintf(stderr, "Peripherals cannot be initialized!!!\n");
    return -1;
  }

  platform_peripheral_run();
  platform_peripheral_terminate();

  return 0;
}
