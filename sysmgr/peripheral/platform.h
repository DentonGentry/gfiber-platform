// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#ifndef BRUNO_PLATFORM_PERIPHERAL_PLATFORM_H_
#define BRUNO_PLATFORM_PERIPHERAL_PLATFORM_H_

#include <iostream>
#include <fstream>
#include <string>
#include "bruno/constructormagic.h"
#include "bruno/scoped_ptr.h"

#define PLATFORM_FILE   "/etc/platform"
#define COMMENT_CHAR '#'

namespace bruno_platform_peripheral {

enum BrunoPlatformTypes {
  BRUNO_PLATFORM_FIRST = 0,
  BRUNO_GFMS100 = 0,      /* Bruno-IS */
  BRUNO_GFHD100,          /* Bruno */
  BRUNO_GFRG200,          /* Optimus noHDD */
  BRUNO_GFRG210,          /* Optimus HDD */
  BRUNO_GFSC100,          /* Spacecast */
  BRUNO_PLATFORM_MAX
};

#define BRUNO_UNKNOWN   BRUNO_PLATFORM_MAX

class Platform {

 public:
  Platform(const std::string name, enum BrunoPlatformTypes type, bool has_hdd)
      : name_(name), type_(type), has_hdd_(has_hdd) {}

  static const Platform kPlatformTable[];

  virtual ~Platform() {}
  void Init(void);
  std::string PlatformName(void) const { return name_; }
  enum BrunoPlatformTypes PlatformType(void) const { return type_; }
  bool PlatformHasHdd(void) const { return has_hdd_; }
  std::string GetLine(char *file, std::string *pattern);

 private:
  std::string name_;
  enum BrunoPlatformTypes type_;
  bool has_hdd_;

  void GetPlatformType(void);
  DISALLOW_COPY_AND_ASSIGN(Platform);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_PLATFORM_H_
