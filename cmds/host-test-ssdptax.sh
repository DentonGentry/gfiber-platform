#!/bin/bash
#
# Copyright 2016 Google Inc. All Rights Reserved.

. ./wvtest/wvtest.sh

SSDP=./host-ssdptax

FIFO="/tmp/ssdptax.test.$$"
python ./ssdptax-test-server.py "$FIFO" &
sleep 0.5

WVSTART "ssdptax test"
WVPASSEQ "$($SSDP -t $FIFO)" "ssdp 00:00:00:00:00:00 Test Device;Google Fiber ssdptax"

rm "$FIFO"
