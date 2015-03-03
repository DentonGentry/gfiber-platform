/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "gpio.h"
#include "../common/io.h"

static unsigned int gpio_debug = 0;

int enable_gpio_63_60_signal() {
  uint32_t value;

  read_physical_addr(MISC_PIN_SELECT_REG, &value);
  value &= ~MISC_PIN_SELECT_GPIO_MASK;
  value |= MISC_PIN_SELECT_GPIO_SELECTED;
  write_physical_addr(MISC_PIN_SELECT_REG, value);
  if (gpio_debug & 2) {
    printf("Set MISC reg 0x%x to 0x%08x\n", MISC_PIN_SELECT_REG, value);
  }
  return 0;
}

int gpio_63_32_out_set(int pin, int pinval) {
  uint32_t value, out;
  int rc = 0;

  if (pin > MAX_GPIO_PIN_NUM || pin < FIRST_GPIO_SET_SIZE) {
    printf("invalid pin %d for gpio63_32\n", pin);
    return -1;
  }
  if (pinval < 0 || pinval > 1) {
    printf("invalid gpio bit value %d, expecting 0 or 1\n", pinval);
    return -1;
  }

  if (pin >= GPIO_MISC_SELET_NUM) {
    rc = enable_gpio_63_60_signal();
  }

  read_physical_addr(GPIO_63_32_PIN_OUTPUT_ENABLE_REG, &value);
  read_physical_addr(GPIO_63_32_PIN_OUTPUT_REG, &out);
  value |= 1 << (pin - FIRST_GPIO_SET_SIZE);
  if (pinval) {
    out |= 1 << (pin - FIRST_GPIO_SET_SIZE);
  } else {
    out &= ~(1 << (pin - FIRST_GPIO_SET_SIZE));
  }
  write_physical_addr(GPIO_63_32_PIN_OUTPUT_REG, out);
  if (gpio_debug & 2) {
    printf("Set GPIO63_32 reg 0x%x to 0x%08x\n", GPIO_63_32_PIN_OUTPUT_REG,
           out);
  }
  write_physical_addr(GPIO_63_32_PIN_OUTPUT_ENABLE_REG, value);
  if (gpio_debug & 2) {
    printf("Set GPIO63_32_en reg 0x%x to 0x%08x\n",
           GPIO_63_32_PIN_OUTPUT_ENABLE_REG, value);
  }

  if (gpio_debug & 1) {
    printf("GPIO pin %d set output to %d\n", pin, pinval);
  }

  return rc;
}

int gpio_31_0_out_set(int pin, int pinval) {
  uint32_t value, out;

  if (pin > 31 || pin < 0) {
    printf("invalid pin %d for gpio31_0\n", pin);
    return -1;
  }
  if (pinval < 0 || pinval > 1) {
    printf("invalid gpio bit value %d, expecting 0 or 1\n", pinval);
    return -1;
  }

  read_physical_addr(GPIO_31_0_PIN_OUTPUT_ENABLE_REG, &value);
  read_physical_addr(GPIO_31_0_PIN_OUTPUT_REG, &out);
  value |= (1 << pin);
  if (pinval) {
    out |= (1 << pin);
  } else {
    out &= ~(1 << pin);
  }
  write_physical_addr(GPIO_31_0_PIN_OUTPUT_REG, out);
  if (gpio_debug & 2) {
    printf("Set GPIO31_0 reg 0x%x to 0x%08x\n", GPIO_31_0_PIN_OUTPUT_REG, out);
  }
  write_physical_addr(GPIO_31_0_PIN_OUTPUT_ENABLE_REG, value);
  if (gpio_debug & 2) {
    printf("Set GPIO31_0_en reg 0x%x to 0x%08x\n",
           GPIO_31_0_PIN_OUTPUT_ENABLE_REG, value);
  }

  if (gpio_debug & 1) {
    printf("GPIO pin %d set output to %d\n", pin, pinval);
  }

  return 0;
}

static void gpioout_usage(void) {
  printf("gpioout <pin (%d-0)> <0|1>\n", MAX_GPIO_PIN_NUM);
  printf("Example:\n");
  printf("gpioout 27 1\n");
  printf("GPIO pin 27 output 1\n");
}

int gpioout(int argc, char *argv[]) {
  int pin, val, rc;

  if (argc != 3) {
    gpioout_usage();
    return -1;
  }

  pin = strtoul(argv[1], NULL, 0);
  val = strtoul(argv[2], NULL, 0);

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) {
    printf("Invalid GPIO pin number %d. Range (%d to 0)\n", pin,
           MAX_GPIO_PIN_NUM);
    return -1;
  }

  if (val < 0 || val > 1) {
    printf("Invalid GPIO pin output value %d. Only 0 or 1 accepted\n", val);
    return -1;
  }

  if (pin < FIRST_GPIO_SET_SIZE) {
    rc = gpio_31_0_out_set(pin, val);
  } else {
    rc = gpio_63_32_out_set(pin, val);
  }

  printf("GPIO pin %d set to %d\n", pin, val);
  return rc;
}

static void gpiodisableout_usage(void) {
  printf("gpiodisableout <pin (%d-0)>\n", MAX_GPIO_PIN_NUM);
  printf("Example:\n");
  printf("gpiodisableout 27\n");
  printf("GPIO pin 27 output disabled\n");
}
int gpiodisableout(int argc, char *argv[]) {
  int pin, rc = 0;
  uint32_t value;

  if (argc != 2) {
    gpiodisableout_usage();
    return -1;
  }

  pin = strtoul(argv[1], NULL, 0);

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) {
    printf("Invalid GPIO pin number %d. Range (%d to 0)\n", pin,
           MAX_GPIO_PIN_NUM);
    return -1;
  }

  if (pin >= GPIO_MISC_SELET_NUM) {
    rc = enable_gpio_63_60_signal();
  }

  if (pin < FIRST_GPIO_SET_SIZE) {
    read_physical_addr(GPIO_31_0_PIN_OUTPUT_ENABLE_REG, &value);
    value &= ~(1 << pin);
    write_physical_addr(GPIO_31_0_PIN_OUTPUT_ENABLE_REG, value);
    printf("Set GPIO31_0_en reg 0x%x to 0x%08x\n",
           GPIO_31_0_PIN_OUTPUT_ENABLE_REG, value);
  } else {
    read_physical_addr(GPIO_63_32_PIN_OUTPUT_ENABLE_REG, &value);
    value &= ~(1 << (pin - FIRST_GPIO_SET_SIZE));
    write_physical_addr(GPIO_63_32_PIN_OUTPUT_ENABLE_REG, value);
    printf("Set GPIO63_32_en reg 0x%x to 0x%08x\n", GPIO_63_32_PIN_OUTPUT_REG,
           value);
  }

  printf("GPIO pin %d output disabled\n", pin);
  return rc;
}

static void gpiooutstat_usage(void) {
  printf("gpiooutstat <pin (%d-0)>\n", MAX_GPIO_PIN_NUM);
  printf("Example:\n");
  printf("gpiooutstat 27\n");
  printf("GPIO pin 27 output status\n");
}

int gpiooutstat(int argc, char *argv[]) {
  int pin, rc = 0;
  uint32_t value, out, enStat, outValue;

  if (argc != 2) {
    gpiooutstat_usage();
    return -1;
  }

  pin = strtoul(argv[1], NULL, 0);

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) {
    printf("Invalid GPIO pin number %d. Range (%d to 0)\n", pin,
           MAX_GPIO_PIN_NUM);
    return -1;
  }

  if (pin >= GPIO_MISC_SELET_NUM) {
    rc = enable_gpio_63_60_signal();
  }

  if (pin < FIRST_GPIO_SET_SIZE) {
    read_physical_addr(GPIO_31_0_PIN_OUTPUT_ENABLE_REG, &value);
    enStat = value & (1 << pin);
    read_physical_addr(GPIO_31_0_PIN_OUTPUT_REG, &out);
    outValue = out & (1 << pin);
    printf("GPIO_31_0_en 0x%08x GPIO_31_0_out 0x%08x\n", value, out);
  } else {
    read_physical_addr(GPIO_63_32_PIN_OUTPUT_ENABLE_REG, &value);
    enStat = value & (1 << (pin - FIRST_GPIO_SET_SIZE));
    read_physical_addr(GPIO_63_32_PIN_OUTPUT_REG, &out);
    outValue = out & (1 << (pin - FIRST_GPIO_SET_SIZE));
    printf("GPIO_63_32_en 0x%08x GPIO_63_32_out 0x%08x\n", value, out);
  }

  printf("GPIO pin %d output %s output value %d\n", pin,
         (enStat) ? "enabled" : "disabled", (outValue) ? 1 : 0);
  return rc;
}

static void board_info_usage(void) {
  printf("board_info\n");
  printf("Example:\n");
  printf("board_info\n");
  printf("query hardware board info\n");
}

int board_info(int argc, char *argv[]) {
  uint32_t gpio_63_32_input, data;

  if ((argc != 1) || (argv == NULL)) {
    board_info_usage();
    return -1;
  }

  // Set both board ID and HW REV bits to input
  read_physical_addr(GPIO_63_32_PIN_OUTPUT_EN_REG, &data);
  data |= (GPIO_HW_REV_MASK << GPIO_HW_REV_SHIFT) |
          (GPIO_BOARD_ID_MASK << GPIO_BOARD_ID_SHIFT);
  write_physical_addr(GPIO_63_32_PIN_OUTPUT_EN_REG, data);

  // Select both board ID and HW REV bits to GPIO
  read_physical_addr(GPIO_63_32_PIN_SELECT_REG, &data);
  data |= (GPIO_HW_REV_MASK << GPIO_HW_REV_SHIFT) |
          (GPIO_BOARD_ID_MASK << GPIO_BOARD_ID_SHIFT);
  write_physical_addr(GPIO_63_32_PIN_SELECT_REG, data);

  // Read board ID and HW rev GPIO pins
  read_physical_addr(GPIO_63_32_PIN_INPUT_REG, &gpio_63_32_input);
  printf("Board ID: %d, Hardware Rev: %d\n",
         (gpio_63_32_input >> GPIO_BOARD_ID_SHIFT) & GPIO_BOARD_ID_MASK,
         (gpio_63_32_input >> GPIO_HW_REV_SHIFT) & GPIO_HW_REV_MASK);
  return 0;
}

static void gpiodebugset_usage(void) {
  printf("gpiodebugset <mask in hex>\n");
  printf("Example:\n");
  printf("gpiodebugset 0x3\n");
  printf("gpio debug bit 0 and 1 set\n");
}

int gpiodebugset(int argc, char *argv[]) {
  if (argc != 2) {
    gpiodebugset_usage();
    return -1;
  }

  gpio_debug = strtoul(argv[1], NULL, 16);
  return 0;
}
