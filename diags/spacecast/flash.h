/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#ifndef VENDOR_GOOGLE_DIAGS_SPACECAST_FLASH_H_
#define VENDOR_GOOGLE_DIAGS_SPACECAST_FLASH_H_

#define FLASH_LAST_FILE_NAME "norreserved0"
#define FLASH_TEST_FILE_NAME "/tmp/flash_test_pattern"
#define FLASH_RESULT_FILE_NAME "/tmp/flash_written"
#define GET_SPARE_FLASH_CMD "cat /proc/mtd | grep norreserved0"

#endif  // VENDOR_GOOGLE_DIAGS_SPACECAST_FLASH_H_
