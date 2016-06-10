#!/bin/bash
#
# Copyright 2016 Google Inc. All Rights Reserved.

. ./wvtest/wvtest.sh

SSDP=./host-ssdptax
FIFO="/tmp/ssdptax.test.$$"

WVSTART "ssdptax test"

python ./ssdptax-test-server.py "$FIFO" 1 &
sleep 0.5
WVPASSEQ "$($SSDP -t $FIFO)" "ssdp 00:00:00:00:00:00 Test Device;Google Fiber ssdptax"
rm "$FIFO"

python ./ssdptax-test-server.py "$FIFO" 2 &
sleep 0.5
WVPASSEQ "$($SSDP -t $FIFO)" "ssdp 00:00:00:00:00:00 REDACTED;server type"
rm "$FIFO"

python ./ssdptax-test-server.py "$FIFO" 3 &
sleep 0.5
WVPASSEQ "$($SSDP -t $FIFO)" "ssdp 00:00:00:00:00:00 Unknown;server type"
rm "$FIFO"
