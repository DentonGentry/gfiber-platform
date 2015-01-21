/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "i2c.h"

#define I2C_READ_BUF_SIZE 1024
#define DISPLAY_WIDTH 8

int i2cread(int argc, char *argv[]);
int i2cwrite(int argc, char *argv[]);
int i2cprobe(int argc, char *argv[]);

static void i2cread_usage(void) {
  printf(
      "i2cread bus# dev-address register-offset"
      " address-len num-byte-to-read\n");
  printf("Example:\n");
  printf("i2cread 1 0x2c 0x40 1 1\n");
  printf(
      "Read from bus 1  device 0x2c, register 0x40,"
      " address length is 1, read 1 byte\n");
}

int i2cread(int argc, char *argv[]) {
  uint8_t device_addr;
  uint32_t cell_addr;
  uint32_t addr_len;
  uint32_t data_len;
  uint8_t *buf;
  int j, k;
  int return_code;
  int controller;

  if (argc < 6) {
    i2cread_usage();
    return -1;
  }

  controller = strtoul(argv[1], NULL, 0);
  device_addr = (uint8_t)strtoul(argv[2], NULL, 0);
  cell_addr = strtoul(argv[3], NULL, 0);
  addr_len = strtoul(argv[4], NULL, 0);
  data_len = strtoul(argv[5], NULL, 0);

  if (data_len >= I2C_READ_BUF_SIZE) {
    printf("ERROR: Size %s too large\n", argv[5]);
    return -1;
  }

  buf = (uint8_t *)malloc(I2C_READ_BUF_SIZE);
  if (buf == NULL) {
    printf("ERROR: malloc failed (out of memory)\n");
    return -1;
  }

  return_code =
      i2cr(controller, device_addr, cell_addr, addr_len, data_len, buf);
  if (return_code != 0) {
    printf("Read ERROR: return code = %d\n", return_code);
    free(buf);
    return return_code;
  }

  /* display */
  for (j = 0; j < (int)(data_len); j += DISPLAY_WIDTH) {
    printf("\n@0x%04X\t:", cell_addr + j);
    for (k = j; (k < (int)(data_len)) && (k < (j + DISPLAY_WIDTH)); k++) {
      printf("%02X", buf[k]);
    }
    /* fill up space if finish before display width */
    if ((k == (int)(data_len)) && (k < (j + DISPLAY_WIDTH))) {
      for (k = data_len; k < (j + DISPLAY_WIDTH); k++) {
        printf("  ");
      }
    }
    printf("\t");
    for (k = j; (k < (int)(data_len)) && (k < (j + DISPLAY_WIDTH)); k++) {
      if ((buf[k] >= 0x20) && (buf[k] < 0x7f)) {
        printf("%c", buf[k]);
      } else {
        printf("%c", '.');
      }
    }
    printf("\n");
  }

  printf("\n--------------------------------------------\n");

  free(buf);
  return 0;
}

static void i2cwrite_usage(void) {
  printf(
      "i2cwrite bus# dev-address register-offset"
      " address-len data-len data\n");
  printf("Example:\n");
  printf("i2cwrite 1 0x2c 0x40 1 1 0x80\n");
  printf(
      "Write to bus 1  device 0x2c, register 0x40,"
      " address length is 1, 1 byte data, data value is 0x80\n");
}

int i2cwrite(int argc, char *argv[]) {
  uint8_t device_addr;
  uint32_t cell_addr;
  uint32_t addr_len;
  uint32_t data_len;
  uint32_t data;
  uint8_t buf[4];
  int return_code;
  int controller;
  int i;

  if (argc < 6) {
    i2cwrite_usage();
    return -1;
  }

  controller = strtoul(argv[1], NULL, 0);
  device_addr = (uint8_t)strtoul(argv[2], NULL, 0);
  cell_addr = strtoul(argv[3], NULL, 0);
  addr_len = strtoul(argv[4], NULL, 0);
  data_len = strtoul(argv[5], NULL, 0);

  if (data_len > 4) {
    printf("ERROR: Size %s too large\n", argv[5]);
    return -1;
  }

  data = strtoul(argv[6], NULL, 0);

  /* store data into buffer */
  for (i = data_len - 1; i >= 0; i--) {
    buf[i] = data & 0xff;
    data >>= 8;
  }

  return_code =
      i2cw(controller, device_addr, cell_addr, addr_len, data_len, buf);
  if (return_code != 0) {
    printf("Write ERROR: return code = %d\n", return_code);
    return return_code;
  }

  return 0;
}

static void i2cprobe_usage(void) {
  printf("i2cprobe bus#\n");
  printf("Example:\n");
  printf("i2cprobe 2\n");
}

int i2cprobe(int argc, char *argv[]) {
  uint8_t device_addr;
  uint8_t buf[1];
  int return_code;
  int controller;

  if (argc < 2) {
    i2cprobe_usage();
    return -1;
  }

  controller = strtoul(argv[1], NULL, 0);

  for (device_addr = 1; device_addr < 127; device_addr++) {
    /* Avoid probing these devices */
    if ((device_addr == 0x69) || (device_addr == 0x0C)) {
      continue;
    }
    return_code = i2cr(controller, device_addr, 0, 1, 1, buf);
    if (return_code != 0) {
      return_code = i2cr(controller, device_addr, 0, 0, 1, buf);
    }
    if (return_code == 0) {
      printf("Address 0x%02X responding\n", device_addr);
    }
  }

  return 0;
}
