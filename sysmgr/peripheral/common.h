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

bool Reboot();
bool Poweroff();
void SetLEDOverheat(const std::string& message);
void ClrLEDOverheat(const std::string& message);
bool ConvertStringToFloat(const std::string& value_str, float *value);
bool ConvertStringToUint16(const std::string& value_str, uint16_t *value);
void ConvertUint16ToString(const uint16_t& value, std::string *value_str);

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_COMMON_H_
