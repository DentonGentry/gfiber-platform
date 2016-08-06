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
 * A program that reads log messages from stdin, processes them, and writes
 * them to /dev/kmsg (usually) or stdout (if LOGOS_DEBUG=1).
 *
 * Features:
 *  - limits the number of log message bytes per second.
 *  - writes only entire lines at a time in a single syscall, to keep the
 *    kernel from overlapping messages from other threads/instances.
 *  - cleans up control characters (ie. chars < 32).
 *  - makes sure output lines are in "facility: message" format.
 *  - doesn't rely on syslogd.
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#ifndef COMPILE_FOR_HOST
#include <stacktrace.h>
#endif  // COMPILE_FOR_HOST

#include "utils.h"


// Total size of kernel log buffer.
// We use CONFIG_PRINTK_PERSIST in the kernel to keep our log buffer across
// reboots, then configure the kernel buffer to be extra large, then dump
// *both* kernel and userspace messages into it.  This gives us a clearly
// timestamped log of all events across the whole system.
// The kernel log buffer size is actually set by the log_buf_len kernel
// parameter; if you change it to be <= BURST_LOG_SIZE, please change it
// here too.
#define BURST_LOG_SIZE     (10*1000LL*1000LL)

// Maximum bytes to log per day.
// This limit reflects our server-side quota (and is also enforced server
// side).  We need to know it client-side in order to calculate the right
// default bucket size so we never run into the server-side quota
// unexpectedly.
#define DAILY_LOG_SIZE     (100*1000LL*1000LL)

// Amount of time between system-wide log uploads.
// (The system might actually upload more of than this, which is harmless.
// If it uploads less often, we risk an overflow, because we're calculating
// our bucket sizes based on this amount.)
#define SECS_PER_BURST     300

// Amount of time in daily bucket.
// (That is, DAILY_LOG_SIZE is a limit reflecting this many seconds.)
#define SECS_PER_DAY       (24*60*60)

// Worst-case number of programs bursting out of control at once
#define MAX_BURSTING_APPS  10

// Worst-case number of programs maxing out the daily byte counter
#define MAX_DAILY_APPS     20

// Default bytes per burst period
#define DEFAULT_BYTES_PER_BURST  (BURST_LOG_SIZE / MAX_BURSTING_APPS)

// Default bytes per day
#define DEFAULT_BYTES_PER_DAY  (DAILY_LOG_SIZE / MAX_DAILY_APPS)

// This is arbitrary.  It matters more when using syslogd (which
// has pretty strict limits) but we could make this arbitrarily large
// if we really wanted to allow obscenely long lines.  Anything larger
// than th minimum bucket size makes no sense, of course.
#define MAX_LINE_LENGTH    768


enum BucketIds {
  B_BURST = 0,     // fast, small bucket (per-cycle limit; allows bursts)
  B_DAILY,         // slow, big bucket (per-day limit)
  B_WARNING,       // slow, small bucket (warns if you've made a burst)
  NUM_BUCKETS
};


enum BucketType {
  BT_INFORMATIONAL = 0,
  BT_MANDATORY = 1,
};


struct Bucket {
  char *name;           // short name of this bucket
  char *msg_start;      // message when bucket is first exceeded
  char *msg_end;        // message when bucket has some space again
  enum BucketType type; // controls whether this bucket causes drops
  ssize_t max_bytes;    // maximum bytes in this bucket when it's full
  ssize_t fill_rate;    // bytes added to this bucket per sec when not full
  ssize_t available;    // bytes currently in this bucket (<= max_bytes)
  int num_skipped;      // number of messages skipped because of this bucket
} buckets[NUM_BUCKETS] = {
  // B_BURST
  {
    "burst",
    "W: burst limit: dropping messages to prevent overflow (%d bytes/sec).",
    "W: burst limit: %d messages were dropped.",
    BT_MANDATORY,
    0, 0, 0, 0,
  },
  // B_DAILY
  {
    "daily",
    "W: daily limit: dropping messages (%d bytes/sec).",
    "W: daily limit: %d messages were dropped.",
    BT_MANDATORY,
    0, 0, 0, 0,
  },
  // B_WARNING
  {
    "warning",
    "I: burst notice: this log rate is unsustainable (%d bytes/sec).",
    "I: burst notice: %d messages would have been dropped.",
    BT_INFORMATIONAL,
    0, 0, 0, 0,
  },
};


static int debug = 0, want_unlimited_mode = 0, unlimited_mode = 0;
static char **g_argv = NULL;


// Returns 1 if 's' starts with 'contains' (which is null terminated).
static int startswith(const void *s, const char *contains) {
  return strncasecmp(s, contains, strlen(contains)) == 0;
}


// However, we want to allow short-term bursts of more bytes, with a lower
// average when taken over the course of a longer time period.  So we
// actually need two token buckets: a "burst" bucket (to control short term
// burstiness so we don't overflow the local buffer) and a "daily" bucket
// (to control the long term average so we don't overflow the remote
// server's quota).
static void init_buckets(ssize_t bytes_per_burst, ssize_t bytes_per_day) {
  // Divide by 2 is just in case we go two cycles between successful log
  // uploads; we want to allow for 2x the buffer usage in that case.
  // Note that this algorithm still isn't perfect: if your program times
  // things exactly right, it could have a full bucket at the beginning
  // of a cycle, empty it out, then it would refill at fill_rate throughout
  // the cycle, allowing more than max_bytes to be written during a given
  // cycle.  I hope this is sufficiently rare that we don't have to pessimize
  // the bucket sizes just to deal with this almost-never occurrence, but it's
  // still worrisome that the condition can exist at all.
  //
  // We initialize buckets with available > 0 to allow for bursts
  // of messages at startup time (which is a common time to want to log
  // logs of stuff).
  buckets[B_BURST].max_bytes = bytes_per_burst / 2;
  buckets[B_BURST].fill_rate = buckets[B_BURST].max_bytes / SECS_PER_BURST;
  buckets[B_BURST].available = buckets[B_BURST].max_bytes / 2;

  // max_bytes divide by 2 not needed here because not affected by uploads.
  buckets[B_DAILY].max_bytes = bytes_per_day;
  buckets[B_DAILY].fill_rate = buckets[B_DAILY].max_bytes / SECS_PER_DAY;
  buckets[B_DAILY].available = buckets[B_DAILY].max_bytes / 2;

  // The warning bucket goes off if you would have emptied the slow (daily)
  // bucket, had it been as small as the burst bucket.  Basically, this
  // triggers a message when you are relying on the short term "burst"
  // feature, giving you early warning that if you keep this up, you will
  // eventually exceed the daily bucket and your bandwidth will be cut.
  // It doesn't actually prevent you from writing anything though.
  buckets[B_WARNING].max_bytes = buckets[B_BURST].max_bytes;
  buckets[B_WARNING].fill_rate = buckets[B_DAILY].fill_rate;
  buckets[B_WARNING].available = buckets[B_BURST].available;
}


static void _flush_unlimited(uint8_t *header, ssize_t headerlen,
                             const uint8_t *buf, ssize_t len) {
  ssize_t total = headerlen + len + 1;
  struct iovec iov[] = {
    { header, headerlen },
    { (uint8_t *)buf, len },
    { "\n", 1 },
  };
  uint8_t lvl;

  assert(headerlen > 3);
  assert(header[0] == '<');
  assert(header[2] == '>');

  if (startswith(buf, "weird:") ||
      startswith(buf, "fatal:") ||
      startswith(buf, "critical:")) {
    lvl = '2';
  } else if (startswith(buf, "e:") ||
             startswith(buf, "error:")) {
    lvl = '3';
  } else if (startswith(buf, "w:") ||
             startswith(buf, "warning:")) {
    lvl = '4';
  } else if (startswith(buf, "n:") ||
             startswith(buf, "notice:")) {
    lvl = '5';
  } else if (startswith(buf, "i:") ||
             startswith(buf, "info:")) {
    lvl = '6';
  } else {
    // default is debug
    lvl = '7';
  }
  header[1] = lvl;  // header starts with <x>; replace the x

  ssize_t wrote = writev(1, iov, sizeof(iov)/sizeof(iov[0]));
  if (wrote >= 0 && wrote < total) {
    // should never happen because stdout should be non-blocking
    fprintf(stderr, "WEIRD: logos: writev(%zd) returned %zd\n", total, wrote);
    // not fatal
  } else if (wrote < 0) {
    perror("logos: writev");
    // not fatal
  }
}


// Returns the kernel monotonic timestamp in milliseconds.
static long long mstime(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    perror("logos: clock_gettime");
    exit(7); // really should never happen, so don't try to recover
  }
  return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}


static long long last_add_time;
static int skipping, backoff = 10*1000 / 2;
static void maybe_fill_buckets(void) {
  long long now = mstime(), tdiff;
  int i;

  if (!last_add_time) {
    // buckets always start out half-full, particularly because programs tend
    // to spew a lot of content at startup.  Also, last_add_time gets
    // reset to 0 when we enable/disable unlimited_mode, so the buckets
    // refill.
    last_add_time = now;
    for (i = 0; i < NUM_BUCKETS; i++) {
      buckets[i].available = buckets[i].max_bytes / 2;
    }
  } else {
    tdiff = now - last_add_time;

    // only update last_add_time if we added any bytes.  Otherwise there's
    // an edge case where if bytes_per_millisecond is < 1.0 and there's
    // a message every millisecond, we'd never add to the bucket.
    //
    // Also, if we had to start dropping messages, wait for a minimal
    // filling of the bucket so we don't just constantly toggle between
    // empty/nonempty.  It's more useful to show fewer uninterrupted bursts
    // of messages than just one message here and there.
    if ((!skipping && tdiff >= 1000) || (skipping && tdiff >= backoff)) {
      for (int i = 0; i < NUM_BUCKETS; i++) {
        long long add = tdiff * buckets[i].fill_rate / 1000;
        assert(add >= 0);
        buckets[i].available += add;
        if (buckets[i].available > buckets[i].max_bytes) {
          buckets[i].available = buckets[i].max_bytes;
        }
      }
      last_add_time = now;
    }
  }
}


static int all_buckets_have_room(uint8_t *header, ssize_t headerlen,
                                 ssize_t total) {
  int all_ok = 1, now_skipping = 0;
  for (int i = 0; i < NUM_BUCKETS; i++) {
    if (buckets[i].available >= total || unlimited_mode) {
      if (buckets[i].num_skipped) {
        char tmp[1024];
        ssize_t n = snprintf(tmp, sizeof(tmp),
                             buckets[i].msg_end, buckets[i].num_skipped);
        _flush_unlimited(header, headerlen, (uint8_t *)tmp, n);
        buckets[i].num_skipped = 0;
      }
      // in unlimited_mode this could go negative; that's ok
      buckets[i].available -= total;
    } else {
      if (!buckets[i].num_skipped) {
        char tmp[1024];
        ssize_t n = snprintf(tmp, sizeof(tmp),
                             buckets[i].msg_start, buckets[i].fill_rate);
        _flush_unlimited(header, headerlen, (uint8_t *)tmp, n);
        buckets[i].available = 0;
        if (!now_skipping && !skipping) backoff *= 2;
        if (backoff > 120*1000) backoff = 120*1000;
      }
      now_skipping = 1;
      buckets[i].num_skipped++;
      switch (buckets[i].type) {
        case BT_MANDATORY:
          all_ok = 0;
          break;
        case BT_INFORMATIONAL:
          break;
      }
    }
  }
  skipping = now_skipping;
  return all_ok;
}


// This implements the rate limiting using a token bucket algorithm.
static void _flush_ratelimited(uint8_t *header, ssize_t headerlen,
                               uint8_t *buf, ssize_t len) {
  ssize_t total = headerlen + len + 1;

  if (debug) {
    char buf[1024], *p = buf;
    assert(sizeof(buf) >= 100 * NUM_BUCKETS);
    p += sprintf(p, "logos: ");
    for (int i = 0; i < NUM_BUCKETS; i++) {
      p += sprintf(p, "%s=%zd ", buckets[i].name, buckets[i].available);
      assert(p < buf + sizeof(buf));
      assert(p < buf + 100*(i+1));
    }
    p += sprintf(p, "want=%zd\n", total);
    fputs(buf, stderr);
  }

  maybe_fill_buckets();

  if (all_buckets_have_room(header, headerlen, total)) {
    _flush_unlimited(header, headerlen, buf, len);
  }
}


// This SIGHUP handler is needed for the unit test, but it may occasionally
// be useful in real life too, in case rate limiting kicks in and you really
// want to see what's going on this instant.
static void refill_ratelimiter(int sig) {
  last_add_time = 0;
}


// SIGUSR1 disables the rate limit entirely, for debugging on test devices
static void disable_ratelimit(int sig) {
  want_unlimited_mode = 1;
}


// SIGUSR2 does the opposite of SIGUSR1.  We could make SIGUSR1 a toggle
// instead, but this way you can just do 'pkill -USR1 logos' and make sure
// all the processes have log limits disabled, where a toggle would leave you
// uncertain.
static void enable_ratelimit(int sig) {
  want_unlimited_mode = 0;
}


// strlen is not async-safe, supply one which is.
static size_t my_strlen(const char *string) {
  size_t i;
  for (i = 0; string[i] != '\0'; ++i);
  return i;
}

// We don't have a way to babysit logos externally, as it is in
// a pipe from some other process. Make it try again if it fails.
static void rejuvinate_process(int sig) {
  char *restart = "<2>logos: restarting on fatal signal\n";
  char *giveup = "<2>logos: Cannot find logos binary to exec\n";
  size_t unused __attribute__((unused));
  unused = write(1, restart, my_strlen(restart));

  // execvp is not async-signal safe, so check likely paths.
  execve("/bin/logos", g_argv, environ);
  execve("/usr/bin/logos", g_argv, environ);
  execve("/sbin/logos", g_argv, environ);
  execve("/usr/sbin/logos", g_argv, environ);
  unused = write(1, giveup, my_strlen(giveup));
  exit(99);
}

// Return a malloc()ed buffer that's a copy of buf, with a terminating
// nul and control characters replaced by printable characters.
static uint8_t *fix_buf(uint8_t *buf, ssize_t len) {
  uint8_t *outbuf = malloc(len * 8 + 1), *inp, *outp;
  if (!outbuf) {
    perror("logos: allocating memory");
    return NULL;
  }
  for (inp = buf, outp = outbuf; inp < buf + len; inp++) {
    if (*inp >= 32 || *inp == '\n') {
      *outp++ = *inp;
    } else if (*inp == '\t') {
      // align tabs (ignoring prefixes etc) for nicer-looking output
      do {
        *outp++ = ' ';
      } while ((outp - outbuf) % 8 != 0);

    } else if (*inp == '\r') {
      // just ignore CR characters
    } else {
      snprintf((char *)outp, 5, "\\x%02x", (int)*inp);
      outp += 4;
    }
  }
  *outp = '\0';
  return outbuf;
}


static void flush(uint8_t *header, ssize_t headerlen,
                  uint8_t *buf, ssize_t len) {
  // We can assume the header doesn't have any invalid bytes in it since
  // it'll tend to be a hardcoded string.  We also pass through chars >=
  // 128 without validating that they're correct utf-8, just in case seeing
  // the verbatim values helps someone sometime.
  uint8_t *p;
  for (p = buf; p < buf + len; p++) {
    if (*p < 32 && *p != '\n') {
      p = fix_buf(buf, len);
      if (p) {
        _flush_ratelimited(header, headerlen, p, strlen((char *)p));
        free(p);
      }
      return;
    }
  }
  // if we get here, there were no special characters
  _flush_ratelimited(header, headerlen, buf, len);
}


static void usage(void) {
  fprintf(stderr,
      "Usage: [LOGOS_DEBUG=1] logos <facilityname> [bytes/burst] [bytes/day]\n"
      "  Copies logs from stdin to /dev/kmsg, formatting them to be\n"
      "  suitable for /dev/kmsg. If LOGOS_DEBUG is >= 1, writes to\n"
      "  stdout instead.\n"
      "  \n"
      "  Default bytes/burst = %ld - use 0 (for default) if possible.\n"
      "  Default bytes/day = %ld - use 0 (for default) if possible.\n"
      "  Signals:\n"
      "    SIGHUP: refill the token buckets once.\n"
      "    SIGUSR1: disable rate limiting.\n"
      "    SIGUSR2: re-enable rate limiting.\n"
      "    Example: pkill -USR1 logos  -- disables rate limit on all logos.\n",
      (long)DEFAULT_BYTES_PER_BURST, (long)DEFAULT_BYTES_PER_DAY);
  exit(99);
}

int main(int argc, char **argv) {
  static uint8_t overlong_warning[] =
      "W: previous log line was split. Use shorter lines.";
  static uint8_t now_unlimited[] =
      "W: SIGUSR1: rate limit disabled.";
  static uint8_t now_limited[] =
      "W: SIGUSR2: rate limit re-enabled.";
  const char *disable_limits_file = "/config/disable-log-limits";
  uint8_t buf[MAX_LINE_LENGTH], *header;
  ssize_t used = 0, got, headerlen;
  int overlong = 0;

  {
    char *p = getenv("LOGOS_DEBUG");
    if (p) {
      debug = atoi(p);
    }
  }

  if (argc < 2 || argc > 4) {
    usage();
  }

  // remove underscores form the facility name
  strip_underscores(argv[1]);
  if (strlen(argv[1]) == 0) {
    fprintf(stderr, "logos: facility name was empty, or all underscores.\n");
    return 1;
  }

#ifndef COMPILE_FOR_HOST
  stacktrace_setup();
#endif  // COMPILE_FOR_HOST
  g_argv = argv;
  signal(SIGHUP, refill_ratelimiter);
  signal(SIGUSR1, disable_ratelimit);
  signal(SIGUSR2, enable_ratelimit);
  signal(SIGILL, rejuvinate_process);
  signal(SIGBUS, rejuvinate_process);
  signal(SIGSEGV, rejuvinate_process);

  headerlen = 3 + strlen(argv[1]) + 1 + 1; // <x>, fac, :, space
  header = malloc(headerlen + 1);
  if (!header) {
    perror("logos: allocating memory");
    return 5;
  }
  snprintf((char *)header, headerlen + 1, "<x>%s: ", argv[1]);

  ssize_t bytes_per_burst = DEFAULT_BYTES_PER_BURST;
  if (argc > 2) {
    bytes_per_burst = atoll(argv[2]);
  }
  if (!bytes_per_burst) {
    bytes_per_burst = DEFAULT_BYTES_PER_BURST;
  }
  if (bytes_per_burst < SECS_PER_BURST * 2) {
    fprintf(stderr, "logos: bytes-per-burst (%s) must be an int >= %d\n",
            argv[2], (int)SECS_PER_BURST * 2);
    return 6;
  }

  ssize_t bytes_per_day = 0;
  if (argc > 3) {
    bytes_per_day = atoll(argv[3]);
  }
  if (!bytes_per_day) {
    bytes_per_day = DEFAULT_BYTES_PER_DAY;
  }
  if (bytes_per_day < SECS_PER_DAY) {
    fprintf(stderr, "logos: bytes-per-day (%s) must be an int >= %d\n",
            argv[2], (int)SECS_PER_DAY);
    return 6;
  }
  init_buckets(bytes_per_burst, bytes_per_day);

  struct stat fst;
  if (stat(disable_limits_file, &fst) == 0) {
    want_unlimited_mode = 1;
  }

  if (!debug) {
    int fd = open("/dev/kmsg", O_WRONLY);
    if (fd < 0) {
      perror("logos: /dev/kmsg");
      return 3;
    }
    dup2(fd, 1);  // make it stdout
    dup2(fd, 2);  // and stderr too
    close(fd);

    // Chdir to / so that we don't prevent filesystems from unmounting just
    // because we happened to be in that directory while starting a long-running
    // task.
    if (chdir("/") != 0) {
      perror("logos: chdir /");
      return 3;
    }
  }

  while (1) {
    if (unlimited_mode != want_unlimited_mode) {
      // we delay setting these variables until this point, in order to avoid
      // race conditions caused by changing unlimited_mode and last_add_time
      // inside a signal handler.
      unlimited_mode = want_unlimited_mode;
      last_add_time = 0;
      if (unlimited_mode) {
        _flush_unlimited(header, headerlen,
                         now_unlimited, strlen((char *)now_unlimited));
      } else {
        _flush_unlimited(header, headerlen,
                         now_limited, strlen((char *)now_limited));
      }
    }
    if (used == sizeof(buf)) {
      flush(header, headerlen, buf, used);
      overlong = 1;
      used = 0;
    }
    got = read(0, buf + used, sizeof(buf) - used);
    if (got == 0) {
      if (used > 0) {
        /* Only output if there is text in the buffer, avoid
         * printing a blank line when a process exits. */
        flush(header, headerlen, buf, used);
      }
      goto done;
    } else if (got < 0) {
      if (errno != EINTR && errno != EAGAIN) {
        flush(header, headerlen, buf, used);
        return 1;
      }
    } else {
      uint8_t *start = buf, *next = buf + used, *end = buf + used + got, *p;
      while ((p = memchr(next, '\n', end - next)) != NULL) {
        ssize_t linelen = p - start;
        flush(header, headerlen, start, linelen);
        if (overlong) {
          // that flush() was the first newline after buffer length
          // exceeded, which means the end of the overly long line.  Let's
          // print a warning about it.
          flush(header, headerlen,
                overlong_warning, strlen((char *)overlong_warning));
          overlong = 0;
        }
        start = next = p + 1;
      }
      used = end - start;
      memmove(buf, start, used);
    }
  }

done:
  free(header);
  return 0;
}
