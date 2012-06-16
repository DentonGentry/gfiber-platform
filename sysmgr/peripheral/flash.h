// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#ifndef BRUNO_PLATFORM_PERIPHERAL_FLASH_H_
#define BRUNO_PLATFORM_PERIPHERAL_FLASH_H_

#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <mntent.h>
#include <list>
#include <vector>
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include "bruno/constructormagic.h"
#include "bruno/scoped_ptr.h"
#include "common.h"
#include "ubifsmon.h"
#include "flash.h"

namespace bruno_platform_peripheral {

class FactoryResetButton;
class Common;

class UbifsMountEntry {
 public:
  UbifsMountEntry()
      : ubi_dev_name_(std::string()),
        ubi_dev_number_(std::string()),
        ubi_vol_name_(std::string()),
        dir_name_(std::string()),
        ubi_vol_id_(std::string()) {
  }

  UbifsMountEntry(std::string dev_name, std::string vol_number,
         std::string vol_name, std::string dir_name, std::string vol_id)
      : ubi_dev_name_(dev_name),
        ubi_dev_number_(vol_number),
        ubi_vol_name_(vol_name),
        dir_name_(dir_name),
        ubi_vol_id_(vol_id) {
  }


  void SetMountEntryInfo(std::string ubi_dev_name, std::string ubi_dev_number,
                     std::string ubi_vol_name, std::string dir_name,
                     std::string vol_id) {
    ubi_dev_name_ = ubi_dev_name;
    ubi_dev_number_ = ubi_dev_number;
    ubi_vol_name_ = ubi_vol_name;
    dir_name_ = dir_name;
    ubi_vol_id_ = vol_id;
  }

  UbifsMountEntry& operator = (UbifsMountEntry& param) {
    ubi_dev_name_   = param.ubi_dev_name_;
    ubi_dev_number_ = param.ubi_dev_number_;
    ubi_vol_name_   = param.ubi_vol_name_;
    dir_name_       = param.dir_name_;
    ubi_vol_id_     = param.ubi_vol_id_;
    return *this;
  }

  std::string GetUbiDevName(void) { return ubi_dev_name_; }
  std::string GetUbiDevNumber(void) { return ubi_dev_number_; }
  void SetUbiDevNumber(std::string& dev_number) {
        ubi_dev_number_ = dev_number; }
  std::string GetUbiVolName(void) { return ubi_vol_name_; }
  std::string GetDirName(void) { return dir_name_; }
  std::string GetUbiVolId(void) { return ubi_vol_id_; }
  void SetVolumeId(std::string vol_id) { ubi_vol_id_ = vol_id; }

 private:
  std::string ubi_dev_name_;      /* ubi1, ubi2... */
  std::string ubi_dev_number_;    /* ubiN - N = 0, 1, 2... */
  std::string ubi_vol_name_;      /* user, config, scrtach... */
  std::string dir_name_;          /* directory name of the volume */
  std::string ubi_vol_id_;

  // DISALLOW_COPY_AND_ASSIGN(UbifsMountEntry);
};


class Flash : public sigslot::has_slots<>, public bruno_base::MessageHandler {
 public:
  enum ResetEventType {
    EVENT_FACTORY_RESET,
    EVENT_ERASE_RO_VOL
  };

  static const std::string  kProcMountsFile;
  static const std::string  kFsType;
  static const std::string  kFsDevDelimiters;
  static const std::string  kVolumeIdStr;
  static const std::string  kFsNameDelimiter;
  static const std::string  kMntOptsDelimiter;
  static const std::string  kMntVolAttr;

  Flash() : mgr_thread_(NULL) {}
  virtual ~Flash() {}

  void Init(bruno_base::Thread*& mgr_thread,
            bruno_base::scoped_ptr<UbifsMon>& ubifs_mon);
  bool ProcessRoUbiVolumes(void);
  bool ProcessSpecifiedUbiVolume(std::string& ubi_vol_name);
  bool ReadOnlyVolumeList(std::list<UbifsMountEntry>& mntList);
  bool UnmountEraseUbiVolume(UbifsMountEntry& mnt_vol);
  bool GetMountedVolumeInfo(std::string& ubi_vol_name, UbifsMountEntry& mnt_vol);
  bool GetUbiVolId(UbifsMountEntry& mnt_list);
  bool GetUbiVolDevNumber(UbifsMountEntry& mnt_vol);
  bool EraseUbiVolume(UbifsMountEntry& mnt_vol);
  bool UmountVolume(UbifsMountEntry& mnt_vol);
  bool TerminateProcesses(UbifsMountEntry& mnt_vol);
  void OnMessage(bruno_base::Message* msg);
  void OnResetEvent(void);
  bool FactoryReset(void);
  void OnRecvRoUbiFsEvent(void);

 private:
  // The manager thread is the thread which handles all message dispatching.
  // e.g. it could be sysmgr thread.
  bruno_base::Thread* mgr_thread_;

  void GenFactoryResetVolList(std::list<std::string>& vol_list);
  bool EraseReadOnlyVolumes(void);

  DISALLOW_COPY_AND_ASSIGN(Flash);
};

}  // namespace bruno_platform_peripheral

#endif  // BRUNO_PLATFORM_PERIPHERAL_FLASH_H
