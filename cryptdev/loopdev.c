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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fs.h>
#include <linux/loop.h>

#include "loopdev.h"

size_t blockdev_get_size(const char* name) {
  int fd;
  size_t size = 0;

  fd = open(name, O_RDWR|O_LARGEFILE);
  if (fd < 0) {
    return 0;
  }

  if (ioctl(fd, BLKGETSIZE64, &size) < 0) {
    size = 0;
  }

  close(fd);

  return size;
}

int loopdev_get_free() {
  int dev = -1;
  int fd = open("/dev/loop-control", O_RDWR);
  if (fd < 0) {
    return -1;
  }
  dev = ioctl(fd, LOOP_CTL_GET_FREE);
  close(fd);
  return dev;
}

int loopdev_set_fd(int loop_fd, int fd) {
  return ioctl(loop_fd, LOOP_SET_FD, fd);
}

int loopdev_remove(const char* name) {
  int ret;
  int fd;
  fd = open(name, O_RDWR|O_LARGEFILE);
  if (fd < 0) {
    return -1;
  }
  ret = ioctl(fd, LOOP_CLR_FD, 0);
  close(fd);
  return ret;
}

int loopdev_open(int dev) {
  char name[32];
  (void)snprintf(name, sizeof(name), "/dev/loop%d", dev);
  return open(name, O_RDWR|O_LARGEFILE);
}

int loopdev_set_name(int fd, const char* name) {
  struct loop_info64 info;
  if (ioctl(fd, LOOP_GET_STATUS64, &info) < 0) {
    return -1;
  }
  (void)strncpy((char*)info.lo_file_name, name, sizeof(info.lo_file_name));
  return ioctl(fd, LOOP_SET_STATUS64, &info);
}

int loopdev_get_number(int fd) {
  int i;
  struct stat st;
  char name[32];

  // Get device and inode numbers.
  if (fstat(fd, &st) < 0) {
    return -1;
  }

  // 256 far exceeds number of loopback devices, but loop will terminate on
  // first failed open.
  for (i = 0; i < 256; i++) {
    int ret;
    int loop_fd;
    struct loop_info64 info;
    (void)snprintf(name, sizeof(name), "/dev/loop%d", i);
    loop_fd = open(name, O_RDONLY|O_LARGEFILE);
    if (loop_fd < 0) {
      break;
    }
    ret = ioctl(loop_fd, LOOP_GET_STATUS64, &info);
    close(loop_fd);

    if (ret < 0) {
      break;
    }

    if (st.st_dev == info.lo_device && st.st_ino == info.lo_inode) {
      return i;
    }
  }

  return -1;
}
