#include "basictypes.h"
#include "criticalsection.h"
#include "logging.h"
#include "flags.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_avs.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char** argv) {
  DEFINE_int(count, 100, "Repeat times");
  DEFINE_int(interval, 1, "Interval");
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

  const int cnt = 100;
  for (int i=0; i<FLAG_count; ++i) {
    NEXUS_AvsStatus avsStatus;
    NEXUS_GetAvsStatus(&avsStatus);
    LOG(LS_INFO) << "[Round " << i << "] voltage:" << avsStatus.voltage
                 << " temperature:" << avsStatus.temperature;
    sleep(FLAG_interval);
  }

  NEXUS_Platform_Uninit();
  return 0;
}

#ifdef __cplusplus
} // extern "C" {
#endif
