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
#define REG_OFFSET    0x90470000
#define REG_LENGTH    0x20

#define REG_DIRECTION           0x04            /* 1 = output */
#define REG_INPUT               0x10
#define REG_OUTPUT              0x00

#define GPIO_BUTTON             6
#define GPIO_ACTIVITY           12
#define GPIO_RED                13

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

static uint32_t getRegister(PinHandle handle, int reg) {
  volatile uint32_t* regaddr = (volatile uint32_t*) (handle->addr + reg);
  return *regaddr;
}

static void setRegister(PinHandle handle, int reg, uint32_t value) {
  volatile uint32_t* regaddr = (volatile uint32_t*) (handle->addr + reg);
  *regaddr = value;
}

static int getGPIO(PinHandle handle, int gpio) {
  uint32_t direction = getRegister(handle, REG_DIRECTION);
  int reg = BIT_IS_SET(direction, gpio) ? REG_OUTPUT : REG_INPUT;
  uint32_t value = getRegister(handle, reg);
  return BIT_IS_SET(value, gpio);
}

static void setGPIO(PinHandle handle, int gpio, int value) {
  uint32_t direction = getRegister(handle, REG_DIRECTION);
  int reg = BIT_IS_SET(direction, gpio) ? REG_OUTPUT : REG_INPUT;
  if (!BIT_IS_SET(direction, gpio)) {
    fprintf(stderr, "setGPIO: gpio %d is not an output register, refusing to set\n", gpio);
    return;
  }
  uint32_t val = getRegister(handle, reg);
  uint32_t newVal = value ? BIT_SET(val, gpio) : BIT_CLR(val, gpio);
  setRegister(handle, reg, newVal);
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
                      handle->fd, REG_OFFSET);
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
      *valueP = getGPIO(handle, GPIO_RED);
      break;

    case PIN_LED_ACTIVITY:
      *valueP = getGPIO(handle, GPIO_ACTIVITY);
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
      setGPIO(handle, GPIO_RED, value);
      break;

    case PIN_LED_ACTIVITY:
      setGPIO(handle, GPIO_ACTIVITY, value);
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
