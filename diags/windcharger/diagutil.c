/*
 * Copyright 2015 Google Inc.
 * All rights reserved.
 * diagutil -- Linux-based Hardware Diagnostic Utilities
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DIAGS_VERSION "1.8"

/* External Functions */
int ioread(int argc, char **argv);
int iowrite(int argc, char **argv);
int iowrite_only(int argc, char *argv[]);
int gpio_out(int argc, char *argv[]);
int gpio_disable_out(int argc, char *argv[]);
int gpio_stat(int argc, char *argv[]);
int gpio_dump(int argc, char *argv[]);
int check_reset_button(int argc, char *argv[]);
int cpu_reset(int argc, char *argv[]);
int set_red_led(int argc, char *argv[]);
int set_blue_led(int argc, char *argv[]);
int set_led_dim(int argc, char *argv[]);
int set_poe(int argc, char *argv[]);
int send_if_to_if(int argc, char *argv[]);
int send_if(int argc, char *argv[]);
int send_if_to_mac(int argc, char *argv[]);
int test_both_ports(int argc, char *argv[]);
int loopback_test(int argc, char *argv[]);
int ethreg_main(int argc, char *argv[]);

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
    {"gpio_out", gpio_out},
    {"gpio_stat", gpio_stat},
    {"gpio_disable_out", gpio_disable_out},
    {"gpio_dump", gpio_dump},
    {"check_reset_button", check_reset_button},
    {"cpu_reset", cpu_reset},
    {"set_red_led", set_red_led},
    {"set_blue_led", set_blue_led},
    {"set_led_dim", set_led_dim},
    {"set_poe", set_poe},
    {"", NULL},
    {"ethreg", ethreg_main},
    {"send_if_to_if", send_if_to_if},
    {"send_if", send_if},
    {"send_if_to_mac", send_if_to_mac},
    {"test_both_ports", test_both_ports},
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
