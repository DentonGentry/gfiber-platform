#!/bin/bash
#
# Copyright 2016 Google Inc. All Rights Reserved.

. ./wvtest/wvtest.sh

SSDP=./host-ssdptax
FIFO="/tmp/ssdptax.test.$$"
OUTFILE="/tmp/ssdptax.test.$$.output"

WVSTART "ssdptax test"

python ./ssdptax-test-server.py "$FIFO" 1 &
sleep 0.5
WVPASS $SSDP -t "$FIFO" >"$OUTFILE"
WVPASS grep -q "ssdp 00:00:00:00:00:00 Test Device;Google Fiber ssdptax" "$OUTFILE"
echo quitquitquit | nc -U "$FIFO"
rm -f "$FIFO" "$OUTFILE"

python ./ssdptax-test-server.py "$FIFO" 2 &
sleep 0.5
WVPASS $SSDP -t "$FIFO" >"$OUTFILE"
WVPASS grep -q "ssdp 00:00:00:00:00:00 REDACTED;server type" "$OUTFILE"
echo quitquitquit | nc -U "$FIFO"
rm -f "$FIFO" "$OUTFILE"

python ./ssdptax-test-server.py "$FIFO" 3 &
sleep 0.5
WVPASS $SSDP -t "$FIFO" >"$OUTFILE"
WVPASS grep -q "ssdp 00:00:00:00:00:00 Unknown;server type" "$OUTFILE"
echo quitquitquit | nc -U "$FIFO"
rm -f "$FIFO" "$OUTFILE"

python ./ssdptax-test-server.py "$FIFO" 4 &
sleep 0.5
WVPASS $SSDP -t "$FIFO" >"$OUTFILE"
WVPASS grep -q "ssdp 00:00:00:00:00:00 Test Device;Google Fiber ssdptax multicast" "$OUTFILE"
echo quitquitquit | nc -U "$FIFO"
rm -f "$FIFO" "$OUTFILE"
