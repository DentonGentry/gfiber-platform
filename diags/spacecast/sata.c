/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include "sata.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "../common/io.h"
#include "../common/util.h"
#include "common.h"

#define SATA_TX_TEST_MFTP 0
#define SATA_TX_TEST_LBP 1
#define SATA_TX_TEST_LFTP 2
#define SATA_TX_TEST_HFTP 3
#define SATA_TX_TEST_SSOP 4
#define SATA_QUERY_INFO "smartctl -a /dev/sda"
#define SATA_QUERY_HEADER "Smartctl"
#define SATA_FAILED "failed"
#define SATA_CRC "CRC"
#define SATA_CRC_COLUMN_NUM 10
#define SATA_CRC_PASS_VAL "0"

char *sataTxTestStr[] = {"Mid Frequency Test Pattern", "Lone Bit Pattern",
                         "Low Frequency Test Pattern",
                         "High Frequency Test Pattern",
                         "Simultaneous Switch Output Pattern"};

int displaySataConfigTerse(uint64_t base) {
  int rc;
  uint32_t data, len, i, tmp;
  static const uint32_t kStep = 0x20, kWordSize = 4;
  uint64_t addr = base;

  printf("SATA config:\n");
  for (len = 0; len <= (SATA_CONFIG_LAST_REG_OFFSET - kStep + kWordSize);
       len += kStep) {
    addr = base + len;
    tmp = addr & UINT_MASK;
    printf("0x%08x:", tmp);
    for (i = 0; i < kStep; i += kWordSize) {
      if ((rc = read_physical_addr(addr, &data)) < 0) {
        tmp = (uint32_t)(addr & UINT_MASK);
        printf("Read SATA config addr 0x%x failed\n", tmp);
        return -1;
      }
      printf(" 0x%08x", data);
      addr += kWordSize;
    }
    printf("\n");
  }

  printf("SATA Port 0 config:\n");
  for (len = SATA_PORT0_FIRST_REG_OFFSET;
       len <= (SATA_PORT0_LAST_REG_OFFSET - kStep + kWordSize); len += kStep) {
    addr = base + len;
    tmp = addr & UINT_MASK;
    printf("0x%08x:", tmp);
    for (i = 0; i < kStep; i += kWordSize) {
      if ((rc = read_physical_addr(addr, &data)) < 0) {
        tmp = (uint32_t)(addr & UINT_MASK);
        printf("Read SATA port 0 config addr 0x%x failed\n", tmp);
        return -1;
      }
      printf(" 0x%08x", data);
      addr += kWordSize;
    }
    printf("\n");
  }

  printf("SATA Port 1 config:\n");
  for (len = SATA_PORT1_FIRST_REG_OFFSET;
       len <= (SATA_PORT1_LAST_REG_OFFSET - kStep + kWordSize); len += kStep) {
    addr = base + len;
    tmp = addr & UINT_MASK;
    printf("0x%08x:", tmp);
    for (i = 0; i < kStep; i += kWordSize) {
      if ((rc = read_physical_addr(addr, &data)) < 0) {
        tmp = (uint32_t)(addr & UINT_MASK);
        printf("Read SATA port 1 config addr 0x%x failed\n", tmp);
        return -1;
      }
      printf(" 0x%08x", data);
      addr += kWordSize;
    }
    printf("\n");
  }
  return 0;
}

int displaySataConfigVerbose(uint64_t base) {
  int rc;
  uint32_t data, len, i, tmp;
  static const uint32_t kStep = 0x20, kWordSize = 4;
  uint64_t addr = base;

  printf("SATA config:\n");
  read_physical_addr(base + SATA_CAP_REG_OFFSET, &data);
  printf("  HBA Capabilities: 0x%08x\n", data);
  read_physical_addr(base + SATA_GHC_REG_OFFSET, &data);
  printf("  Global HBA Control: 0x%08x\n", data);
  read_physical_addr(base + SATA_IS_REG_OFFSET, &data);
  printf("  Interrupt Status: 0x%08x\n", data);
  read_physical_addr(base + SATA_PI_REG_OFFSET, &data);
  printf("  Ports Implemented: 0x%08x\n", data);
  read_physical_addr(base + SATA_VS_REG_OFFSET, &data);
  printf("  AHCI Version Register: 0x%08x\n", data);
  read_physical_addr(base + SATA_CCC_CTL_REG_OFFSET, &data);
  printf("  Command Completion Coalescing Control: 0x%08x\n", data);
  read_physical_addr(base + SATA_CCC_PORTS_REG_OFFSET, &data);
  printf("  Command Completion Coalescing Ports: 0x%08x\n", data);
  read_physical_addr(base + SATA_CAP2_REG_OFFSET, &data);
  printf("  HBA Capabilities Extended: 0x%08x\n", data);
  read_physical_addr(base + SATA_BISTAFR_REG_OFFSET, &data);
  printf("  BIST Activate FIS: 0x%08x\n", data);
  read_physical_addr(base + SATA_BISTCR_REG_OFFSET, &data);
  printf("  BIST Control: 0x%08x\n", data);
  read_physical_addr(base + SATA_BISTFCTR_REG_OFFSET, &data);
  printf("  BIST FIS Count: 0x%08x\n", data);
  read_physical_addr(base + SATA_BISTSR_REG_OFFSET, &data);
  printf("  BIST Status: 0x%08x\n", data);
  read_physical_addr(base + SATA_BISTDECR_REG_OFFSET, &data);
  printf("  BIST DWORD Error Count: 0x%08x\n", data);
  read_physical_addr(base + SATA_OOBR_REG_OFFSET, &data);
  printf("  OOB: 0x%08x\n", data);
  read_physical_addr(base + SATA_GPCR_REG_OFFSET, &data);
  printf("  General Purpose Control: 0x%08x\n", data);
  read_physical_addr(base + SATA_GPSR_REG_OFFSET, &data);
  printf("  General Purpose Status: 0x%08x\n", data);
  read_physical_addr(base + SATA_TIMER1MS_REG_OFFSET, &data);
  printf("  TImer 1-ms: 0x%08x\n", data);
  read_physical_addr(base + SATA_GPARAM1R_REG_OFFSET, &data);
  printf("  Global Parameter 1: 0x%08x\n", data);
  read_physical_addr(base + SATA_GPARAM2R_REG_OFFSET, &data);
  printf("  Global Parameter 2: 0x%08x\n", data);
  read_physical_addr(base + SATA_PPARAMR_REG_OFFSET, &data);
  printf("  Port Parameter: 0x%08x\n", data);
  read_physical_addr(base + SATA_TESTR_REG_OFFSET, &data);
  printf("  Test: 0x%08x\n", data);
  read_physical_addr(base + SATA_VERSIONR_REG_OFFSET, &data);
  printf("  Version: 0x%08x\n", data);
  read_physical_addr(base + SATA_IDR_REG_OFFSET, &data);
  printf("  ID: 0x%08x\n", data);

  printf("SATA Port 0 config:\n");
  for (len = SATA_PORT0_FIRST_REG_OFFSET;
       len <= (SATA_PORT0_LAST_REG_OFFSET - kStep + kWordSize); len += kStep) {
    addr = base + len;
    tmp = addr & UINT_MASK;
    printf("0x%08x:", tmp);
    for (i = 0; i < kStep; i += kWordSize) {
      if ((rc = read_physical_addr(addr, &data)) < 0) {
        tmp = addr & UINT_MASK;
        printf("Read SATA port 0 config addr 0x%x failed\n", tmp);
        return -1;
      }
      printf(" 0x%08x", data);
      addr += kWordSize;
    }
    printf("\n");
  }

  printf("SATA Port 1 config:\n");
  for (len = SATA_PORT1_FIRST_REG_OFFSET;
       len <= (SATA_PORT1_LAST_REG_OFFSET - kStep + kWordSize); len += kStep) {
    addr = base + len;
    tmp = addr & UINT_MASK;
    printf("0x%08x:", tmp);
    for (i = 0; i < kStep; i += kWordSize) {
      if ((rc = read_physical_addr(addr, &data)) < 0) {
        tmp = addr & UINT_MASK;
        printf("Read SATA port 1 config addr 0x%x failed\n", tmp);
        return -1;
      }
      printf(" 0x%08x", data);
      addr += kWordSize;
    }
    printf("\n");
  }
  return 0;
}

static void satacfgdump_usage() {
  printf("satacfgdump\n");
  printf("Example:\n");
  printf("satacfgdump\n");
  printf("Dump SATA config\n");
}

int satacfgdump(int argc, char *argv[]) {
  int rc = 0;
  bool verbose = false;

  if (argc > 2) {
    satacfgdump_usage();
    return -1;
  } else if (argc == 2) {
    if (strcmp(argv[1], "-v") != 0) {
      satacfgdump_usage();
      return -1;
    }
    verbose = true;
  }

  if (verbose) {
    if ((rc = displaySataConfigVerbose(SATA_CONFIG_BASE_ADDR)) < 0)
      printf("Display full SATA config error %d\n", rc);
  } else if ((rc = displaySataConfigTerse(SATA_CONFIG_BASE_ADDR)) < 0) {
    printf("Display SATA config error %d\n", rc);
    return rc;
  }

  return rc;
}

int sataSetTX(int option) {
  int rc;
  rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_TESTR_REG_OFFSET,
                           0x00010000);
  rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_TEST_TX_SET1_REG_OFFSET,
                           0x00000012);
  rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_TEST_TX_SET2_REG_OFFSET,
                           0x00000001);
  rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_TEST_TX_SET1_REG_OFFSET,
                           0x00000013);
  switch (option) {
    case SATA_TX_TEST_MFTP:
      rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_BISTCR_REG_OFFSET,
                               0x00040706);
      break;
    case SATA_TX_TEST_LBP:
      break;
      rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_BISTCR_REG_OFFSET,
                               0x00040705);
    case SATA_TX_TEST_LFTP:
      rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_BISTCR_REG_OFFSET,
                               0x00040708);
      break;
    case SATA_TX_TEST_HFTP:
      rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_BISTCR_REG_OFFSET,
                               0x00040707);
      break;
    case SATA_TX_TEST_SSOP:
      rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_BISTCR_REG_OFFSET,
                               0x00040700);
      break;
    default:
      rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_BISTCR_REG_OFFSET,
                               0x00040706);
      printf("Invalid SATA TX test option %d\n", option);
      rc = -1;
      break;
  }

  return rc;
}

static void satatxtest_usage() {
  printf("satatxtest <mftp|lbp|lftp|hftp|ssop>\n");
  printf("Example:\n");
  printf("satatxtest MFTP\n");
  printf("generate SATA TX test traffic MFTP\n");
  printf("mftp = Mid Frequency Test Pattern\n");
  printf("lbp = Lone Bit Pattern\n");
  printf("lftp = Low Frequency Test Pattern\n");
  printf("hftp = High Frequency Test Pattern\n");
  printf("ssop = Simultaneous Switch Output Pattern\n");
}

int satatxtest(int argc, char *argv[]) {
  int rc = 0;
  int option = -1;

  if (argc != 2) {
    satatxtest_usage();
    return -1;
  }

  if (strcmp(argv[1], "mftp") == 0) {
    option = SATA_TX_TEST_MFTP;
  } else if (strcmp(argv[1], "lbp") == 0) {
    option = SATA_TX_TEST_LBP;
  } else if (strcmp(argv[1], "lftp") == 0) {
    option = SATA_TX_TEST_LFTP;
  } else if (strcmp(argv[1], "hftp") == 0) {
    option = SATA_TX_TEST_HFTP;
  } else if (strcmp(argv[1], "ssop") == 0) {
    option = SATA_TX_TEST_SSOP;
  } else {
    satatxtest_usage();
    return -1;
  }

  rc = sataSetTX(option);
  printf("SATA TX test set to %s\n", sataTxTestStr[option]);
  return rc;
}

int sataSetRX() {
  int rc;
  rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_TESTR_REG_OFFSET,
                           0x00010000);
  rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_TEST_RX_SET1_REG_OFFSET,
                           0x00000000);
  rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_TEST_RX_SET2_REG_OFFSET,
                           0x00000000);
  rc = write_physical_addr(SATA_CONFIG_BASE_ADDR + SATA_TEST_RX_SET3_REG_OFFSET,
                           0x00000000);

  return rc;
}

static void satarxtest_usage() {
  printf("satarxtest\n");
  printf("Example:\n");
  printf("satarxtest\n");
  printf("setup SATA RX test\n");
}

int satarxtest(int argc, char *argv[]) {
  if ((argc != 1) || (argv[0] == NULL)) {
    satarxtest_usage();
    return -1;
  }
  int rc = sataSetRX();
  printf("SATA RX test setup\n");
  return rc;
}

static void satabench_usage() {
  printf("satabench <time in sec>\n");
  printf("Example:\n");
  printf("satabench 300\n");
  printf("run SATA diskbench test for 300 seconds\n");
}

int satabench(int argc, char *argv[]) {
  unsigned int duration;
  char cmd[128], rsp[MAX_PKT_SIZE];
  FILE *fp;
  int i;

  if (argc != 2) {
    printf("%s invalid params\n", FAIL_TEXT);
    satabench_usage();
    return -1;
  }

  duration = strtoul(argv[1], NULL, 0);

  if (duration == 0) {
    printf("%s Cannot run test with 0 time\n", FAIL_TEXT);
    return -1;
  }

  // Check HD present
  sprintf(cmd, SATA_QUERY_INFO " | grep -i " SATA_FAILED);
  fp = popen(cmd, "r");
  if (fp == NULL) {
    printf("%s Cannot run command smartctl\n", FAIL_TEXT);
    return -1;
  } else {
    while (fscanf(fp, "%s", rsp) != EOF) {
      if (strcmp(rsp, SATA_QUERY_HEADER) == 0) {
        printf("%s No hard disk\n", FAIL_TEXT);
        pclose(fp);
        return -1;
      }
    }
    pclose(fp);
  }

  // sprintf(cmd, "cd /var/media;diskbench -i2 -w8 -r4 -DOF -t%d;cd", duration);
  sprintf(cmd, "cd /var/media;diskbench -i2 -w8 -r4 -b768 -s2048 -t%d;cd",
          duration);
  system_cmd(cmd);

  // Check CRC
  sprintf(cmd, SATA_QUERY_INFO " | grep -i " SATA_CRC);
  fp = popen(cmd, "r");
  if (fp == NULL) {
    printf("%s Cannot run command smartctl for CRC\n", FAIL_TEXT);
    return -1;
  } else {
    i = 0;
    while (i++ < SATA_CRC_COLUMN_NUM) {
      if (fscanf(fp, "%s", rsp) == EOF) break;
    }
    pclose(fp);
    if (i < SATA_CRC_COLUMN_NUM) {
      printf("smartctl query CRC num columns %d too small\n", i);
    } else {
      if (strcmp(rsp, SATA_CRC_PASS_VAL) != 0) {
        printf("%s diskbench detect CRC %s\n", FAIL_TEXT, rsp);
        return -1;
      } else {
        printf("diskbench CRC is %s\n", rsp);
      }
    }
  }

  return 0;
}

static void sata_link_reset_usage() {
  printf("sata_link_reset <num> [period (default 5)]\n");
  printf("WARNING: this command clears dmesg\n");
  printf("Example:\n");
  printf("sata_link_reset 300 5\n");
  printf("reset SATA link 300 time every 5 seconds\n");
}

int sata_link_reset(int argc, char *argv[]) {
  unsigned int duration = 5, failedNum = 0;
  char cmd[128];
  int num, i;
  FILE *fp;
  bool found = false;

  if ((argc != 2) && (argc != 3)) {
    sata_link_reset_usage();
    return -1;
  }

  num = strtol(argv[1], NULL, 0);
  if (argc == 3) duration = strtoul(argv[2], NULL, 0);

  if (duration == 0) {
    printf("Cannot run test %d second\n", duration);
    return -1;
  }

  if ((num == 0) || (num < -1)) {
    printf("Number of times can either be -1 (forever) or > 0. %d invalid\n",
           num);
    return -1;
  }

  system_cmd("dmesg -c > /tmp/t");
  i = 0;
  while (i != num) {
    found = false;
    system_cmd("echo \"0 0 0\" > /sys/class/scsi_host/host1/scan");
    fp = popen("dmesg -c | grep ata", "r");
    while (fscanf(fp, "%s", cmd) != EOF) {
      if (strcmp(cmd, "SATA") == 0) {
        if (fscanf(fp, "%s", cmd) <= 0) return -1;
        if (strcmp(cmd, "link") != 0) {
          continue;
        }
        if (fscanf(fp, "%s", cmd) <= 0) return -1;
        if (strcmp(cmd, "up") != 0) {
          continue;
        } else {
          found = true;
          if (fscanf(fp, "%s", cmd) <= 0) return -1;
          break;
        }
      }
    }
    pclose(fp);

    if (!found) {
      ++failedNum;
    } else if (strcmp(cmd, "3.0") != 0) {
      printf("Error: %d SATA link reset up with %s Gbps.\n", i, cmd);
    } else {
      // printf("SATA link up %s Gbps.\n", cmd);
    }
    sleep(duration);
    ++i;
    if (i == -1) i = 0;
  }

  printf("Run %d times, failed %d times\n", num, failedNum);

  return 0;
}
