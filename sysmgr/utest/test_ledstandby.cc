#include "basictypes.h"
#include "criticalsection.h"
#include "logging.h"
#include "flags.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "platform_peripheral_api.h"
#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

static void test_on(int duration) {
  LOG(LS_INFO) << "Turning on LED Standby for " << duration << " seconds...";
  platform_peripheral_turn_on_led_standby();
  sleep(duration);
}

static void test_off(int duration) {
  LOG(LS_INFO) << "Turning off LED Standby for " << duration << " seconds...";
  platform_peripheral_turn_off_led_standby();
  sleep(duration);
}

int main(int argc, char** argv) {
  DEFINE_int(interval, 5000, "Monitor interval in ms");
  DEFINE_int(count, 3, "Repeat times");
  DEFINE_int(duration, 2, "Duration");
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

  platform_peripheral_init(5000);
  for (int i=0; i<FLAG_count; ++i) {
    LOG(LS_INFO) << "Round " << i << " Starts";
    test_off(FLAG_duration);
    test_on(FLAG_duration);
    LOG(LS_INFO) << "Round " << i << " Ends";
  }
  platform_peripheral_terminate();


  NEXUS_Platform_Uninit();
  return 0;
}

#ifdef __cplusplus
} // extern "C" {
#endif
