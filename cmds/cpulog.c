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

#include <fcntl.h>
#include <getopt.h>
#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#define DEFAULT_READ_INTERVAL 60
#define DEFAULT_PROCS_TO_SAMPLE 5
#define DEFAULT_WARMUP_SECONDS 600

#define DEFAULT_PROC_GLOB_PATH "/proc/[0-9]*/stat"

#define CMD_LEN 15
#define STR(X) #X
#define XSTR(X) STR(X)

struct proc {
  pid_t pid;
  char cmd[CMD_LEN + 1];
  unsigned long msec;
};

struct proc_list {
  struct proc *procs;
  int count;
};

static long ticks_per_sec;

void die(const char *msg)
{
  fprintf(stderr, "error: %s\n", msg);
  exit(1);
}

unsigned long ticks_to_ms(unsigned long ticks)
{
  return (1000.0 / ticks_per_sec) * ticks;
}

void read_stat(struct proc *proc, const char *stat_path)
{
  char buf[256];
  int pid_int;
  unsigned long utime;
  unsigned long stime;
  int rc;
  int fd;

  fd = open(stat_path, O_RDONLY);
  /* This isn't an error. We have a list of files which existed the moment the
     glob was run. If a process exits before we get around to reading it, its
     /proc files will go away. */
  if (fd == -1)
    return;

  rc = read(fd, buf, sizeof(buf) - 1);
  if (rc < 1)
    die("read");
  close(fd);
  buf[rc] = '\0';

  rc = sscanf(buf,
      "%d (%" XSTR(CMD_LEN) "[^)]) %*c %*d %*d %*d %*d %*d %*u %*u "
      "%*u %*u %*u %lu %lu", &pid_int, proc->cmd, &utime, &stime);
  if (rc != 4)
    die("sscanf");

  proc->pid = pid_int;
  proc->msec = ticks_to_ms(utime + stime);
}

int proc_pid_cmp(const void *a, const void *b)
{
  const struct proc *_a = a;
  const struct proc *_b = b;
  return _a->pid - _b->pid;
}

int proc_msec_cmp(const void *a, const void *b)
{
  const struct proc *_a = a;
  const struct proc *_b = b;
  return _a->msec - _b->msec;
}

void print_top(struct proc_list *new_procs, struct proc_list *old_procs,
    int procs_to_sample, int interval)
{
  struct proc top[new_procs->count];
  int top_count;
  int i;

  top_count = 0;
  for (i = 0; i < new_procs->count; i++) {
    struct proc *new_proc = &new_procs->procs[i];
    struct proc *old_proc = bsearch(new_proc, old_procs->procs, old_procs->count,
        sizeof(*old_procs->procs), proc_pid_cmp);
    if (old_proc) {
      top[top_count] = *new_proc;
      top[top_count].msec = new_proc->msec - old_proc->msec;
      top_count++;
    }
  }

  qsort(top, top_count, sizeof(*top), proc_msec_cmp);

  printf("%dsec:", interval);
  for (i = 0; i < MIN(procs_to_sample, top_count); i++) {
    printf(" %s(%.3f)", top[i].cmd, top[i].msec / 1000.0);
  }
  printf("\n");
}

void read_procs(struct proc_list *new_procs, const char *proc_glob_path)
{
  glob_t pglob;
  unsigned i;
  int rc;

  rc = glob(proc_glob_path, GLOB_NOSORT, NULL, &pglob);
  if (rc != 0)
    die("glob");

  new_procs->count = pglob.gl_pathc;
  new_procs->procs = malloc(new_procs->count * sizeof(*new_procs->procs));
  if (!new_procs->procs)
    die("out of memory");

  for (i = 0; i < pglob.gl_pathc; i++) {
    read_stat(&new_procs->procs[i], pglob.gl_pathv[i]);
  }

  globfree(&pglob);

  /* glob sorts alphanumerically, so we would need to use an alphanumeric
     comparison to find elements using bsearch. here we sort numerically by pid*/
  qsort(new_procs->procs, new_procs->count, sizeof(*new_procs->procs), proc_pid_cmp);
}

void free_procs(struct proc_list *p)
{
  free(p->procs);
}

void usage_and_die(const char *argv0)
{
  fprintf(stderr, "Usage: %s [options]\n"
      "\n"
      "      -i, --interval=<interval>  sampling interval in seconds (%d)\n"
      "      -n, --num=<num>            number of processes to sample (%d)\n"
      "      -o, --oneshot              one-shot mode, do not loop\n"
      "      -p, --path=<path>          path for process stat files (%s)\n"
      "      -w, --warmup=<warmup>      seconds to wait before sampling begins (%d)\n",
      argv0, DEFAULT_READ_INTERVAL, DEFAULT_PROCS_TO_SAMPLE,
      DEFAULT_PROC_GLOB_PATH, DEFAULT_WARMUP_SECONDS);
  exit(1);
}

int main(int argc, char **argv)
{
  struct proc_list old_procs = {0, 0};
  struct proc_list new_procs = {0, 0};
  int read_interval = DEFAULT_READ_INTERVAL;
  int procs_to_sample = DEFAULT_PROCS_TO_SAMPLE;
  int warmup_seconds = DEFAULT_WARMUP_SECONDS;

  int one_shot_mode = false;
  const char *proc_glob_path = DEFAULT_PROC_GLOB_PATH;

  struct option long_options[] = {
    {"interval", required_argument, 0, 'i'},
    {"num",      required_argument, 0, 'n'},
    {"oneshot",  no_argument,       0, 'o'},
    {"path",     required_argument, 0, 'p'},
    {"warmup",   required_argument, 0, 'w'},
    {0,          0,                 0, 0},
  };

  int c;
  while ((c = getopt_long(argc, argv, "i:n:w:p:o", long_options, NULL)) != -1) {
    switch (c) {
    case 'i':
      read_interval = atoi(optarg);
      if (read_interval < 1)
        die("invalid argument");
      break;
    case 'n':
      procs_to_sample = atoi(optarg);
      if (procs_to_sample < 1)
        die("invalid argument");
      break;
    case 'o':
      one_shot_mode = true;
      break;
    case 'p':
      proc_glob_path = optarg;
      break;
    case 'w':
      warmup_seconds = atoi(optarg);
      if (warmup_seconds < 0)
        die("invalid argument");
      break;
    default:
      usage_and_die(argv[0]);
    }
  }

  if (optind < argc)
    usage_and_die(argv[0]);

  setlinebuf(stdout);

  ticks_per_sec = sysconf(_SC_CLK_TCK);

  sleep(warmup_seconds);

  read_procs(&old_procs, proc_glob_path);
  for (;;) {
    sleep(read_interval);
    read_procs(&new_procs, proc_glob_path);
    print_top(&new_procs, &old_procs, procs_to_sample, read_interval);
    free_procs(&old_procs);
    old_procs = new_procs;

    if (one_shot_mode)
      exit(0);
  }
}
