/*
 * Copyright 2015 Google Inc.
 * All rights reserved.
 * diagutil -- Linux-based Hardware Diagnostic Utilities
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DIAGS_VERSION "1.2"

/* External Functions */
int ioread(int argc, char **argv);
int iowrite(int argc, char **argv);
int iowrite_only(int argc, char *argv[]);
int i2cprobe(int argc, char **argv);
int i2cread(int argc, char **argv);
int i2cwrite(int argc, char **argv);
int board_temp(int argc, char *argv[]);
int led_set(int argc, char *argv[]);
int led_set_pwm(int argc, char *argv[]);
int switch_state(int argc, char *argv[]);
int poe_disable(int argc, char *argv[]);
int phy_read(int argc, char *argv[]);
int phy_write(int argc, char *argv[]);
int loopback_test(int argc, char *argv[]);

/* Define the command structure */
typedef struct {
  const char *name;
  int (*funcp)(int, char **);
} tCOMMAND;

void printVersion() { printf("%s\n", DIAGS_VERSION); }

int version(int argc, char *argv[]) {
  // This is to avoid unused params warning
  if ((argc != 1) || (argv[0] == NULL)) {
    printf("Invalid command parameter\n");
  }
  printVersion();
  return 0;
}

/* Table of supported commands */
tCOMMAND command_list[] = {
    {"ioread", ioread},
    {"iowrite", iowrite},
    {"iowrite_only", iowrite_only},
    {"", NULL},
    {"i2cread", i2cread},
    {"i2cwrite", i2cwrite},
    {"i2cprobe", i2cprobe},
    {"board_temp", board_temp},
    {"led_set", led_set},
    {"led_set_pwm", led_set_pwm},
    {"", NULL},
    {"switch_state", switch_state},
    {"poe_disable", poe_disable},
    {"", NULL},
    {"phy_read", phy_read},
    {"phy_write", phy_write},
    {"loopback_test", loopback_test},
    {"", NULL},
    {"version", version},
    {"", NULL},
    {NULL, NULL},
};

static void usage(void) {
  int i;

  printf("Supported commands:\n");

  for (i = 0; command_list[i].name != NULL; ++i) {
    printf("\t%s\n", command_list[i].name);
  }

  return;
}

int main(int argc, char *argv[]) {
  int i;

  if (argc > 1) {
    /* Search the command list for a match */
    for (i = 0; command_list[i].name != NULL; ++i) {
      if (strcmp(argv[1], command_list[i].name) == 0) {
        return command_list[i].funcp(argc - 1, &argv[1]);
      }
    }
  }

  /* no command or bad command */
  usage();

  return 0;
}
