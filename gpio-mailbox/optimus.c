#ifdef MINDSPEED
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "pin.h"

#define  DEVMEM  "/dev/mem"

/* optimus */
#define REG_PWM_BASE            0x90458000
#define REG_PWM_DIVIDER         (REG_PWM_BASE)
#define REG_PWM_HI(p)           (REG_PWM_BASE+0x08+0x08*(p))
#define REG_PWM_LO(p)           (REG_PWM_HI(p)+0x04)

#define PWM_CLOCK_HZ            250000000                       /* 250 MHz */
#define PWM_DIVIDER_ENABLE_MASK (1<<31)
#define PWM_DIVIDER_VALUE_MASK  ((1<<8)-1)
#define PWM_TIMER_ENABLE_MASK   (1<<31)
#define PWM_TIMER_VALUE_MASK    ((1<<20)-1)
#define PWM_DEFAULT_DIVIDER     PWM_DIVIDER_VALUE_MASK

#define REG_GPIO_BASE           0x90470000
#define REG_GPIO_OUTPUT         (REG_GPIO_BASE+0x00)
#define REG_GPIO_DIRECTION      (REG_GPIO_BASE+0x04)            /* 1 = output */
#define REG_GPIO_INPUT          (REG_GPIO_BASE+0x10)
#define REG_GPIO_SELECT         (REG_GPIO_BASE+0x58)

/* manually maintain these */
#define REG_FIRST               REG_PWM_BASE
#define REG_LAST                REG_GPIO_SELECT
#define REG_LENGTH              (REG_LAST + 0x04 - REG_FIRST)

/* index of gpio pins */
#define GPIO_BUTTON             6
#define GPIO_ACTIVITY           12
#define GPIO_RED                13

/* gpio 12 can be pwm 4, 13 can be 5 */
#define PWM_ACTIVITY            4
#define PWM_RED                 5
#define PWM_LED_HZ              1000    /* 300-1000 is recommended */
#define PWM_DUTY_OFF_PERCENT    90      /* 90% off, 10% on, dim */

struct PinHandle_s {
  int                           fd;
  volatile unsigned char*       addr;
};

#define BIT_IS_SET(data, bit)   (((data) & (1u << (bit))) == (1u << (bit)))
#define BIT_SET(data, bit)      ((data) | (1u << (bit)))
#define BIT_CLR(data, bit)      ((data) & ~(1u << (bit)))

#define SYS_FAN_DIR             "/sys/devices/platform/comcerto_i2c.0/i2c-0/0-004c/"
#define SYS_TEMP1               SYS_FAN_DIR "temp1_input"
#define SYS_TEMP2               SYS_FAN_DIR "temp2_input"
#define SYS_FAN                 SYS_FAN_DIR "pwm1"

/* helper methods */

static int readIntFromFile(char* file) {
  int fd = open(file, O_RDONLY);
  if (fd < 0) {
    perror(file);
    return -1;
  }
  char buf[1024] = { 0 };
  int len = read(fd, buf, sizeof(buf)-1);
  if (len < 0) {
    perror(file);
    close(fd);
    return -1;
  }
  close(fd);
  buf[len] = '\0';
  long int value = strtol(buf, NULL, 10);
  return value;
}

// this is for writing to SYS_FAN
// don't use for writing to a regular file since this is not atomic
static void writeIntToFile(char* file, int value) {
  FILE* fp = fopen(file, "w");
  if (fp == NULL) {
    perror(file);
    return;
  }
  fprintf(fp, "%d", value);
  fclose(fp);
}

/* optimus methods get sensor data */

static uint32_t getRegister(PinHandle handle, unsigned int reg) {
  volatile uint32_t* regaddr = (volatile uint32_t*) (handle->addr + (reg - REG_FIRST));
  if (reg < REG_FIRST || reg > REG_LAST) {
    fprintf(stderr, "getRegister: register 0x%08x is out of range (0x%08x-0x%08x)\n",
      reg, REG_FIRST, REG_LAST);
    return 0;
  }
  return *regaddr;
}

static void setRegister(PinHandle handle, unsigned int reg, uint32_t value) {
  volatile uint32_t* regaddr = (volatile uint32_t*) (handle->addr + (reg - REG_FIRST));
  if (reg < REG_FIRST || reg > REG_LAST) {
    fprintf(stderr, "setRegister: register 0x%08x is out of range (0x%08x-0x%08x)\n",
      reg, REG_FIRST, REG_LAST);
    return;
  }
  *regaddr = value;
}

static int getGPIO(PinHandle handle, int gpio) {
  uint32_t direction = getRegister(handle, REG_GPIO_DIRECTION);
  int reg = BIT_IS_SET(direction, gpio) ? REG_GPIO_OUTPUT : REG_GPIO_INPUT;
  uint32_t value = getRegister(handle, reg);
  return BIT_IS_SET(value, gpio);
}

static void setGPIO(PinHandle handle, int gpio, int value) {
  uint32_t direction = getRegister(handle, REG_GPIO_DIRECTION);
  int reg = BIT_IS_SET(direction, gpio) ? REG_GPIO_OUTPUT : REG_GPIO_INPUT;
  if (!BIT_IS_SET(direction, gpio)) {
    fprintf(stderr, "setGPIO: gpio %d is not an output register, refusing to set\n", gpio);
    return;
  }
  uint32_t val = getRegister(handle, reg);
  uint32_t newVal = value ? BIT_SET(val, gpio) : BIT_CLR(val, gpio);
  setRegister(handle, reg, newVal);
}

static int getPWMValue(PinHandle handle, int gpio, int pwm) {
  uint32_t divider = getRegister(handle, REG_PWM_DIVIDER);      /* shared among all PWM */
  uint32_t lo = getRegister(handle, REG_PWM_LO(pwm));
  uint32_t hi = getRegister(handle, REG_PWM_HI(pwm));
  uint32_t hi_enabled = hi & PWM_TIMER_ENABLE_MASK;
  hi &= ~PWM_TIMER_ENABLE_MASK;
  int is_on = (divider & PWM_DIVIDER_ENABLE_MASK) &&
              hi_enabled &&
              lo < hi;        /* technically true, but maybe not visible */
  return is_on;
}

static void setPWMValue(PinHandle handle, int gpio, int pwm, int value) {
  static uint32_t warn_divider = 0xffffffff;
  uint32_t direction = getRegister(handle, REG_GPIO_DIRECTION);
  if (!BIT_IS_SET(direction, gpio)) {
    fprintf(stderr, "setPWMValue: gpio %d is not an output register, refusing to set\n", gpio);
    return;
  }
  uint32_t select = getRegister(handle, REG_GPIO_SELECT);
  uint32_t mode = (select >> (2*gpio)) & 0x3;
  if (mode != 0x1) {
    fprintf(stderr, "setPWMValue: setting gpio %d to PWM mode\n", gpio);
    select &= ~(0x3 << (2*gpio));
    select |= (0x1 << (2*gpio));
    setRegister(handle, REG_GPIO_SELECT, select);
  }
  uint32_t divider = getRegister(handle, REG_PWM_DIVIDER);      /* shared among all PWM */
  if (! (divider & PWM_DIVIDER_ENABLE_MASK)) {                  /* not enabled */
    fprintf(stderr, "setPWMValue: divider not enabled, enabling\n");
    divider = PWM_DIVIDER_ENABLE_MASK | PWM_DEFAULT_DIVIDER;
    setRegister(handle, REG_PWM_DIVIDER, divider);
  }
  divider &= PWM_DIVIDER_VALUE_MASK;
  divider++;    /* divider reg is 0-based */
  uint32_t timer = PWM_CLOCK_HZ / divider / PWM_LED_HZ;
  if (timer < 1) {
    timer = 1;
    if (warn_divider != divider) {
      fprintf(stderr, "setPWMValue: PWM_LED_HZ too large, LED will be %d Hz\n",
        PWM_CLOCK_HZ/divider/timer);
      warn_divider = divider;
    }
  } else if (timer > PWM_TIMER_VALUE_MASK+1) {
    timer = PWM_TIMER_VALUE_MASK+1;
    if (warn_divider != divider) {
      fprintf(stderr, "setPWMValue: divider too small, LED will be %d Hz\n",
        PWM_CLOCK_HZ/divider/timer);
      warn_divider = divider;
    }
  }
  /* brighter as duty approaches 0, dimmer as it approaches timer */
  uint32_t duty = timer * (value ? PWM_DUTY_OFF_PERCENT : 100) / 100;
  if (duty < 1) {
    duty = 1;
  }
  if (duty > timer) {
    duty = timer;
  }
  setRegister(handle, REG_PWM_LO(pwm), duty-1);                                 /* duty reg is 0-based */
  setRegister(handle, REG_PWM_HI(pwm), (timer-1) | PWM_TIMER_ENABLE_MASK);      /* timer reg is 0-based */
}

static int getFan(PinHandle handle) {
  return readIntFromFile(SYS_FAN);
}

static void setFan(PinHandle handle, int percent) {
  writeIntToFile(SYS_FAN, percent);
}

static int getTemp1(PinHandle handle) {
  return readIntFromFile(SYS_TEMP1);
}

static int getTemp2(PinHandle handle) {
  return readIntFromFile(SYS_TEMP2);
}

/* API implementation */

PinHandle PinCreate(void) {
  PinHandle handle = (PinHandle) calloc(1, sizeof (*handle));
  if (handle == NULL) {
    perror("calloc(PinHandle)");
    return NULL;
  }
  handle->fd = -1;
  handle->addr = NULL;

  handle->fd = open(DEVMEM, O_RDWR);
  if (handle->fd < 0) {
    perror(DEVMEM);
    PinDestroy(handle);
    return NULL;
  }
  handle->addr = mmap(NULL, REG_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED,
                      handle->fd, REG_FIRST);
  if (handle->addr == NULL) {
    perror("mmap");
    PinDestroy(handle);
    return NULL;
  }
  return handle;
}

void PinDestroy(PinHandle handle) {
  if (handle != NULL) {
    if (handle->fd > 0) {
      close(handle->fd);
      handle->fd = -1;
    }
    if (handle->addr != NULL) {
      munmap((void*) handle->addr, REG_LENGTH);
      handle->addr = NULL;
    }
    free(handle);
  }
  return;
}

int PinIsPresent(PinHandle handle, PinId id) {
  switch (id) {
    case PIN_LED_RED:
    case PIN_LED_ACTIVITY:
    case PIN_BUTTON_RESET:
    case PIN_TEMP_CPU:
    case PIN_TEMP_EXTERNAL:
    case PIN_MVOLTS_CPU:
    case PIN_FAN_CHASSIS:
      return 1;

    /* no default here so we can be sure we get all the cases */
    case PIN_LED_BLUE:
    case PIN_LED_STANDBY:
    case PIN_NONE:
    case PIN_MAX:
      break;
  }
  return 0;
}

PinStatus PinValue(PinHandle handle, PinId id, int* valueP) {
  switch (id) {
    case PIN_LED_RED:
      *valueP = getPWMValue(handle, GPIO_RED, PWM_RED);
      break;

    case PIN_LED_ACTIVITY:
      *valueP = getPWMValue(handle, GPIO_ACTIVITY, PWM_ACTIVITY);
      break;

    case PIN_BUTTON_RESET:
      *valueP = !getGPIO(handle, GPIO_BUTTON);  /* inverted */
      break;

    case PIN_TEMP_CPU:
      *valueP = getTemp1(handle);
      break;

    case PIN_TEMP_EXTERNAL:
      *valueP = getTemp2(handle);
      break;

    case PIN_FAN_CHASSIS:
      *valueP = getFan(handle);
      break;

    case PIN_MVOLTS_CPU:
      *valueP = 1000;   /* TODO(edjames) */
      break;

    case PIN_LED_BLUE:
    case PIN_LED_STANDBY:
    case PIN_NONE:
    case PIN_MAX:
      *valueP = 0;
      return PIN_ERROR;
  }
  return PIN_OKAY;
}

PinStatus PinSetValue(PinHandle handle, PinId id, int value) {
  switch (id) {
    case PIN_LED_RED:
      setPWMValue(handle, GPIO_RED, PWM_RED, value);
      break;

    case PIN_LED_ACTIVITY:
      setPWMValue(handle, GPIO_ACTIVITY, PWM_ACTIVITY, value);
      break;

    case PIN_FAN_CHASSIS:
      setFan(handle, value);
      break;

    case PIN_LED_BLUE:
    case PIN_LED_STANDBY:
    case PIN_BUTTON_RESET:
    case PIN_TEMP_CPU:
    case PIN_TEMP_EXTERNAL:
    case PIN_MVOLTS_CPU:
    case PIN_NONE:
    case PIN_MAX:
      return PIN_ERROR;
  }
  return PIN_OKAY;
}
#endif /* MINDSPEED */
