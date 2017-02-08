#ifdef GFRG240

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

struct systemp {
  const char* value_path;
};

struct sysgpio {
  const char* value_path;
};

struct platform_info {
  const char *name;
  struct systemp temp_cpu;
  struct sysgpio led_red;
  struct sysgpio led_activity;
};

struct platform_info platforms[] = {
  {
    .name = "GFRG240",
    .temp_cpu = {
      .value_path = "/sys/class/hwmon/hwmon0/temp1_input",
    },
    .led_red = {
      .value_path = "/sys/class/leds/pca955x:1/brightness",
    },
    .led_activity = {
      .value_path = "/sys/class/leds/pca955x:0/brightness",
    },
  }
};

struct platform_info *platform = &platforms[0];

// Write the given sysfile
static void set_sysfile(const char* s, int level) {
  write_file_int(s, NULL, level);
}

// Read the given sysfile pin
static int get_sysfile(const char* s) {
  return read_file_long(s);
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
    case PIN_LED_ACTIVITY:
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
      *valueP = get_sysfile(platform->led_red.value_path);
      break;
    case PIN_LED_ACTIVITY:
      *valueP = get_sysfile(platform->led_activity.value_path);
      break;
    case PIN_TEMP_CPU:
      *valueP = get_sysfile(platform->temp_cpu.value_path);
      break;

    default:
      *valueP = -1;
      return PIN_ERROR;
  }
  return PIN_OKAY;
}

PinStatus PinSetValue(PinHandle handle, PinId id, int value) {
  int scaled;

  if (handle == NULL) return PIN_ERROR;

  /*
   * gfrg240 led brightness control range is 0-255, value comes in 0-100.
   * scale value up 2.5x so that we get the full brightness range.
   */
  scaled = (value * 25) / 10;
  switch (id) {
    case PIN_LED_RED:
      set_sysfile(platform->led_red.value_path, scaled);
      break;
    case PIN_LED_ACTIVITY:
      set_sysfile(platform->led_activity.value_path, scaled);
      break;
    default:
      return PIN_ERROR;
  }
  return PIN_OKAY;
}
#endif /* GFRG240 */
