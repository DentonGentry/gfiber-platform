/*
 * Copyright 2015 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "common.h"

int dvb_open(int adapter, int device, const char* type, int readonly) {
  int fd;
  char s[100];
  if (snprintf(s, sizeof(s), "/dev/dvb/adapter%d/%s%d",
               adapter, type, device) < 0) {
    errno = EINVAL;
    return -1;
  }
  fd = open(s, readonly ? O_RDONLY : O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "dvb_open: %s: %s\n", strerror(errno), s);
    return -1;
  }
  return fd;
}

int64_t time_ms() {
  struct timespec tv = {0};
  if (clock_gettime(CLOCK_MONOTONIC, &tv) < 0) {
    return -1;
  }
  return tv.tv_sec * 1000LL + tv.tv_nsec / 1000000;
}

void fatal(const char* msg) {
  fprintf(stderr, "ERROR: %s\n", msg);
  exit(EXIT_FAILURE);
}
