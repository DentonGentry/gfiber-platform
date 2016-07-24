#!/bin/bash
#
# Copyright 2016 Google Inc. All Rights Reserved.

. ./wvtest/wvtest.sh

PORTFILE="/tmp/dialcheck.test.$$.port"
OUTFILE="/tmp/dialcheck.test.$$.output"

WVSTART "dialcheck test"

rm -f "$PORTFILE" "$OUTFILE"
python ./dialcheck-test-server.py "$PORTFILE" &
for i in $(seq 50); do if [ ! -f "$PORTFILE" ]; then sleep 0.1; fi; done

port=$(cat "$PORTFILE")
# Dial response will come from the IP address of the builder.
WVPASS ./host-dialcheck -t "$port" >"$OUTFILE"
WVPASS grep "DIAL responses from: " "$OUTFILE"
rm -f "$PORTFILE" "$OUTFILE"
