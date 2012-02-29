// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)

#include "base/logging.h"
#include "gpioconfig.h"
#include "gpio.h"
#include "fancontrol.h"
#include "tempmonitor.h"

namespace bruno_platform_peripheral {

TempMonitor::TempMonitor(uint32_t channel, FanControl* fan_control)
    : channel_(channel), fan_control_(fan_control), handle_(NULL), open_settings_() {
  this->SignalEvent.connect(this, &TempMonitor::OnEvent);
  this->SignalAlarm.connect(this, &TempMonitor::OnAlarm);
}

TempMonitor::~TempMonitor() {
  Terminate();
}

void TempMonitor::OnAlarm(void) {
  // Raise alarm
  // :TODO: Fill this once the alarm manager is ready.
}

void TempMonitor::OnEvent(void) {
  NEXUS_Error rc;
  NEXUS_TempMonitorStatus temp_monitor_status;
  rc = NEXUS_TempMonitor_GetStatus(handle_, &temp_monitor_status);

  if (NEXUS_SUCCESS != rc) {
    LOG(LS_WARNING) << "NEXUS_TempMonitor_GetStatus failed - rc " << rc;
  }

  LOG(LS_INFO) << "Average temp " << temp_monitor_status.avgTemp
               << " RF temp integer " << temp_monitor_status.tempIntegerRf
               << " RF temp fraction " << temp_monitor_status.tempFractionRf
               << " fan operation " << temp_monitor_status.fanOp
               << "sensor mode " << temp_monitor_status.sensorMode
               << " HDDUpdateflag " << temp_monitor_status.tempHddUpdateFlag; 

  switch(temp_monitor_status.fanOp) {
    case NEXUS_TempMonitorFanOp_eAdjust:
      LOG(LS_INFO) << "Adjust the fan speed with average temp " << temp_monitor_status.avgTemp;
      fan_control_->AdjustSpeed(temp_monitor_status.avgTemp);
      break;
    case NEXUS_TempMonitorFanOp_eNoOperation:
      LOG(LS_INFO) << "No Operation on fan";
      break;
    case NEXUS_TempMonitorFanOp_eNotStarted:
      LOG(LS_INFO) << "Fan is stopped. Can be started";
      fan_control_->SelfStart();
      break;
    case NEXUS_TempMonitorFanOp_eOff:
      LOG(LS_INFO) << "Turn off the fan";
      fan_control_->DrivePwm(0);
      break;
    case NEXUS_TempMonitorFanOp_eFullSpeed:
      LOG(LS_INFO) << "Turn on fan in full speed";
      fan_control_->DrivePwm(0xff);
      break;
    default:
      break;
  }
}

void TempMonitor::EventHandler(void *context, int param) {
  UNUSED(param);
  TempMonitor* tempmon= reinterpret_cast<TempMonitor*>(context);
  if (NULL == tempmon) {
    LOG(LS_ERROR) << "NULL TempMonitor pointer";
    return;
  }

  tempmon->SignalEvent();
}

void TempMonitor::AlarmHandler(void *context, int param) {
  UNUSED(param);
  TempMonitor* tempmon= reinterpret_cast<TempMonitor*>(context);
  if (NULL == tempmon) {
    LOG(LS_ERROR) << "NULL TempMonitor pointer";
    return;
  }

  tempmon->SignalAlarm();
}

bool TempMonitor::Init(void) {
  open_settings_.dataReady.callback = (NEXUS_Callback)&TempMonitor::EventHandler;
  open_settings_.dataReady.context = this;
  open_settings_.overTemp.callback = (NEXUS_Callback)&TempMonitor::AlarmHandler;
  open_settings_.overTemp.context = this;
  open_settings_.numTempSamples=5;
  handle_ = NEXUS_TempMonitor_Open(channel_,&open_settings_);
  return fan_control_->Init();
}

void TempMonitor::Terminate(void) {
  fan_control_->Terminate();
  if (handle_) {
    NEXUS_TempMonitor_Close(handle_);
    handle_ = NULL;
  }
}

}  // namespace bruno_platform_peripheral
