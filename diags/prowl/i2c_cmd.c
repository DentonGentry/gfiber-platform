/*
 * (C) Copyright 2014 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../common/util.h"
#include "i2c.h"

#define I2C_READ_BUF_SIZE 1024
#define DISPLAY_WIDTH 8
#define LED_BUS 0
#define LED_ADDR 0x62
#define LED_SELECT_REG 0x5

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
    /*
    if (return_code != 0) {
      return_code = i2cr(controller, device_addr, 0, 0, 1, buf);
    }
    */
    if (return_code == 0) {
      printf("Address 0x%02X responding\n", device_addr);
    }
  }

  return 0;
}

static void board_temp_usage(void) {
  printf("board_temp\n");
  printf("Example:\n");
  printf("board_temp\n");
}

int board_temp(int argc, char *argv[]) {
  if (argc != 1 || argv == NULL) {
    board_temp_usage();
    return -1;
  }

  system_cmd("cat /sys/bus/i2c/drivers/ds1775/0-0048/temp_val");

  return 0;
}

static void led_set_usage(void) {
  printf("led_set <red | blue> <on | off>\n");
  printf("Example:\n");
  printf("led_set blue on\n");
}

int led_set(int argc, char *argv[]) {
  int led = 0; // 0 for blue and 1 for read
  bool is_off = true;
  static const int kLedMask[2] = {0x3, 0xc}, kLedOffMask[2] = {0x1, 0x4};
  uint8_t setting;

  if (argc != 3) {
    led_set_usage();
    return -1;
  }

  if (strcmp(argv[1], "blue") == 0) {
    led = 0;
  } else if (strcmp(argv[1], "red") == 0) {
    led = 1;
  } else {
    printf("Unknown LED %s\n", argv[1]);
    led_set_usage();
    return -1;
  }

  if (strcmp(argv[2], "on") == 0) {
    is_off = false;
  } else if (strcmp(argv[2], "off") == 0) {
    is_off = true;
  } else {
    printf("Unknown LED setting %s\n", argv[2]);
    led_set_usage();
    return -1;
  }

  if (i2cr(LED_BUS, LED_ADDR, LED_SELECT_REG, 1, 1, &setting) < 0) {
    printf("Failed to read LED selector register.\n");
    return -1;
  }

  setting &= ~kLedMask[led];
  if (is_off) setting |= kLedOffMask[led];

  if (i2cw(LED_BUS, LED_ADDR, LED_SELECT_REG, 1, 1, &setting) < 0) {
    printf("Failed to write LED selector register of 0x%x.\n", setting);
    return -1;
  }

  printf("LED %s is set to %s\n", (led == 0) ? "blue" : "red", (is_off) ? "off" : "on");

  return 0;
}

static void led_set_pwm_usage(void) {
  printf("led_set_pwm <red | blue> <0-255>\n");
  printf("Example:\n");
  printf("led_set_pwm blue 10\n");
}

int led_set_pwm(int argc, char *argv[]) {
  int led = 0; // 0 for blue and 1 for read
  bool is_off = true;
  static const int kLedPwmMask[2] = {0x3, 0xc}, kLedPwmVal[2] = {0x2, 0xc};
  static const int kLedPwmReg[2] = {2, 4};
  uint8_t setting, pwm;
  unsigned int tmp;

  if (argc != 3) {
    led_set_pwm_usage();
    return -1;
  }

  if (strcmp(argv[1], "blue") == 0) {
    led = 0;
  } else if (strcmp(argv[1], "red") == 0) {
    led = 1;
  } else {
    printf("Unknown LED %s\n", argv[1]);
    led_set_pwm_usage();
    return -1;
  }

  tmp = get_num(argv[2]);
  if (tmp > 255) {
    printf("Invalid pwm value: %d\n", tmp);
    led_set_pwm_usage();
    return -1;
  }
  pwm = tmp;

  if (i2cr(LED_BUS, LED_ADDR, LED_SELECT_REG, 1, 1, &setting) < 0) {
    printf("Failed to read LED selector register.\n");
    return -1;
  }

  setting &= ~kLedPwmMask[led];
  if (is_off) setting |= kLedPwmVal[led];

  if (i2cw(LED_BUS, LED_ADDR, LED_SELECT_REG, 1, 1, &setting) < 0) {
    printf("Failed to write LED selector register of 0x%x.\n", setting);
    return -1;
  }

  if (i2cw(LED_BUS, LED_ADDR, kLedPwmReg[led], 1, 1, &pwm) < 0) {
    printf("Failed to write LED PWM register %d of 0x%x.\n", kLedPwmReg[led], pwm);
    return -1;
  }

  printf("LED %s PWM is set to %d\n", (led == 0) ? "blue" : "red", pwm);

  return 0;
}
