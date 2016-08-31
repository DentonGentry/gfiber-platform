/*
 * (C) Copyright 2016 Google, Inc.
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

#define STOP_STR "stop"
#define START_STR "start"
#define LINE_MAX 128
#define GET_TEMP "cat /sys/devices/platform/KW2Thermal.0/temp1_input"
#define RED_LED "red"
#define RED_LED_BRIGHTNESS "/sys/class/leds/sys-red/brightness"
#define BLUE_LED "blue"
#define BLUE_LED_BRIGHTNESS "/sys/class/leds/sys-blue/brightness"
#define MPP_CONTROL_REG 0x00018000
#define GPIO_DATA_OUT_REG 0x00018100
#define GPIO_DATA_OUT_EN_REG 0x00018104
#define GPIO_DATA_IN_REG 0x00018110
#define GPIO_HIGH_DATA_OUT_REG 0x00018124
#define GPIO_HIGH_DATA_OUT_EN_REG 0x00018128
#define GPIO_HIGH_DATA_IN_REG 0x00018134
#define GPIO_HIGH_PIN_START_NUM 32
#define GPIO_MAX_PIN_NUM 37
#define GPIO_DIR_IN_STR "in"
#define GPIO_DIR_OUT_STR "out"
#define GPIO_21_PON_TX_DIS 21
#define AVANTA_BASE_ADDR 0xF1000000

#define BOARD_TEMP_BUS 0
#define BOARD_TEMP_ADDR 0x48
#define BOARD_TEMP_ADDR_LEN 1
#define BOARD_TEMP_REG 0
#define BOARD_TEMP_LEN 2

static void gpio_set_tx_enable_usage(void) {
  printf("gpio_set_tx_enable <0 | 1>\n");
  printf("Example:\n");
  printf("gpio_set_tx_enable 0\n");
  printf("set TX_ENABLE pin to low\n");
}

int gpio_set_tx_enable(int argc, char *argv[]) {
  unsigned int tx_en_val, out_en, out, mask = 1 << GPIO_21_PON_TX_DIS;
  unsigned int out_en_reg = AVANTA_BASE_ADDR + GPIO_DATA_OUT_EN_REG;
  unsigned int out_reg = AVANTA_BASE_ADDR + GPIO_DATA_OUT_REG;
  unsigned int mpp_ctrl =
      AVANTA_BASE_ADDR + MPP_CONTROL_REG + (4 * (GPIO_21_PON_TX_DIS / 8));

  if (argc != 2) {
    gpio_set_tx_enable_usage();
    return -1;
  }
  tx_en_val = get_num(argv[1]);
  if (tx_en_val != 0 && tx_en_val != 1) {
    printf("Invalid TX_ENABLE value %d\n", tx_en_val);
    gpio_set_tx_enable_usage();
    return -1;
  }
  if (read_physical_addr(out_en_reg, &out_en) != 0) {
    printf("Read address 0x%x failed\n", out_en_reg);
    return -1;
  }
  if ((out_en & mask) != 0) {
    out_en &= ~mask;
    if (write_physical_addr(out_en_reg, out_en) != 0) {
      printf("Write address 0x%x of value 0x%x failed\n", out_en_reg, out_en);
      return -1;
    }
  }
  if (read_physical_addr(out_reg, &out) != 0) {
    printf("Read address 0x%x failed\n", out_reg);
    return -1;
  }
  if ((out & mask) != (tx_en_val << GPIO_21_PON_TX_DIS)) {
    out = (out & (~mask)) | (tx_en_val << GPIO_21_PON_TX_DIS);
    if (write_physical_addr(out_reg, out) != 0) {
      printf("Write address 0x%x of value 0x%x failed\n", out_reg, out);
      return -1;
    }
  }
  // set MPP function to gpio
  if (read_physical_addr(mpp_ctrl, &out) != 0) {
    printf("Read address 0x%x failed\n", mpp_ctrl);
    return -1;
  }
  out &= ~(0xFF << (4 * (GPIO_21_PON_TX_DIS % 8)));
  if (write_physical_addr(mpp_ctrl, out) != 0) {
    printf("Write address 0x%x of value 0x%x failed\n", mpp_ctrl, out);
    return -1;
  }
  printf("Set TX_ENABLE to %d\n", tx_en_val);
  return 0;
}

static void gpio_stat_usage(void) {
  printf("gpio_stat <GPIO pin num (0 to %d)>\n", GPIO_MAX_PIN_NUM);
  printf("Example:\n");
  printf("gpio_stat 21\n");
  printf("Display the status of the specified GPIO pin\n");
}

int gpio_stat(int argc, char *argv[]) {
  unsigned int pin_num, pin_mask, in, out, out_en;
  unsigned int out_reg, out_en_reg, in_reg;

  if (argc != 2) {
    gpio_stat_usage();
    return -1;
  }

  pin_num = get_num(argv[1]);

  if (pin_num > GPIO_MAX_PIN_NUM) {
    printf("Invalid GPIO pin number %d\n", pin_num);
    gpio_stat_usage();
    return -1;
  }
  if (pin_num >= GPIO_HIGH_PIN_START_NUM) {
    out_reg = AVANTA_BASE_ADDR + GPIO_HIGH_DATA_OUT_REG;
    out_en_reg = AVANTA_BASE_ADDR + GPIO_HIGH_DATA_OUT_EN_REG;
    in_reg = AVANTA_BASE_ADDR + GPIO_HIGH_DATA_IN_REG;
    pin_mask = 1 << (pin_num - GPIO_HIGH_PIN_START_NUM);
  } else {
    out_reg = AVANTA_BASE_ADDR + GPIO_DATA_OUT_REG;
    out_en_reg = AVANTA_BASE_ADDR + GPIO_DATA_OUT_EN_REG;
    in_reg = AVANTA_BASE_ADDR + GPIO_DATA_IN_REG;
    pin_mask = 1 << pin_num;
  }
  if (read_physical_addr(out_reg, &out) != 0) {
    printf("Read address 0x%x failed\n", out_reg);
    return -1;
  }
  if (read_physical_addr(out_en_reg, &out_en) != 0) {
    printf("Read address 0x%x failed\n", out_en_reg);
    return -1;
  }
  if (read_physical_addr(in_reg, &in) != 0) {
    printf("Read address 0x%x failed\n", in_reg);
    return -1;
  }
  printf("GPIO pin %d: DIR: %s IN: 0x%x OUT: 0x%x\n", pin_num,
         (out_en & pin_mask) ? "in" : "out", (in & pin_mask) ? 1 : 0,
         (out & pin_mask) ? 1 : 0);
  printf("GPIO regs: EN 0x%08x OUT 0x%08x IN 0x%08x MASK 0x%08x\n", out_en, out,
         in, pin_mask);

  return 0;
}

static void gpio_set_dir_usage(void) {
  printf("gpio_set_dir <GPIO pin num (0 to %d)> <%s | %s>\n", GPIO_MAX_PIN_NUM,
         GPIO_DIR_IN_STR, GPIO_DIR_OUT_STR);
  printf("Example:\n");
  printf("gpio_set_dir 21 %s\n", GPIO_DIR_OUT_STR);
  printf("set the specified GPIO pin to input or output\n");
}

int gpio_set_dir(int argc, char *argv[]) {
  unsigned int pin_num, pin_mask, out_en, out_en_reg;
  bool is_output = true;

  if (argc != 3) {
    gpio_set_dir_usage();
    return -1;
  }

  pin_num = get_num(argv[1]);

  if (pin_num > GPIO_MAX_PIN_NUM) {
    printf("Invalid GPIO pin number %d\n", pin_num);
    gpio_set_dir_usage();
    return -1;
  }

  if (strcmp(argv[2], GPIO_DIR_IN_STR) == 0)
    is_output = false;
  else if (strcmp(argv[2], GPIO_DIR_OUT_STR) == 0)
    is_output = true;
  else {
    printf("Invalid GPIO pin direction %s\n", argv[2]);
    gpio_set_dir_usage();
    return -1;
  }

  if (pin_num >= GPIO_HIGH_PIN_START_NUM) {
    out_en_reg = AVANTA_BASE_ADDR + GPIO_HIGH_DATA_OUT_EN_REG;
    pin_mask = 1 << (pin_num - GPIO_HIGH_PIN_START_NUM);
  } else {
    out_en_reg = AVANTA_BASE_ADDR + GPIO_DATA_OUT_EN_REG;
    pin_mask = 1 << pin_num;
  }
  if (read_physical_addr(out_en_reg, &out_en) != 0) {
    printf("Read address 0x%x failed\n", out_en_reg);
    return -1;
  }
  if (is_output)
    out_en &= ~pin_mask;
  else
    out_en |= pin_mask;
  if (write_physical_addr(out_en_reg, out_en) != 0) {
    printf("Write address 0x%x of value 0x%x failed\n", out_en_reg, out_en);
    return -1;
  }
  printf("GPIO pin %d set as %s\n", pin_num, (is_output) ? "output" : "input");

  return 0;
}

static void gpio_set_out_val_usage(void) {
  printf("gpio_set_out_val <GPIO pin num (0 to %d)> <0 | 1>\n",
         GPIO_MAX_PIN_NUM);
  printf("Example:\n");
  printf("gpio_set_out_val 21 0\n");
  printf("set the specified GPIO pin output to 0\n");
}

int gpio_set_out_val(int argc, char *argv[]) {
  unsigned int pin_num, pin_val, pin_mask, out, out_reg;

  if (argc != 3) {
    gpio_set_out_val_usage();
    return -1;
  }

  pin_num = get_num(argv[1]);
  pin_val = get_num(argv[2]);

  if (pin_num > GPIO_MAX_PIN_NUM) {
    printf("Invalid GPIO pin number %d\n", pin_num);
    gpio_set_out_val_usage();
    return -1;
  }

  if (pin_val != 0 && pin_val != 1) {
    printf("Invalid GPIO pin value %d\n", pin_val);
    gpio_set_out_val_usage();
    return -1;
  }

  if (pin_num >= GPIO_HIGH_PIN_START_NUM) {
    out_reg = AVANTA_BASE_ADDR + GPIO_HIGH_DATA_OUT_REG;
    pin_mask = 1 << (pin_num - GPIO_HIGH_PIN_START_NUM);
  } else {
    out_reg = AVANTA_BASE_ADDR + GPIO_DATA_OUT_REG;
    pin_mask = 1 << pin_num;
  }
  if (read_physical_addr(out_reg, &out) != 0) {
    printf("Read address 0x%x failed\n", out_reg);
    return -1;
  }
  if (pin_val)
    out |= pin_mask;
  else
    out &= ~pin_mask;
  if (write_physical_addr(out_reg, out) != 0) {
    printf("Write address 0x%x of value 0x%x failed\n", out_reg, out);
    return -1;
  }
  printf("GPIO pin %d output set as %d\n", pin_num, pin_val);

  return 0;
}

static void gpio_mailbox_usage(void) {
  printf("gpio_mailbox <%s | %s>\n", STOP_STR, START_STR);
  printf("Example:\n");
  printf("gpio_mailbox %s\n", STOP_STR);
  printf("Stop gpio_mailbox from running\n");
}

int gpio_mailbox(int argc, char *argv[]) {
  if (argc != 2) {
    gpio_mailbox_usage();
    return -1;
  }

  if (strcmp(argv[1], STOP_STR) == 0) {
    system_cmd("pkill -9 -f gpio-mailbox");
  } else if (strcmp(argv[1], START_STR) == 0) {
    system_cmd("gpio-mailbox 2>&1 | logos gpio-mailbox &");
  } else {
    gpio_mailbox_usage();
    return -1;
  }

  return 0;
}

static void get_temp_usage(void) {
  printf("get_temp\n");
  printf("display CPU temperature in mili-degree C\n");
  printf("Example\n");
  printf("  prism-diags get_temp\n");
}

int get_temp(int argc, char *argv[]) {
  float temp;
  int t;
  uint8_t value[BOARD_TEMP_LEN];
  FILE *fp;

  if (argc != 1 || argv == NULL) {
    get_temp_usage();
    return -1;
  }
  if (i2cr(BOARD_TEMP_BUS, BOARD_TEMP_ADDR, BOARD_TEMP_REG, BOARD_TEMP_ADDR_LEN,
           BOARD_TEMP_LEN, value) != 0) {
    printf("Temp sensor read address 0x%x failed\n", BOARD_TEMP_ADDR);
    return -1;
  }
  temp = (float)(value[0]) + (((float)(value[1])) / 256.0);
  if (value[0] & 0x80) {
    temp -= 256.0;
  }
  printf("  Board Temp: %3.3f\n", temp);
  fp = popen(GET_TEMP, "r");
  if (fp != NULL) {
    if (fscanf(fp, "%d", &t) <= 0) {
      printf("Failed to read CPU temp\n");
      pclose(fp);
      return -1;
    }
    pclose(fp);
    temp = (float)t / 1000.0;
    printf("  CPU Temp: %3.3f\n", temp);
  } else {
    printf("Failed to get CPU temp\n");
    return -1;
  }

  return 0;
}

static void set_leds_usage(void) {
  printf("set_leds <%s | %s> <value>\n", RED_LED, BLUE_LED);
  printf("set specified LED brightness to <valueue>\n");
  printf("  max value is 100. set value to 0 to turn it off\n");
  printf("Example\n");
  printf("  prism-diags set_leds %s 10\n", RED_LED);
}

int set_leds(int argc, char *argv[]) {
  int value;
  char cmd[LINE_MAX];

  if (argc != 3) {
    set_leds_usage();
    return -1;
  }
  value = strtol(argv[2], NULL, 10);

  if (strcmp(argv[1], RED_LED) == 0) {
    sprintf(cmd, "echo %d > %s", value, RED_LED_BRIGHTNESS);
  } else if (strcmp(argv[1], BLUE_LED) == 0) {
    sprintf(cmd, "echo %d > %s", value, BLUE_LED_BRIGHTNESS);
  } else {
    printf("Unknown LED\n");
    set_leds_usage();
    return -1;
  }
  system_cmd(cmd);
  printf("Set %s LED brightness to %d\n", argv[1], value);

  return 0;
}

static void get_leds_usage(void) {
  printf("get_leds <%s | %s>\n", RED_LED, BLUE_LED);
  printf("get specified LED brightness\n");
  printf("Example\n");
  printf("  prism-diags get_leds %s\n", RED_LED);
}

int get_leds(int argc, char *argv[]) {
  char cmd[LINE_MAX];

  if (argc != 2) {
    get_leds_usage();
    return -1;
  }

  if (strcmp(argv[1], RED_LED) == 0) {
    sprintf(cmd, "cat %s", RED_LED_BRIGHTNESS);
  } else if (strcmp(argv[1], BLUE_LED) == 0) {
    sprintf(cmd, "cat %s", BLUE_LED_BRIGHTNESS);
  } else {
    printf("Unknown LED\n");
    get_leds_usage();
    return -1;
  }
  printf("%s LED brightness is ", argv[1]);
  fflush(stdout);
  system_cmd(cmd);

  return 0;
}
