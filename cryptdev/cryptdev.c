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

#include "tpm.h"
#include "keyfile.h"
#include "loopdev.h"
#include "devmap.h"

#define CRYPTO_ALGO "aes-cbc-essiv:sha256"

static void usage(const char* prog) {
  fprintf(stderr, "Usage: %s [options] device_name image_file\n", prog);
  fprintf(stderr, "    -a algo     Encryption algorithm\n");
  fprintf(stderr, "    -k file     Key file\n");
  fprintf(stderr, "    -s start    Image offset (blocks)\n");
  fprintf(stderr, "    -h          Help\n");
  exit(EXIT_FAILURE);
}

static int setup_loopdev(const char* img_file, int* created) {
  int ret = -1;
  int img_fd = -1;
  int loop_fd = -1;
  int loop_no = -1;

  img_fd = open(img_file, O_RDWR|O_LARGEFILE);
  if (img_fd < 0) {
    fprintf(stderr, "ERROR: open(\"%s\") failed\n", img_file);
    goto cleanup;
  }

  // Returns device number if fd is already associated.
  loop_no = loopdev_get_number(img_fd);
  if (loop_no > -1) {
    ret = loop_no;
    if (created) {
      *created = 0;
    }
    goto cleanup;
  }

  loop_no = loopdev_get_free();
  loop_fd = loopdev_open(loop_no);
  if (loop_fd < 0) {
    goto cleanup;
  }

  if (loopdev_set_fd(loop_fd, img_fd) < 0) {
    goto cleanup;
  }

  if (loopdev_set_name(loop_fd, img_file) < 0) {
    goto cleanup;
  }

  ret = loop_no;
  if (created) {
    *created = 1;
  }

cleanup:
  if (loop_fd > -1) {
    close(loop_fd);
  }

  if (img_fd > -1) {
    close(img_fd);
  }

  return ret;
}

static int setup_key(const char* key_file, void* key, size_t key_size) {
  if (read_key_file(key_file, key, key_size) > -1) {
    return 0;
  }

  if (tpm_read_random(key, key_size) < 0) {
    return -1;
  }

  if (write_key_file(key_file, key, key_size) < 0) {
    return -1;
  }

  return 0;
}

static int setup_cryptdev(const char* target_dev, size_t start,
                          const char* algo, void* key, size_t key_size,
                          const char* source_dev) {
  int ret = 0;
  char* params = NULL;
  size_t blk_size = 0;

  // Use 512 byte block size.
  blk_size = blockdev_get_size(source_dev) / 512;
  if (blk_size == 0) {
    fprintf(stderr, "ERROR: unable to get device size!\n");
    return -1;
  }

  params = devmap_make_params(algo, key, key_size, source_dev);
  if (params == NULL) {
    return -1;
  }

  ret = devmap_create(target_dev, start, blk_size, params);
  free(params);

  return ret;
}

int main(int argc, char** argv) {
  int opt;
  char* kp = NULL;
  const char* key_file = NULL;
  const char* img_file = NULL;
  const char* dev_name = NULL;
  const char* algo = CRYPTO_ALGO;
  int status = EXIT_FAILURE;

  size_t start = 0;
  size_t key_size = 16;
  char key[key_size];
  int loop_no = 0;
  int new_loop_dev = 0;
  char loop_name[32];

  while ((opt = getopt(argc, argv, "a:hk:s:")) != -1) {
    switch (opt) {
      case 'a':
        algo = optarg;
        break;
      case 'k':
        key_file = optarg;
        break;
      case 's':
        start = atoi(optarg);
        break;
      default:
        usage(argv[0]);
    }
  }

  if (optind < argc) {
    dev_name = argv[optind];
    optind++;
  }

  if (optind < argc) {
    img_file = argv[optind];
    optind++;
  }

  if (dev_name == NULL) {
    fprintf(stderr, "ERROR: device name required!\n");
    usage(argv[0]);
  }

  if (img_file == NULL) {
    fprintf(stderr, "ERROR: image file required!\n");
    usage(argv[0]);
  }

  if (key_file == NULL) {
    int len = strlen(img_file) + 5;
    kp = (char*) malloc(len);
    (void)snprintf(kp, len, "%s.key", img_file);
    key_file = kp;
  }

  loop_no = setup_loopdev(img_file, &new_loop_dev);
  if (loop_no < 0) {
    fprintf(stderr, "ERROR: unable to setup loopback device!\n");
    goto cleanup;
  }

  (void)snprintf(loop_name, sizeof(loop_name), "/dev/loop%d", loop_no);

  if (setup_key(key_file, key, key_size) < 0) {
    fprintf(stderr, "ERROR: unable to setup encryption key!\n");
    goto cleanup;
  }

  if (setup_cryptdev(dev_name, start, algo, key, key_size, loop_name) < 0) {
    fprintf(stderr, "ERROR: unable to setup crypt device!\n");
    goto cleanup;
  }

  status = EXIT_SUCCESS;

cleanup:
  if (kp != NULL) {
    free(kp);
  }

  if (status != EXIT_SUCCESS && new_loop_dev) {
    loopdev_remove(loop_name);
  }

  return status;
}
