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
#include "i2c.h"

#define SFP_BUS 0
#define SFP_A0_ADDR 0x50
#define SFP_A2_ADDR 0x51
#define SFP_ADDR_LEN 1
#define SFP_REG_LEN 1
#define SFP_MAX_DATA_LEN 128
#define SFP_INFO_REG_ADDR 0x60
#define SFP_INFO_REG_LEN 16
#define SFP_VENDOR_REG_ADDR 20
#define SFP_VENDOR_REG_LEN 16
#define SFP_PN_REG_ADDR 40
#define SFP_PN_REG_LEN 16
#define SFP_SN_REG_ADDR 68
#define SFP_SN_REG_LEN 16
#define SFP_DATE_REG_ADDR 84
#define SFP_DATE_REG_LEN 8
#define SFP_WL_REG_ADDR 144
#define SFP_WL_REG_LEN 2
#define SFP_PW_REG 0x7B
#define SFP_PW_REG_LEN 4

static void sfp_reg_read_usage(void) {
  printf("sfp_reg_read <addr>\n");
  printf("read SFP registers\n");
  printf("Example:\n");
  printf("sfp_reg_read 0x40\n");
}

int sfp_reg_read(int argc, char *argv[]) {
  unsigned int reg_addr;
  uint8_t value;

  if (argc != 2) {
    sfp_reg_read_usage();
    return -1;
  }

  reg_addr = get_num(argv[1]);
  if (i2cr(SFP_BUS, SFP_A0_ADDR, reg_addr, SFP_ADDR_LEN, SFP_REG_LEN, &value) !=
      0) {
    printf("SFP read address 0x%x failed\n", reg_addr);
    return -1;
  }
  printf("SFP 0x%x = 0x%x\n", reg_addr, value);

  return 0;
}

static void sfp_reg_write_usage(void) {
  printf("sfp_reg_write <addr> <data>\n");
  printf("write SFP registers\n");
  printf("Example:\n");
  printf("sfp_reg_write 0x60 0x0\n");
}

int sfp_reg_write(int argc, char *argv[]) {
  unsigned int reg_addr;
  uint8_t value;

  if (argc != 3) {
    sfp_reg_write_usage();
    return -1;
  }

  reg_addr = get_num(argv[1]);
  value = get_num(argv[2]);
  if (i2cw(SFP_BUS, SFP_A0_ADDR, reg_addr, SFP_ADDR_LEN, SFP_REG_LEN, &value) !=
      0) {
    printf("SFP write address 0x%x value 0x%x failed\n", reg_addr, value);
    return -1;
  }
  printf("SFP 0x%x set to 0x%x\n", reg_addr, value);

  return 0;
}

static void sfp_diags_reg_read_usage(void) {
  printf("sfp_diags_reg_read <addr>\n");
  printf("read SFP 0xA2 registers\n");
  printf("Example:\n");
  printf("sfp_diags_reg_read 0x40\n");
}

int sfp_diags_reg_read(int argc, char *argv[]) {
  unsigned int reg_addr;
  uint8_t value;

  if (argc != 2) {
    sfp_diags_reg_read_usage();
    return -1;
  }

  reg_addr = get_num(argv[1]);
  if (i2cr(SFP_BUS, SFP_A2_ADDR, reg_addr, SFP_ADDR_LEN, SFP_REG_LEN, &value) !=
      0) {
    printf("SFP 0xA2 read address 0x%x failed\n", reg_addr);
    return -1;
  }
  printf("SFP 0xA2 0x%x = 0x%x\n", reg_addr, value);

  return 0;
}

static void sfp_diags_reg_write_usage(void) {
  printf("sfp_diags_reg_write <addr> <data>\n");
  printf("write SFP 0xA2 registers\n");
  printf("Example:\n");
  printf("sfp_diags_reg_write 0x60 0x0\n");
}

int sfp_diags_reg_write(int argc, char *argv[]) {
  unsigned int reg_addr;
  uint8_t value;

  if (argc != 3) {
    sfp_diags_reg_write_usage();
    return -1;
  }

  reg_addr = get_num(argv[1]);
  value = get_num(argv[2]);
  if (i2cw(SFP_BUS, SFP_A2_ADDR, reg_addr, SFP_ADDR_LEN, SFP_REG_LEN, &value) !=
      0) {
    printf("SFP write 0xA2 address 0x%x value 0x%x failed\n", reg_addr, value);
    return -1;
  }
  printf("SFP 0xA2 0x%x set to 0x%x\n", reg_addr, value);

  return 0;
}

static void sfp_info_usage(void) {
  printf("sfp_info\n");
  printf("read SFP info\n");
  printf("Example:\n");
  printf("sfp_info\n");
}

int sfp_info(int argc, char *argv[]) {
  uint8_t value[SFP_INFO_REG_LEN];
  float temp;
  float vcc, tx_bias, tx_power, rx_power, mod_curr;

  if (argc != 1 || argv == NULL) {
    sfp_info_usage();
    return -1;
  }

  if (i2cr(SFP_BUS, SFP_A2_ADDR, SFP_INFO_REG_ADDR, SFP_ADDR_LEN,
           SFP_INFO_REG_LEN, value) != 0) {
    printf("SFP read address %d failed\n", SFP_INFO_REG_ADDR);
    return -1;
  }
  if (value[0] & 0x80) {
    value[0] &= 0x7F;
    temp = -128.0 + value[0] + ((float)value[1]) / 256.0;
  } else {
    temp = value[0] + ((float)value[1]) / 256.0;
  }
  vcc = ((float)((value[2] << 8) + value[3])) / 10000.0;
  tx_bias = ((float)((value[4] << 8) + value[5])) / 1000.0;
  tx_power = ((float)((value[6] << 8) + value[7])) / 10000.0;
  rx_power = ((float)((value[8] << 8) + value[9])) / 10000.0;
  mod_curr = ((float)((value[12] << 8) + value[13])) / 1000.0;
  printf("SFP temp: %f, Vcc: %3.3f V, TX bias %3.3f mA\n", temp, vcc, tx_bias);
  printf("    TX power: %3.3f mW, RX power: %3.3f mW, mod curr: %3.3f mA\n",
         tx_power, rx_power, mod_curr);

  return 0;
}

static void sfp_vendor_usage(void) {
  printf("sfp_vendor\n");
  printf("read SFP vendor\n");
  printf("Example:\n");
  printf("sfp_vendor\n");
}

int sfp_vendor(int argc, char *argv[]) {
  uint8_t value[SFP_MAX_DATA_LEN];

  if (argc != 1 || argv == NULL) {
    sfp_vendor_usage();
    return -1;
  }

  printf("SFP vendor:\n");
  if (i2cr(SFP_BUS, SFP_A0_ADDR, SFP_VENDOR_REG_ADDR, SFP_ADDR_LEN,
           SFP_VENDOR_REG_LEN, value) != 0) {
    printf("SFP read address %d failed\n", SFP_VENDOR_REG_ADDR);
    return -1;
  }
  value[SFP_VENDOR_REG_LEN] = '\0';
  printf("  Name: %s\n", value);
  if (i2cr(SFP_BUS, SFP_A0_ADDR, SFP_PN_REG_ADDR, SFP_ADDR_LEN, SFP_PN_REG_LEN,
           value) != 0) {
    printf("SFP read address %d failed\n", SFP_PN_REG_ADDR);
    return -1;
  }
  value[SFP_PN_REG_LEN] = '\0';
  printf("  PN:   %s\n", value);
  if (i2cr(SFP_BUS, SFP_A0_ADDR, SFP_SN_REG_ADDR, SFP_ADDR_LEN, SFP_SN_REG_LEN,
           value) != 0) {
    printf("SFP read address %d failed\n", SFP_SN_REG_ADDR);
    return -1;
  }
  value[SFP_SN_REG_LEN] = '\0';
  printf("  SN:   %s\n", value);
  if (i2cr(SFP_BUS, SFP_A0_ADDR, SFP_DATE_REG_ADDR, SFP_ADDR_LEN,
           SFP_DATE_REG_LEN, value) != 0) {
    printf("SFP read address %d failed\n", SFP_DATE_REG_ADDR);
    return -1;
  }
  value[SFP_DATE_REG_LEN] = '\0';
  printf("  Date: %s\n", value);

  return 0;
}

static void sfp_pn_usage(void) {
  printf("sfp_pn\n");
  printf("read SFP part number\n");
  printf("Example:\n");
  printf("sfp_pn\n");
}

int sfp_pn(int argc, char *argv[]) {
  uint8_t value[SFP_MAX_DATA_LEN];

  if (argc != 1 || argv == NULL) {
    sfp_pn_usage();
    return -1;
  }

  if (i2cr(SFP_BUS, SFP_A0_ADDR, SFP_PN_REG_ADDR, SFP_ADDR_LEN, SFP_PN_REG_LEN,
           value) != 0) {
    printf("SFP read address %d failed\n", SFP_PN_REG_ADDR);
    return -1;
  }
  value[SFP_PN_REG_LEN] = '\0';
  printf("SFP part number: %s\n", value);

  return 0;
}

static void sfp_wavelength_usage(void) {
  printf("sfp_wavelength\n");
  printf("read SFP laser wavelength\n");
  printf("Example:\n");
  printf("sfp_wavelength\n");
}

int sfp_wavelength(int argc, char *argv[]) {
  uint8_t value[SFP_MAX_DATA_LEN];
  unsigned int data;

  if (argc != 1 || argv == NULL) {
    sfp_wavelength_usage();
    return -1;
  }

  if (i2cr(SFP_BUS, SFP_A2_ADDR, SFP_WL_REG_ADDR, SFP_ADDR_LEN, SFP_WL_REG_LEN,
           value) != 0) {
    printf("SFP read address %d failed\n", SFP_WL_REG_ADDR);
    return -1;
  }
  data = (value[0] << 8) + value[1];
  printf("SFP wavelength: %d\n", data);

  return 0;
}

static void sfp_set_wavelength_usage(void) {
  printf("sfp_set_wavelength <wavelength>\n");
  printf("set SFP laser wavelength\n");
  printf("Example:\n");
  printf("sfp_set_wavelength 1520\n");
}

int sfp_set_wavelength(int argc, char *argv[]) {
  uint8_t value[SFP_MAX_DATA_LEN];
  unsigned int data;

  if (argc != 2) {
    sfp_set_wavelength_usage();
    return -1;
  }

  data = get_num(argv[1]);
  value[0] = (data >> 8);
  value[1] = data & 0xFF;
  if (i2cw(SFP_BUS, SFP_A2_ADDR, SFP_WL_REG_ADDR, SFP_ADDR_LEN, SFP_WL_REG_LEN,
           value) != 0) {
    printf("SFP write address %d of %d failed\n", SFP_WL_REG_ADDR, data);
    return -1;
  }
  data = (value[0] << 8) + value[1];
  printf("SFP wavelength: %d\n", data);

  return 0;
}

static void sfp_set_pw_usage(void) {
  printf("sfp_set_pw <password>\n");
  printf("set SFP access password\n");
  printf("Example:\n");
  printf("sfp_set_pw 0x80818283\n");
}

int sfp_set_pw(int argc, char *argv[]) {
  uint8_t value[SFP_MAX_DATA_LEN];
  unsigned int data;

  if (argc != 2) {
    sfp_set_pw_usage();
    return -1;
  }

  data = get_num(argv[1]);
  value[0] = (data >> 24) & 0xFF;
  value[1] = (data >> 16) & 0xFF;
  value[2] = (data >> 8) & 0xFF;
  value[3] = data & 0xFF;
  if (i2cw(SFP_BUS, SFP_A2_ADDR, SFP_PW_REG, SFP_ADDR_LEN, SFP_PW_REG_LEN,
           value) != 0) {
    printf("SFP write address %d of %d failed\n", SFP_PW_REG, data);
    return -1;
  }
  printf("SFP password set to 0x%2x%2x%2x%2x\n", value[0], value[1], value[2],
         value[3]);

  return 0;
}
