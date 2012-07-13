// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#include <sstream>
#include <string>
#include "bruno/logging.h"
#include <fcntl.h>
#include "common.h"


namespace bruno_platform_peripheral {

const std::string Common::kErrorString = "ERROR";


std::string Common::ExecCmd(std::string& cmd, std::string *pattern,
                           enum ExecCmdCompareTypes action) {
  char buffer[256];
  std::string result = "";
  FILE* pipe = popen(cmd.c_str(), "r");

  LOG(LS_INFO) << "ExecCmd: cmd= " << cmd << " action= " << action << std::endl
               << "pattern= " << ((pattern == NULL)? "NULL": *pattern) << std::endl;
  if (!pipe) {
    LOG(LS_ERROR) << "ExecCmd(): ERROR" << std::endl;
    return kErrorString;
  }
  int is_continue = true;
  size_t found;

  while((!feof(pipe)) && (is_continue)) {
    if(fgets(buffer, sizeof(buffer), pipe) != NULL) {
      /* pattern == NULL, read and return all of lines
       * pattern != NULL, return the line if found the pattern in the line
       */
      if (pattern != NULL) {
        result = std::string(buffer);
      }
      switch (action) {
        case STRING_COMPARE:
          /* Compare strings when the pattern is in beginning of the
           * compared string.
           */
          if (result.compare(0, pattern->size(), *pattern) == 0) {
            is_continue = false;  /* Found the pattern. Exit. */
            break;
          }
          result.clear();
          break;
        case STRING_FIND:
          /* Find in the compared string if the starting position is unknown.
           * More time consuming.
           */
          found = result.find(*pattern);
          if (found != std::string::npos) {
            LOG(LS_VERBOSE) << "ExecCmd: FOUND **result= " << result << std::endl;
            is_continue = false;  /* Found the pattern. Exit. */
            break;
          }
          result.clear();
          break;
        case STRING_RETRUN_ALL_MSGS:
          result += buffer;
          break;
      }
    }
  }

  pclose(pipe);
  return result;
}

void Common::Split(const std::string& str,
            const std::string& delimiters, std::vector<std::string>& tokens) {
  /* Skip delimiters at beginning. */
  LOG(LS_VERBOSE) << "Split: str= " << str
                  << "delimiters= " << delimiters << std::endl;
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  /* Find first "non-delimiter". */
  std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos) {
      // Found a token, add it to the vector.
      tokens.push_back(str.substr(lastPos, pos - lastPos));
      // Skip delimiters.
      lastPos = str.find_first_not_of(delimiters, pos);
      // Find next "non-delimiter"
      pos = str.find_first_of(delimiters, lastPos);
  }

  /* Debug print only. Remove later. */
  for (size_t i = 0; i < tokens.size(); i++) {
    LOG(LS_VERBOSE) << "idx= " << i << " token= " << tokens[i] << std::endl;
  }

  LOG(LS_VERBOSE) << "Split: exit." << std::endl;
}


bool Common::Reboot() {
  bool  is_ok = true;

  sync();
  int ret = reboot(LINUX_REBOOT_CMD_RESTART);
  if (ret < 0) {
    LOG(LS_ERROR) << "Reboot: Failed reboot (ret= " << ret << ")" << std::endl;
    is_ok = false;
  }
  return(is_ok);
}

void Common::SetLED(const std::string led, const std::string message) {
  std::ofstream file;

  /* Set LED by sending string to GPIO mailbox */
  file.open (GPIO_LED_FILE);
  file << led << std::endl;
  file.close();

  if (!message.empty()) {
    /* Send the message to TR69 */
    file.open(TR69_MSG_FILE, std::ios::app);
    file << message << std::endl;
    file.close();
  }
}

/* Convert from string to floating point number */
bool Common::ConvertStringToFloat(const std::string& value_str, float *value) {
  bool  rtn = true;
  std::stringstream ss(value_str);

  if((ss >> *value).fail()) {
    *value = 0.0;
    rtn = false;
    LOG(LS_ERROR) << "ConvertStringToFloat: Failed to convert" << std::endl;
  }
  return rtn;
}

/* Convert from string to integer */
bool Common::ConvertStringToUint16(const std::string& value_str, uint16_t *value) {
  bool  rtn = true;
  std::stringstream ss(value_str);

  if((ss >> *value).fail()) {
    *value = 0;
    rtn = false;
    LOG(LS_ERROR) << "ConvertStringToInt: Failed to convert" << std::endl;
  }
  return rtn;
}


/* Convert from integer to string */
void Common::ConvertUint16ToString(const uint16_t& value, std::string *value_str) {
  std::stringstream ss;
  ss << value;
  *value_str = ss.str();
  LOG(LS_VERBOSE) << "ConvertUint16ToString: value_str=" << *value_str << std::endl;
}

}  // namespace bruno_platform_peripheral
