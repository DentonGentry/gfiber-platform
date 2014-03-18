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

/*
 * A program to test/validate realtime disk performance under various
 * conditions.
 */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "ioprio.h"


#ifndef SCHED_IDLE
// not defined in glibc nor uclibc for some reason, but in Linux since 2.6.23
#define SCHED_IDLE  5
#endif

static int _posix_fallocate(int fd, __off64_t offset, __off64_t len) {
#ifdef __UCLIBC__
  return syscall(
      SYS_fallocate, fd, 0,
      __LONG_LONG_PAIR((uint32_t)(offset >> 32), (uint32_t)offset),
      __LONG_LONG_PAIR((uint32_t)(len >> 32), (uint32_t)len));
#else
  return posix_fallocate(fd, offset, len);
#endif
}


struct TaskStatus {
  int tasknum;
  volatile long long counter;
};

#define MAX_TASKS 128
static struct TaskStatus *spinners[MAX_TASKS];
static struct TaskStatus writers[MAX_TASKS];
static struct TaskStatus readers[MAX_TASKS];

#define MAX_FILE_SIZE (2*1000*1000*1000)
#define MAX_BUF (128*1024*1024)
static char *buf;

// command-line arguments
static int timeout = -1;
static int nspins = 0;
static int nwriters = 0;
static int nreaders = 0;
static int blocksize = 128*1024;
static int bytes_per_sec = 2*1024*1024;
static int keep_old_files = 0;
static int use_stagger = 0;
static int use_o_direct = 0;
static int use_mmap = 0;
static int use_fallocate = 0;
static int use_fsync = 0;
static int use_realtime_prio = 0;
static int use_ionice = 0;

#define CHECK(x) _check(#x, x)

static void _check(const char *str, int result) {
  if (!result) {
    perror(str);
    assert(result);
  }
}


// Returns the kernel monotonic timestamp in microseconds.
static long long ustime(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    perror("clock_gettime");
    exit(7); // really should never happen, so don't try to recover
  }
  return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}


static void _set_priority(int policy, int prio) {
  struct sched_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.sched_priority = prio;
  CHECK(sched_setscheduler(0, policy, &sp) == 0);
}


static long _pagesize(void) {
  static long pagesize;
  if (!pagesize) {
    pagesize = sysconf(_SC_PAGESIZE);
    fprintf(stderr, "pagesize=%ld\n", pagesize);
  }
  return pagesize;
}


// write one byte every PAGESIZE bytes inside the buffer, thus forcing the
// kernel to actually page all the accessed pages out to disk (eventually).
static void _page_out(char *buf, size_t count) {
  static long seed;
  if (!seed) {
    seed = random();
  }
  for (size_t i = 0; i < count; i += _pagesize()) {
    buf[i] = ++seed;
  }
}


// read one byte every PAGESIZE bytes inside the buffer, thus forcing the
// kernel to actually page all the accessed pages in from disk.
static volatile char page_tempbyte;
static void _page_in(char *buf, size_t count) {
  for (size_t i = 0; i < count; i += _pagesize()) {
    page_tempbyte = buf[i];
  }
}


static ssize_t _do_write(int fd, char *buf, size_t count) {
  assert(buf);
  if (use_mmap) {
    off_t oldpos = lseek(fd, 0, SEEK_CUR);
    struct stat st;
    CHECK(fstat(fd, &st) >= 0);
    if (st.st_size < oldpos + (ssize_t)count) {
      if (ftruncate(fd, oldpos + count) < 0) {
        // probably disk full
        return -1;
      }
    }
    char *wbuf;
    CHECK((wbuf = mmap(NULL, count, PROT_WRITE, MAP_SHARED, fd, oldpos))
          != MAP_FAILED);
    off_t newpos = lseek(fd, count, SEEK_CUR);
    count = newpos - oldpos;
    _page_out(wbuf, count);
    CHECK(munmap(wbuf, count) >= 0);
    return count;
  } else {
    // non-mmap version
    return write(fd, buf, count);
  }
}


static ssize_t _do_read(int fd, char **buf, size_t count) {
  assert(buf);
  if (use_mmap) {
    off_t oldpos = lseek(fd, 0, SEEK_CUR);
    if (*buf) {
      CHECK(munmap(*buf, count) >= 0);
      *buf = NULL;
    }
    CHECK((*buf = mmap(NULL, count, PROT_READ, MAP_SHARED, fd, oldpos))
          != MAP_FAILED);
    off_t newpos = lseek(fd, count, SEEK_CUR);
    count = newpos - oldpos;
    _page_in(*buf, count);
    return count;
  } else {
    // non-mmap version
    if (!*buf) {
      CHECK(posix_memalign((void **)buf, _pagesize(), blocksize) == 0);
    }
    return read(fd, *buf, count);
  }
}


static void *spinner(void *_status) {
  struct TaskStatus *status = _status;
  fprintf(stderr, "s#%d ", status->tasknum);

  // use IDLE priority so that this task *never* runs unless nobody else
  // is interested.  Thus the spinners should only count upward if there's
  // an actual available idle CPU core to run the task.
  _set_priority(SCHED_IDLE, 0);

  volatile long long *counter = &status->counter;
  while (1) {
    // Note: it is not necessarily safe to increment a counter here without
    // a lock (since it's read from other threads). However, introducing
    // locking or atomic operations would slow down the counter and possibly
    // introduce CPU barriers or cache line flushes, which defeats the main
    // purpose of this counter: counting raw, uninterrupted CPU cycles.
    // Also this number isn't critical to the operation of the program, so
    // occasional mis-reads of the counter should not be harmful and should
    // be pretty obvious (an occasional, wildly wrong reading).
    // Locking would be much more critical if we incremented a given counter
    // from more than one thread, but we never do that.
    (*counter)++;
  }
  return NULL;
}


static void *writer(void *_status) {
  struct TaskStatus *status = _status;
  fprintf(stderr, "w#%d ", status->tasknum);

  int nblocks = MAX_FILE_SIZE / blocksize;
  long long blockdelay = blocksize * 1000000LL / bytes_per_sec;

  if (use_realtime_prio) _set_priority(SCHED_FIFO, 10);
  if (use_stagger) {
    // the 0.5 is to stagger the writers in between the staggered readers,
    // in case nreaders==nwriters.
    usleep(blockdelay * (0.5 + status->tasknum) / nwriters);
  }

  long long starttime = ustime();
  for (int fileno = 0; fileno < 1000000; fileno++) {
    char filename[1024];
    sprintf(filename, "db.%d.%d.tmp", status->tasknum, fileno);
    int fd;
    mode_t mode = O_RDWR|O_CREAT;
    if (use_o_direct) mode |= O_DIRECT;
    CHECK((fd = open(filename, mode, 0666)) >= 0);
    for (int blocknum = 0; blocknum < nblocks; blocknum++) {
      if (use_fallocate) {
        struct stat st;
        CHECK(fstat(fd, &st) == 0);
        if (st.st_size <= blocknum * blocksize) {
          CHECK(_posix_fallocate(fd, 0, blocknum * blocksize + 100*1024*1024)
                == 0);
        }
      }
      CHECK(_do_write(fd, buf + blocknum * 4096, blocksize) > 0);
      if (use_fsync) fdatasync(fd);
      long long now = ustime();
      starttime += blockdelay;
      if (now > starttime) {
        // disk fell behind
        while (now > starttime) {
          status->counter++;
          starttime += blockdelay;
        }
      } else {
        // ahead of schedule, wait until next timeslot
        usleep(starttime - now);
      }
    }
    close(fd);
  }
  assert(!"created an impossible number of files");
  return NULL;
}


static int open_random_file(int mode) {
  DIR *dir;
  struct dirent dentbuf, *dent = NULL;

  while (1) {
    CHECK((dir = opendir(".")) != NULL);

    int count = 0;
    while (readdir_r(dir, &dentbuf, &dent) == 0 && dent != NULL) {
      struct stat st;
      if (stat(dent->d_name, &st) < 0) continue;
      if (st.st_size > blocksize) {
        count++;
      }
    }
    if (!count) {
      fprintf(stderr, "reader: no big files to read yet.\n");
      closedir(dir);
      sleep(1);
      continue;
    }

    int want = random() % count, cur = 0;
    rewinddir(dir);
    while (readdir_r(dir, &dentbuf, &dent) == 0 && dent != NULL) {
      struct stat st;
      if (stat(dent->d_name, &st) < 0) continue;
      if (st.st_size > blocksize) {
        if (cur == want) {
          closedir(dir);
          return open(dent->d_name, mode);
        }
        cur++;
      }
    }

    // if we get here, one of the expected files has disappeared; try again.
    closedir(dir);
  }
  // not reached
  assert(!"should never get here");
}


static void *reader(void *_status) {
  struct TaskStatus *status = _status;
  fprintf(stderr, "r#%d ", status->tasknum);

  long long blockdelay = blocksize * 1000000LL / bytes_per_sec;
  char *rbuf = NULL;

  if (use_realtime_prio) _set_priority(SCHED_FIFO, 10);
  if (use_stagger) usleep(blockdelay * status->tasknum / nreaders);

  while (1) {
    int fd;
    mode_t mode = O_RDONLY;
    if (use_o_direct) mode |= O_DIRECT;
    CHECK((fd = open_random_file(mode)) >= 0);
    struct stat st;
    CHECK(fstat(fd, &st) == 0);

    // start reading at a random offset into the file.  If there aren't too
    // many files and we have lots of parallel readers, we might otherwise
    // get two readers reading the same blocks from the same file at the
    // same time, and if the disk cache kicks in, that reduces disk load
    // unnecessarily.
    off_t start_offset = (random() % (st.st_size / 65536 + 1)) * 65536;
    lseek(fd, start_offset, SEEK_SET);

    long long starttime = ustime(), got, totalbytes = start_offset;
    // we intentionally stop reading after we reach the *original* size of
    // the file, even if the file has grown since then.  Continuing to read
    // a growing file (presumably being written by a separate writer thread)
    // seems like a good test, because that's how the system works in real
    // life.  But it turns out to be so beneficial (when not using O_DIRECT,
    // the kernel can avoid doing disk reads) that it gets in the way of our
    // benchmark.  We need to check worst-case performance (reading old files
    // while new ones are being written) not average case.
    while (totalbytes + blocksize < st.st_size &&
           (got = _do_read(fd, &rbuf, blocksize)) > 0) {
      long long now = ustime();
      totalbytes += got;
      starttime += blockdelay;
      if (now > starttime) {
        // disk fell behind
        while (now > starttime) {
          status->counter++;
          starttime += blockdelay;
        }
      } else {
        // ahead of schedule, wait until next timeslot
        usleep(starttime - now);
      }
    }
    close(fd);
  }
  return NULL;
}


static long long count_spins(void) {
  static long long last_end, last_total;
  long long total = 0;
  for (int i = 0; i < nspins; i++) {
    total += spinners[i]->counter;
  }
  long long this_end = ustime(), this_spin;
  if (last_end) {
    this_spin = (total - last_total) / (this_end - last_end);
  } else {
    this_spin = 0;
  }
  last_end = this_end;
  last_total = total;
  return this_spin;
}


static long long sum_tasks(struct TaskStatus *array, int nelems) {
  long long total = 0;
  for (int i = 0; i < nelems; i++) {
    total += array[i].counter;
  }
  return total;
}


static void usage(void) {
  fprintf(stderr,
          "\n"
          "Usage: diskbench [options]\n"
          "    -h, -?  This help message\n"
          "    -t ...  Timeout (number of seconds to run test)\n"
          "    -i ...  Number of idle spinners (to occupy CPU threads)\n"
          "    -w ...  Number of parallel writers (creating files)\n"
          "    -r ...  Number of parallel readers (reading files)\n"
          "    -b ...  Block size (kbyte size of a single read/write)\n"
          "    -s ...  Speed (kbytes read/written per sec, per stream)\n"
          "    -K      Keep old temp output files from previous run\n"
          "    -S      Stagger reads and writes evenly (default: clump them)\n"
          "    -D      Use O_DIRECT\n"
          "    -M      Use mmap()\n"
          "    -F      Use fallocate()\n"
          "    -Y      Use fdatasync() after writing\n"
          "    -R      Use CPU real-time priority\n"
          "    -I      Use ionice real-time disk priority\n");
  exit(99);
}


int main(int argc, char **argv) {
  srandom(time(NULL));

  int opt;
  while ((opt = getopt(argc, argv, "?ht:i:w:r:b:s:KSDMFYRI")) != -1) {
    switch (opt) {
    case '?':
    case 'h':
      usage();
      break;
    case 't':
      timeout = atoi(optarg);
      break;
    case 'i':
      nspins = atoi(optarg);
      break;
    case 'w':
      nwriters = atoi(optarg);
      break;
    case 'r':
      nreaders = atoi(optarg);
      break;
    case 'b':
      blocksize = atoi(optarg) * 1024;
      break;
    case 's':
      bytes_per_sec = atoi(optarg) * 1024;
      break;
    case 'K':
      keep_old_files = 1;
      break;
    case 'S':
      use_stagger = 1;
      break;
    case 'D':
      use_o_direct = 1;
      break;
    case 'M':
      use_mmap = 1;
      break;
    case 'F':
      use_fallocate = 1;
      break;
    case 'Y':
      use_fsync = 1;
      break;
    case 'R':
      use_realtime_prio = 1;
      break;
    case 'I':
      use_ionice = 1;
      break;
    }
  }

  if (nspins > MAX_TASKS || nreaders > MAX_TASKS || nwriters > MAX_TASKS) {
    fprintf(stderr, "\nfatal: idlers, readers, and writers must all be <= %d\n",
            MAX_TASKS);
    return 8;
  }

  if (!nspins && !nreaders && !nwriters) {
    fprintf(stderr, "\nfatal: must specify at least one of -i, -r, -w\n");
    return 9;
  }

  CHECK(posix_memalign((void **)&buf, _pagesize(), MAX_BUF) == 0);
  for (int i = 0; i < MAX_BUF; i++) {
    buf[i] = i % 257;
  }

  if (nwriters == 0) {
    fprintf(stderr, "not clearing old temp files (-w 0)\n");
  } else if (keep_old_files) {
    fprintf(stderr, "not clearing old temp files (-K)\n");
  } else {
    fprintf(stderr, "clearing old temp files.\n");
    CHECK(system("rm -f db.*.*.tmp") == 0);
  }

  fprintf(stderr, "syncing disks.\n");
  sync();

  fprintf(stderr, "starting: %d idlers, %d readers, %d writers\n",
          nspins, nreaders, nwriters);

  for (int i = 0; i < nspins; i++) {
    // spinners[] could just be an array of structs, instead of pointers to
    // structs, but we want to make sure the counters don't all share the
    // same cache line.  Prevening this sharing more than doubles the
    // counting rate on my x86_64 (Xeon X5650) machine, although it seems
    // to make no difference on BCM7425. I don't want an endless stream of
    // conflicting cache line flushes to artificially inflate CPU usage.
    spinners[i] = calloc(1, sizeof(struct TaskStatus));
    spinners[i]->tasknum = i + 1;
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, spinner, spinners[i]) == 0);
  }

  for (int i = 0; i < nspins; i++) {
    spinners[i]->counter = 0;
  }
  count_spins(); // initialize timings
  sleep(1);  // run for one cycle without any non-spinner activity
  long long best_spin = count_spins();
  if (!best_spin) best_spin = 1;
  fprintf(stderr, "\nidle spins:%lld\n", best_spin);

  if (use_ionice) {
    int realtime = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_RT, 0);
    CHECK(ioprio_set(IOPRIO_WHO_PROCESS, getpid(), realtime) != -1);
  }

  for (int i = 0; i < nwriters; i++) {
    memset(&writers[i], 0, sizeof(writers[i]));
    writers[i].tasknum = i;
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, writer, &writers[i]) == 0);
  }

  for (int i = 0; i < nreaders; i++) {
    memset(&readers[i], 0, sizeof(readers[i]));
    readers[i].tasknum = i;
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, reader, &readers[i]) == 0);
  }

  // that cycle was filled with startup traffic, just ignore it
  usleep(100*1000);
  count_spins();
  fprintf(stderr, "\n");

  long long count = 0;
  while (timeout == -1 || count < timeout) {
    sleep(1);
    long long this_spin = count_spins();
    if (this_spin > best_spin) best_spin = this_spin;
    printf("%5lld  spins:%lld/%lld  cpu:%.2f%%  overruns: w=%lld r=%lld\n",
           ++count,
           this_spin, best_spin,
           100 * (1-(this_spin*1.0/best_spin)),
           sum_tasks(writers, nwriters),
           sum_tasks(readers, nreaders));
    fflush(stdout);
  }
  return 0;
}
