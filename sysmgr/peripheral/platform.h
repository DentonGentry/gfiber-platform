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
  BRUNO_GFRG200,          /* Sideswipe noHDD */
  BRUNO_GFRG210,          /* Optimus HDD */
  BRUNO_GFRG250,          /* Optimus Prime HDD */
  BRUNO_GFSC100,          /* Spacecast */
  BRUNO_GFHD200,          /* Camaro */
  BRUNO_GFLT110,          /* Fiber Jack */
  BRUNO_GFHD254,          /* Lockdown */
  BRUNO_GFLT300,          /* Go-Long FiberJack */
  BRUNO_GFLT400,          /* Co-ax Jack */
  BRUNO_GFCH100,          /* Chimera mm-wave */
  BRUNO_UNKNOWN
};

class Platform {

 public:
  explicit Platform()
      : name_("Unknown"), type_(BRUNO_UNKNOWN), has_hdd_(false),
        has_aux1_(false), has_fan_(false) {}

  Platform(const std::string& name, BrunoPlatformTypes type, bool has_hdd,
           bool has_aux1, bool has_fan)
      : name_(name), type_(type), has_hdd_(has_hdd), has_aux1_(has_aux1),
        has_fan_(has_fan) {}

  virtual ~Platform() {}

  static const Platform kPlatformTable[];

  void Init(void);
  std::string PlatformName(void) const { return name_; }
  enum BrunoPlatformTypes PlatformType(void) const { return type_; }
  bool has_hdd(void) const { return has_hdd_; }
  bool has_fan(void) const { return has_fan_; }
  bool has_aux1(void) const { return has_aux1_; }
  std::string GetLine(char *file, std::string *pattern);

 private:
  std::string name_;
  BrunoPlatformTypes type_;
  bool has_hdd_;
  bool has_aux1_;
  bool has_fan_;

  void GetPlatformType(void);
  DISALLOW_COPY_AND_ASSIGN(Platform);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_PLATFORM_H_
