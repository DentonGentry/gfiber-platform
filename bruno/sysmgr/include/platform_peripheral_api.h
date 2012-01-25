/* Copyright 2012 Google Inc. All Rights Reserved.
   Author: kedong@google.com (Ke Dong)
 */

#ifndef BRUNO_PLATFORM_PERIPHERAL_API_H_
#define BRUNO_PLATFORM_PERIPHERAL_API_H_

#ifdef __cplusplus
extern "C" {
#endif

int platform_peripheral_init(unsigned int monitor_interval);
void platform_peripheral_run(void);
int platform_peripheral_terminate(void);

void platform_peripheral_turn_on_led_main(void);
void platform_peripheral_turn_off_led_main(void);
void platform_peripheral_turn_on_led_standby(void);
void platform_peripheral_turn_off_led_standby(void);

typedef enum led_status_color_e {
  LED_STATUS_RED,
  LED_STATUS_GREEN,
  LED_STATUS_YELLOW
} led_status_color_e;

int platform_peripheral_set_led_status_color(led_status_color_e color);
void platform_peripheral_turn_off_led_status(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* BRUNO_PLATFORM_PERIPHERAL_API_H_ */
