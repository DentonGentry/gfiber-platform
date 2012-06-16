#include "bruno/basictypes.h"
#include "bruno/criticalsection.h"
#include "bruno/logging.h"
#include "bruno/flags.h"
#include "bruno/time.h"
#include "platform_peripheral_api.h"
#include "fancontrol.h"
#include "platform.h"
#include "mailbox.h"
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

  bruno_platform_peripheral::FanControl fan_control(NULL);

  fan_control.Init();
  std::string fan_speed;
  std::string soc_voltage;

  fan_control.DrivePwm(DUTY_CYCLE_PWM_MAX_VALUE*FLAG_percent/100);

  int i, h;
  FLAG_soc_high = TIMES_VALUE(FLAG_soc_high);
  FLAG_soc_low = TIMES_VALUE(FLAG_soc_low);
  FLAG_hdd_high = TIMES_VALUE(FLAG_hdd_high);
  FLAG_hdd_low = TIMES_VALUE(FLAG_hdd_low);
  for (i=FLAG_soc_low, h=FLAG_hdd_low; ((i<FLAG_soc_high) || (h<FLAG_hdd_high));
       i+=FLAG_resolution, h+=FLAG_resolution) {
    for (int j=0; j<FLAG_count; ++j) {
      fan_control.AdjustSpeed_PControl(
                  static_cast<uint16_t>(i), static_cast<uint16_t>(h));
      fan_control.ReadFanSpeed(&fan_speed);
      fan_control.ReadSocVoltage(&soc_voltage);
      LOG(LS_INFO) << "voltage:" << soc_voltage
                   << "  emu-soc_temperature:" << (i/MULTI_VALUE_IN_FLOAT)
                   << "  emu-hdd_temperature:" << (h/MULTI_VALUE_IN_FLOAT)
                   << "  fanspeed:" << fan_speed;
      sleep(FLAG_interval);
    }
  }

  fan_control.Terminate();
  return 0;
}

#ifdef __cplusplus
 } // extern "C" {
#endif
