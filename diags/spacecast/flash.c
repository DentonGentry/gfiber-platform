/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "flash.h"
#include "../common/util.h"

#define MAX_NAME_SIZE 64

int flash_test_patterns[] = {0x5A, 0xA5, 0x55, 0xAA, 0x00, 0x0F, 0xF0, 0xFF};

static int flash_test_usage() {
  printf("flash_test\n");
  printf("Example:\n");
  printf("flash_test\n");
  printf("This runs tests on the spare section of the NOR flash\n");

  return -1;
}

int flash_test(int argc, char *argv[]) {
  FILE *fp;
  char flash_filename[MAX_NAME_SIZE], size_str[MAX_NAME_SIZE];
  char flash[MAX_NAME_SIZE], erase_flash_cmd[MAX_NAME_SIZE];
  char read_flash_cmd[MAX_NAME_SIZE], write_flash_cmd[MAX_NAME_SIZE];
  char erase_size_str[MAX_NAME_SIZE];
  int size, erase_size, i, j, ch, int_size = sizeof(int);
  int pattern_size = sizeof(flash_test_patterns), num_blocks;
  bool test_passed = true;

  if ((argc != 1) || (argv[0] == NULL)) {
    return flash_test_usage();
  }

  fp = popen(GET_SPARE_FLASH_CMD, "r");
  if (fp == NULL) {
    printf("No flash file\n");
    return -1;
  }

  if (fscanf(fp, "%s", flash_filename) == EOF) {
    printf("Cannot find flash file name\n");
    pclose(fp);
    return -1;
  }

  if (fscanf(fp, "%s", size_str) == EOF) {
    printf("Cannot find flash file size\n");
    pclose(fp);
    return -1;
  }

  if (fscanf(fp, "%s", erase_size_str) == EOF) {
    printf("Cannot find flash file erase size\n");
    pclose(fp);
    return -1;
  }
  pclose(fp);

  size = strlen(flash_filename);
  if ((size > 1) && (flash_filename[size - 1] == ':')) {
    // Get rid of : from the name
    flash_filename[size - 1] = '\0';
  }
  size = strtoul(size_str, NULL, 16);
  erase_size = strtoul(erase_size_str, NULL, 16);
  if (erase_size == 0) {
    printf("Invalid file erase size %d\n", erase_size);
    return -1;
  }
  num_blocks = size / erase_size;

  flash[0] = '\0';
  strcpy(flash, "/dev/");
  strcat(flash, flash_filename);

  sprintf(write_flash_cmd, "cat %s > %s", FLASH_TEST_FILE_NAME, flash);
  sprintf(read_flash_cmd, "cat %s > %s", flash, FLASH_RESULT_FILE_NAME);
  sprintf(erase_flash_cmd, "flash_erase %s 0 %d", flash, num_blocks);

  printf("Test flash %s size %d blocks %d\n", flash, size, num_blocks);

  for (i = 0; i < (pattern_size / int_size); ++i) {
    fp = fopen(FLASH_TEST_FILE_NAME, "w+");
    if (fp == NULL) {
      printf("Failed to open flash test file: %s\n", FLASH_TEST_FILE_NAME);
      return -1;
    }
    printf("Writing 0x%02x to flash test file ... \n", flash_test_patterns[i]);
    for (j = 0; j < size; ++j) {
      if (fputc(flash_test_patterns[i], fp) == EOF) {
        printf("Write 0x%x to flash test location %d failed.\n",
               flash_test_patterns[i], j);
        test_passed = false;
        break;
      }
    }
    fclose(fp);
    printf("erase flash ...\n");
    system_cmd(erase_flash_cmd);
    printf("Write test pattern to flash ...\n");
    system_cmd(write_flash_cmd);
    printf("Read back flash ...\n");
    system_cmd(read_flash_cmd);
    fp = fopen(FLASH_RESULT_FILE_NAME, "r");
    if (fp == NULL) {
      printf("Failed to open flash result file: %s\n", FLASH_RESULT_FILE_NAME);
      return -1;
    }
    printf("Verifying flash ...\n");
    for (j = 0; j < size; ++j) {
      ch = fgetc(fp);
      if (feof(fp)) {
        printf("Read from flash result location %d failed.\n", j);
        test_passed = false;
        break;
      }
      if (ch != (flash_test_patterns[i])) {
        printf("Flash test failed at location %d of pattern 0x%02x:0x%02x\n", j,
               ch, flash_test_patterns[i]);
        test_passed = false;
        break;
      }
    }
    fclose(fp);
    if (test_passed) {
      printf("Flash passed test pattern 0x%02x\n", flash_test_patterns[i]);
    } else {
      printf("Flash failed test pattern 0x%02x\n", flash_test_patterns[i]);
      break;
    }
  }

  if (test_passed) {
    printf("Flash test passed\n");
  } else {
    printf("Flash test failed\n");
  }

  return 0;
}
