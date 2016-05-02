#!/bin/bash
#
# Copyright 2016 Google Inc. All Rights Reserved.

. ./wvtest/wvtest.sh

SSDP=./host-ssdptax

WVSTART "ssdptax test"
WVPASSEQ "$($SSDP -t)" "ssdp 00:01:02:03:04:05 Test Device"
