#!/bin/bash

. ./wvtest/wvtest.sh

WVSTART "hash_mac_addr test"

HASH_MAC_ADDR=./host-hash_mac_addr

WVFAIL $HASH_MAC_ADDR
WVFAIL $HASH_MAC_ADDR -a nonsense

WVPASSEQ "$($HASH_MAC_ADDR -a 00:00:00:00:00:00)" \
  85cce83032eb6bd39ddea68e0be917e4665b5d26

WVPASSEQ "$($HASH_MAC_ADDR -a aa:bb:cc:dd:ee:ff)" \
  "$($HASH_MAC_ADDR -a AA:BB:CC:DD:EE:FF)"
