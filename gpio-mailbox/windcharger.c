#ifdef WINDCHARGER

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

#include "fileops.h"
#include "pin.h"

#define DEVMEM                "/dev/mem"
#define GPIO_OUT_FUNCTION0    0xB
#define GPIO_OUT_ENABLE       0x0
#define GPIO_CNTL_PER_REG     4

/* CPU Temperature Monitoring. */
#define SYS_TEMP_DIR    "/sys/devices/virtual/hwmon/hwmon0/"
#define SYS_TEMP1       SYS_TEMP_DIR "temp1_input"

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
static uint32_t mmap_offset;
static size_t mmap_size = 0;
static int mmap_fd = -1;

struct Gpio {
  int is_present;

  /* Pin number */
  unsigned int shift;

  /* for offset_direction and offset_data */
  unsigned int direction_value;         // 0 is output
  int old_val;
};

struct platform_info {
  const char *name;
  off_t mmap_base;
  size_t mmap_size;
  void (*init)(struct platform_info* p);

  unsigned in_offset;
  unsigned out_offset;
  unsigned set_offset;
  unsigned clear_offset;

  struct Gpio led_red;
  struct Gpio led_blue;
  struct Gpio reset_button;
};

struct platform_info platforms[] = {
  {
    .name = "GFMN100",
    .mmap_base = 0x18040000,
    .mmap_size = 0x40,
    .in_offset = 0x1,
    .out_offset = 0x2,
    .set_offset = 0x3,
    .clear_offset = 0x4,
    .led_red = {
      .is_present = 1,
      .shift = 16,
      .direction_value = 0,
      .old_val = -1,
    },
    .led_blue = {
      .is_present = 1,
      .shift = 11,
      .direction_value = 0,
      .old_val = -1,
    },
   .reset_button = {
      .is_present = 1,
      .shift = 13,
      .direction_value = 1,
      .old_val = -1,
    },
  }
};

struct platform_info *platform = NULL;

// Write the given GPIO pin.
static void set_gpio(struct Gpio *g, int level) {
  volatile uint32_t* reg = mmap_addr;

  if (g->old_val == level) {
    return;
  }
  g->old_val = level;

  reg += (level > 0) ? platform->set_offset : platform->clear_offset;
  *reg = 1 << g->shift;
}

// Read the given GPIO pin
static int get_gpio(int pin) {
  volatile uint32_t* reg = mmap_addr;
  uint32_t value;

  reg += platform->in_offset;
  value = *reg;
  return (value >> pin) & 0x1;
}

static int get_temp1(PinHandle handle) {
  return read_file_long(SYS_TEMP1);
}

// initialize GPIO to input or output
static void set_direction(struct Gpio *g)
{
  volatile uint32_t* reg = mmap_addr;
  uint32_t data, reg_addr;

  reg_addr = GPIO_OUT_ENABLE;
  reg += reg_addr;
  data = *reg;
  data &= ~(1 << g->shift);
  data |= 0 << g->shift;
  *reg = data;
}

// initialize pin to LED or GPIO etc
static void set_pinmux(struct Gpio *g) {
  volatile uint32_t* reg = mmap_addr;
  uint32_t data, reg_addr, addr_offset, byte_offset;

  addr_offset = (g->shift / GPIO_CNTL_PER_REG) ;
  byte_offset = g->shift - addr_offset;
  reg_addr = GPIO_OUT_FUNCTION0 + addr_offset;

  reg += reg_addr;
  data = *reg;
  data &= ~(0xFF << (8 * byte_offset));
  data |= 0 << (8 * byte_offset);
  *reg = data;
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
  int page_size;
  uint64_t file_start;

  platform_cleanup();
  mmap_fd = open(DEVMEM, O_RDWR);
  if (mmap_fd < 0) {
    perror(DEVMEM);
    return -1;
  }

  page_size = getpagesize();

  /* map to file at file_start, which has to be page aligned */
  file_start = (p->mmap_base / page_size) * page_size;
  mmap_offset = p->mmap_base % page_size;

  mmap_size = p->mmap_size;
  mmap_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   mmap_fd, file_start);
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

int has_cpu_temp(void) {
  return 1;
}

int has_red_led(void) {
  return (platform->led_red.is_present);
}

int has_blue_led(void) {
  return (platform->led_blue.is_present);
}

int has_reset_button(void) {
  return (platform->reset_button.is_present);
}

int get_red_led(void) {
  return get_gpio(platform->led_red.shift);
}

int get_blue_led(void) {
  return get_gpio(platform->led_blue.shift);
}

void set_red_led(int level) {
  set_gpio(&platform->led_red, level ? 1 : 0);
}

void set_blue_led(int level) {
  set_gpio(&platform->led_blue, level ? 1 : 0);
}

static void init_platform(struct platform_info* p) {
  if (p->init) {
    (*p->init)(p);
  }
}

static void initialize_gpios(void) {
  init_platform(platform);

  set_pinmux(&platform->led_red);
  set_pinmux(&platform->led_blue);

  set_direction(&platform->led_red);
  set_direction(&platform->led_blue);
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

    case PIN_BUTTON_RESET:
      return has_reset_button();

    case PIN_TEMP_CPU:
      return has_cpu_temp();

    case PIN_MVOLTS_CPU:
    case PIN_FAN_CHASSIS:
    case PIN_TEMP_EXTERNAL:
    case PIN_NONE:
    case PIN_MAX:
    default:
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

    case PIN_BUTTON_RESET:
      *valueP = !get_gpio(platform->reset_button.shift);
      break;

    case PIN_TEMP_CPU:
      *valueP = get_temp1(handle);
      break;
    case PIN_MVOLTS_CPU:
    case PIN_TEMP_EXTERNAL:
    case PIN_NONE:
    case PIN_MAX:
    default:
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
    case PIN_LED_STANDBY:
    case PIN_FAN_CHASSIS:
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
#endif /* WINDCHARGER */
