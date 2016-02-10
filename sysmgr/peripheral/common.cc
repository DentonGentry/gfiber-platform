// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#include <sstream>
#include <string>
#include <stdlib.h>
#include "bruno/logging.h"
#include <fcntl.h>
#include "common.h"


namespace bruno_platform_peripheral {

bool Reboot() {
  sync();
  int ret = reboot(LINUX_REBOOT_CMD_RESTART);
  if (ret < 0) {
    LOG(LS_ERROR) << "Reboot: failed (ret=" << ret << ")" << std::endl;
    return false;
  }
  return true;
}


bool Poweroff() {
  sync();
  int ret = system("poweroff-with-message 'poweroff requested by sysmgr'");
  if (ret != 0) {
    LOG(LS_ERROR) << "Poweroff: failed (ret=" << ret << ")" << std::endl;
    return false;
  }
  return true;
}


void SetLEDOverheat(const std::string& message) {
  std::ofstream file;
  const char *filename = OVERHEATING_LED_FILE;
  const char *led_pattern = OVERHEATING_LED_ON;

  /* Set LED by sending string to GPIO mailbox */
  file.open(filename);
  file << led_pattern << std::endl;
  file.close();

  if (!message.empty()) {
    /* Send the message to TR69 */
    file.open(TR69_MSG_FILE, std::ios::app);
    file << message << std::endl;
    file.close();
  }
}

void ClrLEDOverheat(const std::string& message) {
  std::string filename = OVERHEATING_LED_FILE;

  unlink(filename.c_str());

  if (!message.empty()) {
    /* Send the message to TR69 */
    std::ofstream file;
    file.open(TR69_MSG_FILE, std::ios::app);
    file << message << std::endl;
    file.close();
  }
}

/* Convert from string to floating point number */
bool ConvertStringToFloat(const std::string& value_str, float *value) {
  std::stringstream ss(value_str);

  if((ss >> *value).fail()) {
    *value = 0.0;
    LOG(LS_ERROR) << "ConvertStringToFloat: Failed to convert" << std::endl;
    return false;
  }
  return true;
}

/* Convert from string to integer */
bool ConvertStringToUint16(const std::string& value_str, uint16_t *value) {
  std::stringstream ss(value_str);

  if((ss >> *value).fail()) {
    *value = 0;
    LOG(LS_ERROR) << "ConvertStringToInt: Failed to convert" << std::endl;
    return false;
  }
  return true;
}

/* Convert from integer to string */
void ConvertUint16ToString(const uint16_t& value, std::string *value_str) {
  std::stringstream ss;
  ss << value;
  *value_str = ss.str();
  LOG(LS_VERBOSE) << "ConvertUint16ToString: value_str=" << *value_str << std::endl;
}

}  // namespace bruno_platform_peripheral
