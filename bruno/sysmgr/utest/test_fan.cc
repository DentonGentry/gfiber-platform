#include "basictypes.h"
#include "criticalsection.h"
#include "logging.h"
#include "flags.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_avs.h"
#include "platform_peripheral_api.h"
#include "gpio.h"
#include "gpioconfig.h"
#include "gpiofanspeed.h"
#include "fancontrol.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char** argv) {
  DEFINE_int(low, 1, "Low temporature");
  DEFINE_int(high, 10, "High temporature");
  DEFINE_int(percent, 50, "Percentage of the maximum speed the fan starts at");
  DEFINE_int(count, 10, "Repeat times");
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

  bruno_platform_peripheral::FanControl fan_control(0);
  bruno_platform_peripheral::GpIoFanSpeed fan_speed;

  fan_control.Init();
  fan_speed.Init();
  bruno_base::TimeStamp last_time = 0;

  NEXUS_AvsStatus avsStatus;
  bruno_base::TimeStamp now;

  fan_control.DrivePwm(0x000000FF*FLAG_percent/100);

  for (int i=FLAG_low; i<FLAG_high; ++i) {
    for (int j=0; j<FLAG_count; ++j) {
      fan_control.AdjustSpeed(i);
      NEXUS_GetAvsStatus(&avsStatus);
      now = bruno_base::Time();

      LOG(LS_INFO) << "voltage:" << avsStatus.voltage/1000.0
                   << " temperature:" << avsStatus.temperature/1000.0
                   << " fanspeed:" << fan_speed.ResetCounter()*1000.0/(now - last_time);
      last_time = now;
      sleep(FLAG_interval);
    }
  }

  fan_control.Terminate();
  fan_speed.Terminate();

  NEXUS_Platform_Uninit();
  return 0;
}

#ifdef __cplusplus
} // extern "C" {
#endif
