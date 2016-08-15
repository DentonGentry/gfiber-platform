#!/bin/bash
. ./wvtest/wvtest.sh

WVSTART "anonid test"
ANONID="./host-anonid"

WVPASSEQ "$($ANONID -a 00:11:22:33:44:55 -k 0123456789)" "KEALAE"
WVPASSEQ "$($ANONID -a 00:11:22:33:44:66 -k 6789abcdef)" "AAKLYK"
