/*
 * Copyright 2012-2014 Google Inc. All rights reserved.
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

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define GPIO_INPUT_REG_ADDR 0xf1018110
#define TRUE 1
#define FALSE 0

/* GFLT110: The reset button is connected to MPP18 */
#define RESET_BIT_GFLT110 18
#define RESET_BIT_MASK_GFLT110 (1 << RESET_BIT_GFLT110)

/* GFLT300: The reset button is connected to MPP17 */
#define RESET_BIT_GFLT300 17
#define RESET_BIT_MASK_GFLT300 (1 << RESET_BIT_GFLT300)

typedef enum {
  PlatType_GFLT110 = 1,
  PlatType_GFLT300,
  PlatType_Unknown
} PlatType;
PlatType plat_type = PlatType_Unknown;

int IsSupportedPlatform() {
  int bytes_read;
  char buf[64];
  memset(buf, 0, sizeof(buf));
  FILE* f = fopen("/proc/board_type", "r");
  if (!f) {
    printf("Failed to open /proc/board_type\n");
    return FALSE;
  }
  bytes_read = fread(&buf[0], 1, sizeof(buf)-1, f);
  fclose(f);
  if (bytes_read <= 0) {
    printf("fread of /proc/board_type returned 0 data.\n");
    return FALSE;
  }

  if (strncmp(buf, "GFLT110", strlen("GFLT110")) == 0) {
    plat_type = PlatType_GFLT110;
  } else if (strncmp(buf, "GFLT300", strlen("GFLT300")) == 0) {
    plat_type = PlatType_GFLT300;
  } else {
    /* This platform is not supported. */
    return FALSE;
  }
  return TRUE;
}


int64_t GetTick() {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    printf("clock_gettime failed... exiting.\n");
    exit(1);
  }
  return ((int64_t)(ts.tv_sec) * 1000LL) + (ts.tv_nsec / 1000000);
}


void MonitorReset() {
  uint32_t page_size = sysconf(_SC_PAGESIZE);
  uint32_t page_mask = page_size - 1;
  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd == -1) {
    printf("Failed to open /dev/mem\n");
    exit(1);
  }

  uint32_t* base = (uint32_t*)mmap(
      0, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
      GPIO_INPUT_REG_ADDR & ~page_mask);
  if (base == (uint32_t*)-1) {
    printf("Failed to mmap /dev/mem\n");
    exit(1);
  }

  volatile uint32_t* reg_addr = base + (
      (GPIO_INPUT_REG_ADDR & page_mask) / sizeof(*base));
  int button_down = FALSE;
  int button_down_sent = -1;
  uint64_t button_down_start_tick = 0;
  for(;;) {
    int button_down_now;
    if (plat_type == PlatType_GFLT110)
      button_down_now = (*reg_addr & RESET_BIT_MASK_GFLT110) == 0;
    else if (plat_type == PlatType_GFLT300)
      button_down_now = (*reg_addr & RESET_BIT_MASK_GFLT300) == 0;
    else
      button_down_now = FALSE;

    if (!button_down && button_down_now) {
      // Handle button down toggle.
      button_down_start_tick = GetTick();
      button_down = TRUE;
      button_down_sent = -1;
    } else if (button_down) {
      uint64_t dt = GetTick() - button_down_start_tick;
      int sec = dt / 1000;

      // send a message that can be used to do something like flicker
      // the led.
      if (sec > button_down_sent) {
        printf("buttondown %d\n", sec);
        button_down_sent = sec;
      }

      // send click on release
      if (!button_down_now) {
        printf("click %d\n", sec);
        button_down_start_tick = 0;
        button_down = FALSE;
      }
    }
    button_down = button_down_now;
    usleep(1000*100);  // sleep 100ms
  }
}


int main() {
  if (!IsSupportedPlatform()) {
    printf("resetmonitor only works on GFLT platforms.\n");
    return 1;
  }
  setlinebuf(stdout);
  MonitorReset();
  return 0;
}

