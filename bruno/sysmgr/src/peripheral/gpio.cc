// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "gpioconfig.h"
#include "gpio.h"

namespace bruno_platform_peripheral {

GpIo::~GpIo() {
  Terminate();
}

bool GpIo::Init() {
  NEXUS_GpioSettings gpio_default_settings;
  NEXUS_Gpio_GetDefaultSettings(config_.type_, &gpio_default_settings);
  gpio_default_settings.mode=config_.mode_;
  gpio_default_settings.interruptMode=config_.interrupt_mode_;
  handle_ = NEXUS_Gpio_Open(config_.type_, config_.pin_, &gpio_default_settings);
  if (NULL == handle_) {
    LOG(LS_ERROR) << "NEXUS_Gpio_Open returns NULL - type:" << config_.type_
        << " pin:" << config_.pin_;
    return false;
  }
  // NEXUS_Gpio_Open does not set init value, we have to set it explicitly
  if(config_.init_value_ != NEXUS_GpioValue_eMax) {
    if (!Write(config_.init_value_)) {
      LOG(LS_ERROR) << "GpIo::Write failed";
      return false;
    }
  }

  return true;
}

void GpIo::Terminate() {
  if (handle_ != NULL) {
    NEXUS_Gpio_Close(handle_);
    handle_ = NULL;
  }
}

bool GpIo::Read(NEXUS_GpioValue *value) {
  NEXUS_Error err_code = NEXUS_SUCCESS;
  NEXUS_GpioStatus gpio_status;
  if (NULL == handle_) {
    LOG(LS_ERROR) << "Gpio not initialized yet";
    return false;
  }

  err_code = NEXUS_Gpio_GetStatus(handle_, &gpio_status);
  if(NEXUS_SUCCESS != err_code) {
    LOG(LS_ERROR) << "NEXUS_Gpio_SetSettings failed - err " << err_code;
    return false;
  }
  *value = gpio_status.value;
  return true;
}

bool GpIo::Write(NEXUS_GpioValue value) {
  NEXUS_Error err_code = NEXUS_SUCCESS;
  NEXUS_GpioSettings gpio_settings;
  if (NULL == handle_) {
    LOG(LS_ERROR) << "Gpio not initialized yet";
    return false;
  }

  if (config_.mode_ == NEXUS_GpioMode_eOutputPushPull) {
    NEXUS_Gpio_GetSettings(handle_, &gpio_settings);
    gpio_settings.value = value;
    err_code = NEXUS_Gpio_SetSettings(handle_, &gpio_settings);
    if(NEXUS_SUCCESS != err_code) {
      LOG(LS_ERROR) << "NEXUS_Gpio_SetSettings failed - err " << err_code;
      return false;
    }
  } else {
    LOG(LS_WARNING) << "Gpio mode does not allow write - mode " << config_.mode_;
    return false;
  }
  return true;
}

bool GpIo::RegisterInterrupt(NEXUS_Callback isr, void *context, int param) {
  NEXUS_Error err_code = NEXUS_SUCCESS;
  NEXUS_GpioSettings gpio_settings;
  if (NULL == handle_) {
    LOG(LS_ERROR) << "Gpio not initialized yet";
    return false;
  }

  if (config_.mode_ == NEXUS_GpioMode_eInput) {
    NEXUS_Gpio_GetSettings(handle_, &gpio_settings);
    gpio_settings.interrupt.callback = isr;
    gpio_settings.interrupt.context = context;
    gpio_settings.interrupt.param = param;
    err_code = NEXUS_Gpio_SetSettings(handle_, &gpio_settings);
    if(NEXUS_SUCCESS != err_code) {
      LOG(LS_ERROR) << "NEXUS_Gpio_SetSettings failed - err " << err_code;
      return false;
    }
  } else {
    LOG(LS_WARNING) << "Gpio mode does not allow interrupt - mode " << config_.mode_;
    return false;
  }
  return true;
}

}  // namespace bruno_platform_peripheral
