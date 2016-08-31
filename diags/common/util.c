/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

void safe_strncpy(char *dest, const char *src, int len) {
  if ((dest == NULL) || (src == NULL)) {
    return;
  }
  strncpy(dest, src, len - 1);
  dest[len - 1] = 0;

  return;
}

void get_mask_shift(uint32_t msb, uint32_t lsb, uint32_t *mask,
                    uint32_t *shift) {
  if ((msb < lsb) || (msb >= 32)) {
    // Error case, return 0 for mask and shift
    *mask = 0;
    *shift = 0;
    // printf("Incorrect input values msb %u lsb %u. mask 0x%x shift %d\n",
    //        msb, lsb, *mask, *shift);
    return;
  }

  if (msb == 31) {
    *mask = UINT_MASK;
  } else {
    *mask = (1 << (msb + 1)) - 1;
  }
  *shift = lsb;

  *mask -= (1 << lsb) - 1;
}

int get_index(char *argv) {
  if (strcmp(argv, "all") == 0) {
    return 0;
  }

  return strtoul(argv, NULL, 0);
}

int get_text_from_file(char *text, int text_size, const char *filename) {
  FILE *file;

  if ((text == NULL) || (text_size <= 0)) {
    return -1;
  }

  file = fopen(filename, "r");
  if (!file) {
    return -1;
  }

  if (!fgets(text, text_size, file)) {
    return -1;
  }
  fclose(file);

  return 0;
}

void system_cmd(const char *cmd) {
  int rc;

  if ((rc = system(cmd)) < 0) {
    printf("ERROR: system command %s return %d\n", cmd, rc);
  }
}

unsigned int get_num(char *numstr) {
  unsigned int value;

  if (strlen(numstr) > 2) {
    if (strncmp(numstr, "0x", 2) == 0) {
      value = strtoul(numstr, NULL, 16);
      return value;
    }
  }
  value = strtoul(numstr, NULL, 10);
  return value;
}
