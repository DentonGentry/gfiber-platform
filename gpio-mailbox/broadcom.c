#ifdef BROADCOM

#define _POSIX_C_SOURCE 199309L /* for clock_gettime */
#define _BSD_SOURCE             /* for usleep */
#include <features.h>

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <stacktrace.h>

#include "pin.h"

#define DEVMEM          "/dev/mem"

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

static volatile void* mmap_addr = MAP_FAILED;
static size_t mmap_size = 0;
static int mmap_fd = -1;


struct Gpio {
  int is_present;
  unsigned int offset_direction;
  unsigned int offset_data;
  unsigned int mask;                    // eg, (*reg & mask) >> shift == on_value
  unsigned int shift;
  unsigned int off_value;
  unsigned int on_value;
  unsigned int direction_value;         // 0 is output
  int old_val;
};


struct Fan {
  int is_present;
  unsigned int offset_data;
  unsigned int channel;
  int old_percent;
};


struct Temp {
  int is_present;
  unsigned int offset_data;
};


struct Voltage {
  int is_present;
  unsigned int offset_data;
};


struct platform_info {
  const char *name;
  off_t mmap_base;
  size_t mmap_size;
  struct Gpio led_red;
  struct Gpio led_blue;
  struct Gpio led_activity;
  struct Gpio led_standby;
  struct Gpio reset_button;
  struct Gpio fan_tick;
  struct Fan fan_control;
  struct Temp temp_monitor;
  struct Voltage voltage_monitor;
};

struct platform_info platforms[] = {
  {
    .name = "GFHD100",
    .mmap_base = 0x10400000,            // base of many brcm registers
    .mmap_size = 0x40000,
    .led_red = {
      .is_present = 1,                  // GPIO 17
      .offset_direction = 0x94c8,       // GIO_AON_IODIR_LO
      .offset_data = 0x94c4,            // GIO_AON_DATA_LO
      .mask = 0x00020000,               // 1<<17
      .shift = 17,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 0,
      .old_val = -1,
    },
    .led_blue = {
      .is_present = 1,                  // GPIO 12
      .offset_direction = 0x94c8,       // GIO_AON_IODIR_LO
      .offset_data = 0x94c4,            // GIO_AON_DATA_LO
      .mask = 0x00001000,               // 1<<12
      .shift = 12,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 0,
      .old_val = -1,
    },
    .led_activity = {
      .is_present = 1,                  // GPIO 13
      .offset_direction = 0x94c8,       // GIO_AON_IODIR_LO
      .offset_data = 0x94c4,            // GIO_AON_DATA_LO
      .mask = 0x00002000,               // 1<<13
      .shift = 13,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 0,
      .old_val = -1,
    },
    .led_standby = {
      .is_present = 1,                  // GPIO 10
      .offset_direction = 0x94c8,       // GIO_AON_IODIR_LO
      .offset_data = 0x94c4,            // GIO_AON_DATA_LO
      .mask = 0x00000400,               // 1<<10
      .shift = 10,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 0,
      .old_val = -1,
    },
    .reset_button = {
      .is_present = 1,                  // GPIO 4
      .offset_direction = 0x94c8,       // GIO_AON_IODIR_LO
      .offset_data = 0x94c4,            // GIO_AON_DATA_LO
      .mask = 0x00000010,               // 1<<4
      .shift = 4,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 1,
      .old_val = -1,
    },
    .fan_tick = {
      .is_present = 1,                  // GPIO 98
      .offset_direction = 0x6768,       // GIO_IODIR_EXT_HI
      .offset_data = 0x6764,            // GIO_DATA_EXT_HI
      .mask = 0x00000100,               // 1<<8
      .shift = 8,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 1,
      .old_val = -1,
    },
    .fan_control = {
      .is_present = 1,                  // PWM 1
      .offset_data = 0x6580,            // PWM_CTRL ...
      .channel = 0,
    },
    .temp_monitor = {
      .is_present = 1,                  // 7425 AVS_RO_REGISTERS_0
      .offset_data = 0x32b00,           // BCHP_AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS
    },
    .voltage_monitor = {
      .is_present = 1,                  // 7425 AVS_RO_REGISTERS_0
      .offset_data = 0x32b0c,           // BCHP_AVS_RO_REGISTERS_0_PVT_1P10V_0_MNTR_STATUS
    },
  },
  {
    .name = "GFMS100",
    .mmap_base = 0x10400000,            // base of many brcm registers
    .mmap_size = 0x40000,
    .led_red = {
      .is_present = 1,                  // GPIO 17
      .offset_direction = 0x94c8,       // GIO_AON_IODIR_LO
      .offset_data = 0x94c4,            // GIO_AON_DATA_LO 0..17
      .mask = 0x00020000,               // 1<<17
      .shift = 17,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 0,
      .old_val = -1,
    },
    .led_blue = {
      .is_present = 0,
    },
    .led_activity = {
      .is_present = 1,                  // GPIO 13
      .offset_direction = 0x94c8,       // GIO_AON_IODIR_LO
      .offset_data = 0x94c4,            // GIO_AON_DATA_LO
      .mask = 0x00002000,               // 1<<13
      .shift = 13,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 0,
      .old_val = -1,
    },
    .led_standby = {
      .is_present = 0,
    },
    .reset_button = {
      .is_present = 1,                  // GPIO 4
      .offset_direction = 0x94c8,       // GIO_AON_IODIR_LO
      .offset_data = 0x94c4,            // GIO_AON_DATA_LO
      .mask = 0x00000010,               // 1<<4
      .shift = 4,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 1,
      .old_val = -1,
    },
    .fan_tick = {
      .is_present = 1,                  // GPIO 98
      .offset_direction = 0x6768,       // GIO_IODIR_EXT_HI
      .offset_data = 0x6764,            // GIO_DATA_EXT_HI
      .mask = 0x00000100,               // 1<<8
      .shift = 8,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 1,
      .old_val = -1,
    },
    .fan_control = {
      .is_present = 1,                  // PWM 1
      .offset_data = 0x6580,            // PWM_CTRL ...
      .channel = 0,
    },
    .temp_monitor = {
      .is_present = 1,                  // 7425 AVS_RO_REGISTERS_0
      .offset_data = 0x32b00,           // BCHP_AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS
    },
    .voltage_monitor = {
      .is_present = 1,                  // 7425 AVS_RO_REGISTERS_0
      .offset_data = 0x32b0c,           // BCHP_AVS_RO_REGISTERS_0_PVT_1P10V_0_MNTR_STATUS
    },
  },
  {
    .name = "GFHD200",
    .mmap_base = 0x10400000,            // AON_PIN_CTRL ...
    .mmap_size = 0x30000,
    .led_red = {
      .is_present = 1,                  // GPIO 5
      .offset_direction = 0x9808,       // GIO_AON_IODIR_LO
      .offset_data = 0x9804,            // GIO_AON_DATA_LO
      .mask = 0x00000020,               // 1<<5
      .shift = 5,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 0,
      .old_val = -1,
    },
    .led_blue = {
      .is_present = 0,
    },
    .led_activity = {
      .is_present = 1,                  // GPIO 4
      .offset_direction = 0x9808,       // GIO_AON_IODIR_LO
      .offset_data = 0x9804,            // GIO_AON_DATA_LO
      .mask = 0x00000010,               // 1<<4
      .shift = 4,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 0,
      .old_val = -1,
    },
    .led_standby = {
      .is_present = 0,
    },
    .reset_button = {
      .is_present = 1,                  // GPIO 3
      .offset_direction = 0x9808,       // GIO_AON_IODIR_LO
      .offset_data = 0x9804,            // GIO_AON_DATA_LO
      .mask = 0x00000008,               // 1<<3
      .shift = 3,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 1,
      .old_val = -1,
    },
    .fan_control = {
      .is_present = 0,
    },
    .temp_monitor = {
      .is_present = 1,                  // 7429 AVS_RO_REGISTERS_0
      .offset_data = 0x23300,           // BCHP_AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS
    },
    .voltage_monitor = {
      .is_present = 1,                  // 7429 AVS_RO_REGISTERS_0
      .offset_data = 0x2330c,           // BCHP_AVS_RO_REGISTERS_0_PVT_1P10V_0_MNTR_STATUS
    },
  }
};

struct platform_info *platform = NULL;

// Set the given PWM (pulse width modulator) to the given percent duty cycle.
static void set_pwm(struct Fan *f, int percent) {
  volatile uint32_t* reg;
  uint32_t mask0, val0, mask1, val1, on;

  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  if (f->old_percent == percent) return;
  f->old_percent = percent;

  reg = mmap_addr + f->offset_data;
  if (f->channel == 0) {
    mask0 = 0xf0;       // preserve other channel
    val0 = 0x09;        // open-drain|start
    mask1 = 0x10;       // preserve
    val1 = 0x01;        // constant-freq
    on = 6;
  } else {
    mask0 = 0x0f;       // see above
    val0 = 0x90;
    mask1 = 0x01;
    val1 = 0x10;
    on = 8;
  }
  reg[0] = (reg[0] & mask0) | val0;
  reg[1] = (reg[1] & mask1) | val1;
  reg[on] = 0x63 * percent/100;         // 0x63 is what old code used
  reg[on+1] = 0x63;
}

// Get the CPU temperature.  I think it's in Celsius.
static double get_avs_temperature(struct Temp* t) {
  volatile uint32_t* reg;
  uint32_t value, valid, raw_data;

  reg = mmap_addr + t->offset_data;
  value = *reg;
  // see 7425-PR500-RDS.pdf
  valid = (value & 0x00000400) >> 10;
  raw_data = value & 0x000003ff;
  if (!valid) return -1.0;
  return (418000 - (556 * raw_data)) / 1000.0;
}

static double get_avs_voltage(struct Voltage* v) {
  volatile uint32_t* reg;
  uint32_t value, valid, raw_data;

  reg = mmap_addr + v->offset_data;
  value = *reg;
  // see 7425-PR500-RDS.pdf
  valid = (value & 0x00000400) >> 10;
  raw_data = value & 0x000003ff;
  if (!valid) return -1.0;
  return ((990 * raw_data * 8) / (7*1024)) / 1000.0;
}

// Write the given GPIO pin.
static void set_gpio(struct Gpio *g, int level) {
  volatile uint32_t* reg;
  uint32_t value;

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

  reg = mmap_addr + g->offset_data;
  value = *reg;
  value &= ~g->mask;
  value |= (level ? g->on_value : g->off_value) << g->shift;
  *reg = value;
}


// Read the given GPIO pin
static int get_gpio(struct Gpio *g) {
  volatile uint32_t* reg;
  uint32_t value;

  reg = mmap_addr + g->offset_data;
  value = (*reg & g->mask) >> g->shift;
  return (value == g->on_value);
}


// initialize GPIO to input or output
static void set_direction(struct Gpio *g)
{
  volatile uint32_t* reg;
  uint32_t value;

  if (!g->is_present)
    return;

  reg = mmap_addr + g->offset_direction;
  value = *reg;
  value &= ~g->mask;
  value |= g->direction_value << g->shift;
  *reg = value;
}

// Same as time(), but in monotonic clock milliseconds instead.
static long long msec_now(void) {
  struct timespec ts;
  CHECK(clock_gettime(CLOCK_MONOTONIC, &ts));
  return ((long long)ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
}

static void platform_cleanup(void) {
  if (mmap_addr != MAP_FAILED) {
    if (munmap((void*) mmap_addr, mmap_size) < 0) {
      perror("munmap");
    }
    mmap_addr = MAP_FAILED;
    mmap_size = 0;
  }
  if (mmap_fd >= 0) {
    close(mmap_fd);
    mmap_fd = -1;
  }
}

static int platform_init(struct platform_info* p) {
  platform_cleanup();

  mmap_fd = open(DEVMEM, O_RDWR);
  if (mmap_fd < 0) {
    perror(DEVMEM);
    return -1;
  }
  mmap_size = p->mmap_size;
  mmap_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   mmap_fd, p->mmap_base);
  if (mmap_addr == MAP_FAILED) {
    perror("mmap");
    platform_cleanup();
    return -1;
  }
  return 0;
}

static struct platform_info *get_platform_info(const char *platform_name) {
  int lim = sizeof(platforms) / sizeof(platforms[0]);
  for (int i = 0; i < lim; ++i) {
    struct platform_info *p = &platforms[i];
    if (0 == strncmp(platform_name, p->name, strlen(p->name))) {
      return p;
    }
  }
  fprintf(stderr, "No support for platform %s", platform_name);
  exit(1);
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
 */
#define FAN_POLL_HZ             2000
#define FAN_USEC_PER_TICK       (1000000 / (FAN_POLL_HZ))

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
  return (fan_flips * 1000 / (fan_loop_time + 1));
}

void set_fan(int wantspeed) {
  set_pwm(&platform->fan_control, wantspeed);
}

double get_cpu_temperature(void) {
  return get_avs_temperature(&platform->temp_monitor);
}

double get_cpu_voltage(void) {
  return get_avs_voltage(&platform->voltage_monitor);
}

int get_reset_button() {
  return !get_gpio(&platform->reset_button);    /* inverted */
}

int has_red_led(void) {
  return (platform->led_red.is_present);
}

int has_blue_led(void) {
  return (platform->led_blue.is_present);
}

int has_activity_led(void) {
  return (platform->led_activity.is_present);
}

int has_standby_led(void) {
  return (platform->led_standby.is_present);
}

int get_red_led(void) {
  return get_gpio(&platform->led_red);
}

int get_blue_led(void) {
  return get_gpio(&platform->led_blue);
}

int get_activity_led(void) {
  return get_gpio(&platform->led_activity);
}

int get_standby_led(void) {
  return get_gpio(&platform->led_standby);
}

void set_red_led(int level) {
  set_gpio(&platform->led_red, level ? 1 : 0);
}

void set_blue_led(int level) {
  set_gpio(&platform->led_blue, level ? 1 : 0);
}

void set_activity_led(int level) {
  set_gpio(&platform->led_activity, level ? 1 : 0);
}

void set_standby_led(int level) {
  set_gpio(&platform->led_standby, level ? 1 : 0);
}

static void initialize_gpios(void) {
  set_direction(&platform->led_red);
  set_direction(&platform->led_blue);
  set_direction(&platform->led_activity);
  set_direction(&platform->led_standby);
  set_direction(&platform->reset_button);
  set_direction(&platform->fan_tick);
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
  initialize_gpios();
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
