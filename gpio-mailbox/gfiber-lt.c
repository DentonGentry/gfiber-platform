#ifdef GFIBER_LT

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "fileops.h"
#include "pin.h"

struct PinHandle_s {
  int   unused;
};

struct Gpio {
  int is_present;
  const char* file_path;
};


struct platform_info {
  const char *name;
  struct Gpio led_red;
  struct Gpio led_blue;
  struct Gpio temp_cpu;
};

struct platform_info platforms[] = {
  {
    .name = "GFLT200",
    .led_red = {
      .file_path = "/sys/devices/platform/board/leds:sys-red/brightness",
    },
    .led_blue = {
      .file_path = "/sys/devices/platform/board/leds:sys-blue/brightness",
    },
    .temp_cpu = {
      .file_path = "/sys/devices/platform/KW2Thermal.0/temp1_input",
    },
  }
};

struct platform_info *platform = &platforms[0];

// Write the given GPIO pin.
static void set_gpio(struct Gpio *g, int level) {
  write_file_int(g->file_path, NULL, level);
}

// Read the given GPIO pin
static int get_gpio(struct Gpio *g) {
  return read_file_long(g->file_path);
}

/* standard API follows */

PinHandle PinCreate(void) {
  PinHandle handle = (PinHandle) calloc(1, sizeof (*handle));
  if (handle == NULL) {
    perror("calloc(PinHandle)");
    return NULL;
  }
  return handle;
}

void PinDestroy(PinHandle handle) {
  if (handle == NULL)
    return;

  free(handle);
}

int PinIsPresent(PinHandle handle, PinId id) {
  if (handle == NULL) return PIN_ERROR;
  switch (id) {
    case PIN_LED_RED:
    case PIN_LED_BLUE:
    case PIN_TEMP_CPU:
      return 1;

    default:
      return 0;
  }
  return 0;
}

PinStatus PinValue(PinHandle handle, PinId id, int* valueP) {
  if (handle == NULL) return PIN_ERROR;
  switch (id) {
    case PIN_LED_RED:
      *valueP = get_gpio(&platform->led_red);
      break;

    case PIN_LED_BLUE:
      *valueP = get_gpio(&platform->led_blue);
      break;

    case PIN_TEMP_CPU:
      *valueP = get_gpio(&platform->temp_cpu);
      break;

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
      set_gpio(&platform->led_red, value);
      break;

    case PIN_LED_BLUE:
      set_gpio(&platform->led_blue, value);
      break;

    default:
      return PIN_ERROR;
  }
  return PIN_OKAY;
}
#endif /* GFIBER_LT */
