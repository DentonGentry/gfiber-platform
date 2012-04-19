// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#include <sstream>
#include <string>
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/sigslot.h"
#include "base/thread.h"
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <mtd/ubi-user.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include "flash.h"


namespace bruno_platform_peripheral {

const std::string  Flash::kProcMountsFile = "/proc/mounts";
const std::string  Flash::kFsType = "ubifs";
const std::string  Flash::kFsDevDelimiters = "0123456789";
const std::string  Flash::kVolumeIdStr = "Volume";
const std::string  Flash::kFsNameDelimiter = ":";
const std::string  Flash::kMntOptsDelimiter = ",";
const std::string  Flash::kMntVolAttr = "ro";

void Flash::Init(bruno_base::Thread*& mgr_thread,
            bruno_base::scoped_ptr<FactoryResetButton>& factory_reset_button,
            bruno_base::scoped_ptr<UbifsMon>& ubifs_mon) {
  mgr_thread_ = mgr_thread;
  factory_reset_button->SignalResetEvent.connect(this, &Flash::OnResetEvent);
  ubifs_mon->SignalRecvRoUbiFsEvent.connect(this, &Flash::OnRecvRoUbiFsEvent);
}

bool Flash::ProcessRoUbiVolumes() {
  std::list<UbifsMountEntry> mnt_list;
  bool  is_ok = true;

  do {
    /* Get mounted Read-Only UBI volume list */
    if (ReadOnlyVolumeList(mnt_list) == false) {
      /* Cannot find mounted RO volume */
      LOG(LS_ERROR) << "ProcessRoUbiVolumes(): cannot find RO UBI volume" << std::endl;
      is_ok = false;
      break;
    }

    /* Unmount them */
    std::list<UbifsMountEntry>::iterator mnt_vol;
    for (mnt_vol = mnt_list.begin(); mnt_vol != mnt_list.end(); mnt_vol++) {
      if (UnmountEraseUbiVolume(*mnt_vol) == false) {
        LOG(LS_ERROR) << "ProcessRoUbiVolumes(): cannot erase the RO UBI volume"
                      << std::endl;
        /* Indicate at least one UBI volume can't be unmounted/erased */
        is_ok = false;
      }
    }
  } while (false);

  LOG(LS_VERBOSE) << "ProcessRoUbiVolumes(): is_ok= " << is_ok << std::endl;
  return is_ok;
}


bool Flash::ProcessSpecifiedUbiVolume(std::string& ubi_vol_name) {
  bool  is_ok = false;
  UbifsMountEntry mnt_vol;

  LOG(LS_INFO) << "ProcessSpecifiedUbiVolume: ubi_vol_name= " << ubi_vol_name
               << std::endl;
  do {
    /* Get mounted Read-Only UBI volume list */
    if (GetMountedVolumeInfo(ubi_vol_name, mnt_vol) == false) {
      /* Cannot find mounted RO volume */
      LOG(LS_ERROR) << "ProcessSpecifiedUbiVolume: cannot find volume=  "
                    << ubi_vol_name << std::endl;
      break;
    }

    /* Unmount the UBI volume. */
    if (UnmountEraseUbiVolume(mnt_vol) == false) {
      LOG(LS_ERROR) << "ProcessSpecifiedUbiVolume: cannot erase the RO UBI volume"
                    << std::endl;
      /* Indicate at least one UBI volume can't be unmounted/erased */
      break;
    }
    is_ok = true;
  } while (false);

  LOG(LS_INFO) << "ProcessSpecifiedUbiVolume: is_ok= " << is_ok << std::endl;
  return is_ok;
}


bool Flash::ReadOnlyVolumeList(std::list<UbifsMountEntry>& mnt_list) {
  FILE    *mtab = NULL;
  struct  mntent *part = NULL;
  bool    is_found = false;

  LOG(LS_VERBOSE) << "ReadOnlyVolumeList()" << std::endl;
  if ((mtab = setmntent(kProcMountsFile.c_str(), "r")) != NULL) {
    LOG(LS_VERBOSE) << "is_mounted(): mtab=" << std::hex << mtab << std::endl;
    while ((part = getmntent(mtab)) != NULL) {
      LOG(LS_VERBOSE) << "is_mounted(): part=" << std::hex << part << std::endl;
      if ((part->mnt_fsname != NULL) &&
          (kFsType.compare(part->mnt_type)) == 0) {
        std::vector<std::string> tokens;
        /* Split the mnt_opts */
        Common::Split(part->mnt_opts, kMntOptsDelimiter, tokens);
        if ((tokens.at(0).compare(kMntVolAttr)) == 0) {
          LOG(LS_INFO) << "ReadOnlyVolumeList: volume is " << tokens.at(0) << std::endl;
          tokens.clear();     /* Empty the tokens */
          Common::Split(part->mnt_fsname, kFsNameDelimiter, tokens);
          mnt_list.push_back(UbifsMountEntry(tokens.at(0), std::string(), tokens.at(1),
                             part->mnt_dir, std::string()));
          GetUbiVolDevNumber(mnt_list.back());
          GetUbiVolId(mnt_list.back());
          is_found = 1;
        }
      }
    }
    endmntent (mtab);
  }

  if (is_found) {
    LOG(LS_INFO) << "Read-only UBIFS volume: size= " << mnt_list.size() << std::endl;
    for (std::list<UbifsMountEntry>::iterator it = mnt_list.begin();
         it != mnt_list.end(); it++) {
      LOG(LS_INFO) << "ubi_dev_name_= " << it->GetUbiDevName() << std::endl
                   << "ubi_dev_number_= " << it->GetUbiDevNumber() << std::endl
                   << "ubi_vol_name_= " << it->GetUbiVolName() << std::endl
                   << "dir_name_= " << it->GetDirName() << std::endl
                   << "ubi_vol_id_= " << it->GetUbiVolId() << std::endl;
    }
  }
  LOG(LS_VERBOSE) << "ReadOnlyVolumeList: is_found= " << is_found << std::endl;
  return is_found;
}


bool Flash::UnmountEraseUbiVolume(UbifsMountEntry& mnt_vol) {
  bool  is_erased = false;

  do {
    bool  is_unmounted = false;
    /* Retry up to 3 times */
    for (int retry = 0; retry < 3; retry++) {
      /* Scan and terminate the processes using the volume */
      if (TerminateProcesses(mnt_vol) == false) {
        LOG(LS_ERROR) << "UnmountEraseUbiVolume: failed." << std::endl;
        break;
      }

      /* Unmount the ubi volume */
      if (UmountVolume(mnt_vol) == true) {
        /* The command is succeeded */
        is_unmounted = true;
        break;
      }
      LOG(LS_ERROR) << "UnmountEraseUbiVolume: failed." << std::endl;
    }

    if (is_unmounted == false) {
      /* Failed to kill the or unmount the processes */
      LOG(LS_ERROR) << "UnmountEraseUbiVolume: failed." << std::endl;
      break;
    }

    /* Erase the volume */
    if (EraseUbiVolume(mnt_vol) == false) {
      /* Failed to erase UBI volume */
      LOG(LS_ERROR) << ": EraseUbiVolume failed " << std::endl;
      break;
    }

    is_erased = true;

  } while(false);

  LOG(LS_INFO) << "UnmountEraseUbiVolume(): is_erased= " << is_erased << std::endl;
  return is_erased;
}


bool Flash::GetMountedVolumeInfo(std::string& ubi_vol_name, UbifsMountEntry& mnt_vol) {
  FILE    *mtab = NULL;
  struct  mntent *part = NULL;
  bool    is_found = false;

  LOG(LS_VERBOSE) << "GetMountedVolumeInfo: ubi_vol_name= " << ubi_vol_name << std::endl;

  if (( mtab = setmntent (kProcMountsFile.c_str(), "r")) != NULL) {
    LOG(LS_VERBOSE) << "GetMountedVolumeInfo: mtab=" << std::hex << mtab << std::endl;
    while ((part = getmntent( mtab) ) != NULL) {
      LOG(LS_VERBOSE) << "GetMountedVolumeInfo: part=" << std::hex << part << std::endl;
      if ((part->mnt_fsname != NULL) &&
          ((kFsType.compare(part->mnt_type)) == 0)) {
        std::vector<std::string> tokens;
        /* Split the mnt_fsname to ubi_dev_name_ and ubi_ubi_vol_name_ */
        Common::Split(part->mnt_fsname, kFsNameDelimiter, tokens);
        /* Compare the ubi_vol_name */
        if ((ubi_vol_name.compare(tokens.at(1))) == 0) {
          mnt_vol.SetMountEntryInfo(tokens.at(0), std::string(), tokens.at(1),
                                part->mnt_dir, std::string());
          GetUbiVolDevNumber(mnt_vol);
          GetUbiVolId(mnt_vol);
          is_found = 1;
          break;
        }
      }
    }
    endmntent (mtab);
  }

  LOG(LS_VERBOSE) << "GetMountedVolumeInfo: is_mounted= " << is_found << std::endl
               << "ubi_dev_name_= " << mnt_vol.GetUbiDevName() << std::endl
               << "ubi_dev_number_= " << mnt_vol.GetUbiDevNumber() << std::endl
               << "ubi_vol_name_= " << mnt_vol.GetUbiVolName() << std::endl
               << "dir_name_= " << mnt_vol.GetDirName() << std::endl
               << "ubi_vol_id_= " << mnt_vol.GetUbiVolId() << std::endl;

  return is_found;
}


bool Flash::GetUbiVolId(UbifsMountEntry& mnt_vol) {
  bool        is_found = false;
  std::string cmd = "ubinfo -d " + mnt_vol.GetUbiDevNumber() + " -N " +
                    mnt_vol.GetUbiVolName();
  std::vector<std::string> tokens;

  do {
    LOG(LS_VERBOSE) << "GetUbiVolId: cmd= " << cmd << std::endl;
    std::string result = Common::ExecCmd(cmd, (std::string*)&kVolumeIdStr, Common::STRING_COMPARE);
    if ((result == "ERROR") || (result.empty() == true)) {
      /* The command failed. Exit */
      LOG(LS_ERROR) << "GetUbiVolId: Can't find volume ID";
      break;
    }

    /* Split the returned string and the Volume ID is in the 3rd element */
    Common::Split(result, " ", tokens);
    mnt_vol.SetVolumeId(tokens.at(2));
    is_found = true;
  } while (false);

  LOG(LS_VERBOSE) << "GetUbiVolId(): is_found=" << is_found
                  << " vol_num= " << mnt_vol.GetUbiVolId() << std::endl;

  return is_found;
}


bool Flash::GetUbiVolDevNumber(UbifsMountEntry& mnt_vol) {
  bool is_found = false;
  size_t  pos;
  std::string ubi_dev_name = mnt_vol.GetUbiDevName();
  std::string ubi_dev_num;

  pos = ubi_dev_name.find_first_of(kFsDevDelimiters);
  while (pos != std::string::npos) {
    ubi_dev_num += ubi_dev_name[pos];
    pos = ubi_dev_name.find_first_of(kFsDevDelimiters, pos+1);
    is_found = true;
  }

  if (is_found == true) {
    mnt_vol.SetUbiDevNumber(ubi_dev_num);
  }

  LOG(LS_VERBOSE) << "GetUbiVolDevNumber(): is_found=" << is_found
               << " vol_num= " << mnt_vol.GetUbiDevNumber() << std::endl;
  return is_found;
}


bool Flash::EraseUbiVolume(UbifsMountEntry& mnt_vol) {
  bool is_erased = false;
  // long long bytes = 0;
  int64_t bytes = 0;
  int  fd = 0;
  int  ioc_status;
  std::string ubi_node = "/dev/" + mnt_vol.GetUbiDevName() + "_" + mnt_vol.GetUbiVolId();

  LOG(LS_INFO) << "EraseUbiVolume: ubi_node=" << ubi_node << std::endl;

  /* Same as "ubiupdatevol <UBI volume node file name> -t" */
  do {
    fd = open(ubi_node.c_str(), O_RDWR);
    if (fd == -1) {
      LOG(LS_ERROR) << "EraseUbiVolume: open() failed" << std::endl;
      break;
    }

    if ((ioc_status = ioctl(fd, UBI_IOCVOLUP, &bytes))) {
      LOG(LS_ERROR) << "EraseUbiVolume: IOCTL failed ioc_status=" << std::hex << ioc_status << std::endl;
      break;
    }
    is_erased = true;
  } while (false);

  if (fd) {
    close(fd);
  }
  LOG(LS_INFO) << "GetUbiVolId(): is_erased=" << is_erased
               << " ubi_node= " << ubi_node << std::endl;
  return is_erased;
}


bool Flash::UmountVolume(UbifsMountEntry& mnt_vol) {
  bool  is_unmounted = true;
  int   status;
  std::string target = mnt_vol.GetDirName();

  LOG(LS_VERBOSE) << "UmountVolume: targete=" << target << std::endl;

  if ((status = umount(target.c_str()))) {
    LOG(LS_ERROR) << "UnmountVolume: umount() failed unmount " << target
                  << std::endl;
  }

  LOG(LS_INFO) << "UmountVolume: is_unmounted=" << is_unmounted
               << " target= " << target << std::endl;

  return is_unmounted;
}


/* Scan open files and terminate the processes using the volume
 * Assumption: Unmount the volume first.
 */
bool Flash::TerminateProcesses(UbifsMountEntry& mnt_vol) {
  bool        is_ok = true;

  std::string cmd;
  std::string lsof_cmd("lsof");
  std::string pattern = mnt_vol.GetDirName();
  std::vector<std::string> tokens;

  LOG(LS_VERBOSE) << "TerminateProcesses()" << std::endl;
  do {
    std::string result = Common::ExecCmd(lsof_cmd, &pattern, Common::STRING_FIND);
    if (result == "ERROR") {
      /* The command failed. Exit */
      LOG(LS_ERROR) << "TerminateProcesses: Can't find open file in "
                    << pattern << std::endl;
      is_ok = false;
      break;
    }
    else if (result.empty() == true) {
      LOG(LS_INFO) << "TerminateProcesses: result empty " << std::endl;
      break;
    }

    LOG(LS_VERBOSE) << "TerminateProcesses: result= " << result << std::endl;
    tokens.clear();
    /* Split the returned string and PID is in the 2rd element */
    Common::Split(result, " ", tokens);
    LOG(LS_INFO) << "TerminateProcesses: PID= " << tokens.at(1) << std::endl;
    pid_t proc_pid;
    std::stringstream ss(tokens.at(1));
    ss >> proc_pid;
    if (kill(proc_pid, SIGKILL) != 0) {
      /* Failed to send kill signal to the pid process. */
      LOG(LS_ERROR) << "TerminateProcesses: Can't terminate pid= "
                    << proc_pid << std::endl;
      is_ok = false;
      break;
    }
  } while (true);

  return is_ok;
}

void Flash::OnMessage(bruno_base::Message* msg) {
  LOG(LS_VERBOSE) << "Received message " << msg->message_id << std::endl;
  uint32_t type = msg->message_id;
  switch (type) {
    case EVENT_FACTORY_RESET:
      LOG(LS_VERBOSE) << "Received message EVENT_FACTORY_RESET" << std::endl;
      FactoryReset();
      break;
    case EVENT_ERASE_RO_VOL:
      LOG(LS_VERBOSE) << "Received message EVENT_ERASE_RO_VOL" << std::endl;
      EraseReadOnlyVolumes();
      break;
    default:
      LOG(LS_WARNING) << "Invalid message type, ignore ... " << type;
      break;
  }
}

void Flash::OnResetEvent() {
  LOG(LS_INFO) << "Received factory reset event" << std::endl;
  mgr_thread_->Post(this, static_cast<uint32>(EVENT_FACTORY_RESET));
}


bool Flash::FactoryReset() {
  bool is_ok = true;

  do {
    std::list<std::string> vol_list;
    Flash::GenFactoryResetVolList(vol_list);

    std::list<std::string>::iterator vol;
    for (vol = vol_list.begin(); vol != vol_list.end(); vol++) {
      is_ok = ProcessSpecifiedUbiVolume(*vol);
      if (is_ok == false) {
        LOG(LS_ERROR) << "Fail to erase " << *vol << std::endl;
        break;
      }
    }
    if (is_ok == false) {
      break;
    }

    is_ok = Common::Reboot();
    if (is_ok == false) {
      LOG(LS_ERROR) << "Fail to reboot" << std::endl;
      break;
    }
  } while (false);

  return is_ok;
}


void Flash::OnRecvRoUbiFsEvent() {
  LOG(LS_INFO) << "Received read-only UBI volume event" << std::endl;
  mgr_thread_->Post(this, static_cast<uint32>(EVENT_ERASE_RO_VOL));
}


bool Flash::EraseReadOnlyVolumes() {
  bool is_ok = true;

  do {
    is_ok = ProcessRoUbiVolumes();
    if (is_ok == false) {
      break;
    }

    is_ok = Common::Reboot();
    if (is_ok == false) {
      LOG(LS_ERROR) << "Fail to reboot" << std::endl;
      break;
    }
  } while (false);

  return is_ok;
}


void Flash::GenFactoryResetVolList(std::list<std::string>& vol_list) {
  vol_list.push_back("user");
  vol_list.push_back("config");
}

}  // namespace bruno_platform_peripheral
