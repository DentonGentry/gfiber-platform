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
  DEFINE_int(percent, 0, "Percentage of the maximum speed the fan starts at");
  DEFINE_int(count, 10, "Repeat times");
  DEFINE_int(interval, 1, "Interval");
  DEFINE_bool(dec_temp, false, "Test with decreasing temperature");
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

  bruno_platform_peripheral::FanControl fan_control(NULL);

  fan_control.Init(NULL);
  uint16_t fan_speed;
  std::string soc_voltage;

  fan_control.DrivePwm(FLAG_percent);
  sleep(2);

  int i, h;

  LOG(LS_VERBOSE) << "soc_low=" << FLAG_soc_low << " soc_high=" << FLAG_soc_high
                  << " hdd_low=" << FLAG_hdd_low << " hdd_high=" << FLAG_hdd_high;

  for (i=FLAG_soc_low, h=FLAG_hdd_low;
       ((i<FLAG_soc_high) || (h<FLAG_hdd_high)); i++, h++) {
    for (int j=0; j<FLAG_count; ++j) {
      fan_control.ReadFanSpeed(&fan_speed);
      fan_control.AdjustSpeed(
                  static_cast<uint16_t>(i), static_cast<uint16_t>(h), fan_speed);
      fan_control.ReadFanSpeed(&fan_speed);
      fan_control.ReadSocVoltage(&soc_voltage);
      LOG(LS_INFO) << "voltage:" << soc_voltage
                   << "  emu-soc_temperature:" << i
                   << "  emu-hdd_temperature:" << h
                   << "  fanspeed:" << fan_speed;
      sleep(FLAG_interval);
    }
  }
  if (FLAG_dec_temp == true) {
    for (i=FLAG_soc_high, h=FLAG_hdd_high;
         ((i>FLAG_soc_low) || (h>FLAG_hdd_low)); i--, h--) {
      for (int j=0; j<FLAG_count; ++j) {
        fan_control.ReadFanSpeed(&fan_speed);
        fan_control.AdjustSpeed(
                    static_cast<uint16_t>(i), static_cast<uint16_t>(h), fan_speed);
        fan_control.ReadFanSpeed(&fan_speed);
        fan_control.ReadSocVoltage(&soc_voltage);
        LOG(LS_INFO) << "voltage:" << soc_voltage
                     << "  emu-soc_temperature:" << i
                     << "  emu-hdd_temperature:" << h
                     << "  fanspeed:" << fan_speed;
        sleep(FLAG_interval);
      }
    }
  }

  fan_control.Terminate();
  return 0;
}

#ifdef __cplusplus
 } // extern "C" {
#endif
