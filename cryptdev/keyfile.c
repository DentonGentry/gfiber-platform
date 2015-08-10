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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "tpm.h"
#include "keyfile.h"

// read_file reads an entire file into an allocated buffer. Caller must free
// returned buffer.
static ssize_t read_file(const char* name, void** buf_out) {
  int fd;
  int bytes;
  off_t size;
  void* buf = NULL;
  ssize_t ret = -1;

  if (name == NULL || buf_out == NULL) {
    return -1;
  }

  fd = open(name, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  size = lseek(fd, 0, SEEK_END);
  if (size == -1) {
    goto cleanup;
  }

  if (lseek(fd, 0, SEEK_SET) != 0) {
    goto cleanup;
  }

  buf = malloc(size);
  if (buf == NULL) {
    goto cleanup;
  }

  for (bytes = 0; bytes < size; ) {
    int r = read(fd, buf + bytes, size - bytes);
    if (r < 0) {
      free(buf);
      goto cleanup;
    }
    bytes += r;
  }

  *buf_out = buf;
  ret = bytes;

cleanup:
  close(fd);
  return ret;
}

static ssize_t write_file(const char* name, void* buf, size_t count) {
  int fd;
  int bytes;
  ssize_t ret = -1;

  if (name == NULL || buf == NULL) {
    return -1;
  }

  fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0600);
  if (fd < 0) {
    return -1;
  }

  for (bytes = 0; bytes < count; ) {
    int w = write(fd, buf + bytes, count - bytes);
    if (w < 0) {
      goto cleanup;
    }
    bytes += w;
  }

  fsync(fd);
  ret = 0;

cleanup:
  close(fd);
  return ret;
}

int read_key_file(const char* name, void* key, size_t key_size) {
  int ret = -1;
  ssize_t ciphertext_size = 0;
  void* ciphertext = NULL;
  size_t plaintext_size = 0;
  void* plaintext = NULL;
  tpm_handle_t handle;

  ciphertext_size = read_file(name, &ciphertext);
  if (ciphertext_size <= 0) {
    return -1;
  }

  handle = tpm_open();
  if (!handle) {
    free(ciphertext);
    return -1;
  }

  if (tpm_decrypt(handle, ciphertext, ciphertext_size, &plaintext, &plaintext_size) < 0) {
    goto cleanup;
  }

  if (key_size != plaintext_size) {
    goto cleanup;
  }

  memcpy(key, plaintext, plaintext_size);
  ret = 0;

cleanup:
  tpm_close(handle);
  free(ciphertext);
  return ret;
}

int write_key_file(const char* name, void* key, size_t key_size) {
  int ret = -1;
  size_t ciphertext_size = 0;
  void* ciphertext = NULL;
  tpm_handle_t handle;

  handle = tpm_open();
  if (!handle) {
    return -1;
  }

  if (tpm_encrypt(handle, key, key_size, &ciphertext, &ciphertext_size) < 0) {
    goto cleanup;
  }

  if (write_file(name, ciphertext, ciphertext_size) < 0) {
    goto cleanup;
  }

  ret = 0;

cleanup:
  tpm_close(handle);
  return ret;
}
