// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#ifndef BRUNO_PLATFORM_PERIPHERAL_COMMON_H_
#define BRUNO_PLATFORM_PERIPHERAL_COMMON_H_

#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <list>
#include <vector>
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include "base/constructormagic.h"

namespace bruno_platform_peripheral {

class Common {
 public:
  enum ExecCmdCompareTypes {
    STRING_COMPARE,
    STRING_FIND,
    STRING_RETRUN_ALL_MSGS
  };

  static std::string ExecCmd(std::string& cmd, std::string *pattern,
                             enum ExecCmdCompareTypes action);
  static void Split(const std::string& str,
              const std::string& delimiters, std::vector<std::string>& tokens);
  static bool Reboot();

};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_COMMON_H_
