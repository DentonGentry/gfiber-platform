#!/bin/sh

TARGET_DIR=$1

rsync -aP ${TARGET_DIR}/../build/bruno-HEAD/bruno/gfhd100/skel/ ${TARGET_DIR}/
if (grep "\-test" ${TARGET_DIR}/etc/version); then
  perl -p -i -e "s%^# TEST_AGETTY%S0:1:respawn:/sbin/agetty -L ttyS0 115200 vt100%" ${TARGET_DIR}/etc/inittab
fi
