#define _GNU_SOURCE             /* for sighandler_t */
#define _POSIX_C_SOURCE 199309L /* for clock_gettime */
#define _BSD_SOURCE             /* for strsep */
#include <features.h>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <stacktrace.h>

#include "fileops.h"
#include "gfiber-lt.h"
#include "pin.h"

#define WRITE(s)  write(2, s, strlen(s))
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#endif

/*
 * We're polling at a very high frequency, which is a pain.  This would be
 * slightly less gross inside the kernel (for less context switching and
 * because it could more easily use the tick interrupt instead of polling).
 *
 * This setting isn't as bad as it sounds, though, because we don't poll
 * 100% of the time; we only do it for a fraction of a second every now
 * and then.
 */
#define POLL_HZ 2000    // polls per sec
#define USEC_PER_TICK (1000000 / POLL_HZ)

/*
 * At this temp, if sysmgr isn't setting fan, jump to 100% as a failsafe.
 * sysmgr has a per-platform setting, but we don't share code with that here.
 * Setting to MAX(temp_max) from fancontrol.cc in sysmgr, which is 100 now.
 */
#define HIGH_TEMP_OVERRIDE		100.0

#define CHECK(x) do { \
    int rv = (x); \
    if (rv) { \
      fprintf(stderr, "CHECK: %s returned %d\n", #x, rv); \
      _exit(99); \
    } \
  } while (0)

static int is_limited_leds;
static int platform_b0;
static PinHandle handle;

// Turn the leds on or off depending on the bits in fields.  Currently
// the bits are:
//   1: red
//   2: blue (green on B0)
//   4: activity (blue)
//   8: standby (bright white)
static void set_leds_from_bitfields(int fields, int brightness) {
  // allow a way to disable led control
  if (access("disable", R_OK) == 0) {
    return;
  }
  if (is_limited_leds) {
    // GFMS100 only has red and activity lights.  Substitute activity for blue
    // (they're both blue anyhow) and red+activity (purple) for standby.
    if (fields & 0x02) fields |= 0x04;
    if (fields & 0x08) fields |= 0x05;
  } else if (platform_b0) {
    // B0 fat devices are as above (limited_leds).
    // B0 fat devices had the leds switched around, and the polarities
    //  inverted.
    fields = ( (fields & 0x8) |
              ((fields & 0x4) >> 1) |
              ((fields & 0x2) >> 1) |
              ((fields & 0x1) << 2));
    fields ^= 0x0f;
  }
  if (PinIsPresent(handle, PIN_LED_RED))
    PinSetValue(handle, PIN_LED_RED, (fields & 0x01) ? brightness : 0);
  if (PinIsPresent(handle, PIN_LED_BLUE))
    PinSetValue(handle, PIN_LED_BLUE, (fields & 0x02) ? brightness : 0);
  if (PinIsPresent(handle, PIN_LED_ACTIVITY))
    PinSetValue(handle, PIN_LED_ACTIVITY, (fields & 0x04) ? brightness : 0);
  if (PinIsPresent(handle, PIN_LED_STANDBY))
    PinSetValue(handle, PIN_LED_STANDBY, (fields & 0x08) ? brightness : 0);
}


// read a file containing a single short string.
// Returns a static buffer.  Be careful!
static char *read_file(const char *filename) {
  static char buf[1024];
  int fd = open(filename, O_RDONLY);
  if (fd >= 0) {
    size_t got = read(fd, buf, sizeof(buf) - 1);
    buf[got] = '\0';
    close(fd);
    return buf;
  }
  buf[0] = '\0';
  return buf;
}


// create the given (empty) file.
static void create_file(const char *filename) {
  // use O_EXCL here to save a close() syscall when it already exists
  int fd = open(filename, O_WRONLY|O_CREAT|O_EXCL, 0666);
  if (fd >= 0) close(fd);
}

// read led_sequence from the given file.  For example, if a file contains
//       x5 0 1 0 2 0 0x0f
// that means 5/6 of a second off, then red, then off, then blue, then off,
// then all the lights on at once, for a total of 5 seconds.
// Enhanced in prism, if the pin is followed by "@<number>[x<number>]", then
// the number after the @ indicates brightness, and the number after x means
// the repetition. For example, 1@50x3 means turn red LED with brightness 50
// for 3 times of period (compared with w/o this param, wich is equivalent as
// x1).
static char led_sequence[16];
static int led_sequence_for_brightness[16];
static unsigned led_sequence_len = 1;
static unsigned led_total_time = 1000;
static void read_led_sequence_file(const char *filename) {
  char *buf = read_file(filename), *p;
  led_sequence_len = 0;
  led_total_time = 1000;
  while ((p = strsep(&buf, " \t\n\r")) != NULL &&
         led_sequence_len <= sizeof(led_sequence)/sizeof(led_sequence[0])) {
    if (!*p) continue;
    if (p[0] == 'x') {
      led_total_time = strtoul(p+1, NULL, 0) * 1000;
      if (led_total_time > 10000) led_total_time = 10000;
      if (led_total_time < 1000) led_total_time = 1000;
    } else {
      char *separator;
      int brightness = 100, repetition = 1, rep_idx;

      int leds = strtoul(p, &separator, 0);
      if (*separator == '@') {
        brightness = strtoul(separator + 1, &separator, 0);
      }
      if (*separator == 'x') {
        repetition = strtoul(separator + 1, NULL, 0);
      }
      for (rep_idx = 0; rep_idx < repetition; rep_idx++) {
        if (led_sequence_len >= ARRAY_SIZE(led_sequence)) {
          fprintf(stderr, "LED pattern is too large.\n");
          break;
        }
        led_sequence[led_sequence_len] = leds;
        led_sequence_for_brightness[led_sequence_len] = brightness;
        led_sequence_len++;
      }
    }
  }
  if (!led_sequence_len) {
    led_sequence[0] = 1; // red = error
    led_sequence_len = 1;
  }
}

// switch to the next led combination in led_sequence.
static void led_sequence_update(long long frac) {
  int i = led_sequence_len * frac / led_total_time;
  if (i >= (int)led_sequence_len)
    i = led_sequence_len;
  if (i < 0)
    i = 0;

  // if the 'activity' file exists, unlink() will succeed, giving us exactly
  // one inversion of the activity light.  That causes exactly one delightful
  // blink.
  int activity_toggle = (unlink("activity") == 0) ? 0x04 : 0;

  set_leds_from_bitfields(led_sequence[i] ^ activity_toggle,
                          led_sequence_for_brightness[i]);
}


// Same as time(), but in monotonic clock milliseconds instead.
static long long msec_now(void) {
  struct timespec ts;
  CHECK(clock_gettime(CLOCK_MONOTONIC, &ts));
  return ((long long)ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
}


// Same as time(), but in realtime milliseconds instead.
// Avoid using this when possible, as ntpd can make it jump around.
static long long msec_realtime_now(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((long long)tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
}


// The offset of msec_now() vs. wall clock time.
// Don't use this for anything important, since you can't trust wall clock
// time on our devices.  But it's useful for syncing LED blinking between
// devices.  Because it's prettier.
static long long msec_offset(void) {
  long long mono = msec_now(), real = msec_realtime_now();
  // The math here is slightly silly because C doesn't guarantee what happens
  // with % of a negative number, and we want the offset to always come out
  // positive, so that nothing weird will happen when mono < led_total_time
  // (which is true right after boot).
  return (((mono % led_total_time) - (real % led_total_time))
          + led_total_time) % led_total_time;
}


// like signal(), but always creates a one-shot signal handler
static void _signal(int sig, sighandler_t handler) {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = handler;
  act.sa_flags = SA_NODEFER | SA_RESETHAND;
  sigaction(sig, &act, NULL);
}


static volatile int shutdown_sig = 0;
static void sig_handler(int sig) {
  shutdown_sig = sig;

  // even in case of a segfault, we still want to try to shut down
  // politely so we can fix the fan speed etc.  writev() is a syscall
  // so this sequence should be safe since it has no outside dependencies.
  // fprintf() is not 100% safe in a signal handler.
  char buf[] = {
    '0' + (sig / 100 % 10),
    '0' + (sig / 10 % 10),
    '0' + (sig % 10),
  };
  struct iovec iov[] = {
    { "exiting on signal ", 18 },
    { buf, sizeof(buf)/sizeof(buf[0]) },
    { "\n", 1 },
  };
  writev(2, iov, sizeof(iov)/sizeof(iov[0]));

  if (sig != SIGINT && sig != SIGTERM) stacktrace();
}


static void alarm_handler(int sig) {
  WRITE("\nexiting on SIGALRM\n");
  abort();
}

void run_gpio_mailbox(void) {
  _signal(SIGALRM, alarm_handler);
  alarm(30);  // die loudly if we freeze for any reason (probably libnexus)
  platform_b0 = (NULL != strstr(read_file("/proc/cpuinfo"), "BCM7425B0"));
  int has_fan = PinIsPresent(handle, PIN_FAN_CHASSIS);
  int has_reset_button = PinIsPresent(handle, PIN_BUTTON_RESET);
  int has_cpu_temp = PinIsPresent(handle, PIN_TEMP_CPU);
  int has_cpu_voltage = PinIsPresent(handle, PIN_MVOLTS_CPU);

  is_limited_leds = !PinIsPresent(handle, PIN_LED_BLUE) || !PinIsPresent(handle, PIN_LED_STANDBY);

#if 0
  // check if cpu has been locked
  if (PinIsSecureBoot(handle)) {
    create_file("ledcontrol/secure_boot");
  }
#endif

  fprintf(stderr, "gpio mailbox running.\n");
  write_file_longlong_atomic("/var/run/gpio-mailbox", NULL, getpid());
  _signal(SIGINT, sig_handler);
  _signal(SIGTERM, sig_handler);
  _signal(SIGSEGV, sig_handler);
  _signal(SIGBUS, sig_handler);
  _signal(SIGFPE, sig_handler);

  int inner_loop_ticks = 0, msec_per_led = 0;
  int fan_loop_count = 0;
  long long last_time = 0, last_print_time = msec_now(),
      last_led = 0, reset_start = 0, offset = msec_offset();
  long long fanspeed = -42, reset_amt = -42, readyval = -42;
  double cpu_temp = -42.0, cpu_volts = -42.0;
  int wantspeed_warned = -42, wantspeed = 0;
  int fan_detected_speed = 0;
  while (!shutdown_sig) {
    long long now = msec_now();
    alarm(30);  // die loudly if we freeze for 30 seconds or more

    // blink the leds
    if (now - last_led >= msec_per_led) {
      read_led_sequence_file("leds");
      assert(led_sequence_len > 0);
      inner_loop_ticks = POLL_HZ / led_sequence_len + 1;
      while (inner_loop_ticks > POLL_HZ / 16) {
        // make sure we poll at least every 1/8 of a second, or else the
        // activity light won't blink impressively enough.
        inner_loop_ticks /= 2;
      }
      msec_per_led = led_total_time / led_sequence_len + 1;
      last_led = now;
      offset = msec_offset();
      create_file("leds-ready");
    }
    led_sequence_update((now + led_total_time - offset) % led_total_time);

    if (now - last_time > 2000) {
      if (has_fan) {
        // set the fan speed control
        char *wantspeed_str = read_file("fanpercent");
        if (wantspeed_str[0]) {
          wantspeed = strtol(wantspeed_str, NULL, 0);
          if (wantspeed < 0 || wantspeed > 100) {
            if (wantspeed_warned != wantspeed) {
              fprintf(stderr,
                      "gpio/fanpercent (%d) is invalid: must be 0-100\n",
                      wantspeed);
              wantspeed_warned = wantspeed;
            }
            wantspeed = 100;
          } else if (wantspeed < 100 && cpu_temp >= HIGH_TEMP_OVERRIDE) {
            if (wantspeed_warned != wantspeed) {
              fprintf(stderr,
                      "DANGER: fanpercent (%d) is too low for CPU temp %.2f; "
                      "using 100%%.\n", wantspeed, cpu_temp);
              wantspeed_warned = wantspeed;
            }
            wantspeed = 100;
          } else {
            wantspeed_warned = -42;
          }
        } else {
          if (wantspeed_warned != 1)
              fprintf(stderr,
                      "gpio/fanpercent is empty: using default value\n");
          wantspeed_warned = 1;
          wantspeed = 100;
        }
        (void) PinSetValue(handle, PIN_FAN_CHASSIS, wantspeed);

        // capture the fan cycle counter
        write_file_longlong_atomic("fanspeed", &fanspeed, fan_detected_speed);
      }

      // capture the CPU temperature and voltage
      int cpu_temp_millidegrees;
      if (PinValue(handle, PIN_TEMP_CPU, &cpu_temp_millidegrees) == 0) {
        write_file_double_atomic("cpu_temperature", &cpu_temp, cpu_temp_millidegrees / 1000.0);
      }
      int cpu_millivolts;
      if (!has_cpu_voltage) {
        write_file_double_atomic("cpu_voltage", &cpu_volts, 0.0);
      } else if (PinValue(handle, PIN_MVOLTS_CPU, &cpu_millivolts) == 0) {
        write_file_double_atomic("cpu_voltage", &cpu_volts, cpu_millivolts / 1000.0);
      }
      last_time = now;
    }

    int reset_button = 0;
    (void) PinValue(handle, PIN_BUTTON_RESET, &reset_button);

    if (now - last_print_time >= 6000) {
      if (has_fan) {
        fprintf(stderr, "fan:%lld/sec:%d%% reads:%d ", fanspeed, wantspeed, 0);
      }
      if (has_reset_button) {
        fprintf(stderr, "button:%d ", reset_button);
      }
      if (has_cpu_temp) {
        fprintf(stderr, "temp:%.2f ", cpu_temp);
      }
      if (has_cpu_voltage) {
        fprintf(stderr, "volts:%.2f", cpu_volts);
      }
      fprintf(stderr, "\n");
      last_print_time = now;
    }

    // handle the reset button
    if (reset_button) {
      if (!reset_start) reset_start = now - 1;
        write_file_longlong_atomic("reset_button_msecs", &reset_amt, now - reset_start);
    } else {
      if (reset_amt) unlink("reset_button_msecs");
      reset_amt = reset_start = 0;
    }

    // this is last.  it indicates we've made it once through the loop,
    // so all the files in /tmp/gpio have been written at least once.
    write_file_longlong_atomic("ready", &readyval, 1);

    if (has_fan) {
      // poll for fan ticks.  This is a bit complicated since we want to be
      // sure to count the exact time for an integer number of ticks.
      fan_loop_count = (fan_loop_count + 1) % 16;
      if (!fan_loop_count) {
        PinValue(handle, PIN_FAN_CHASSIS, &fan_detected_speed);
      } else {
        // no need to poll *every* time.
        // For the last tick of each second, adjust it slightly so our LED
        // blinks can be aligned on the led_total_time boundary.
        long long time_to_boundary =
            (led_total_time - (now - offset) % led_total_time) * 1000;
        long long delay = USEC_PER_TICK * inner_loop_ticks;
        if (delay > time_to_boundary) delay = time_to_boundary;
        usleep(delay);
      }
    } else {
      // platform has no fan
      usleep(USEC_PER_TICK * inner_loop_ticks);
    }
  }

  // shut down cleanly

#ifndef  GFIBER_LT
  set_leds_from_bitfields(1, 1);  // red light to indicate a problem
#else
  set_leds_from_bitfields(1, GFLT_DEFAULT_BRIGHTNESS);
#endif

  if (has_fan) (void) PinSetValue(handle, PIN_FAN_CHASSIS, 100); // for safety
}


static void parent_died(void) {
  // normally the child process does this step.
  //
  // do it again here just in case the child process dies early; the boot
  // process will wait on this file, and we don't want it to get jammed
  // forever.
  int fd = open("/var/run/gpio-mailbox", O_WRONLY|O_CREAT, 0666);
  if (fd >= 0) close(fd);
}


static void parent_sighandler(int sig) {
  WRITE("\n\nOWNER PROCESS DIED\n\n");
  parent_died();
  kill(getpid(), sig);
  // should never get here, but just in case
  abort();
}


int main(void) {
  int status = 98;
  fprintf(stderr, "starting gpio mailbox in /tmp/gpio.\n");
  _signal(SIGSEGV, parent_sighandler);
  _signal(SIGBUS, parent_sighandler);
  _signal(SIGFPE, parent_sighandler);

  mkdir("/tmp/gpio", 0775);
  if (chdir("/tmp/gpio") != 0) {
    perror("chdir /tmp/gpio");
    return 1;
  }
  mkdir("/tmp/leds", 0775);

  handle = PinCreate();
  if (handle == NULL) {
    fprintf(stderr, "PinCreate() failed\n");
    exit(status);
  }

  run_gpio_mailbox();

  parent_died();
  exit(status);
}
