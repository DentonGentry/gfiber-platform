/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../common/util.h"

#define TPM_INIT_MODE 'a'
#define TPM_OPTION "a"
#define TPM_BASIC_STARTUP_CMD "tpmstartup"
#define TPM_INIT_STARTUP_CMD "tpmstartup -a"

static int tpm_startup_usage() {
  printf("tpm_startup [-%c]\n", TPM_INIT_MODE);
  printf("NOTE: Lock Physical Presence only works the first time after\n");
  printf("      powering up. Subsequent lock will result in error.\n");
  printf("      Assert Physical Presence only works if the chip was never\n");
  printf("      initialized. Subsequent assert will result in error.\n");
  printf("Example:\n");
  printf("tpm_startup\n");
  printf("Perform Startup Clear, Selftest and Lock Physical Presence\n");
  printf("tpm_startup -%c\n", TPM_INIT_MODE);
  printf("Perform Startup Clear, Selftest, Enable Physical Presence,\n");
  printf("  Assert Physical Presence, Enable TPM and Activate TPM\n");

  return -1;
}

int tpm_startup(int argc, char *argv[]) {
  int opt;

  if (argc == 1) {
    system_cmd(TPM_BASIC_STARTUP_CMD);
  } else {
    switch (opt = getopt(argc, argv, TPM_OPTION)) {
      case TPM_INIT_MODE:
        system_cmd(TPM_INIT_STARTUP_CMD);
        break;
      default:
        return tpm_startup_usage();
    }
  }

  return 0;
}
