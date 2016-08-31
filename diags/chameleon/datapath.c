/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../common/io.h"
#include "../common/util.h"

#define AVANTA_BASE_ADDR 0xF1000000
#define GPON_RECV_STATUS_FEC0 (AVANTA_BASE_ADDR + 0x000AC80C)
#define GPON_RECV_STATUS_FEC1 (AVANTA_BASE_ADDR + 0x000AC810)
#define GPON_RECV_STATUS_FEC2 (AVANTA_BASE_ADDR + 0x000AC814)
#define GPON_RECV_STATUS_SUPER_FRAME_CNT (AVANTA_BASE_ADDR + 0x000AC818)
#define PON_PHY_TEST_PRBS_COUNTER_0 (AVANTA_BASE_ADDR + 0x000A2E70)
#define PON_PHY_TEST_PRBS_COUNTER_1 (AVANTA_BASE_ADDR + 0x000A2E74)
#define PON_PHY_TEST_PRBS_COUNTER_2 (AVANTA_BASE_ADDR + 0x000A2E78)
#define PON_PHY_TEST_PRBS_ERROR_COUNTER_0 (AVANTA_BASE_ADDR + 0x000A2E7C)
#define PON_PHY_TEST_PRBS_ERROR_COUNTER_1 (AVANTA_BASE_ADDR + 0x000A2E80)
#define PON_PHY_CTRL0 (AVANTA_BASE_ADDR + 0x000184F4)
#define PON_PHY_RESET_BIT 0x8

static int set_pon_phy_out_of_reset() {
  unsigned int phy_ctrl0;

  if (read_physical_addr(PON_PHY_CTRL0, &phy_ctrl0) != 0) {
    printf("Read address 0x%x failed\n", PON_PHY_CTRL0);
    return -1;
  }
  if (phy_ctrl0 & PON_PHY_RESET_BIT) {
    phy_ctrl0 &= ~PON_PHY_RESET_BIT;
    if (write_physical_addr(PON_PHY_CTRL0, phy_ctrl0) != 0) {
      printf("Write address 0x%x value 0x%x failed\n", PON_PHY_CTRL0,
             phy_ctrl0);
      return -1;
    }
  }
  return 0;
}

static void soc_reg_read_usage(void) {
  printf("soc_reg_read <addr>\n");
  printf("read Marvell 88F6601 registers\n");
  printf("Example:\n");
  printf("soc_reg_read 0x00018810\n");
}

int soc_reg_read(int argc, char *argv[]) {
  unsigned int reg_addr, addr, value;

  if (argc != 2) {
    soc_reg_read_usage();
    return -1;
  }

  reg_addr = strtoul(argv[1], NULL, 16);
  addr = reg_addr + AVANTA_BASE_ADDR;
  if (read_physical_addr(addr, &value) != 0) {
    printf("Read address 0x%x failed\n", addr);
    return -1;
  }
  printf("0x%x = 0x%x\n", reg_addr, value);

  return 0;
}

static void soc_reg_write_usage(void) {
  printf("soc_reg_write <addr> <data>\n");
  printf("write Marvell 88F6601 registers\n");
  printf("Example:\n");
  printf("soc_reg_write 0x0007241C 0x0\n");
}

int soc_reg_write(int argc, char *argv[]) {
  unsigned int reg_addr, addr, value;

  if (argc != 3) {
    soc_reg_write_usage();
    return -1;
  }

  reg_addr = strtoul(argv[1], NULL, 16);
  value = strtoul(argv[2], NULL, 16);
  addr = reg_addr + AVANTA_BASE_ADDR;
  if (write_physical_addr(addr, value) != 0) {
    printf("Write address 0x%x value 0x%x failed\n", addr, value);
    return -1;
  }
  printf("0x%x set to 0x%x\n", reg_addr, value);

  return 0;
}

static void gpon_rx_status_usage(void) {
  printf("gpon_rx_status\n");
  printf("read Marvell 88F6601 GPON RX status registers\n");
  printf("Example:\n");
  printf("gpon_rx_status\n");
}

int gpon_rx_status(int argc, char *argv[]) {
  unsigned int fec0, fec1, fec2, frame_cnt;

  if (argc != 1 || argv == NULL) {
    gpon_rx_status_usage();
    return -1;
  }

  if (read_physical_addr(GPON_RECV_STATUS_FEC0, &fec0) != 0) {
    printf("Read address 0x%x failed\n", GPON_RECV_STATUS_FEC0);
    return -1;
  }
  if (read_physical_addr(GPON_RECV_STATUS_FEC1, &fec1) != 0) {
    printf("Read address 0x%x failed\n", GPON_RECV_STATUS_FEC1);
    return -1;
  }
  if (read_physical_addr(GPON_RECV_STATUS_FEC2, &fec2) != 0) {
    printf("Read address 0x%x failed\n", GPON_RECV_STATUS_FEC2);
    return -1;
  }
  if (read_physical_addr(GPON_RECV_STATUS_SUPER_FRAME_CNT, &frame_cnt) != 0) {
    printf("Read address 0x%x failed\n", GPON_RECV_STATUS_SUPER_FRAME_CNT);
    return -1;
  }
  printf(
      "Bytes Received: 0x%x COR: 0x%x RX words Received: 0x%x Frame CNT: "
      "0x%x\n",
      fec0, fec1, fec2, frame_cnt);

  return 0;
}

static void rx_prbs_cnt_usage(void) {
  printf("rx_prbs_cnt\n");
  printf("read Marvell 88F6601 RX PRBS coutner registers\n");
  printf("Example:\n");
  printf("rx_prbs_cnt\n");
}

int rx_prbs_cnt(int argc, char *argv[]) {
  unsigned int cnt0, cnt1, cnt2;

  if (argc != 1 || argv == NULL) {
    rx_prbs_cnt_usage();
    return -1;
  }

  if (set_pon_phy_out_of_reset() != 0) {
    printf("Failed to take PHY out of reset\n");
    return -1;
  }
  if (read_physical_addr(PON_PHY_TEST_PRBS_COUNTER_0, &cnt0) != 0) {
    printf("Read address 0x%x failed\n", PON_PHY_TEST_PRBS_COUNTER_0);
    return -1;
  }
  if (read_physical_addr(PON_PHY_TEST_PRBS_COUNTER_1, &cnt1) != 0) {
    printf("Read address 0x%x failed\n", PON_PHY_TEST_PRBS_COUNTER_0);
    return -1;
  }
  if (read_physical_addr(PON_PHY_TEST_PRBS_COUNTER_2, &cnt2) != 0) {
    printf("Read address 0x%x failed\n", PON_PHY_TEST_PRBS_COUNTER_0);
    return -1;
  }
  printf("RX PRBS count: 0x%x%04x%04x\n", cnt0, cnt1, cnt2);

  return 0;
}

static void rx_prbs_err_cnt_usage(void) {
  printf("rx_prbs_err_cnt\n");
  printf("read Marvell 88F6601 RX PRBS error coutner registers\n");
  printf("Example:\n");
  printf("rx_prbs_err_cnt\n");
}

int rx_prbs_err_cnt(int argc, char *argv[]) {
  unsigned int cnt0, cnt1;

  if (argc != 1 || argv == NULL) {
    rx_prbs_err_cnt_usage();
    return -1;
  }

  if (set_pon_phy_out_of_reset() != 0) {
    printf("Failed to take PHY out of reset\n");
    return -1;
  }
  if (read_physical_addr(PON_PHY_TEST_PRBS_ERROR_COUNTER_0, &cnt0) != 0) {
    printf("Read address 0x%x failed\n", PON_PHY_TEST_PRBS_ERROR_COUNTER_0);
    return -1;
  }
  if (read_physical_addr(PON_PHY_TEST_PRBS_ERROR_COUNTER_0, &cnt1) != 0) {
    printf("Read address 0x%x failed\n", PON_PHY_TEST_PRBS_ERROR_COUNTER_0);
    return -1;
  }
  printf("RX PRBS error count: 0x%x%04x\n", cnt0, cnt1);

  return 0;
}

static void gpon_cnts_usage(void) {
  printf("gpon_cnts\n");
  printf("dump all of Marvell 88F6601 GPON related error coutner registers\n");
  printf("Example:\n");
  printf("gpon_cnts\n");
}

int gpon_cnts(int argc, char *argv[]) {
  if (argc != 1 || argv == NULL) {
    gpon_cnts_usage();
    return -1;
  }

  system_cmd("cat /sys/devices/platform/gpon/pm/bwMapCnt");
  system_cmd("cat /sys/devices/platform/gpon/pm/fecCnt");
  system_cmd("cat /sys/devices/platform/gpon/pm/gemCnt");
  system_cmd("cat /sys/devices/platform/gpon/pm/rxPloamCnt");
  system_cmd("cat /sys/devices/platform/gpon/pm/stdCnt");
  system_cmd("cat /sys/devices/platform/gpon/pm/txPktCnt");
  system_cmd("cat /sys/devices/platform/gpon/pm/txPloamCnt");

  return 0;
}

static void gpon_alarms_usage(void) {
  printf("gpon_alarms\n");
  printf("Show 88F6601 GPON alarms\n");
  printf("Example:\n");
  printf("gpon_alarms\n");
}

int gpon_alarms(int argc, char *argv[]) {
  if (argc != 1 || argv == NULL) {
    gpon_alarms_usage();
    return -1;
  }

  system_cmd("cat /sys/devices/platform/gpon/info/alarmGpon");

  return 0;
}
