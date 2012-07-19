#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_gpio.h"
#include "nexus_gpio_init.h"
#include "nexus_pwm.h"
#include "nexus_temp_monitor.h"
#include "nexus_avs.h"

/*
 * This is disgustingly over-frequent.  But we don't get an accurate fan
 * speed measurement without a pretty high sampling rate.  At full speed,
 * the fan ticks about 220 times per second, and we need at least two polls
 * (rising and falling edge) each.
 *
 * We could try using interrupts instead of polling, but it wouldn't make
 * much difference; 220 edges per second is still 220 edges per second.  It
 * would be slightly less gross inside the kernel instead.
 */
#define POLL_HZ 500    // polls per sec
#define USEC_PER_TICK (1000000 / POLL_HZ)

#define PWM_50_KHZ 0x7900
#define PWM_26_KHZ 0x4000
#define PWM_206_HZ 0x0080

#define CHECK(x) do { \
    int rv = (x); \
    if (rv) { \
      fprintf(stderr, "CHECK: %s returned %d\n", #x, rv); \
      _exit(99); \
    } \
  } while (0)

static int platform_limited_leds, platform_b0;


struct Gpio {
  NEXUS_GpioType type;
  unsigned int pin;
  NEXUS_GpioMode mode;
  NEXUS_GpioInterrupt interrupt_mode;

  NEXUS_GpioHandle handle;
  int old_val;
};


struct Pwm {
  unsigned int channel;

  NEXUS_PwmChannelHandle handle;
  int old_percent;
};


struct Gpio led_red = {
  NEXUS_GpioType_eAonStandard, 17,
  NEXUS_GpioMode_eOutputPushPull, NEXUS_GpioInterrupt_eDisabled, 0, -1
};
struct Gpio led_blue = {
  NEXUS_GpioType_eAonStandard, 12,
  NEXUS_GpioMode_eOutputPushPull, NEXUS_GpioInterrupt_eDisabled, 0, -1
};
struct Gpio led_activity = {
  NEXUS_GpioType_eAonStandard, 13,
  NEXUS_GpioMode_eOutputPushPull, NEXUS_GpioInterrupt_eDisabled, 0, -1
};
struct Gpio led_standby = {
  NEXUS_GpioType_eAonStandard, 10,
  NEXUS_GpioMode_eOutputPushPull, NEXUS_GpioInterrupt_eDisabled, 0, -1
};

struct Gpio reset_button = {
  NEXUS_GpioType_eAonStandard, 4,
  NEXUS_GpioMode_eInput, NEXUS_GpioInterrupt_eDisabled/*eEdge*/, 0, -1
};

struct Gpio fan_tick = {
  NEXUS_GpioType_eStandard, 98,
  NEXUS_GpioMode_eInput, NEXUS_GpioInterrupt_eDisabled/*eFallingEdge*/, 0, -1
};

struct Pwm fan_control = { 0, 0, -1 };


// Open the given PWM.  You have to do this before writing it.
static void pwm_open(struct Pwm *p) {
  NEXUS_PwmChannelSettings settings;
  NEXUS_Pwm_GetDefaultChannelSettings(&settings);
  settings.eFreqMode = NEXUS_PwmFreqModeType_eConstant;
  p->handle = NEXUS_Pwm_OpenChannel(p->channel, &settings);
  if (!p->handle) {
    fprintf(stderr, "Pwm_Open returned null\n");
    _exit(1);
  }
}


// Set the given PWM (pulse width modulator) to the given percent duty cycle.
static void set_pwm(struct Pwm *p, int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  if (p->old_percent == percent) return;
  p->old_percent = percent;
  CHECK(NEXUS_Pwm_SetControlWord(p->handle, PWM_26_KHZ));
  CHECK(NEXUS_Pwm_SetPeriodInterval(p->handle, 99));
  CHECK(NEXUS_Pwm_SetOnInterval(p->handle, percent));
  CHECK(NEXUS_Pwm_Start(p->handle));
}


// Get the CPU temperature.  I think it's in Celsius.
static double get_cpu_temperature(void) {
  NEXUS_AvsStatus status;
  CHECK(NEXUS_GetAvsStatus(&status));
  return status.temperature / 100 / 10.0;  // round to nearest 0.1
}


// Get the CPU voltage.
static double get_cpu_voltage(void) {
  NEXUS_AvsStatus status;
  CHECK(NEXUS_GetAvsStatus(&status));
  return status.voltage / 10 / 100.0;  // round to nearest 0.01
}


// Open the given GPIO pin.  You have to do this before reading or writing it.
static void gpio_open(struct Gpio *g) {
  NEXUS_GpioSettings settings;

  NEXUS_Gpio_GetDefaultSettings(g->type, &settings);
  settings.mode = g->mode;
  settings.interruptMode = g->interrupt_mode;
  settings.value = NEXUS_GpioValue_eLow;
  g->handle = NEXUS_Gpio_Open(g->type, g->pin, &settings);
  if (!g->handle) {
    fprintf(stderr, "Gpio_Open returned null\n");
    _exit(1);
  }
}


// Write the given GPIO pin.
// I don't actually know what's the difference between eHigh and eMax.
static void set_gpio(struct Gpio *g, int level) {
  if (g->old_val == level) {
    // If this is the same value as last time, don't do anything, for two
    // reasons:
    //   1) If you set the gpio too often, it seems to stay low (the led
    //      stays off).
    //   2) If some process other than us is twiddling a led, this way we
    //      won't interfere with it.
    return;
  }
  g->old_val = level;

  NEXUS_GpioValue val;
  switch (level) {
    case 0: val = NEXUS_GpioValue_eLow; break;
    case 1: val = NEXUS_GpioValue_eHigh; break;
    case 2: val = NEXUS_GpioValue_eMax; break;
    default:
      assert(level >= 0);
      assert(level <= 2);
      val = NEXUS_GpioValue_eLow;
      break;
  }

  NEXUS_GpioSettings settings;
  NEXUS_Gpio_GetSettings(g->handle, &settings);
  settings.value = val;
  NEXUS_Gpio_SetSettings(g->handle, &settings);
}


// Read the given GPIO pin
static NEXUS_GpioValue get_gpio(struct Gpio *g) {
  NEXUS_GpioStatus status;
  CHECK(NEXUS_Gpio_GetStatus(g->handle, &status));
  return status.value;
}


// Turn the leds on or off depending on the bits in fields.  Currently
// the bits are:
//   1: red
//   2: blue (green on B0)
//   4: activity (blue)
//   8: standby (bright white)
static void set_leds_from_bitfields(int fields) {
  if (platform_limited_leds) {
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
  set_gpio(&led_red, (fields & 0x01) ? 1 : 0);
  set_gpio(&led_blue, (fields & 0x02) ? 1 : 0);
  set_gpio(&led_activity, (fields & 0x04) ? 1 : 0);
  set_gpio(&led_standby, (fields & 0x08) ? 1 : 0);
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


// write a file containing the given string.
static void write_file(const char *filename, const char *content) {
  char *tmpname = malloc(strlen(filename) + 4 + 1);
  sprintf(tmpname, "%s.tmp", filename);
  int fd = open(tmpname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
  if (fd >= 0) {
    write(fd, content, strlen(content));
    close(fd);
    rename(tmpname, filename);
  }
  free(tmpname);
}


// write a file containing just a single integer value (as a string, not
// binary)
static void write_file_int(const char *filename,
                           long long *oldv, long long newv) {
  if (!oldv || *oldv != newv) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%lld", newv);
    buf[sizeof(buf)-1] = 0;
    write_file(filename, buf);
    if (oldv) *oldv = newv;
  }
}


// write a file containing just a single floating point value (as a string,
// not binary)
static void write_file_float(const char *filename,
                             double *oldv, double newv) {
  if (!oldv || *oldv != newv) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%.2f", newv);
    buf[sizeof(buf)-1] = 0;
    write_file(filename, buf);
    if (oldv) *oldv = newv;
  }
}


// read led_sequence from the given file.  For example, if a file contains
//       0 1 0 2 0 0x0f
// that means 1/6 of a second off, then red, then off, then blue, then off,
// then all the lights on at once.
static char led_sequence[16];
static unsigned led_sequence_len = 1;
static void read_led_sequence_file(const char *filename) {
  char *buf = read_file(filename), *p;
  led_sequence_len = 0;
  while ((p = strsep(&buf, " \t\n\r")) != NULL &&
         led_sequence_len <= sizeof(led_sequence)/sizeof(led_sequence[0])) {
    if (!*p) continue;
    led_sequence[led_sequence_len++] = strtol(p, NULL, 0);
  }
  if (!led_sequence_len) {
    led_sequence[0] = 1; // red = error
    led_sequence_len = 1;
  }
}


// switch to the next led combination in led_sequence.
static void led_sequence_update(bool next) {
  static unsigned i;

  // if the 'activity' file exists, unlink() will succeed, giving us exactly
  // one inversion of the activity light.  That causes exactly one delightful
  // blink.
  int activity_toggle = (unlink("activity") == 0) ? 0x04 : 0;

  if (i >= led_sequence_len)
    i = 0;
  set_leds_from_bitfields(led_sequence[i] ^ activity_toggle);
  if (next) i++;
}


// Same as time(), but in milliseconds instead.
static long long msec_now(void) {
  struct timespec ts;
  CHECK(clock_gettime(CLOCK_MONOTONIC, &ts));
  return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}


static volatile int shutdown_sig = 0;
static void sig_handler(int sig) {
  shutdown_sig = sig;
  signal(sig, SIG_DFL);

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
}


void run_gpio_mailbox(void) {
  platform_limited_leds = (0 == strncmp(read_file("/etc/platform"),
                                        "GFMS100", 7));
  platform_b0 = (NULL != strstr(read_file("/proc/cpuinfo"), "BCM7425B0"));
  gpio_open(&led_standby);
  gpio_open(&led_red);
  gpio_open(&led_activity);
  gpio_open(&led_blue);
  gpio_open(&reset_button);
  gpio_open(&fan_tick);
  pwm_open(&fan_control);

  // close any extra fds, especially /dev/brcm0.  That way we're
  // certain we won't interfere with any other nexus process's interrupt
  // handling.  Only one process can be doing interrupt handling at a time.
  for (int i = 3; i < 100; i++)
    close(i);

  fprintf(stderr, "gpio mailbox running.\n");
  write_file_int("/var/run/gpio-mailbox", NULL, getpid());
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGSEGV, sig_handler);
  signal(SIGFPE, sig_handler);

  int inner_loop_ticks = 0, msec_per_led = 0;
  int reads = 0, fan_flips = 0, last_fan = 0, cur_fan;
  long long last_time = 0, last_print_time = msec_now(),
      last_led = 0, reset_start = 0;
  long long fanspeed = -42, reset_amt = -42, readyval = 0;
  double cpu_temp = -42.0, cpu_volts = -42.0;
  int wantspeed_warned = 0;
  while (!shutdown_sig) {
    long long now = msec_now();

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
      msec_per_led = 1000 / led_sequence_len + 1;
      led_sequence_update(true);
      last_led = now;
    } else {
      led_sequence_update(false);
    }

    if (now - last_time > 2000) {
      // set the fan speed control
      char *wantspeed_str = read_file("fanpercent");
      int wantspeed;
      if (wantspeed_str[0]) {
        wantspeed = strtol(wantspeed_str, NULL, 0);
        if (wantspeed < 0 || wantspeed > 100) {
          if (wantspeed_warned != wantspeed) {
            fprintf(stderr, "gpio/fanpercent (%d) is invalid: must be 0-100\n",
                    wantspeed);
            wantspeed_warned = wantspeed;
          }
          wantspeed = 100;
        } else {
          wantspeed_warned = 0;
        }
      } else {
        if (wantspeed_warned != 1)
            fprintf(stderr, "gpio/fanpercent is empty: using default value\n");
        wantspeed_warned = 1;
        wantspeed = 100;
      }
      set_pwm(&fan_control, wantspeed);

      // capture the fan cycle counter
      write_file_int("fanspeed", &fanspeed,
                     fan_flips * 1000 / (now - last_time + 1));
      fan_flips = reads = 0;

      // capture the CPU temperature and voltage
      write_file_float("cpu_temperature", &cpu_temp, get_cpu_temperature());
      write_file_float("cpu_voltage", &cpu_volts, get_cpu_voltage());
      last_time = now;
    }

    if (now - last_print_time >= 6000) {
      fprintf(stderr,
              "fan_flips:%lld/sec reads:%d button:%d temp:%.2f volts:%.2f\n",
              fanspeed, reads,
              get_gpio(&reset_button), cpu_temp, cpu_volts);
      last_print_time = now;
    }

    // handle the reset button
    int reset = !get_gpio(&reset_button); // 0x1 means *not* pressed
    if (reset) {
      if (!reset_start) reset_start = now - 1;
      write_file_int("reset_button_msecs", &reset_amt, now - reset_start);
    } else {
      if (reset_amt) unlink("reset_button_msecs");
      reset_amt = reset_start = 0;
    }

    // this is last.  it indicates we've made it once through the loop,
    // so all the files in /tmp/gpio have been written at least once.
    write_file_int("ready", &readyval, 1);

    // poll for fan ticks
    for (int tick = 0; tick < inner_loop_ticks; tick++) {
      cur_fan = get_gpio(&fan_tick);
      if (last_fan && !cur_fan)
        fan_flips++;
      reads++;
      last_fan = cur_fan;
      if (shutdown_sig) break;
      usleep(USEC_PER_TICK);
    }
  }

  set_leds_from_bitfields(1);
  set_pwm(&fan_control, 100); // for safety

  // do *not* clean up nicely in the child; we use _exit() instead of
  // returning or calling exit().  A polite shutdown is what the parent
  // process should have done.  No need to do it twice.
  if (shutdown_sig > 0) {
    kill(getpid(), shutdown_sig);
  }
  _exit(0);
}


int main(void) {
  int status = 98;
  fprintf(stderr, "starting gpio mailbox in /tmp/gpio.\n");

  mkdir("/tmp/gpio", 0775);
  if (chdir("/tmp/gpio") != 0) {
    perror("chdir /tmp/gpio");
    return 1;
  }

  NEXUS_PlatformSettings platform_settings;
  NEXUS_Platform_GetDefaultSettings(&platform_settings);
  platform_settings.openFrontend = false;
  if (NEXUS_Platform_Init(&platform_settings) != 0)
      goto end;

  // Fork into the background so we can shut down most of our copy of nexus.
  // Otherwise it leaves things like the video threads running, which results
  // in a mess.  But it happens that the gpio/pwm stuff is just done through
  // mmap, which will be inherited across a fork, unlike all the extra junk
  // threads.
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    _exit(99);
  } else if (pid == 0) {
    // child process
    run_gpio_mailbox();
    _exit(0);
  }

  // parent process.  Uninit nexus here, to kill the unnecessary threads.
  NEXUS_Platform_Uninit();

  // now wait for the child process to exit so we can propagate its exit
  // code to our own parent, who can make decisions about restarting.
  while (waitpid(pid, &status, 0) != pid) { }

end:
  // normally the child process does this step.
  //
  // do it again here just in case the child process dies early; the boot
  // process will wait on this file, and we don't want it to get jammed
  // forever.
  write_file_int("/var/run/gpio-mailbox", NULL, getpid());

  exit(status);
}
