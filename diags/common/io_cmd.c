/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "io.h"

static void ioread_usage(void) {
  printf("ioread <address> [MSB] [LSB]\n");
  printf("Example:\n");
  printf("ioread 0xf0000000 31 0\n");
  printf("Read address 0xf0000000, bit 31 to bit 0 \n");
}

int ioread(int argc, char *argv[]) {
  uint32_t address;
  uint32_t value;
  uint32_t msb = 31;
  uint32_t lsb = 0;

  if (argc < 2) {
    ioread_usage();
    return -1;
  }

  address = strtoul(argv[1], NULL, 0);

  if (argc >= 3) {
    msb = strtoul(argv[2], NULL, 0);
  }

  if (argc >= 4) {
    lsb = strtoul(argv[3], NULL, 0);
  }

  if (io_r_field(address, &value, msb, lsb) != 0) {
    return -1;
  }

  printf("0x%x\n", value);

  return 0;
}

static void iowrite_usage(void) {
  printf("iowrite <address> <value> [MSB] [LSB]\n");
  printf("Example:\n");
  printf("iowrite 0xf0000000 0xa5a5a5a5 31 0\n");
  printf("Write address 0xf0000000, value 0xa5a5a5a5, bit 31 to bit 0 \n");
}

int iowrite(int argc, char *argv[]) {
  uint32_t address;
  uint32_t value;
  uint32_t msb = 31;
  uint32_t lsb = 0;

  if (argc < 3) {
    iowrite_usage();
    return -1;
  }

  address = strtoul(argv[1], NULL, 0);
  value = strtoul(argv[2], NULL, 0);

  if (argc >= 4) {
    msb = strtoul(argv[3], NULL, 0);
  }

  if (argc >= 5) {
    lsb = strtoul(argv[4], NULL, 0);
  }

  return io_w_field(address, value, msb, lsb);
}

static void iowrite_only_usage(void) {
  printf("iowrite_only <address> <value>\n");
  printf("Example:\n");
  printf("iowrite_only 0xf0000000 0xa5a5a5a5\n");
  printf("Write only address 0xf0000000, value 0xa5a5a5a5\n");
}

int iowrite_only(int argc, char *argv[]) {
  uint32_t address;
  uint32_t value;

  if (argc != 3) {
    iowrite_only_usage();
    return -1;
  }

  address = strtoul(argv[1], NULL, 0);
  value = strtoul(argv[2], NULL, 0);

  return io_w(address, value);
}
