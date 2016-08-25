#ifdef BROADCOM

#define _POSIX_C_SOURCE 199309L /* for clock_gettime */
#define _BSD_SOURCE             /* for usleep */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stacktrace.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "brcm-platform.h"
#include "pin.h"

struct PinHandle_s {
  int   unused;
};

#define CHECK(x) do { \
    int rv = (x); \
    if (rv) { \
      fprintf(stderr, "CHECK: %s returned %d\n", #x, rv); \
      _exit(99); \
    } \
  } while (0)

struct platform_info *platform = NULL;

// Same as time(), but in monotonic clock milliseconds instead.
static long long msec_now(void) {
  struct timespec ts;
  CHECK(clock_gettime(CLOCK_MONOTONIC, &ts));
  return ((long long)ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
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


/* API follows */

int has_fan(void) {
  return (platform->fan_control.is_present);
}

/*
 * We're polling at a very high frequency, which is a pain.  This would be
 * slightly less gross inside the kernel (for less context switching and
 * because it could more easily use the tick interrupt instead of polling).
 *
 * This setting isn't as bad as it sounds, though, because we don't poll
 * 100% of the time; we only do it for a fraction of a second every now
 * and then.
 *
 * Fans in CPE1.0 generate 2 pulses per revolution
 */
#define FAN_POLL_HZ             2000
#define FAN_USEC_PER_TICK       (1000000 / (FAN_POLL_HZ))
#define PULSES_PER_REV          2

int get_fan(void) {
  long long start = 0, end = 0;
  int inner_loop_ticks = 0;
  int reads = 0, fan_flips = 0;
  long fan_loop_time = 0;
  int divider = 20;    // poll for 1/20th of a sec
  int start_fan = get_gpio(&platform->fan_tick), last_fan = start_fan;
  inner_loop_ticks = FAN_POLL_HZ / divider + 1;
  for (int tick = 0; tick < inner_loop_ticks; tick++) {
    int cur_fan = get_gpio(&platform->fan_tick);
    if (last_fan != cur_fan && start_fan == cur_fan) {
      if (!start) {
        start = msec_now();
      } else {
        fan_flips++;
        end = msec_now();
      }
    }
    reads++;
    last_fan = cur_fan;
    usleep(FAN_USEC_PER_TICK);
  }
  fan_loop_time += end - start;

  // return pulses/sec from the fan
  // number of pulses/rotation varies with fan model, so this isn't rpm
  return (fan_flips * 1000 / (fan_loop_time + 1) / PULSES_PER_REV);
}

void set_fan(int wantspeed) {
  set_pwm(&platform->fan_control, wantspeed);
}

double get_cpu_temperature(void) {
  if (platform->temp_monitor.get_temp)
    return platform->temp_monitor.get_temp(&platform->temp_monitor);
  return -1;
}

double get_cpu_voltage(void) {
  if (platform->voltage_monitor.get_voltage)
    return platform->voltage_monitor.get_voltage(&platform->voltage_monitor);
  return -1;
}

int get_reset_button() {
  return !get_gpio(&platform->reset_button);    /* inverted */
}

int has_red_led(void) {
  return (platform->leds.led_red.is_present);
}

int has_blue_led(void) {
  return (platform->leds.led_blue.is_present);
}

int has_activity_led(void) {
  return (platform->leds.led_activity.is_present);
}

int has_standby_led(void) {
  return (platform->leds.led_standby.is_present);
}

int get_red_led(void) {
  return get_gpio(&platform->leds.led_red);
}

int get_blue_led(void) {
  return get_gpio(&platform->leds.led_blue);
}

int get_activity_led(void) {
  return get_gpio(&platform->leds.led_activity);
}

int get_standby_led(void) {
  return get_gpio(&platform->leds.led_standby);
}

/* TODO(doughorn): The set_*_led functions should apply the brightness
   as well */
void set_red_led(int level) {
  set_gpio(&platform->leds.led_red, level ? 1 : 0);
}

void set_blue_led(int level) {
  set_gpio(&platform->leds.led_blue, level ? 1 : 0);
}

void set_activity_led(int level) {
  set_gpio(&platform->leds.led_activity, level ? 1 : 0);
}

void set_standby_led(int level) {
  set_gpio(&platform->leds.led_standby, level ? 1 : 0);
}

void set_led_brightness(int level) {
  set_pwm(&platform->leds.led_brightness, level);
}

/* standard API follows */

PinHandle PinCreate(void) {
  PinHandle handle = (PinHandle) calloc(1, sizeof (*handle));
  if (handle == NULL) {
    perror("calloc(PinHandle)");
    return NULL;
  }

  platform = get_platform_info(read_file("/etc/platform"));
  if (platform_init(platform) < 0) {
    fprintf(stderr, "platform_init failed\n");
    PinDestroy(handle);
    return NULL;
  }

  return handle;
}

void PinDestroy(PinHandle handle) {
  if (handle == NULL)
    return;

  platform_cleanup();

  free(handle);
}

int PinIsPresent(PinHandle handle, PinId id) {
  if (handle == NULL) return PIN_ERROR;
  switch (id) {
    case PIN_LED_RED:
      return has_red_led();

    case PIN_LED_BLUE:
      return has_blue_led();

    case PIN_LED_ACTIVITY:
      return has_activity_led();

    case PIN_LED_STANDBY:
      return has_standby_led();

    case PIN_FAN_CHASSIS:
      return has_fan();

    case PIN_BUTTON_RESET:
    case PIN_TEMP_CPU:
    case PIN_MVOLTS_CPU:
      return 1;

    case PIN_TEMP_EXTERNAL:
    case PIN_NONE:
    case PIN_MAX:
      return 0;
  }
  return 0;
}

PinStatus PinValue(PinHandle handle, PinId id, int* valueP) {
  if (handle == NULL) return PIN_ERROR;
  switch (id) {
    case PIN_LED_RED:
      *valueP = get_red_led();
      break;

    case PIN_LED_BLUE:
      *valueP = get_blue_led();
      break;

    case PIN_LED_ACTIVITY:
      *valueP = get_activity_led();
      break;

    case PIN_LED_STANDBY:
      *valueP = get_standby_led();
      break;

    case PIN_BUTTON_RESET:
      *valueP = get_reset_button();
      break;

    case PIN_TEMP_CPU:
      *valueP = (int)(get_cpu_temperature() * 1000);
      break;

    case PIN_MVOLTS_CPU:
      *valueP = (int)(get_cpu_voltage() * 1000);
      break;

    case PIN_FAN_CHASSIS:
      *valueP = get_fan();
      break;

    case PIN_TEMP_EXTERNAL:
    case PIN_NONE:
    case PIN_MAX:
      *valueP = -1;
      return PIN_ERROR;
  }
  return PIN_OKAY;
}

PinStatus PinSetValue(PinHandle handle, PinId id, int value) {
  if (handle == NULL) return PIN_ERROR;
  switch (id) {
    case PIN_LED_RED:
      set_red_led(value);
      break;

    case PIN_LED_BLUE:
      set_blue_led(value);
      break;

    case PIN_LED_ACTIVITY:
      set_activity_led(value);
      break;

    case PIN_LED_STANDBY:
      set_standby_led(value);
      break;

    case PIN_FAN_CHASSIS:
      set_fan(value);
      break;

    case PIN_BUTTON_RESET:
    case PIN_MVOLTS_CPU:
    case PIN_TEMP_CPU:
    case PIN_TEMP_EXTERNAL:
    case PIN_NONE:
    case PIN_MAX:
      return PIN_ERROR;
  }
  return PIN_OKAY;
}
#endif /* BROADCOM */
