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
#include "platform.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// using namespace bruno_platform_peripheral;

int main(int argc, char** argv) {
  DEFINE_int(soc_low, 1, "SOC Low temperature");
  DEFINE_int(soc_high, 10, "SOC High temperature");
  DEFINE_int(hdd_low, 1, "HDD Low temperature");
  DEFINE_int(hdd_high, 10, "HDD High temperature");
  DEFINE_int(percent, 50, "Percentage of the maximum speed the fan starts at");
  DEFINE_int(count, 10, "Repeat times");
  DEFINE_int(resolution, 10,
             "Temperature Resolution (10=increase 0.1 degC, range is 1 - 100)");
  DEFINE_int(interval, 1, "Interval");
  DEFINE_bool(debug, false, "Enable debug log");
  DEFINE_bool(help, false, "Prints this message");

  // parse options
  if (0 != FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    FlagList::Print(NULL, false);
    return 0;
  }

  if ((FLAG_help) || ((FLAG_resolution == 0) || (FLAG_resolution > 100))) {
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

  bruno_platform_peripheral::FanControl fan_control(0, NULL);
  bruno_platform_peripheral::GpIoFanSpeed fan_speed;

  fan_control.Init();
  fan_speed.Init();
  bruno_base::TimeStamp last_time = 0;

  NEXUS_AvsStatus avsStatus;
  bruno_base::TimeStamp now;
  uint16_t hdd_temp;

  fan_control.DrivePwm(0x000000FF*FLAG_percent/100);

  int i, h;
  FLAG_soc_high = TIMES_VALUE(FLAG_soc_high);
  FLAG_soc_low = TIMES_VALUE(FLAG_soc_low);
  FLAG_hdd_high = TIMES_VALUE(FLAG_hdd_high);
  FLAG_hdd_low = TIMES_VALUE(FLAG_hdd_low);
  for (i=FLAG_soc_low, h=FLAG_hdd_low; ((i<FLAG_soc_high) || (h<FLAG_hdd_high));
       i+=FLAG_resolution, h+=FLAG_resolution) {
    for (int j=0; j<FLAG_count; ++j) {
      fan_control.AdjustSpeed_PControl((uint16_t)i, (uint16_t)h);
      NEXUS_GetAvsStatus(&avsStatus);
      now = bruno_base::Time();
      fan_control.GetHddTemperature(&hdd_temp);

      LOG(LS_INFO) << "voltage:" << avsStatus.voltage/1000.0
                   << "  soc_temperature:" << avsStatus.temperature/1000.0
                   << "  hdd_temperature:" << hdd_temp/HDD_MULTI_VALUE_IN_FLOAT
                   << "  fanspeed:" << fan_speed.ResetCounter()*1000.0/(now - last_time);
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
