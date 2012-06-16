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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* BRUNO_PLATFORM_PERIPHERAL_API_H_ */
