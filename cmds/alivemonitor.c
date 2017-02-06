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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#ifdef __ANDROID__
#include <libgen.h>  // For basename() on Android.
#endif
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


static volatile sig_atomic_t got_signal = 0;
const char *keepalive_file;
pid_t p_pid;
time_t old_time;


void sighandler(int sig) {
  got_signal = sig;
}

static void usage(char *name) {
  fprintf(stderr,
          "Usage: %s [-S <prekill_signal> [-T <prekill_timeout>]]\n"
          "          <keepalive_file> <first_check> <incr_checks>\n"
          "          <timeout> <command> [args...]\n"
          "    -S <prekill_signal>  try this signal (numeric) before SIGKILL\n"
          "    -T <prekill_timeout> wait time (secs) after prekill_signal\n"
          "    <keepalive_file>     name of the stamp file to monitor\n"
          "    <first_check>        time (secs) before first check\n"
          "    <incr_checks>        time (secs) before subsequent checks\n"
          "    <timeout>            time (secs) before killing process\n"
          "    <command> [args...]  the command to kill upon timeout\n"
          "\n"
          "    The keepalive logic runs in cycles. A cycle begins and ends\n"
          "    with a successful check of the <keepalive_file>, i.e., it was\n"
          "    touched since the last cycle. The first check starts\n"
          "    <first_check> secs after the cycle begins. Incremental checks\n"
          "    are done at <incr_checks> intervals, until <keepalive_file>\n"
          "    was found to be updated or <timeout> is reached. In the\n"
          "    former case, the cycle restarts, while in the latter\n"
          "    (timeout) case, the process is restarted and the cycle starts\n"
          "    again.\n\n", name);
}

long long parse_to_msec(const char *str) {
  return atof(str) * 1000;
}

// return the current (monotonic) time in secs
long long now() {
  struct timespec tp;

  if (clock_gettime(CLOCK_MONOTONIC, &tp)) {
    perror("alivemonitor: clock_gettime failed.");
    exit(1);
  }
  return tp.tv_sec * 1000 + (tp.tv_nsec / 1000000);
}

enum Aliveness {
  EXITED = 2,
  NO_CHANGE = 1,
  ALIVE = 0,
  ERROR = -1,
};

// Sleep a given amount of time, while continuously checking on the parent.
// Return codes:
//  EXITED: parent exited
//  NO_CHANGE: no change in alive status
//  ALIVE: alive!
//  ERROR: system error, abort
enum Aliveness sleep_check_alive(long long stime) {
  struct stat fst;
  long long n = now(), endtime = n + stime;

  while (n < endtime) {
    int s = endtime - n;
    usleep(s * 1000);

    if (got_signal)
      break;

    // check on the parent
    assert(p_pid > 0);
    if (kill(p_pid, 0) == -1) {
      if (errno == ESRCH) {
        fprintf(stderr, "alivemonitor: parent pid %d exited.\n", p_pid);
        return EXITED;
      } else {
        perror("alivemonitor: kill(p_pid, 0) failed");
        return ERROR;
      }
    }
    n = now();
  }

  memset(&fst, 0, sizeof(fst));
  if (stat(keepalive_file, &fst) != 0) {
    perror("alivemonitor: stat failed");
    return ERROR;
  }

  if (fst.st_mtime == old_time) {
    return NO_CHANGE;
  }

  // alive!
  old_time = fst.st_mtime;
  return ALIVE;
}

void die(const char *argv0, const char *msg) {
  fprintf(stderr, "%s: %s\n", argv0, msg);
  exit(99);
}

int main(int argc, char *const *argv) {
  int fd;
  long long timeout, first_check, incr_check, next_check;
  struct stat fst;
  mode_t old_mask;
  pid_t pid;
  long long start_time;
  char *keepalive_name;
  int prekill_signal = 0;
  long long prekill_timeout = 1000;

  if (argc < 6) {
    usage(basename(argv[0]));
    return 99;
  }

  signal(SIGTERM, sighandler);
  signal(SIGHUP, sighandler);

  // GNU getopt() will helpfully try to grab options from the [args...]
  // section unless we set this.  We want those options to be set aside
  // for the subprogram, not for us.
  int opt;
  while ((opt = getopt(argc, argv, "+?S:T:")) > 0) {
    switch (opt) {
    case 'S':
      prekill_signal = atoi(optarg);
      if (prekill_signal <= 0) die(argv[0], "invalid signal number provided");
      break;
    case 'T':
      prekill_timeout = parse_to_msec(optarg);
      if (prekill_timeout <= 0) die(argv[0], "prekill timeout must be > 0");
      break;
    case '?':
      usage(basename(argv[0]));
      return 99;
    }
  }

  // <keepalive_file> <first_check> <incr_checks> <timeout> <command>
  keepalive_file = argv[optind];
  keepalive_name = basename(keepalive_file);
  first_check = parse_to_msec(argv[optind + 1]);
  incr_check = parse_to_msec(argv[optind + 2]);
  timeout = parse_to_msec(argv[optind + 3]);

  if (first_check <= 0) die(argv[0], "first_check must be > 0");
  if (incr_check <= 0) die(argv[0], "incr_check must be > 0");
  if (timeout <= 0) die(argv[0], "timeout must be > 0");
  if (first_check > timeout) die(argv[0], "first_check must be <= timeout");

  // create the keepalive file if it doesn't already exist
  memset(&fst, 0, sizeof(fst));
  if (stat(keepalive_file, &fst) != 0) {
    old_mask = umask(0000);
    fd = creat(keepalive_file, 0666);
    if (fd < 0) {
      perror("alivemonitor: creat failed");
      return 99;
    }
    // Revert the umask to default so that the child doesn't
    // inherit the changed value.
    umask(old_mask);
    close(fd);
  }
  old_time = fst.st_mtime;

  fprintf(stderr, "alivemonitor: Start monitoring '%s' with timeout=%lldms, "
          "first_check=%lldms, incr_check=%lldms\n",
          keepalive_file, timeout, first_check, incr_check);

  // create a new process group with pgid=pid
  if (setpgid(0, 0)) {
    perror("alivemonitor: setpgid failed");
    return 99;
  }

  // spawn the child process
  p_pid = getpid();
  pid = fork();
  if (pid == -1) {
    perror("alivemonitor: fork failed");
    return 99;
  } else if (pid > 0) { // parent
    execvp(argv[optind + 4], argv + optind + 4);
    perror("alivemonitor: execv failed");
    return 99;
  }

  // from here: child

  while (1) {
    start_time = now();

    // sleep until first check
    switch (sleep_check_alive(first_check)) {
    case EXITED: return 0;
    case ERROR: goto kill_it;
    case ALIVE: goto not_dead;
    case NO_CHANGE: break;  // fall through and enter the inner loop
    }

    // no sign of life yet, run the increments
    long long time_passed = now() - start_time;
    int cnt = 1;
    while (1) {
      if (got_signal) {
        fprintf(stderr, "alivemonitor(%s): signal %d received, killing.\n",
                keepalive_name, got_signal);
        goto kill_it;
      } else if (time_passed >= timeout) {
        fprintf(stderr, "alivemonitor(%s): Timeout!\n", keepalive_name);
        goto kill_it;
      }
      fprintf(stderr, "alivemonitor(%s): %d-No sign of life @ %lld/%lld ms\n",
              keepalive_name, cnt++, time_passed, timeout);
      next_check = timeout - time_passed;
      if (incr_check < next_check) next_check = incr_check;
      switch (sleep_check_alive(next_check)) {
      case EXITED: return 0;
      case ERROR: goto kill_it;
      case NO_CHANGE: break;  // do nothing
      case ALIVE:
        fprintf(stderr, "alivemonitor(%s): it's alive after all!\n",
                keepalive_name);
        goto not_dead;
      }
      time_passed = now() - start_time;
    }
not_dead:
    continue;
  }

kill_it:
  fprintf(stderr, "alivemonitor(%s): kill parent process group %d\n",
          keepalive_name, p_pid);
  assert(p_pid > 0);
  if (prekill_signal) {
    // Send prekill signal only to the parent process (which might kill the
    // rest of its group politely)
    long long prekill_start = now();
    if (kill(p_pid, prekill_signal)) {
      if (errno != ESRCH) perror("alivemonitor: prekill failed");
    } else {
      do {
        if (kill(p_pid, 0)) {
          if (errno != ESRCH) perror("alivemonitor: prekill(0) failed");
          break;
        }
        usleep(100*1000);
      } while (now() - prekill_start < prekill_timeout);
    }
  }

  // Send kill signal to whole process group.
  if (kill(-p_pid, SIGKILL))
    perror("alivemonitor: killing parent process group failed");

  // NOTE: Code after this point will not run since we just killed ourselves

  return 98;
}
