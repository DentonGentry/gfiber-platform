#include "bruno/basictypes.h"
#include "bruno/criticalsection.h"
#include "bruno/logging.h"
#include "bruno/flags.h"
#include "bruno/scoped_ptr.h"
#include "platform_peripheral_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "mailbox.h"

#ifdef __cplusplus
extern "C" {
#endif

int main(int argc, char** argv) {
  DEFINE_int(interval, 5, "Monitor interval in second");
  DEFINE_int(count, 10, "Repeat times");
  DEFINE_bool(debug, false, "Enable debug log");
  DEFINE_bool(help, false, "Prints this message");
  DEFINE_bool(fan_speed, false, "Get fan speed");
  DEFINE_bool(cpu_temperature, false, "Get cpu temperature");
  DEFINE_bool(cpu_voltage, false, "Get cpu voltage");
  DEFINE_int(fan_percent, 60, "fan PWM (0 - 100)");

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

  LOG(LS_INFO) << "interval= " << FLAG_interval << std::endl;

  bruno_platform_peripheral::Mailbox mail_box;
  float soc_temperature;
  std::string str_value;
  uint16_t  fan_dutycycle = 0;
  uint16_t  fan_percent = static_cast<uint16_t>(FLAG_fan_percent);
  bool rtn;

  for (int i=0; i<FLAG_count; ++i) {
    LOG(LS_INFO) << "i=" << i ;

    if (FLAG_fan_speed == true) {
      if (mail_box.ReadFanSpeed(&str_value) == true) {
        LOG(LS_INFO) << " fan_speed=" << str_value;
      }
    }

    if (FLAG_cpu_temperature == true) {
      rtn = mail_box.ReadSocTemperature(&soc_temperature);
      if (rtn == true) {
        LOG(LS_INFO) << " cpu_temperature= float-" << soc_temperature;
      }
    }

    if (FLAG_cpu_voltage == true) {
      if (mail_box.ReadSocVoltage(&str_value) == true) {
        LOG(LS_INFO) << " cpu_voltage=" << str_value;
      }
    }

    rtn = mail_box.WriteFanDutyCycle(fan_percent);
    if (rtn == true) {
      fan_dutycycle = 0;
      mail_box.ReadFanDutyCycle(&fan_dutycycle);
      LOG(LS_INFO) << " fan_percent=" << fan_dutycycle;
    }
    LOG(LS_INFO) << std::endl;

    sleep(FLAG_interval);
  }

  return 0;
}


#ifdef __cplusplus
 } // extern "C" {
#endif
