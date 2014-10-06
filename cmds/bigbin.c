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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// assembly routine with a huge number of addiu instructions
extern unsigned int manyadds(int a0);

void usage(char *progname) {
  printf("usage: %s [-f]\n", progname);
  printf("  -f: fork a trivial child process.\n");
  exit(1);
}

int main(int argc, char** argv) {
  int i, opt;
  int do_fork = 0;

  while ((opt = getopt(argc, argv, "f")) != -1) {
    switch (opt) {
      case 'f':
        do_fork = 1;
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  for (i = 0; i < 5; ++i) {
    unsigned int tmp;
    unsigned int expected = 0x06ea0500 + i;
    if (do_fork && fork() == 0) {
      char *const args[1] = { NULL };
      execv("true", args);
    }
    tmp = manyadds(i);
    if (tmp != expected) {
      printf("manyadds() return 0x%08x != 0x%08x!\n", tmp, expected);
    }
    if (do_fork) {
      wait(NULL);
    }
  }
  exit(1);
}
