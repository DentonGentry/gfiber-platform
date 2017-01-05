#ifdef ANDROID    /* Should work fine on non Android platforms, but is only necessary on Android */
#ifdef BROADCOM

/* Broadcom platform implementation using the NEXUS API.
   Only the GFHD254 is supported via this API as of now. */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brcm-platform.h"

#include "nexus_avs.h"
#include "nexus_pwm.h"
#include "nexus_gpio.h"
#include "nxclient.h"

#define UNUSED        __attribute__((unused))
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))

static void init_gfhd254(struct platform_info* p);
static double get_avs_voltage_7252(struct Voltage* v);
static double get_avs_temperature_7252(struct Temp* t);

/* This is an array. Of structs! It contains structs of
   the type platform_info. The platform_info struct provides
   much useful information for use in all sorts of fun
   operations. Structs are considered "aggregate types",
   which you can of course read all about by pointing your
   browser at Google and searching for your favorite version
   of our beloved C standard, such as ISO/IEC 9899:TC3! */
struct platform_info platforms[] = {
  {
    .name = "GFHD254",
    .init = init_gfhd254,
    .leds = {
      .led_red = {
        .is_present = 1,                  // AON_GPIO_05
        .type = AON,
        .pin = 5,
        .old_val = -1,
      },
      .led_blue = {
        .is_present = 0,
      },
      .led_activity = {
        .is_present = 1,                  // AON_GPIO_04
        .pin = 4,
        .type = AON,
        .old_val = -1,
      },
      .led_standby = {
        .is_present = 0,
      },
      .led_brightness = {
        .is_present = 1,                // GPIO_098
        .open_drain = 0,
        .pwm_index = 2,
        .old_percent = -1,
      },
    },
    .reset_button = {
      .is_present = 1,                  // GPIO_009
      .pin = 9,
      .type = STANDARD,
      .old_val = -1,
    },
    .fan_tick = {
      .is_present = 1,                  // GPIO 78
      .pin = 78,
      .type = STANDARD,
      .old_val = -1,
    },
    .fan_control = {
      .is_present = 1,                // GPIO_098
      .open_drain = 0,
      .pwm_index = 3,
      .old_percent = -1,
    },
    .temp_monitor = {
      .is_present = 1,                  // 7252 AVS_RO_REGISTERS_0
      .get_temp = get_avs_temperature_7252,
    },
    .voltage_monitor = {
      .is_present = 1,
      .get_voltage = get_avs_voltage_7252,
    },
  }
};

struct platform_info *get_platform_info(const char *platform_name) {
  for (unsigned int i = 0; i < ARRAYSIZE(platforms); ++i) {
    struct platform_info *p = &platforms[i];
    if (0 == strncmp(platform_name, p->name, strlen(p->name))) {
      return p;
    }
  }
  fprintf(stderr, "No support for platform %s", platform_name);
  return NULL;
}

static NEXUS_GpioType get_nexus_type(enum GpioType type) {
  switch (type) {
    case STANDARD:
      return NEXUS_GpioType_eStandard;
    case AON:
      return NEXUS_GpioType_eAonStandard;
  }

  /* If we added a GpioType and are using it and never updated this, we
     are going to have a bad time somewhere. Return an invalid val and let
     nxclient handle it, while logging the problem. */
  fprintf(stderr, "No matching NEXUS type for GPIO: %d\n", type);
  return -1;
}

// Don't need to set direction for GFHD254
static void initialize_gpio(struct Gpio* gpio) {
  if (!gpio || !gpio->is_present)
    return;

  /* TODO(doughorn): Cannot set pinmux from NEXUS? Can retrieve
     pinmux information but can't set it because it is 'dangerous'.
     So we can use the raw Read/WriteRegister functions. These, however,
     are also dangerous and their "indiscriminate use will result in
     system failure." Seems to work properly without us setting... */
  NEXUS_GpioSettings gpioSettings;
  NEXUS_GpioHandle handle;
  NEXUS_GpioType type = get_nexus_type(gpio->type);

  NEXUS_Gpio_GetDefaultSettings(type, &gpioSettings);
  gpioSettings.mode = NEXUS_GpioMode_eOutputPushPull;
  gpioSettings.interruptMode = NEXUS_GpioInterrupt_eDisabled;

  handle = NEXUS_Gpio_Open(type, gpio->pin, &gpioSettings);
  if (!handle) {
    fprintf(stderr, "Failed opening GPIO pin %d. gpio-mailbox cannot continue.\n", gpio->pin);
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  NEXUS_Gpio_Close(handle);
}

/* TODO(doughorn): We can probably avoid calling GetAvsStatus twice for
   the voltage and temp individually, but the poll rate is low enough
   that it most likely doesn't matter...*/
static double get_avs_voltage_7252(UNUSED struct Voltage* v) {
  NEXUS_AvsStatus status;
  if (NEXUS_GetAvsStatus(&status)) {
    fprintf(stderr, "Could not get AVS status. Aborting...\n");
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  /* NEXUS_AvsStatus.voltage measured in millivolts */
  return (double)status.voltage / 1000;
}

static double get_avs_temperature_7252(UNUSED struct Temp* t) {
  NEXUS_AvsStatus status;

  if (NEXUS_GetAvsStatus(&status)) {
    fprintf(stderr, "Could not get AVS status. Aborting...\n");
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  /* Temp is in thousands of a degree. */
  return (double)status.temperature / 1000;
}

static void init_gfhd254(struct platform_info* p) {
  NEXUS_PwmChannelSettings pwmSettings;
  NEXUS_PwmChannelHandle pwm;
  NEXUS_PwmFreqModeType frequencyMode;

  /* Set the control word for the led brightness PWM to 0xf.
   This is used to control the output frequency from the
   variable rate clock. */
  NEXUS_Pwm_GetDefaultChannelSettings(&pwmSettings);
  pwm = NEXUS_Pwm_OpenChannel(2, &pwmSettings);
  if (NEXUS_Pwm_SetControlWord(pwm, 0xf)) {
    fprintf(stderr, "Failed setting control word for PWM.\n");
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  NEXUS_Pwm_CloseChannel(pwm);

  /* Set the control word for the fan to 0x2000. */
  NEXUS_Pwm_GetDefaultChannelSettings(&pwmSettings);
  pwm = NEXUS_Pwm_OpenChannel(3, &pwmSettings);
  if (NEXUS_Pwm_SetControlWord(pwm, 0x2000)) {
    fprintf(stderr, "Failed setting control word for PWM.\n");
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  NEXUS_Pwm_CloseChannel(pwm);

  set_pwm(&p->leds.led_brightness, 27);
}

void platform_cleanup() {
  NxClient_Uninit();
}

int platform_init(struct platform_info* p) {
  if (NxClient_Join(NULL)) {
    fprintf(stderr, "gpio-mailbox failed to connect to nxserver. Aborting...\n");
    return -1;
  }

  if (p->init) {
    (*p->init)(p);
  }

  initialize_gpio(&p->leds.led_red);
  initialize_gpio(&p->leds.led_blue);
  initialize_gpio(&p->leds.led_activity);
  initialize_gpio(&p->leds.led_standby);
  return 0;
}

void set_gpio(struct Gpio *gpio, int level) {
  if (!gpio || !gpio->is_present || gpio->old_val == level) {
    // If this is the same value as last time, don't do anything, for two
    // reasons:
    //   1) If you set the gpio too often, it seems to stay low (the led
    //      stays off).
    //   2) If some process other than us is twiddling a led, this way we
    //      won't interfere with it.
    return;
  }

  gpio->old_val = level;

  NEXUS_GpioSettings gpioSettings;
  NEXUS_GpioHandle handle;
  uint32_t error;
  NEXUS_GpioType type = get_nexus_type(gpio->type);

  handle = NEXUS_Gpio_Open(type, gpio->pin, NULL);
  if (!handle) {
    fprintf(stderr, "Failed opening GPIO pin %d. Cannot continue.\n", gpio->pin);
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  NEXUS_Gpio_GetSettings(handle, &gpioSettings);
  gpioSettings.value = level ? NEXUS_GpioValue_eHigh : NEXUS_GpioValue_eLow;
  if ((error = NEXUS_Gpio_SetSettings(handle, &gpioSettings))) {
    fprintf(stderr, "Failed setting GPIO pin %d. Cannot continue.\n", gpio->pin);
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  NEXUS_Gpio_Close(handle);
}

int get_gpio(struct Gpio *gpio) {
  if (!gpio || !gpio->is_present)
    return 0;

  NEXUS_GpioStatus status;
  NEXUS_GpioHandle handle;
  uint32_t error;
  NEXUS_GpioType type = get_nexus_type(gpio->type);

  handle = NEXUS_Gpio_Open(type, gpio->pin, NULL);

  if (!handle) {
    fprintf(stderr, "Failed opening GPIO pin %d. Cannot continue.\n", gpio->pin);
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  if ((error = NEXUS_Gpio_GetStatus(handle, &status))) {
    fprintf(stderr, "Failed getting status of GPIO pin %d. Cannot continue.\n", gpio->pin);
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  NEXUS_Gpio_Close(handle);

  return status.value != NEXUS_GpioValue_eLow;
}

/*
  Set the pwm. See set_pwm in brcm-direct.c
  for details.
*/
void set_pwm(struct PwmControl *f, int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  if (percent == f->old_percent) return;
  f->old_percent = percent;
  uint32_t period = f->pwm_index % 2 ? 0x91 : 0x63;

  NEXUS_PwmChannelSettings pwmSettings;
  NEXUS_PwmChannelHandle pwm;
  uint16_t onInterval = (period * percent)/100;

  NEXUS_Pwm_GetDefaultChannelSettings(&pwmSettings);
  pwmSettings.openDrain = f->open_drain;
  pwmSettings.eFreqMode = NEXUS_PwmFreqModeType_eConstant;
  pwm = NEXUS_Pwm_OpenChannel(f->pwm_index, &pwmSettings);

  if (NEXUS_Pwm_SetOnAndPeriodInterval(pwm, onInterval, period)) {
    fprintf(stderr, "Could not set ON and PERIOD for PWM. Aborting...\n");
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  if (NEXUS_Pwm_Start(pwm)) {
    fprintf(stderr, "Could not start PWM %d!\n", f->pwm_index);
    platform_cleanup();
    exit(EXIT_FAILURE);
  }

  NEXUS_Pwm_CloseChannel(pwm);
}

#endif // BROADCOM
#endif // ANDROID
