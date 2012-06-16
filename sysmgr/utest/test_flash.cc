#include "bruno/basictypes.h"
#include "bruno/criticalsection.h"
#include "bruno/logging.h"
#include "bruno/flags.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_avs.h"
#include <stdio.h>
#include <signal.h>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <mntent.h>
#include <string.h>
#include <sstream>
#include <string>
#include "flash.h"
#include <sys/time.h>
#include <sys/resource.h>

#ifdef __cplusplus
extern "C" {
#endif

class UbifsMountEntry;

int main(int argc, char** argv) {
  DEFINE_int(interval, 5, "Interval");
  DEFINE_bool(debug, false, "Enable debug log");
  DEFINE_bool(help, false, "Prints this message");
  DEFINE_string(ubidev, "", "ubi device name");
  DEFINE_string(testitem, "", "test item");

  // parse options
  if (0 != FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    FlagList::Print(NULL, false);
    return 0;
  }

  if (FLAG_help) {
    FlagList::Print(NULL, false);
    return 0;
  }

  if (FLAG_debug) {
    bruno_base::LogMessage::LogToDebug(bruno_base::LS_VERBOSE);
  } else {
    bruno_base::LogMessage::LogToDebug(bruno_base::LS_INFO);
  }


  std::string test_item(FLAG_testitem);
  bruno_platform_peripheral::Flash flash_control;
  bruno_platform_peripheral::UbifsMountEntry mnt_vol;

  std::string ubi_vol_name(FLAG_ubidev);
  if (test_item.compare("SPEC") == 0) {
    std::string dev_vol_name(FLAG_ubidev);
    flash_control.ProcessSpecifiedUbiVolume(dev_vol_name);
  }
  else if (test_item.compare("RO") == 0) {
    flash_control.ProcessRoUbiVolumes();
  }
  return 0;
}

#ifdef __cplusplus
} // extern "C" {
#endif
