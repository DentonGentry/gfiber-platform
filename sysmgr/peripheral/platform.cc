// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#include "bruno/logging.h"
#include "bruno/sigslot.h"
#include "platform.h"

namespace bruno_platform_peripheral {

// Global Platform class instance
Platform* platformInstance_ = NULL;

/* Platform table */
const Platform Platform::kPlatformTable[] = {
  Platform("GFMS100", BRUNO_GFMS100, true),
  Platform("GFHD100", BRUNO_GFHD100, false),
  Platform("UNKNOWN PLATFORM", BRUNO_UNKNOWN, false),
};

void Platform::Init(void) {
  /* Default the platform type */
  name_ = kPlatformTable[BRUNO_UNKNOWN].name_;
  type_ = kPlatformTable[BRUNO_UNKNOWN].type_;
  has_hdd_ = kPlatformTable[BRUNO_UNKNOWN].has_hdd_;

  GetPlatformType();
}

void Platform::GetPlatformType(void) {
  std::ifstream platform_file;

  std::string result = GetLine((char *)PLATFORM_FILE, NULL);

  if (result.empty() == false) {
    for (int i = BRUNO_GFMS100; i < BRUNO_MAX_NUM; i++) {
      if ((result.size() == kPlatformTable[i].name_.size()) &&
          (result.compare(0, kPlatformTable[i].name_.size(), kPlatformTable[i].name_) == 0)) {
        name_ = kPlatformTable[i].name_;
        type_ = kPlatformTable[i].type_;
        has_hdd_ = kPlatformTable[i].has_hdd_;
        break;
      }
    }
  }

  if (type_ == BRUNO_UNKNOWN) {
    LOG(LS_ERROR) << "Unsupported Platform - " << result << std::endl;
  }
  return;
}

/* Search the pattern in the file.
 * 1) If the pattern found in the string, return the string.
 * 2) Otherwse, a NULL string.
 */
std::string Platform::GetLine(char *file, std::string *pattern) {
  std::string result;
  std::ifstream opened_file;

  result.clear();
  opened_file.open (file);
  if(opened_file.is_open()) {
    while(!opened_file.eof()) {
      getline(opened_file, result);
      if (pattern == NULL) {
        /* If no pattern specified, return the first read line */
        break;
      }
      /* skip this line if leading chararcter is '#' */
      else if (result[0] != COMMENT_CHAR &&
          result.compare(0, pattern->size(), *pattern) == 0) {
        break;      /* Found the pattern. Exit */
      }
    }
    opened_file.close();
  }
  else {
    LOG(LS_INFO) << "Unable to open this file." << std::endl;
  }

  return result;
}

}  // namespace bruno_platform_peripheral
