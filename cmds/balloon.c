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
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define BLOCKSIZE (8192 * 1024 - 32)  // leave room for per-block overhead

// If you run this on your desktop without a memory limit like this, you're
// gonna have a bad time.
#define MAXBLOCKS ((int)(2048LL * 1024 * 1024 / BLOCKSIZE))


void *alloc_and_fill_block(void) {
  char *block = malloc(BLOCKSIZE);
  if (block) {
    // make sure the kernel can't just use "zeroed" pages and not really
    // give us the memory.
    memset(block, 1, BLOCKSIZE);
  }
  return block;
}

void usage(char *progname) {
  printf("%s: [-e] [-m N]\n", progname);
  printf("  -e: exit after allocating memory.\n");
  printf("  -m N: allocate at most N blocks (of %d bytes each)\n", BLOCKSIZE);
  exit(1);
}

int main(int argc, char **argv) {
  int opt;
  int blocks = 0;
  int exit_when_done = 0;
  int maxblocks = MAXBLOCKS;

  while ((opt = getopt(argc, argv, "em:")) != -1) {
    switch (opt) {
      case 'e':
        exit_when_done = 1;
        break;
      case 'm':
        maxblocks = atoi(optarg);
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  for (blocks = 0; blocks < maxblocks; blocks++) {
    if (alloc_and_fill_block() == NULL) {
      break;
    }
  }
  printf("%d blocks allocated (%lld bytes)\n",
         blocks, ((long long)BLOCKSIZE) * blocks);
  if (blocks >= MAXBLOCKS) {
    printf("WARNING: maximum blocks allocated. Stopping for safety.\n");
  }

  if (exit_when_done) {
    exit(0);
  }

  // just in case some memory becomes available
  while (1) {
    if (blocks < maxblocks && alloc_and_fill_block() != NULL) {
      blocks++;
    } else {
      sleep(1);
    }
  }
}
