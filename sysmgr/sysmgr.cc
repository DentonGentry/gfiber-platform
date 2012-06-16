// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "bruno/basictypes.h"
#include "bruno/criticalsection.h"
#include "bruno/logging.h"
#include "bruno/flags.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "platform_peripheral_api.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  DEFINE_int(interval, 5000, "Monitor interval in ms");
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

  if (FLAG_debug) {
    bruno_base::LogMessage::LogToDebug(bruno_base::LS_VERBOSE);
  } else {
    bruno_base::LogMessage::LogToDebug(bruno_base::LS_INFO);
  }

  NEXUS_PlatformSettings platform_settings;
  NEXUS_Platform_GetDefaultSettings(&platform_settings);
  platform_settings.openFrontend = false;
  NEXUS_Platform_Init(&platform_settings);

  if (0 != platform_peripheral_init((unsigned int)FLAG_interval)) {
    fprintf(stderr, "Peripherals cannot be initialized!!!\n");
    return -1;
  }

  // Initialize lights
  platform_peripheral_turn_on_led_main();
  platform_peripheral_turn_off_led_standby();
  platform_peripheral_set_led_status_color(LED_STATUS_ACT_BLUE);

  platform_peripheral_run();
  platform_peripheral_terminate();

  NEXUS_Platform_Uninit();
  return 0;
}
