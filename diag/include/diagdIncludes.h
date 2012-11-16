/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics monitoring related functions
 *
 */

#ifndef _DIAGD_INCLUDES_H_
#define _DIAGD_INCLUDES_H_

#include <asm/types.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <stddef.h>

#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <pthread.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <math.h>
#include <sys/time.h>
#include <stacktrace.h>

#include "diagdDefs.h"
#include "diagMonApis.h"
#include "diagError.h"
#include "diagParseKernMsgs.h"

#ifdef DIAG_TEST_UTIL
 #include "devctl_moca.h"
 #include "mocalib.h"
#else
 #include "moca/dslcompat/devctl_moca.h"
 #include "moca/mocalib.h"
#endif /* DIAG_TEST_TUILE */

#include "diagMoca.h"
#include "diagSubs.h"
#include "diagNetworkTests.h"
#include "diagApisHostCmd.h"
#include "diagLogging.h"
#include "diagApis.h"
#include "diagParseRefData.h"

#define MOD_NAME  "diagd\0"

#endif // end of _DIAGD_INCLUDES_H_
