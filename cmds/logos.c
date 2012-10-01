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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

// Total size of kernel log buffer.
#define TOTAL_LOG_SIZE   1000000

// Amount of time between system-wide log uploads.
#define SECS_PER_CYCLE   300

// Our log buffers are 1MB, uploaded every 300 seconds.  Let's assume a
// worst-likely-case of two log periods between uploads and up to 10
// programs logging at excessive speeds all at once.
//
// This isn't quite optimal, since generally you won't have *all* the
// programs logging out of control at once, so it would be better if one
// program could steal log space from another.  But that gets complicated
// fast, so let's just be conservative here.  Overflowing the buffer is
// bad for everyone.
#define DEFAULT_MAX_BYTES_PER_SEC  (TOTAL_LOG_SIZE/(SECS_PER_CYCLE*2)/10)

// Fill the token bucket with up to this many seconds' worth of tokens, to
// allow for bursty output.  The per-log-upload time period is a good value
// to start with, but it's slightly too optimistic since everyone
// starts off with a full token bucket.  That means during the first
// BUCKET_SIZE_SECS after booting, tasks could produce twice as much
// data as they're supposed to.  Let's use slightly less.
#define BUCKET_SIZE_SECS   (SECS_PER_CYCLE/2)

// BUCKET_SIZE_SECS, but expressed in bytes instead of seconds.
#define BUCKET_SIZE (max_bytes_per_sec * BUCKET_SIZE_SECS)

// This is kind of arbitrary.  It matters more when using syslogd (which
// has pretty strict limits) but we could make this arbitrarily large
// if we really wanted to allow obscenely long lines.  Anything larger
// than max_bytes_per_sec*BUCKET_SIZE_SECS makes no sense, of course.
#define MAX_LINE_LENGTH    768


static int debug = 0;
static ssize_t max_bytes_per_sec = DEFAULT_MAX_BYTES_PER_SEC;


// Returns 1 if 's' starts with 'contains' (which is null terminated).
static int startswith(void *s, char *contains) {
  return strncasecmp(s, contains, strlen(contains)) == 0;
}


static void _flush_unlimited(uint8_t *header, ssize_t headerlen,
                             uint8_t *buf, ssize_t len) {
  ssize_t total = headerlen + len + 1;
  struct iovec iov[] = {
    { header, headerlen },
    { buf, len },
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
    perror("logos writev");
    // not fatal
  }
}


// Returns the kernel monotonic timestamp in milliseconds.
static long long mstime(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    perror("clock_gettime");
    exit(7); // really should never happen, so don't try to recover
  }
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


// This implements the rate limiting using a token bucket algorithm.
static long long last_add_time;
static void _flush_ratelimited(uint8_t *header, ssize_t headerlen,
                               uint8_t *buf, ssize_t len) {
  static ssize_t bucket;
  static int num_skipped;
  ssize_t total = headerlen + len + 1;
  long long now = mstime(), tdiff, add;

  if (debug) {
    fprintf(stderr, "logos: bucket=%zd total=%zd\n", bucket, total);
  }

  if (!last_add_time) {
    // bucket always starts out full, particularly because programs tend
    // to spew a lot of content at startup.
    last_add_time = now;
    bucket = BUCKET_SIZE;
  }

  tdiff = now - last_add_time;
  add = tdiff * max_bytes_per_sec / 1000;

  // only update last_add_time if we added any bytes.  Otherwise there's
  // an edge case where if bytes_per_millisecond is < 1.0 and there's
  // a message every millisecond, we'd never add to the bucket.
  //
  // Also, if we had to start dropping messages, wait for a minimal
  // filling of the bucket so we don't just constantly toggle between
  // empty/nonempty.  It's more useful to show fewer uninterrupted bursts
  // of messages than just one message here and there.
  if ((!num_skipped && add) ||
      (num_skipped && tdiff > 10*1000)) {
    if (add + bucket > BUCKET_SIZE) {
      bucket = BUCKET_SIZE;
    } else {
      bucket += add;
    }
    last_add_time = now;
  }

  if (bucket >= total) {
    if (num_skipped) {
      char tmp[1024];
      ssize_t n = snprintf(tmp, sizeof(tmp),
          "W: rate limit: %d messages were dropped.",
          num_skipped);
      _flush_unlimited(header, headerlen, (uint8_t *)tmp, n);
      num_skipped = 0;
    }
    _flush_unlimited(header, headerlen, buf, len);
    bucket -= total;
  } else {
    if (!num_skipped) {
      char tmp[1024];
      ssize_t n = snprintf(tmp, sizeof(tmp),
          "W: rate limit: dropping messages to prevent overflow.");
      _flush_unlimited(header, headerlen, (uint8_t *)tmp, n);
      bucket = 0;
    }
    num_skipped++;
  }
}


// This SIGHUP handler is needed for the unit test, but it may occasionally
// be useful in real life too, in case rate limiting kicks in and you really
// want to see what's going on this instant.
static void refill_ratelimiter(int sig) {
  last_add_time = 0;
}


// Return a malloc()ed buffer that's a copy of buf, with a terminating
// nul and control characters replaced by printable characters.
static uint8_t *fix_buf(uint8_t *buf, ssize_t len) {
  uint8_t *outbuf = malloc(len * 8 + 1), *inp, *outp;
  if (!outbuf) {
    perror("allocating memory");
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
          "Usage: [LOGOS_DEBUG=1] logos <facilityname> [bytes/cycle]\n"
          "  Copies logs from stdin to stdout, formatting them to be\n"
          "  suitable for /dev/kmsg. If LOGOS_DEBUG is >= 1, writes to\n"
          "  stdout instead.\n"
          "  \n"
          "  Default bytes/cycle = %ld - use the default if possible.\n",
          (long)DEFAULT_MAX_BYTES_PER_SEC * SECS_PER_CYCLE);
  exit(99);
}


int main(int argc, char **argv) {
  static uint8_t overlong_warning[] =
      "W: previous log line was split. Use shorter lines.";
  uint8_t buf[MAX_LINE_LENGTH], *header;
  ssize_t used = 0, got, headerlen;
  int overlong = 0;

  {
    char *p = getenv("LOGOS_DEBUG");
    if (p) {
      debug = atoi(p);
    }
  }

  if (argc != 2 && argc != 3) {
    usage();
  }

  signal(SIGHUP, refill_ratelimiter);

  headerlen = 3 + strlen(argv[1]) + 1 + 1; // <x>, fac, :, space
  header = malloc(headerlen + 1);
  if (!header) {
    perror("allocating memory");
    return 5;
  }
  snprintf((char *)header, headerlen + 1, "<x>%s: ", argv[1]);

  if (argc > 2) {
    max_bytes_per_sec = atoi(argv[2]) / SECS_PER_CYCLE;
    if (max_bytes_per_sec <= 0) {
      fprintf(stderr, "logos: bytes-per-cycle (%s) must be an int >= %d\n",
              argv[2], (int)SECS_PER_CYCLE);
      return 6;
    }
  }

  if (!debug) {
    int fd = open("/dev/kmsg", O_WRONLY);
    if (fd < 0) {
      perror("/dev/kmsg");
      return 3;
    }
    dup2(fd, 1);  // make it stdout
    close(fd);
  }

  while (1) {
    if (used == sizeof(buf)) {
      flush(header, headerlen, buf, used);
      overlong = 1;
      used = 0;
    }
    got = read(0, buf + used, sizeof(buf) - used);
    if (got == 0) {
      flush(header, headerlen, buf, used);
      goto done;
    } else if (got < 0) {
      if (errno != EINTR && errno != EAGAIN) {
        flush(header, headerlen, buf, used);
        return 1;
      }
    } else {
      uint8_t *start = buf, *next = buf + used, *end = buf + used + got, *p;
      while ((p = memchr(next, '\n', end - next)) != NULL) {
        flush(header, headerlen, start, p - start);
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
