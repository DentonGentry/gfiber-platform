/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "atheros.h"

static void switchreset_usage(void) {
  printf("switchreset <0=not reset|1=reset)>\n");
  printf("Example:\n");
  printf("switchreset 0\n");
  printf("Switch out of reset\n");
}

int switchreset(int argc, char *argv[]) {
  int val;
  int rc;

  if (argc != 2) {
    switchreset_usage();
    return -1;
  }

  val = strtoul(argv[1], NULL, 0);

  if (val < 0 || val > 1) {
    printf("Invalid switchreset request %d. Range 0 or 1\n", val);
    return -1;
  }

  rc = gpio_31_0_out_set(AR8337_RST_GPIO_PIN_NUM, val ^ 1);

  printf("switch %s\n", (val) ? "in reset" : "out of reset");
  return rc;
}
