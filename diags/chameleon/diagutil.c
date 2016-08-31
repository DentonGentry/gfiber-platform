/*
 * Copyright 2015 Google Inc.
 * All rights reserved.
 * diagutil -- Linux-based Hardware Diagnostic Utilities
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIAGS_VERSION "1.1"

/* External Functions */
int ioread(int argc, char **argv);
int iowrite(int argc, char **argv);
int iowrite_only(int argc, char *argv[]);
int i2cread(int argc, char *argv[]);
int i2cwrite(int argc, char *argv[]);
int i2cprobe(int argc, char *argv[]);
int gpio_stat(int argc, char *argv[]);
int gpio_set_dir(int argc, char *argv[]);
int gpio_set_out_val(int argc, char *argv[]);
int gpio_set_tx_enable(int argc, char *argv[]);
int gpio_mailbox(int argc, char *argv[]);
int get_temp(int argc, char *argv[]);
int set_leds(int argc, char *argv[]);
int get_leds(int argc, char *argv[]);
int phy_read(int argc, char *argv[]);
int phy_write(int argc, char *argv[]);
int loopback_test(int argc, char *argv[]);
int phy_read(int argc, char *argv[]);
int phy_write(int argc, char *argv[]);
int soc_reg_read(int argc, char *argv[]);
int soc_reg_write(int argc, char *argv[]);
int gpon_rx_status(int argc, char *argv[]);
int rx_prbs_cnt(int argc, char *argv[]);
int rx_prbs_err_cnt(int argc, char *argv[]);
int gpon_cnts(int argc, char *argv[]);
int gpon_alarms(int argc, char *argv[]);
int sfp_reg_read(int argc, char *argv[]);
int sfp_reg_write(int argc, char *argv[]);
int sfp_diags_reg_read(int argc, char *argv[]);
int sfp_diags_reg_write(int argc, char *argv[]);
int sfp_info(int argc, char *argv[]);
int sfp_vendor(int argc, char *argv[]);
int sfp_pn(int argc, char *argv[]);
int sfp_wavelength(int argc, char *argv[]);
int sfp_set_wavelength(int argc, char *argv[]);
int sfp_set_pw(int argc, char *argv[]);

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
    {"", NULL},
    {"gpio_stat", gpio_stat},
    {"gpio_set_dir", gpio_set_dir},
    {"gpio_set_out_val", gpio_set_out_val},
    {"gpio_set_tx_enable", gpio_set_tx_enable},
    {"gpio_mailbox", gpio_mailbox},
    {"get_temp", get_temp},
    {"set_leds", set_leds},
    {"get_leds", get_leds},
    {"", NULL},
    {"phy_read", phy_read},
    {"phy_write", phy_write},
    {"loopback_test", loopback_test},
    {"", NULL},
    {"soc_reg_read", soc_reg_read},
    {"soc_reg_write", soc_reg_write},
    {"gpon_rx_status", gpon_rx_status},
    {"rx_prbs_cnt", rx_prbs_cnt},
    {"rx_prbs_err_cnt", rx_prbs_err_cnt},
    {"gpon_cnts", gpon_cnts},
    {"gpon_alarms", gpon_alarms},
    {"", NULL},
    {"sfp_reg_read", sfp_reg_read},
    {"sfp_reg_write", sfp_reg_write},
    {"sfp_diags_reg_read", sfp_diags_reg_read},
    {"sfp_diags_reg_write", sfp_diags_reg_write},
    {"sfp_info", sfp_info},
    {"sfp_vendor", sfp_vendor},
    {"sfp_pn", sfp_pn},
    {"sfp_wavelength", sfp_wavelength},
    {"sfp_set_wavelength", sfp_set_wavelength},
    {"sfp_set_pw", sfp_set_pw},
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
