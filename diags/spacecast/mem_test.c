/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "mem_test.h"
#include "common.h"
#include "../common/util.h"

static const char *kMemTestUseMemtesterOption = "-l";
static const char *kMemTestUseMemtesterString = "memtester %d 1";

unsigned int mem_test_patterns[] = {0xA5A5A5A5, 0x5A5A5A5A, 0xAAAAAAAA,
                                    0x55555555, 0x00000000, 0xFFFFFFFF};

static int mem_test_usage() {
  printf("mem_test <num of blocks (%d bytes)> [%s]\n", MIN_MEM_TEST_SIZE,
         kMemTestUseMemtesterOption);
  printf("Example:\n");
  printf("mem_test 10\n");
  printf("test 10*(min_test_size) bytes\n");
  printf("NOTE: negative number means test max allocatable\n");
  printf("      %s option to run memtester\n", kMemTestUseMemtesterOption);

  return -1;
}

int mem_test(int argc, char *argv[]) {
  int num, max;
  volatile unsigned int *tmp;
  unsigned int *mem, *first_failed = NULL, failed_pattern = 0xDEADBEEF;
  unsigned int err_cnt = 0, size = MIN_MEM_TEST_SIZE, i, j;
  bool use_memtester = false;
  char cmd[MEM_TESTER_CMD_LEN];

  if (argc == 3) {
    if (strcmp(kMemTestUseMemtesterOption, argv[2]) == 0) {
      use_memtester = true;
    } else {
      return mem_test_usage();
    }
  } else if (argc != 2) {
    printf("%s invalid params\n", FAIL_TEXT);
    return mem_test_usage();
  }

  num = strtoul(argv[1], NULL, 0);

  if (num == 0) {
    printf("Test of %d memory sector done\n", num);
    return 0;
  }

  if (num > 0) {
    size = num * MIN_MEM_TEST_SIZE;
  } else {
    printf("Finding maximum available memory\n");
    max = 1;
    num = 0;
    // Max 1G mem
    while (num++ < ((1024 * 1024 * 1024) / MIN_MEM_TEST_SIZE)) {
      mem = malloc(max * MIN_MEM_TEST_SIZE);
      if (mem != NULL) {
        free(mem);
        ++max;
      } else {
        if ((max - MEM_TEST_LEFT_IN_M) > 0) {
          num = max - MEM_TEST_LEFT_IN_M;
        } else {
          num = 0;
        }
        size = num * MIN_MEM_TEST_SIZE;
        // Test a long word at a time (4 bytes)
        size &= ~0x3;
        printf("Found max free memory size %d bytes, left %d bytes\n", size,
               MEM_TEST_LEFT_IN_M * MIN_MEM_TEST_SIZE);
        break;
      }
    }
  }

  if (use_memtester) {
    sprintf(cmd, kMemTestUseMemtesterString, num);
    system_cmd(cmd);
    return 0;
  }

  mem = (unsigned int *)malloc(size);
  if (mem == NULL) {
    printf("%s There is not enough memory of size %u to be tested\n", FAIL_TEXT,
           size);
    return -1;
  }

  for (i = 0; i < (sizeof(mem_test_patterns) / (sizeof(unsigned int))); ++i) {
    for (j = 0; j < size / (sizeof(unsigned int)); ++j) {
      tmp = mem + j;
      *tmp = mem_test_patterns[i];
    }
    printf("Written %p to %p of pattern 0x%08x\n", mem,
           (mem + (size / (sizeof(unsigned int))) - 1), mem_test_patterns[i]);
    for (j = 0; j < size / (sizeof(unsigned int)); ++j) {
      tmp = mem + j;
      if (*tmp != mem_test_patterns[i]) {
        ++err_cnt;
        if (first_failed == NULL) {
          first_failed = (unsigned int *)tmp;
          failed_pattern = mem_test_patterns[i];
        }
      }
    }
    printf("Verified %p to %p of pattern 0x%08x\n", mem,
           (mem + (size / (sizeof(unsigned int))) - 1), mem_test_patterns[i]);
  }

  if (err_cnt == 0) {
    printf("Tested memory %p to %p passed\n", mem,
           (mem + (size / (sizeof(unsigned int))) - 1));
  } else {
    printf("%s Tested memory %p to %p failed\n", FAIL_TEXT, mem,
           (mem + (size / (sizeof(unsigned int))) - 1));
    printf("  Error count %d, first failed addr %p pattern 0x%08x\n", err_cnt,
           first_failed, failed_pattern);
  }

  free(mem);
  mem = NULL;
  return 0;
}
