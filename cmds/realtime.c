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
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr,
            "Usage: %s <prio> <command line...>\n"
            "   or: %s <prio> -p <pids...>\n",
            argv[0], argv[0]);
    return 99;
  }

  int prio = atoi(argv[1]);
  if (prio < 0 || prio > 99) {
    fprintf(stderr, "%s: invalid prio %d: must be between 0 and 99\n",
            argv[0], prio);
    return 98;
  }

  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = prio;

  if (strcmp(argv[2], "-p") == 0) {
    for (int i = 3; i < argc; i++) {
      pid_t pid = atoi(argv[i]);
      if (pid <= 0) {
        fprintf(stderr, "%s: pid %d is invalid\n", argv[0], (int)pid);
      } else {
        if (sched_setscheduler(pid, prio ? SCHED_RR : SCHED_OTHER, &sp) < 0) {
          perror("sched_setscheduler");
          return 97;
        }
      }
    }
  } else {
    if (sched_setscheduler(0, prio ? SCHED_RR : SCHED_OTHER, &sp) < 0) {
      perror("sched_setscheduler");
      return 97;
    }
    if (execvp(argv[2], argv+2) != 0) {
      perror(argv[2]);
      return 96;
    }
  }
  return 95;
}
