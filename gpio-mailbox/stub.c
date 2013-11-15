#ifdef STUB

#define _BSD_SOURCE     /* for M_PI */
#include <features.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <math.h>

#include "pin.h"

struct PinHandle_s {
  int           want[PIN_MAX];
  int           have[PIN_MAX];
  time_t        last_sim;
};

/* simulation loop length seconds */
#define PERIOD          30

static void simulate(PinHandle handle) {
  time_t t1 = time(NULL);
  int dt = t1 - handle->last_sim;

  /* leds */
  int change = 0;
  int changed[PIN_MAX] = { };
  for (int i = PIN_LED_RED; i <= PIN_LED_STANDBY; i++) {
    if (handle->have[i] != handle->want[i]) {
      handle->have[i] = handle->want[i];
      changed[i] = 1;
      change++;
    }
  }

  if (dt > 0) {
    int point = t1 % PERIOD;
    double curve = sin(2 * M_PI * point / (double)PERIOD);

    /* some jitter */
    handle->have[PIN_TEMP_CPU] = 70000 + curve * 30000;         /* milli degrees C */
    handle->have[PIN_TEMP_EXTERNAL] = 40000 - curve * 10000;    /* milli degrees C */
    handle->have[PIN_MVOLTS_CPU] = 3300 + curve * 100;          /* 3.3 v */


    /* fan takes a few seconds to adjust to requests */
    int want = handle->want[PIN_FAN_CHASSIS];
    if (want < 0) want = 0;
    if (want > 100) want = 100;
    int dfan = want - handle->have[PIN_FAN_CHASSIS];
    if (dfan) {
      int adjust = .5 * dfan;
      if (adjust == 0) adjust = dfan;
      handle->have[PIN_FAN_CHASSIS] += adjust;
      changed[PIN_FAN_CHASSIS] = 1;
    }

    handle->last_sim = t1;
  }

  if (dt > 0 || change) {
    for (int i = 1; i < PIN_MAX; i++) {
      fprintf(stderr, "%c%d/%d ", changed[i] ? '*' : ' ', handle->want[i], handle->have[i]);
    }
    fprintf(stderr, "\n");
  }
}

PinHandle PinCreate(void) {
  PinHandle handle = (PinHandle) calloc(1, sizeof (*handle));
  if (handle == NULL) {
    perror("malloc");
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
  return 1;
}

PinStatus PinValue(PinHandle handle, PinId id, int* valueP) {
  simulate(handle);
  *valueP = handle->have[id];
  return PIN_OKAY;
}

PinStatus PinSetValue(PinHandle handle, PinId id, int value) {
  handle->want[id] = value;
  simulate(handle);
  return PIN_OKAY;
}
#endif /* MINDSPEED */
