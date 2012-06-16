// Copyright 2012 Google Inc. All Rights Reserved.
// Author: alicejwang@google.com (Alice Wang)

#include "bruno/logging.h"
#include "mailbox.h"

namespace bruno_platform_peripheral {

/* GPIO mailbox access files */
const std::string  Mailbox::kMailboxFanPercentFile = "/tmp/gpio/fanpercent";
const std::string  Mailbox::kMailboxFanSpeedFile = "/tmp/gpio/fanspeed";
const std::string  Mailbox::kMailboxCpuTemperatureFile = "/tmp/gpio/cpu_temperature";
const std::string  Mailbox::kMailboxCpuVoltageFile = "/tmp/gpio/cpu_voltage";
const std::string  Mailbox::kMailboxReadyFile = "/tmp/gpio/ready";


/* Read fan speed
 *
 * Return:
 *  true  - soc_speed - fan spinning count per second
 *  false - soc_speed - an invalid string
 */
bool Mailbox::ReadFanSpeed(std::string *fan_speed) {
  return ReadValueString(kMailboxFanSpeedFile, fan_speed);
}


/* Read CPU temperature
 * rtn = true, soc_temperature - current CPU temperature
 *       false soc_temperature - an invalid value
 */
bool Mailbox::ReadSocTemperature(float *soc_temperature) {
  std::string value_str;
  bool  rtn;

  *soc_temperature = 0.0;
  rtn = ReadValueString(kMailboxCpuTemperatureFile, &value_str);
  if (rtn == true) {
    rtn = Common::ConvertStringToFloat(value_str, soc_temperature);
  }
  return rtn;
}


/* Read CPU voltage
 *
 * Return:
 *  true  - soc_voltage - current CPU voltage
 *  false - soc_voltage - an invalid value
 */
bool Mailbox::ReadSocVoltage(std::string *soc_voltage) {
  return ReadValueString(kMailboxCpuVoltageFile, soc_voltage);
}


/* Write fan duty cycle
 *
 * Return:
 *  true  - send to gpio mailbox OK
 *  false - otherwise
 */
bool Mailbox::WriteFanDutyCycle(uint16_t duty_cycle) {
  std::string value_str;

  Common::ConvertUint16ToString(duty_cycle, &value_str);
  return WriteValueString(kMailboxFanPercentFile, value_str);
}


/* Read fan duty cycle
 *
 * Return:
 *  true  - read from gpio mailbox OK
 *  false - otherwise
 */
bool Mailbox::ReadFanDutyCycle(uint16_t *duty_cycle) {
  bool  rtn;
  std::string value_str;

  rtn = ReadValueString(kMailboxFanPercentFile, &value_str);
  if (rtn == true) {
    rtn = Common::ConvertStringToUint16(value_str, duty_cycle);
  }
  return rtn;
}


/* Check if gpio_mailbox is ready */
bool Mailbox::CheckIfMailBoxIsReady(void) {
  std::string value_str;
  bool is_ready;
  is_ready = ReadValueString(kMailboxReadyFile, &value_str);
  if (is_ready == true)
    LOG(LS_INFO) << "CheckIfMailBoxIsReady::" << kMailboxReadyFile << "=" << value_str;
  return is_ready;
}

/* Write value to the text file */
bool Mailbox::WriteValueString(const std::string& out_file, const std::string& value_str) {
  bool  rtn = false;
  std::ofstream file;
  std::ofstream tmp_file;
  std::string out_tmp_file = out_file + ".sysmgr_tmp";

  LOG(LS_VERBOSE) << "out_file=" << out_file << " out_tmp_file=" << out_tmp_file;
  tmp_file.open(out_tmp_file.c_str(), std::ios::out | std::ios::trunc);
  if (tmp_file.is_open()) {
    tmp_file << value_str;
    tmp_file.close();
    rename(out_tmp_file.c_str(), out_file.c_str());
    rtn = true;
  } else {
    LOG(LS_ERROR) << "WriteValueString: Failed to open: " << out_file;
  }

  LOG(LS_VERBOSE) << "rtn=" << rtn;
  return rtn;
}


/* Read value string from the text file */
bool Mailbox::ReadValueString(const std::string& in_file, std::string *value_str) {
  bool  rtn = false;

  std::ifstream file;

  file.open(in_file.c_str(), std::ios::in);
  if (file.is_open()) {
    std::getline(file, *value_str);
    file.close();
    rtn = true;
  } else {
    *value_str = Common::kErrorString;         /* Retrun a default error string */
    LOG(LS_ERROR) << "ReadValueString: Failed to open: " << in_file;
  }

  LOG(LS_VERBOSE) << "rtn=" << rtn;
  return rtn;
}

} // ce bruno_platform_peripheral
