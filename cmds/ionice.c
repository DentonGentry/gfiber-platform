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
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <getopt.h>
#include "ioprio.h"


static void usage(char *name, int exitcode) {
  fprintf(stderr,
          "\n%s - sets or gets process io scheduling class and priority.\n"
          "\nUsage:\n"
          "  %s -p PID\n"
          "  %s [-c class] [-n prio] -p PID\n"
          "  %s [-c class] [-n prio] COMMAND [ARG]\n", name, name, name, name);
  fprintf(stderr, "\nOptions:\n"
          "  -c <class>    scheduling class\n"
          "                  0: none, 1: realtime, 2: best-effort, 3: idle\n"
          "  -n <prio>     priority level\n"
          "                  0 (highest) to 7 (lowest)\n"
          "  -p <pid>      PID of existing process to view or modify\n"
          "  -h            this help\n\n"
          );
  exit(exitcode);
}

static char *clsNames[] = {"none", "real-time", "best-effort", "idle"};

int main(int argc, char **argv) {
  int   value, prio = 4, cls = IOPRIO_CLASS_BE;
  int   opt, set = 0;
  pid_t pid = 0;
  char *name = basename(argv[0]);

  while ((opt = getopt(argc, argv, "+c:n:p:h")) != -1) {
    switch (opt) {
    case 'c':
      cls = strtol(optarg, NULL, 10);
      set = 1;
      break;
    case 'n':
      prio = strtol(optarg, NULL, 10);
      set = 1;
      break;
    case 'p':
      pid = strtol(optarg, NULL, 10);
      break;
    case 'h':
      usage(name, 0);
    default:
      usage(name, 100);
    }
  }

  if (!set) {
    if (!pid) {
      usage(name, 101);
    }
    value = ioprio_get(IOPRIO_WHO_PROCESS, pid);
    if (value < 0) {
      perror("ioprio_get");
      return 102;
    }
    prio = IOPRIO_PRIO_DATA(value);
    cls  = IOPRIO_PRIO_CLASS(value);
    if (cls < IOPRIO_CLASS_NONE || cls > IOPRIO_CLASS_IDLE) {
      fprintf(stderr, "Invalid class value (%d) returned", cls);
      return 103;
    }
    if (cls == IOPRIO_CLASS_NONE) {
      fprintf(stdout, "%s\n", clsNames[cls]);
    } else {
      fprintf(stdout, "%s: prio %d\n", clsNames[cls], prio);
    }
  } else {
    value = IOPRIO_PRIO_VALUE(cls, prio);
    if (pid) {
      if (ioprio_set(IOPRIO_WHO_PROCESS, pid, value) == -1) {
        perror("ioprio_set");
        return 104;
      }
    } else if (argv[optind]) {
      if (ioprio_set(IOPRIO_WHO_PROCESS, getpid(), value) == -1) {
        perror("ioprio_set");
        return 105;
      }
      if (execvp(argv[optind], &argv[optind]) != 0) {
        perror("execvp");
        return 106;
      }
    }
  }
  return 0;
}
