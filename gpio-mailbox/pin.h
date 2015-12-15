#ifndef PIN_H_
#define PIN_H_

/*
 * Pin API to talk to LED, fan, temp sensor, etc
 */

typedef struct PinHandle_s* PinHandle;

/* 0 on success, -1 on error (eg, read-only control) */

#define PIN_ERROR  -1
#define PIN_OKAY  0

typedef int PinStatus;

typedef enum PinId_e {
  PIN_NONE = 0,

  /* on = 1, off = 0 */
  PIN_LED_RED,
  PIN_LED_BLUE,
  PIN_LED_ACTIVITY,
  PIN_LED_STANDBY,

  /* pressed = 1, not pressed = 0 */
  PIN_BUTTON_RESET,

  /* milli-degrees celsius */
  PIN_TEMP_CPU,
  PIN_TEMP_EXTERNAL,

  /* millivolts */
  PIN_MVOLTS_CPU,

  /* percent: 0-100 */
  PIN_FAN_CHASSIS,

  PIN_MAX,
} PinId;

/* constructor, destructor */
PinHandle PinCreate(void);
void PinDestroy(PinHandle handle);

/* check for features */
int PinIsPresent(PinHandle handle, PinId id);

/* control features */
PinStatus PinValue(PinHandle handle, PinId id, int* valueP);
PinStatus PinSetValue(PinHandle handle, PinId id, int value);

#endif /* PIN_H_ */
