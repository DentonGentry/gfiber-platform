/*
 * Copyright 2015 Google Inc.
 * All rights reserved.
 * diagutil -- Linux-based Hardware Diagnostic Utilities
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DIAGS_VERSION "1.0.1"

/* External Functions */
int i2cprobe(int argc, char **argv);
int i2cread(int argc, char **argv);
int i2cwrite(int argc, char **argv);
int ioread(int argc, char **argv);
int iowrite(int argc, char **argv);
int gpioout(int argc, char *argv[]);
int gpiodisableout(int argc, char *argv[]);
int gpiooutstat(int argc, char *argv[]);
int send_ip(int argc, char *argv[]);
int send_eth(int argc, char *argv[]);
int send_if_to_mac(int argc, char *argv[]);
int geloopback(int argc, char *argv[]);
int lan_lpbk(int argc, char *argv[]);
int set_lan_snake(int argc, char *argv[]);
int ge_traffic(int argc, char *argv[]);
int start_server(int argc, char *argv[]);
int sendcmd(int argc, char *argv[]);
int switchreset(int argc, char *argv[]);
int atheros_init(int argc, char *argv[]);
int phy_init(int argc, char *argv[]);
int satacfgdump(int argc, char *argv[]);
int satabench(int argc, char *argv[]);
int sata_link_reset(int argc, char *argv[]);
int mem_test(int argc, char *argv[]);
int fan_mon(int argc, char *argv[]);
int temperature(int argc, char *argv[]);
int fan_speed(int argc, char *argv[]);
int fan_rpm(int argc, char *argv[]);
int flash_test(int argc, char *argv[]);
int tpm_startup(int argc, char *argv[]);

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
    {"i2cread", i2cread},
    {"i2cwrite", i2cwrite},
    {"i2cprobe", i2cprobe},
    {"", NULL},
    {"ioread", ioread},
    {"iowrite", iowrite},
    {"", NULL},
    {"satacfgdump", satacfgdump},
    {"satabench", satabench},
    {"sata_link_reset", sata_link_reset},
    {"", NULL},
    {"gpioout", gpioout},
    {"gpiooutstat", gpiooutstat},
    {"gpiodisableout", gpiodisableout},
    {"", NULL},
    {"send_ip", send_ip},
    {"send_eth", send_eth},
    {"send_if_to_mac", send_if_to_mac},
    {"loopback", geloopback},
    {"switchreset", switchreset},
    {"atheros_init", atheros_init},
    {"phy_init", phy_init},
    {"lan_lpbk", lan_lpbk},
    {"set_lan_snake", set_lan_snake},
    {"ge_traffic", ge_traffic},
    {"", NULL},
    {"mem_test", mem_test},
    {"", NULL},
    {"fan_mon", fan_mon},
    {"temperature", temperature},
    {"fan_speed", fan_speed},
    {"fan_rpm", fan_rpm},
    {"", NULL},
    {"flash_test", flash_test},
    {"", NULL},
    {"tpm_startup", tpm_startup},
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
