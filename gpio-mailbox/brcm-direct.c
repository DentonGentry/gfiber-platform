#ifdef BROADCOM

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "brcm-platform.h"

#define UNUSED        __attribute__((unused))
#define DEVMEM       "/dev/mem"

const int PWM_CYCLE_PERIOD = 0x63;

static volatile void* mmap_addr = MAP_FAILED;
static size_t mmap_size = 0;
static int mmap_fd = -1;

static void init_gfhd200(UNUSED struct platform_info* p);
static void init_gfhd254(UNUSED struct platform_info* p);
static double get_avs_voltage_7252(struct Voltage* v);
static double get_avs_voltage_74xx(struct Voltage* v);
static double get_avs_temperature_7252(struct Temp* t);
static double get_avs_temperature_74xx(struct Temp* t);

struct platform_info platforms[] = {
  {
    .name = "GFHD100",
    .mmap_base = 0x10400000,            // base of many brcm registers
    .mmap_size = 0x40000,
    .leds = {
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
      .open_drain = 1,
    },
    .temp_monitor = {
      .is_present = 1,                  // 7425 AVS_RO_REGISTERS_0
      .offset_data = 0x32b00,           // BCHP_AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS
      .get_temp = get_avs_temperature_74xx,
    },
    .voltage_monitor = {
      .is_present = 1,                  // 7425 AVS_RO_REGISTERS_0
      .offset_data = 0x32b0c,           // BCHP_AVS_RO_REGISTERS_0_PVT_1P10V_0_MNTR_STATUS
      .get_voltage = get_avs_voltage_74xx,
    },
  },
  {
    .name = "GFMS100",
    .mmap_base = 0x10400000,            // base of many brcm registers
    .mmap_size = 0x40000,
    .leds = {
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
      .open_drain = 1,
    },
    .temp_monitor = {
      .is_present = 1,                  // 7425 AVS_RO_REGISTERS_0
      .offset_data = 0x32b00,           // BCHP_AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS
      .get_temp = get_avs_temperature_74xx,
    },
    .voltage_monitor = {
      .is_present = 1,                  // 7425 AVS_RO_REGISTERS_0
      .offset_data = 0x32b0c,           // BCHP_AVS_RO_REGISTERS_0_PVT_1P10V_0_MNTR_STATUS
      .get_voltage = get_avs_voltage_74xx,
    },
  },
  {
    .name = "GFHD200",
    .init = init_gfhd200,
    .mmap_base = 0x10400000,            // AON_PIN_CTRL ...
    .mmap_size = 0x30000,
    .leds = {
      .led_red = {
        .is_present = 1,                  // GPIO 5
        .pinmux_offset = 0x8500,          // PIN_MUX_CTRL_0
        .pinmux_mask = 0xf0000000,
        .pinmux_value = 0x10000000,       // LED_LD1 (segment 1 on led digit1)
        .offset_data = 0x9018,            // GIO_AON_DATA_LO
        .mask = 0x00000002,               // 1<<1
        .shift = 1,
        .off_value =1,
        .on_value = 0,
        .old_val = -1,
      },
      .led_blue = {
        .is_present = 0,
      },
      .led_activity = {
        .is_present = 1,                  // GPIO 4
        .pinmux_offset = 0x8500,          // PIN_MUX_CTRL_0
        .pinmux_mask = 0x0f000000,
        .pinmux_value = 0x01000000,       // LED_LD0 (segment 0 on led digit1)
        .offset_data = 0x9018,            // GIO_AON_DATA_LO
        .mask = 0x00000001,               // 1<<0
        .shift = 0,
        .off_value = 1,
        .on_value = 0,
        .old_val = -1,
      },
      .led_standby = {
        .is_present = 0,
      },
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
      .get_temp = get_avs_temperature_74xx,
    },
    .voltage_monitor = {
      .is_present = 1,                  // 7429 AVS_RO_REGISTERS_0
      .offset_data = 0x2330c,           // BCHP_AVS_RO_REGISTERS_0_PVT_1P10V_0_MNTR_STATUS
      .get_voltage = get_avs_voltage_74xx,
    },
  },
  {
    .name = "GFHD254",
    .init = init_gfhd254,
    .mmap_base = 0xf0400000,            // AON_PIN_CTRL ...
    .mmap_size =    0xe0000,
    .leds = {
      .led_red = {
        .is_present = 1,                  // AON_GPIO_05
        .pinmux_offset = 0x10700,         // PIN_MUX_CTRL_0
        .pinmux_mask =  0x00f00000,
        .pinmux_value = 0x00200000,       // LED_LD_13
        .offset_data = 0x1701c,           // LDK_DIGIT1
        .mask = 1<<13,                    // 1<<13
        .shift = 13,
        .off_value =1,
        .on_value = 0,
        .old_val = -1,
      },
      .led_blue = {
        .is_present = 0,
      },
      .led_activity = {
        .is_present = 1,                  // AON_GPIO_04
        .pinmux_offset = 0x10700,         // PIN_MUX_CTRL_0
        .pinmux_mask = 0x000f0000,
        .pinmux_value = 0x00020000,       // LED_LD_12
        .offset_data = 0x1701c,           // LDK_DIGIT1
        .mask = 1<<12,                    // 1<<12
        .shift = 12,
        .off_value = 1,
        .on_value = 0,
        .old_val = -1,
      },
      .led_standby = {
        .is_present = 0,
      },
      .led_brightness = {
        .is_present = 1,                // GPIO_098
        .open_drain = 0,
        .offset_data = 0x9000,          // PWM_2
        .channel = 0,
        .old_percent = -1,
      },
    },
    .reset_button = {
      .is_present = 1,                  // GPIO_009
      .pinmux_offset = 0x4120,          // SUN_TOP_CTRL_PIN_MUX_CTRL_8
      .pinmux_mask = 0xf0000000,
      .pinmux_value = 0x00000000,       // GPIO_009
      .offset_direction = 0xa608,       // GIO_IODIR_LO
      .offset_data = 0xa604,            // GIO_DATA_LO
      .mask = 0x00000200,               // 1<<9
      .shift = 9,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 1,
      .old_val = -1,
    },
    .fan_tick = {
      .is_present = 1,                  // GPIO 78
      .offset_direction = 0xa648,       // GIO_IODIR_EXT_HI
      .offset_data = 0xa644,            // GIO_DATA_EXT_HI
      .mask = 1<<14,
      .shift = 14,
      .off_value = 0,
      .on_value = 1,
      .direction_value = 1,
      .old_val = -1,
    },
    .fan_control = {
      .is_present = 1,                  // PWM 3
      .offset_data = 0x9000,            // PWM_CTRL ...
      .channel = 1,
      .open_drain = 0,
    },
    .temp_monitor = {
      .is_present = 1,                  // 7252 AVS_RO_REGISTERS_0
      .offset_data = 0xd2200,           // BCHP_AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS
      .get_temp = get_avs_temperature_7252,
    },
    .voltage_monitor = {
      .is_present = 1,                  // 7252 AVS_RO_REGISTERS_0
      .offset_data = 0xd220c,           // BCHP_AVS_RO_REGISTERS_0_PVT_1V_0_MNTR_STATUS
      .get_voltage = get_avs_voltage_7252,
    },
  }
};


struct platform_info *get_platform_info(const char *platform_name) {
  int lim = sizeof(platforms) / sizeof(platforms[0]);
  for (int i = 0; i < lim; ++i) {
    struct platform_info *p = &platforms[i];
    if (0 == strncmp(platform_name, p->name, strlen(p->name))) {
      return p;
    }
  }
  fprintf(stderr, "No support for platform %s", platform_name);
  return NULL;
}

/* set LED/Keypad timings to control LED brightness */
static void init_gfhd200(UNUSED struct platform_info* p) {
  volatile uint32_t* reg;

  reg = mmap_addr + 0x9034;     // LDK_CONTROL
  *reg = 0x01;                  // reset
  *reg = 0x18;                  // ver=1 inv_led=1

  reg = mmap_addr + 0x9008;     // LDK_PRESCHI, LO (clock divisor)
  reg[0] = 0x00;
  reg[1] = 0x10;                // tick = clock / 0x0010, not sure what clock is

  reg = mmap_addr + 0x9010;     // LDK_DUTYOFF, ON
  reg[0] = 0x40;
  reg[1] = 0xc0;                // 0x40 off ticks then 0xc0 on ticks == 75% brightness
}

/* set LED/Keypad timings to control LED brightness */
static void init_gfhd254(struct platform_info* p) {
  volatile uint32_t* reg;

  // The following comment explains how the LED controller works on <= EVT3.
  //  For EVT4+, the LED controller was changed to control via PWM. We currently
  //  configure both. The EVT3 specific code can be removed at a later date.
  //
  // The led display controller works like this:
  //  - there are 16 gpios (we connect our leds to 2 of these)
  //  - the controller steps through digit1-4 and then status
  //  - bit0 in a register maps to a particular gpio
  //     when digit1 is being displayed the controller uses digit1_bit[15:0] to
  //     drive the gpios.  When digit 2 is displayed digit2[15:0] and so forth.
  //  - duty_on controls how many clocks a digit is displayed
  //  - duty_off controls number of clocks of all off time when switching
  //    between digits
  //
  //  To get 100% brightness you set all of digit1-4 and status to 1 for the led
  //  you are drivng, and set duty_off to 0.
  //
  //  Here we also invert the values, so a 1 means off, and 0 means on, this is
  //  done because for unknown reasons the time between status and digit1 is on,
  //  so we can't get the brightness to 0 unless we invert.
  //
  //  For simplicity we enable only one of the digits because the leds are
  //  already insanely bright, and then to disable an led we simply toggle the
  //  bit in that one digit register.
  //
  //  The red led is attached to bit 13 and blue led is attached to bit 12.
  reg = mmap_addr + 0x17034;     // LDK_CONTROL
  *reg = 0x01;                   // reset
  *reg = 0x18;                   // ver=1

  reg = mmap_addr + 0x17018;
  reg[0] = 0xffff;               // LDK_DIGIT2
  reg[1] = 0xcfff;               // LDK_DIGIT1
  reg[2] = 0xffff;               // LDK_DIGIT4
  reg[3] = 0xffff;               // LDK_DIGIT3
  reg[5] = 0xffff;               // LDK_STATUS

  reg = mmap_addr + 0x17008;     // LDK_PRESCHI, LO (clock divisor)
  reg[0] = 0x00;
  reg[1] = 0x10;                 // tick = clock / 0x0010, not sure what clock is

  reg = mmap_addr + 0x17010;     // LDK_DUTYOFF, ON
  reg[0] = 0x40;
  reg[1] = 0xc0;                 // 0x40 off ticks then 0xc0 on ticks to dim a bit more.

  // The fan is connected to PWM3, the register PWM3_CWORD_LSB is set to 1,
  // this is the frequency of the PWM, the other pwm register control
  // the duty cycle.
  reg = mmap_addr + 0x9014;       // PWM3_CWORD_LSB
  reg[0] = 1;

  // LEDs are connected to PWM2. Setting CWORD_LSB to 0xf to control
  // the output freq of the var rate clock.
  reg = mmap_addr + 0x900c;
  reg[0] = 0xf;

  // Default the LED brightness to 50.
  set_pwm(&p->leds.led_brightness, 50);
}

static double get_avs_voltage_7252(struct Voltage* v) {
  volatile uint32_t* reg;
  uint32_t value, valid, raw_data;

  reg = mmap_addr + v->offset_data;
  value = *reg;
  valid = (value & 0x00000400) >> 10;
  raw_data = value & 0x000003ff;
  if (!valid) return -1.0;
  return ((880.0/1024.0)/(0.7)*raw_data) / 1000;
}

static double get_avs_voltage_74xx(struct Voltage* v) {
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

static double get_avs_temperature_74xx(struct Temp* t) {
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

static double get_avs_temperature_7252(struct Temp* t) {
  volatile uint32_t* reg;
  uint32_t value, valid, raw_data;

  reg = mmap_addr + t->offset_data;
  value = *reg;
  valid = (value & 0x00000400) >> 10;
  raw_data = value & 0x000003ff;
  if (!valid) return -1.0;
  return 410.04 - (0.48705 * raw_data);
}

/*
   platform_init has three steps:
   1) Generic platform init. This sets up the mmap.
   2) Platform-specific init. Calls the gf* function
   corresponding to the passed in platform.
   3) GPIO init. Sets up the gpios properly.
*/
int platform_init(struct platform_info* p) {
  platform_cleanup();

  mmap_fd = open(DEVMEM, O_RDWR);
  if (mmap_fd < 0) {
    perror(DEVMEM);
    return -1;
  }
  mmap_size = p->mmap_size;
  mmap_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, p->mmap_base);
  if (mmap_addr == MAP_FAILED) {
    perror("mmap");
    platform_cleanup();
    return -1;
  }

  if (p->init) {
    (*p->init)(p);
  }

  set_pinmux(&p->leds.led_red);
  set_pinmux(&p->leds.led_blue);
  set_pinmux(&p->leds.led_activity);
  set_pinmux(&p->leds.led_standby);

  set_direction(&p->leds.led_red);
  set_direction(&p->leds.led_blue);
  set_direction(&p->leds.led_activity);
  set_direction(&p->leds.led_standby);
  set_direction(&p->reset_button);
  set_direction(&p->fan_tick);

  return 0;
}

void platform_cleanup() {
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

void set_gpio(struct Gpio *g, int level) {
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

int get_gpio(struct Gpio *g) {
  volatile uint32_t* reg;
  uint32_t value;

  reg = mmap_addr + g->offset_data;
  value = (*reg & g->mask) >> g->shift;
  return (value == g->on_value);
}

void set_pwm(struct PwmControl *f, int percent) {
  volatile uint32_t* reg;
  uint32_t mask0, val0, mask1, val1, on;

  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  if (f->old_percent == percent) return;
  f->old_percent = percent;

  reg = mmap_addr + f->offset_data;
  if (f->channel == 0) {
    mask0 = 0xf0;       // preserve other channel
    val0 = 0x01;        // open-drain|start
    if (f->open_drain)
      val0 |= 0x08;
    mask1 = 0x10;       // preserve
    val1 = 0x01;        // constant-freq
    on = 6;
  } else {
    mask0 = 0x0f;       // see above
    val0 = 0x10;
    if (f->open_drain)
      val0 |= 0x80;
    mask1 = 0x01;
    val1 = 0x10;
    on = 8;
  }
  reg[0] = (reg[0] & mask0) | val0;
  reg[1] = (reg[1] & mask1) | val1;
  reg[on] = (PWM_CYCLE_PERIOD * percent)/100;         // 0x63 is what old code used
  reg[on+1] = PWM_CYCLE_PERIOD;
}

/* PWM operates on either channel 0 or 1. We want to get the duty cycle value
   by calculating it from the "ON" register, located offset 6 for channel 0
   and 8 for channel 1.

   Duty cycle is calculated by ON / Period.
*/
int get_pwm(struct PwmControl *f) {
  volatile uint32_t* reg = mmap_addr + f->offset_data;
  uint8_t offset = f->channel ? 8 : 6;
  uint32_t val = reg[offset];
  return ((uint64_t)val * 100) / PWM_CYCLE_PERIOD;
}

void set_direction(struct Gpio *g) {
  volatile uint32_t* reg;
  uint32_t value;

  if (!g->is_present || g->offset_direction == 0)
    return;

  reg = mmap_addr + g->offset_direction;
  value = *reg;
  value &= ~g->mask;
  value |= g->direction_value << g->shift;
  *reg = value;
}

void set_pinmux(struct Gpio *g) {
  volatile uint32_t* reg;
  uint32_t value;

  if (!g->is_present || g->pinmux_offset == 0)
    return;

  reg = mmap_addr + g->pinmux_offset;
  value = *reg;
  value &= ~g->pinmux_mask;
  value |= g->pinmux_value;
  *reg = value;
}

#endif // BROADCOM
