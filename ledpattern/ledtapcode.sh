#!/bin/sh

. /etc/utils.sh

LEDPATTERN="ledpattern /etc/ledpatterns"
SYSFS_GPON_PATH="/sys/devices/platform/gpon"
MONITOR_PATH="/tmp/gpio/ledcontrol"
LASER_STATUS_FILE="/tmp/laser_i2c_status"
ALARM_GPON_FILE="$SYSFS_GPON_PATH/info/alarmGpon"
GPON_INFO_FILE="$SYSFS_GPON_PATH/info/infoGpon"
HALTED_FILE="$MONITOR_PATH/halted"
HW_FAILURE="$MONITOR_PATH/hardware_failure"
LASER_CHANNEL_FILE="$SYSFS_GPON_PATH/misc/laserChannel"
ACS_FILE="$MONITOR_PATH/acsconnected"

PlayPatternAndExit()
{
  state="$1"
  # ledpattern takes care of all the LED management and state selection.
  result="$($LEDPATTERN $state)"
  if [ "$?" -ne 0 ]; then
    echo "Failed to display pattern $state: $result"
    exit 1
  fi
  exit 0
}

if [ ! -f "$ALARM_GPON_FILE" ]; then
  echo "$ALARM_GPON_FILE does not exist"
  PlayPatternAndExit UNKNOWN_ERROR
fi

if [ ! -f "$GPON_INFO_FILE" ]; then
  echo "$GPON_INFO_FILE does not exist"
  PlayPatternAndExit UNKNOWN_ERROR
fi

if [ ! -f "$LASER_CHANNEL_FILE" ]; then
  echo "$LASER_CHANNEL_FILE does not exist"
  PlayPatternAndExit UNKNOWN_ERROR
fi

# It is a valid state that there may not be a LASER_STATUS_FILE yet.
if [ -f "$LASER_STATUS_FILE" ]; then
  laser_status=$(cat "$LASER_STATUS_FILE")
  if [ "$laser_status" -ne 0 ]; then
    echo "Playing SET_LASER_FAILED pattern"
    PlayPatternAndExit SET_LASER_FAILED
  fi
fi

if [ -f "$HW_FAILURE" ]; then
  echo "Playing HALTED pattern on HW_FAILURE"
  PlayPatternAndExit HALTED
fi

if [ -f "$HALTED_FILE" ]; then
  echo "Playing HALTED pattern on HALTED_FILE"
  PlayPatternAndExit HALTED
fi

# Chop the table headers off the output using tail, otherwise grep gets
# confused later.
alarm_info=$(cat "$ALARM_GPON_FILE" | tail -n+7)
los_output=$(echo "$alarm_info" | grep "LOS" | grep "ON")
lof_output=$(echo "$alarm_info" | grep "LOF" | grep "ON")
if [ -n "$los_output" ] || [ -n "$lof_output" ]; then
  echo "Playing LOSLOF_ALARM pattern"
  PlayPatternAndExit LOSLOF_ALARM
fi
other_alarm=$(echo "$alarm_info" | grep "ON")
if [ -n "$other_alarm" ]; then
  echo "Playing OTHER_ALARM pattern"
  PlayPatternAndExit OTHER_ALARM
fi

gpon_info=$(cat "$GPON_INFO_FILE" | grep "ONU STATE")
if contains "$gpon_info" "INITIAL"; then
  echo "Playing GPON_INITIAL pattern"
  PlayPatternAndExit GPON_INITIAL
elif contains "$gpon_info" "STANDBY"; then
  echo "Playing GPON_STANDBY pattern"
  PlayPatternAndExit GPON_STANDBY
elif contains "$gpon_info" "SERIAL"; then
  echo "Playing GPON_SERIAL pattern"
  PlayPatternAndExit GPON_SERIAL
elif contains "$gpon_info" "RANGING"; then
  echo "Playing GPON_RANGING pattern"
  PlayPatternAndExit GPON_RANGING
fi

laser_channel=$(cat "$LASER_CHANNEL_FILE")
if [ ! -f "$ACS_FILE" ] && [ "$laser_channel" -eq "-1" ]; then
  echo "Playing NO_LASER_CHANNEL pattern"
  PlayPatternAndExit NO_LASER_CHANNEL
elif [ ! -f "$ACS_FILE" ] && [ $laser_channel -ne "-1" ]; then
  echo "Playing WAIT_ACS pattern"
  PlayPatternAndExit WAIT_ACS
elif [ -f "$ACS_FILE" ] && [ $laser_channel -ne "-1" ]; then
  echo "Playing ALL_OK pattern"
  PlayPatternAndExit ALL_OK
else
  # If we get all the way here and nothing triggered on the way then this really
  # is an unknown error...
  echo "Nothing triggered? Playing UNKNOWN_ERROR pattern..."
  PlayPatternAndExit UNKNOWN_ERROR
fi
