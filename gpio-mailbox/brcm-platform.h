/*
 * Hardware abstraction layer for GPIO's and PWMs.
 */

#ifndef BRCM_PLATFORM_
#define BRCM_PLATFORM_

/*
 * Defines the category that a given GPIO falls under.
 */
enum GpioType {
  STANDARD,
  AON,
};

struct Gpio {
  int is_present;

  unsigned int pinmux_offset;
  unsigned int pinmux_mask;
  unsigned int pinmux_value;

  unsigned int offset_direction;
  unsigned int offset_data;

  /* for offset_direction and offset_data */
  unsigned int mask;                    // eg, (*reg & mask) >> shift == on_value
  unsigned int shift;
  unsigned int off_value;
  unsigned int on_value;
  unsigned int direction_value;         // 0 is output
  unsigned int pin;                     // gpio #
  enum GpioType type;                   // 'type' of gpio (aon/standard)
  int old_val;
};

struct PwmControl {
  int is_present;
  int open_drain;
  unsigned int offset_data;
  unsigned int pwm_index;               // index of this pwm.
  unsigned int channel;
  int old_percent;
};

struct Temp {
  int is_present;
  unsigned int offset_data;
  double (*get_temp)(struct Temp* t);
};

struct Voltage {
  int is_present;
  unsigned int offset_data;
  double (*get_voltage)(struct Voltage* v);
};

struct Leds {
  struct Gpio led_red;
  struct Gpio led_blue;
  struct Gpio led_activity;
  struct Gpio led_standby;
  struct PwmControl led_brightness;
};

struct platform_info {
  const char *name;
  off_t mmap_base;
  size_t mmap_size;
  void (*init)(struct platform_info* p);
  struct Leds leds;
  struct Gpio reset_button;
  struct Gpio fan_tick;
  struct PwmControl fan_control;
  struct Temp temp_monitor;
  struct Voltage voltage_monitor;
};

/* This value, from old code, controls the pwm period. The duty cycle
  is defined as on/(period + 1) and on is defined as (on/Fv). Fv is
  the frequency of the variable rate PWM.*/
extern const int PWM_CYCLE_PERIOD;

/* Return the master platform_info struct for the provided platforn_name.
   If no platform matches, returns NULL */
extern struct platform_info *get_platform_info(const char *);

/* Initialize the platform! */
extern int platform_init(struct platform_info *);

/* Cleanup the platform! */
extern void platform_cleanup();

/* Set the gpio represented by to the provided level.
   Level is restricted to [0, 1] */
extern void set_gpio(struct Gpio *, int);

/* Get the value of the gpio provided. */
extern int get_gpio(struct Gpio *);

/* Set the provided PWM to the given duty cycle percent */
extern void set_pwm(struct PwmControl *, int);

/* Return the duty cycle for the given PWM */
extern int get_pwm(struct PwmControl *);

/* Init GPIO to input or output. */
extern void set_direction(struct Gpio *);

/* Set the pinmux (init pin to LED, GPIO, etc) */
extern void set_pinmux(struct Gpio *);

#endif
