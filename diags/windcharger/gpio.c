/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"
#include "gpio.h"
#include "../common/io.h"

#define LED_ON_OPTION "on"
#define LED_OFF_OPTION "off"

int enable_gpio_pin_out(int pin, int en) {
  uint32_t value;

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) return -1;
  if (en < 0 || en > 1) return -1;

  read_physical_addr(GPIO_OE, &value);
  value &= ~(1 << pin);
  value |= en << pin;
  write_physical_addr(GPIO_OE, value);
  return 0;
}

int is_gpio_pin_out_enabled(int pin) {
  uint32_t value;

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) return -1;

  read_physical_addr(GPIO_OE, &value);
  // 0 - output, 1 - input
  if ((value & (1 << pin)) != 0) {
    return 0;
  } else {
    return 1;
  }
}

int get_gpio_pin_out_value(int pin) {
  uint32_t value;

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) return -1;

  read_physical_addr(GPIO_OUT, &value);

  return ((value >> pin) & 0x1);
}

int get_gpio_pin_in_value(int pin) {
  uint32_t value;

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) return -1;

  read_physical_addr(GPIO_IN, &value);

  return ((value >> pin) & 0x1);
}

int set_gpio_pin_out_value(int pin, int value) {
  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) return -1;
  if (value < 0 || value > 1) return -1;

  if (value == 0) {
    write_physical_addr(GPIO_CLEAR, 1 << pin);
  } else {
    write_physical_addr(GPIO_SET, 1 << pin);
  }

  return 0;
}

int get_gpio_mux_value(int pin) {
  uint32_t value, reg_addr, addr_offset, byte_offset;

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) return -1;

  addr_offset = (pin / GPIO_CNTL_PER_REG) * GPIO_CNTL_PER_REG;
  byte_offset = pin - addr_offset;
  reg_addr = GPIO_OUT_FUNCTION0 + addr_offset;

  read_physical_addr(reg_addr, &value);

  return ((value >> (8 * byte_offset)) & 0xFF);
}

int set_gpio_mux_value(int pin, int value) {
  uint32_t data, reg_addr, addr_offset, byte_offset;

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) return -1;
  if (value < 0 || value > GPIO_CPU_CNTL_MAX_VAL) return -1;

  addr_offset = (pin / GPIO_CNTL_PER_REG) * GPIO_CNTL_PER_REG;
  byte_offset = pin - addr_offset;
  reg_addr = GPIO_OUT_FUNCTION0 + addr_offset;

  read_physical_addr(reg_addr, &data);
  data &= ~(0xFF << (8 * byte_offset));
  data |= value << (8 * byte_offset);
  write_physical_addr(reg_addr, data);

  return 0;
}

int get_gpio_pin_status(int pin) {
  int mux_sel, out_enabled, value;

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) {
    printf("%s invalid pin number %d\n", FAIL_TEXT, pin);
    return -1;
  }

  mux_sel = get_gpio_mux_value(pin);
  if (mux_sel < 0) {
    printf("%s to get MUX select for pin %d\n", FAIL_TEXT, pin);
    return -1;
  }
  out_enabled = is_gpio_pin_out_enabled(pin);
  if (out_enabled < 0) {
    printf("%s to get pin %d output enable status\n", FAIL_TEXT, pin);
    return -1;
  }
  if (out_enabled == 1) {
    value = get_gpio_pin_out_value(pin);
    printf("GPIO %d output enabled: value %d mux %d\n", pin, value, mux_sel);
  } else {
    value = get_gpio_pin_in_value(pin);
    printf("GPIO %d input enabled: value %d mux %d\n", pin, value, mux_sel);
  }

  return 0;
}

int set_gpio_pin(int pin, int value) {
  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) {
    printf("%s invalid pin number %d (0  to %d)\n", FAIL_TEXT, pin,
           MAX_GPIO_PIN_NUM);
    return -1;
  }
  if (value < 0 || value > 1) {
    printf("%s invalid pin value %d (0 or 1)\n", FAIL_TEXT, value);
    return -1;
  }

  // Set MUX to CPU control value
  if (set_gpio_mux_value(pin, GPIO_CPU_CNTL_VAL) < 0) {
    printf("%s to enable pin %d to output\n", FAIL_TEXT, pin);
    return -1;
  }
  if (enable_gpio_pin_out(pin, 0) < 0) {
    printf("%s to enable pin %d to output\n", FAIL_TEXT, pin);
    return -1;
  }
  if (set_gpio_pin_out_value(pin, value) < 0) {
    printf("%s to set pin %d out register to %d\n", FAIL_TEXT, pin, value);
    return -1;
  }

  printf("GPIO pin %d set to output %d\n", pin, value);
  return 0;
}

static void gpio_dump_usage(void) {
  printf("gpio_dump\n");
  printf("GPIO registerd dump\n");
}

int gpio_dump(int argc, char *argv[]) {
  uint32_t oe, in, out, mux0, mux1, mux2, mux3, mux4;

  if (argc != 1 || argv == NULL) {
    gpio_dump_usage();
    return -1;
  }
  read_physical_addr(GPIO_OE, &oe);
  read_physical_addr(GPIO_IN, &in);
  read_physical_addr(GPIO_OUT, &out);
  read_physical_addr(GPIO_OUT_FUNCTION0, &mux0);
  read_physical_addr(GPIO_OUT_FUNCTION1, &mux1);
  read_physical_addr(GPIO_OUT_FUNCTION2, &mux2);
  read_physical_addr(GPIO_OUT_FUNCTION3, &mux3);
  read_physical_addr(GPIO_OUT_FUNCTION4, &mux4);

  printf("GPIO OE %08x IN %08X OUT %08X MUX %08X %08X %08X %08X %08X\n", oe, in,
         out, mux0, mux1, mux2, mux3, mux4);
  return 0;
}

static void gpio_out_usage(void) {
  printf("gpio_out <pin (%d-0)> <0|1>\n", MAX_GPIO_PIN_NUM);
  printf("Example:\n");
  printf("gpio_out 17 1\n");
  printf("GPIO pin 17 output 1\n");
}

int gpio_out(int argc, char *argv[]) {
  int pin, value;

  if (argc != 3) {
    gpio_out_usage();
    return -1;
  }

  pin = strtol(argv[1], NULL, 0);
  value = strtol(argv[2], NULL, 0);
  return set_gpio_pin(pin, value);
}

static void gpio_disable_out_usage(void) {
  printf("gpio_disable_out <pin (%d-0)>\n", MAX_GPIO_PIN_NUM);
  printf("Example:\n");
  printf("gpio_disable_out 27\n");
  printf("GPIO pin 27 output disabled\n");
}
int gpio_disable_out(int argc, char *argv[]) {
  int pin;

  if (argc != 2) {
    gpio_disable_out_usage();
    return -1;
  }

  pin = strtol(argv[1], NULL, 0);

  if (pin < 0 || pin > MAX_GPIO_PIN_NUM) {
    printf("%s invalid pin number %d. Range ( 0 to %d)\n", FAIL_TEXT, pin,
           MAX_GPIO_PIN_NUM);
    return -1;
  }

  if (enable_gpio_pin_out(pin, 1) < 0) {
    printf("%s disable pin number %d error\n", FAIL_TEXT, pin);
    return -1;
  }

  printf("GPIO pin %d output disabled\n", pin);
  return 0;
}

static void gpio_stat_usage(void) {
  printf("gpio_stat <pin (%d-0)>\n", MAX_GPIO_PIN_NUM);
  printf("Example:\n");
  printf("gpio_stat 17\n");
  printf("GPIO pin 17 output status\n");
}

int gpio_stat(int argc, char *argv[]) {
  int pin;

  if (argc != 2) {
    gpio_stat_usage();
    return -1;
  }

  pin = strtol(argv[1], NULL, 0);
  return get_gpio_pin_status(pin);
}

static void set_red_led_usage(void) {
  printf("set_red_led <on/off>\n");
  printf("Example:\n");
  printf("set_red_led on\n");
  printf("Turn on red LED\n");
}

int set_red_led(int argc, char *argv[]) {
  if (argc != 2) {
    set_red_led_usage();
    return -1;
  }

  if (strcmp(argv[1], LED_ON_OPTION) == 0) {
    return set_gpio_pin(GPIO_RED_LED_PIN, 1);
  } else if (strcmp(argv[1], LED_OFF_OPTION) == 0) {
    return set_gpio_pin(GPIO_RED_LED_PIN, 0);
  } else {
    set_red_led_usage();
    return -1;
  }
}

static void set_blue_led_usage(void) {
  printf("set_blue_led <on/off>\n");
  printf("Example:\n");
  printf("set_blue_led on\n");
  printf("Turn on blue LED\n");
}

int set_blue_led(int argc, char *argv[]) {
  if (argc != 2) {
    set_blue_led_usage();
    return -1;
  }

  if (strcmp(argv[1], LED_ON_OPTION) == 0) {
    return set_gpio_pin(GPIO_BLUE_LED_PIN, 1);
  } else if (strcmp(argv[1], LED_OFF_OPTION) == 0) {
    return set_gpio_pin(GPIO_BLUE_LED_PIN, 0);
  } else {
    set_blue_led_usage();
    return -1;
  }
}

static void set_led_dim_usage(void) {
  printf("set_led_dim <on/off>\n");
  printf("Example:\n");
  printf("set_led_dim on\n");
  printf("dim LED\n");
}

int set_led_dim(int argc, char *argv[]) {
  if (argc != 2) {
    set_led_dim_usage();
    return -1;
  }

  if (strcmp(argv[1], LED_ON_OPTION) == 0) {
    return set_gpio_pin(GPIO_DIM_LED_PIN, 1);
  } else if (strcmp(argv[1], LED_OFF_OPTION) == 0) {
    return set_gpio_pin(GPIO_DIM_LED_PIN, 0);
  } else {
    set_led_dim_usage();
    return -1;
  }
}

static void set_poe_usage(void) {
  printf("set_poe <on/off>\n");
  printf("Example:\n");
  printf("set_poe on\n");
  printf("Turn on PoE\n");
}

int set_poe(int argc, char *argv[]) {
  if (argc != 2) {
    set_poe_usage();
    return -1;
  }

  if (strcmp(argv[1], LED_ON_OPTION) == 0) {
    return set_gpio_pin(GPIO_POE_PIN, 0);
  } else if (strcmp(argv[1], LED_OFF_OPTION) == 0) {
    return set_gpio_pin(GPIO_POE_PIN, 1);
  } else {
    set_poe_usage();
    return -1;
  }
}

static void check_reset_button_usage(void) {
  printf("check_reset_button\n");
  printf("Example:\n");
  printf("check_reset_button\n");
  printf("Check if the external reset button on or off\n");
}

int check_reset_button(int argc, char *argv[]) {
  if (argc != 1 || argv == NULL) {
    check_reset_button_usage();
    return -1;
  }

  // Reset button is active low
  if (get_gpio_pin_in_value(GPIO_RESET_BUTTON_PIN) > 0) {
    printf("Reset button is off\n");
  } else {
    printf("Reset button is on\n");
  }
  return 0;
}

static void cpu_reset_usage(void) {
  printf("cpu_reset\n");
  printf("Example:\n");
  printf("cpu_reset\n");
  printf("Perform CPU cold reset\n");
}

int cpu_reset(int argc, char *argv[]) {
  if (argc != 1 || argv == NULL) {
    cpu_reset_usage();
    return -1;
  }

  printf("CPU cold reset ...\n");
  sleep(1);
  write_physical_addr(RST_RESET, 1 << CPU_COLD_RESET_BIT);

  return 0;
}
