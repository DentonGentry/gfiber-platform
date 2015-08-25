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
#ifndef _LOOPDEV_H_
#define _LOOPDEV_H_

#include <stddef.h>

// blockdev_get_size returns the size of block device in bytes.
int64_t blockdev_get_size(const char* name);

// loopdev_open opens a loopback device.
// Returns -1 on error.
int loopdev_open(int dev);

// loopdev_get_free finds first available loopback device.
// Returns -1 on error.
int loopdev_get_free();

// loopdev_get_number returns the loop device associated with fd.
// Returns -1 on error.
int loopdev_get_number(int fd);

// loopdev_set_fd associates a file with a loopback device.
// Returns -1 on error.
int loopdev_set_fd(int loop_fd, int fd);

// loopdev_set_name sets file name for losetup command.
// Returns -1 on error.
int loopdev_set_name(int fd, const char* name);

// loopdev_remove detaches a file from a loopback device.
// Returns -1 on error.
int loopdev_remove(const char* name);

#endif /* _LOOPDEV_H_ */
