// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#ifndef BUNO_PLATFORM_PERIPHERAL_MAILBOX_H_
#define BUNO_PLATFORM_PERIPHERAL_MAILBOX_H_

#include "bruno/constructormagic.h"
#include "common.h"

namespace bruno_platform_peripheral {

class Common;

/*
 * GPIO defintions for B2 boards:
 *
 * Bruno:
 *   Front view LEDs positions
 *   D20     D4621    D21  D21
 *
 * Name              | Schematics net name | Ref Des | Front view LED position
 * ==========================================================================
 * GPIO_LED_BLUE     | BT_B_LED_N          | D20     | The most left LED
 * GPIO_LED_RED      | 7425_R_LED_N        | D4621   | The 2nd left LED (dual color)
 * GPIO_LED_ACT_BLUE | 7425_ACT_LED_N      | D4621   | The 2nd left LED (dual color)
 * GPIO_LED_STANDBY0 | STANDBY_LED_N0      | D21/D22 | The 2 right LEDs
 *
 *
 * Bruno-IS:
 * Name              | Schematics net name | Ref Des
 * ==========================================================================
 * GPIO_LED_RED      | 7425_R_LED_N        | D4621-D4623 (dual color LEDs)
 * GPIO_LED_ACT_BLUE | 7425_ACT_LED_N      | D4621-D4623 (dual color LEDs)
 *
 */
class Mailbox {
 public:

  static const std::string  kMailboxFanPercentFile;
  static const std::string  kMailboxFanSpeedFile;
  static const std::string  kMailboxCpuTemperatureFile;
  static const std::string  kMailboxAux1TemperatureFile;
  static const std::string  kMailboxCpuVoltageFile;
  static const std::string  kMailboxReadyFile;

  explicit Mailbox() {}
  virtual ~Mailbox() {}

  bool ReadFanSpeed(uint16_t *fan_speed);
  bool ReadSocTemperature(float *soc_temperature);
  bool ReadAux1Temperature(float *soc_temperature);
  bool ReadSocVoltage(std::string *soc_voltage);
  bool WriteFanDutyCycle(uint16_t duty_cycle);
  bool ReadFanDutyCycle(uint16_t *duty_cycle);
  bool CheckIfMailBoxIsReady(void);

 private:
  bool WriteValueString(const std::string& out_file, const std::string& value_str);
  bool ReadValueString(const std::string& in_file, std::string *value_str);

  DISALLOW_COPY_AND_ASSIGN(Mailbox);
};

}  // namespace bruno_platform_peripheral

#endif // BUNO_PLATFORM_PERIPHERAL_MAILBOX_H_
