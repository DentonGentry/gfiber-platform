/*
 * Copyright 2016 Google Inc. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static uint64_t monotime(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    perror("clock_gettime(CLOCK_MONOTONIC)");
    exit(1);
  } else {
    return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
  }
}


/* Return 1 if at least one second has passed since the
 * last successful call to pacing(). */
int pacing() {
  static uint64_t last = 0;
  uint64_t now = monotime();
  int rc = 0;

  if ((now - last) > 1000000) {
    last = now;
    rc = 1;
  }

  return rc;
}


int get_socket_or_die()
{
  int s;

  if ((s = socket(AF_UNIX, SOCK_NONBLOCK | SOCK_DGRAM, 0)) < 0) {
    perror("socket(AF_UNIX)");
    exit(1);
  }

  return s;
}
