#include "basictypes.h"
#include "criticalsection.h"
#include "logging.h"
#include "flags.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "platform_peripheral_api.h"
#include <stdio.h>
#include <unistd.h>

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

  platform_peripheral_init(FLAG_interval);
  platform_peripheral_run();
  platform_peripheral_terminate();

  NEXUS_Platform_Uninit();
  return 0;
}
