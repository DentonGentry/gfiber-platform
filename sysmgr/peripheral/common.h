// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#ifndef BRUNO_PLATFORM_PERIPHERAL_COMMON_H_
#define BRUNO_PLATFORM_PERIPHERAL_COMMON_H_

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <list>
#include <vector>
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include "bruno/constructormagic.h"

#define OVERHEATING_LED_FILE   "/tmp/leds/overheating"
#define TR69_MSG_FILE   "/tmp/cwmp/sysmgr"

#define OVERHEATING_LED_ON  "1 0 1 0 1 0"

namespace bruno_platform_peripheral {

class Common {
 public:
  enum ExecCmdCompareTypes {
    STRING_COMPARE,
    STRING_FIND,
    STRING_RETRUN_ALL_MSGS
  };

  enum LedControl {
    OVERHEATING
  };

  static const std::string kErrorString;

  static std::string ExecCmd(std::string& cmd, std::string *pattern,
                             enum ExecCmdCompareTypes action);
  static void Split(const std::string& str,
              const std::string& delimiters, std::vector<std::string>& tokens);
  static bool Reboot();
  static void SetLED(LedControl led, const std::string message);
  static void ClrLED(LedControl led, const std::string message);
  static bool ConvertStringToFloat(const std::string& value_str, float *value);
  static bool ConvertStringToUint16(const std::string& value_str, uint16_t *value);
  static void ConvertUint16ToString(const uint16_t& value, std::string *value_str);

  DISALLOW_COPY_AND_ASSIGN(Common);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_COMMON_H_
