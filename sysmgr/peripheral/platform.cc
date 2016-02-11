// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#include "bruno/logging.h"
#include "bruno/sigslot.h"
#include "platform.h"

namespace bruno_platform_peripheral {

/* Platform table */
const Platform Platform::kPlatformTable[] = {
         /* model     type           hdd    fan */
  Platform("GFMS100", BRUNO_GFMS100, true,  true),
  Platform("GFHD100", BRUNO_GFHD100, false, true),
  Platform("GFHD200", BRUNO_GFHD200, false, false),
  Platform("GFRG200", BRUNO_GFRG200, false, true),
  Platform("GFRG210", BRUNO_GFRG210, true,  true),
  Platform("GFRG250", BRUNO_GFRG250, true,  true),
  Platform("GFSC100", BRUNO_GFSC100, true,  true),
  Platform("GFLT110", BRUNO_GFLT110, false, false),
  Platform("GFLT120", BRUNO_GFLT110, false, false),
  Platform("GFHD254", BRUNO_GFHD254, false, true),
  Platform("UNKNOWN PLATFORM", BRUNO_UNKNOWN, false, false),
};

void Platform::Init(void) {
  GetPlatformType();
}

#define ARRAYSIZE(a) \
  (static_cast<size_t>((sizeof(a) / sizeof(*(a)))) /  \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

void Platform::GetPlatformType(void) {
  std::string result = GetLine((char *)PLATFORM_FILE, NULL);

  for (int i = 0; i < (int)ARRAYSIZE(Platform::kPlatformTable); i++) {
    if (result == kPlatformTable[i].name_) {
      name_ = kPlatformTable[i].name_;
      type_ = kPlatformTable[i].type_;
      has_hdd_ = kPlatformTable[i].has_hdd_;
      has_fan_ = kPlatformTable[i].has_fan_;
      break;
    }
  }

  if (type_ == BRUNO_UNKNOWN) {
    LOG(LS_ERROR) << "Unsupported Platform - " << result << std::endl;
  }
  fprintf(stderr, "plat_type=%s\n", name_.c_str());
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
