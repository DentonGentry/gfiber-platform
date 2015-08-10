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

#include <libdevmapper.h>

#include "devmap.h"

int devmap_create(const char* name, int start, int size, const char* params) {
  int ret = -1;
  struct dm_task* dmt = NULL;

  dmt = dm_task_create(DM_DEVICE_CREATE);
  if (dmt == NULL) {
    return -1;
  }

  if (!dm_task_set_name(dmt, name)) {
    goto cleanup;
  }

  if (!dm_task_add_target(dmt, start, size, "crypt", params)) {
    goto cleanup;
  }

  if (!dm_task_run(dmt)) {
    goto cleanup;
  }

  ret = 0;

cleanup:
  if (dmt != NULL) {
    dm_task_destroy(dmt);
  }

  return ret;
}

int devmap_remove(const char* name) {
  int ret = -1;
  struct dm_task* dmt = NULL;

  dmt = dm_task_create(DM_DEVICE_REMOVE);
  if (dmt == NULL) {
    return -1;
  }

  if (!dm_task_set_name(dmt, name)) {
    goto cleanup;
  }

  if (!dm_task_run(dmt)) {
    goto cleanup;
  }

  ret = 0;

cleanup:
  if (dmt != NULL) {
    dm_task_destroy(dmt);
  }

  return ret;
}

char* devmap_make_params(const char* alg, void* key, size_t key_size, const char* blk_dev) {
  int i;
  int bufsz = 1024;
  void* buf = malloc(bufsz);
  char* s = (char*) buf;
  int sz = 0;
  uint8_t* pk = (uint8_t*) key;

  sz = snprintf(s, bufsz, "%s ", alg);
  if (sz < 0 || sz >= bufsz) {
    s = NULL;
    goto cleanup;
  }
  s += sz;
  bufsz -= sz;

  for (i = 0; i < key_size; i++) {
    sz = snprintf(s, bufsz, "%.2x", pk[i]);
    if (sz < 0 || sz >= bufsz) {
      s = NULL;
      goto cleanup;
    }
    s += sz;
    bufsz -= sz;
  }

  sz = snprintf(s, bufsz, " 0 %s 0", blk_dev);
  if (sz < 0 || sz >= bufsz) {
    s = NULL;
    goto cleanup;
  }
  s += sz;
  bufsz -= sz;

cleanup:
  if (s != NULL) {
    s = strdup((char*) buf);
  }
  free(buf);
  return s;
}
