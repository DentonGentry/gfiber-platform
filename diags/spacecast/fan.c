/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "i2c.h"
#include "../common/util.h"
#include "common.h"

#define TMP_FAN_FILE "/tmp/diags.fan"
#define FAN_TEST_PERIOD 30
#define FAN_SPEED_CMD "/sys/bus/i2c/devices/0-004c/pwm1"
#define FAN_RPM_CMD "/sys/bus/i2c/devices/0-004c/fan1_input"
#define FAN_TEMP_CMD "/sys/bus/i2c/devices/0-004c/temp2_input"
#define HDD_TEMP_CMD "smartctl -a /dev/sda | grep Temperature_Celsius"

static int fan_mon_usage() {
  printf("fan_mon <start|stop>\n");
  printf("Example:\n");
  printf("fan_mon stop\n");
  printf("Stop fan monitoring so its speed can be changed manually\n");

  return -1;
}

int fan_mon(int argc, char *argv[]) {
  if (argc != 2) {
    return fan_mon_usage();
  }

  if (strcmp(argv[1], "start") == 0) {
    system_cmd("reboot-if-fail gpio-mailbox 2>&1 | logos gpio-mailbox &");
    printf("Auto fan control started\n");
  } else if (strcmp(argv[1], "stop") == 0) {
    system_cmd("pkill -9 -f gpio-mailbox");
    printf("Auto fan control stopped\n");
  } else
    return fan_mon_usage();

  return 0;
}

static int temperature_usage() {
  printf("temperatur\n");
  printf("Example:\n");
  printf("temperatur\n");
  printf("show temperature in millicentigrade\n");

  return -1;
}

int temperature(int argc, char *argv[]) {
  if ((argc != 1) || (argv[0] == NULL)) {
    return temperature_usage();
  }

  printf("Board temp in millicentigrade: ");
  fflush(stdout);
  system_cmd("cat  /sys/bus/i2c/devices/0-004c/temp2_input");
  printf("HDD temp in centigrade:\n");
  fflush(stdout);
  system_cmd(HDD_TEMP_CMD);

  return 0;
}

static int fan_speed_usage() {
  printf("fan_speed <percentage (0-100)>\n");
  printf("Example:\n");
  printf("fan_speed 100\n");
  printf("run fan at 100 percent\n");

  return -1;
}

int fan_speed(int argc, char *argv[]) {
  unsigned int fan_speed_val, percent;
  char cmd[64];
  static const char kPer = '\%';

  if (argc != 2) {
    return fan_speed_usage();
  }

  percent = strtoul(argv[1], NULL, 0);

  if (percent > 100) {
    printf("%s Invalid fan speed %d\n", FAIL_TEXT, percent);
    return fan_speed_usage();
  }

  // system_cmd("pkill -9 -f gpio-mailbox");
  fan_speed_val = (255 * percent) / 100;
  printf("Board temp before fan test in millicentigrade: ");
  fflush(stdout);
  system_cmd("cat " FAN_TEMP_CMD);
  printf("HDD temp before fan test in centigrade: ");
  fflush(stdout);
  system_cmd(HDD_TEMP_CMD);
  sprintf(cmd, "echo %d > " FAN_SPEED_CMD, fan_speed_val);
  system_cmd(cmd);
  sleep(FAN_TEST_PERIOD);
  // system_cmd("reboot-if-fail gpio-mailbox 2>&1 | logos gpio-mailbox &");

  printf("Fan speed set to %d = %d%c\n", fan_speed_val, percent, kPer);
  printf("Board temp after fan test in millicentigrade: ");
  fflush(stdout);
  system_cmd("cat " FAN_TEMP_CMD);
  printf("HDD temp in centigrade: ");
  fflush(stdout);
  system_cmd(HDD_TEMP_CMD);

  return 0;
}

static int fan_rpm_usage() {
  printf("fan_rpm\n");
  printf("Example:\n");
  printf("fan_rpm\n");
  printf("get the current fan rpm\n");

  return -1;
}

int fan_rpm(int argc, char *argv[]) {
  int rc = 0;

  if ((argc != 1) || (argv[0] == NULL)) {
    printf("%s invalid params\n", FAIL_TEXT);
    return fan_rpm_usage();
  }

  printf("Current fan speed (RPM) is ");
  fflush(stdout);
  system_cmd("cat " FAN_RPM_CMD);
  printf("Board temp after fan test in millicentigrade: ");
  fflush(stdout);
  system_cmd("cat " FAN_TEMP_CMD);
  printf("HDD temp is:\n");
  fflush(stdout);
  system_cmd(HDD_TEMP_CMD);

  return rc;
}
