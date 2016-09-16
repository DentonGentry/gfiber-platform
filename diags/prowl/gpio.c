/*
 * (C) Copyright 2016 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../common/util.h"
#include "i2c.h"

static void switch_state_usage(void) {
  printf("switch_state\n");
  printf("Example:\n");
  printf(" switch_state\n");
}

int  switch_state(int argc, char *argv[]) {
  if (argc != 1 || argv == NULL) {
    switch_state_usage();
    return -1;
  }

  system_cmd("echo 5 > /sys/class/gpio/export");
  system_cmd("cat /sys/class/gpio/gpio5/value");

  return 0;
}

static void poe_disable_usage(void) {
  printf("poe_disable [<0 | 1>]\n");
  printf("Example:\n");
  printf(" poe_disable 1\n");
}

int poe_disable(int argc, char *argv[]) {
  FILE *fp;
  char rsp[1024];

  if (argc > 2) {
    poe_disable_usage();
    return -1;
  }

  system_cmd("echo 4 > /sys/class/gpio/export");
  fp = popen("cat /sys/class/gpio/gpio4/direction", "r");
  if (fp == NULL) {
    printf("Failed to open GPIO4\n");
    return -1;
  }
  fscanf(fp, "%s", rsp);
  pclose(fp);
  if (strcmp(rsp, "out") != 0) {
    system_cmd("echo \"out\" > /sys/class/gpio/gpio4/direction");
  }
  if ( argc == 1) {
    printf("PoE Disable: ");
    fflush(stdout);
    system_cmd("cat /sys/class/gpio/gpio4/value");
  } else {
    if (strcmp(argv[1], "0") == 0) {
      system_cmd("echo 0 > /sys/class/gpio/gpio4/value");
      printf("PoE Disable set to ");
      fflush(stdout);
      system_cmd("cat /sys/class/gpio/gpio4/value");
    } else if (strcmp(argv[1], "1") == 0) {
      system_cmd("echo 1 > /sys/class/gpio/gpio4/value");
      printf("PoE Disable set to ");
      fflush(stdout);
      system_cmd("cat /sys/class/gpio/gpio4/value");
    } else {
      poe_disable_usage();
      return -1;
    }
  }

  return 0;
}

