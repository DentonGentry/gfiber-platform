#!/bin/bash
. ./wvtest/wvtest.sh

RANDOMDATA=./host-randomdata
pid=$$

WVSTART "randomdata test"

WVPASS $RANDOMDATA 1 1025 >test1a.$pid.tmp
WVPASS $RANDOMDATA 1 1025 >test1b.$pid.tmp
WVPASS $RANDOMDATA 2 1025 >test2a.$pid.tmp
WVPASS $RANDOMDATA 2 1025 >test2b.$pid.tmp
WVPASS $RANDOMDATA 0 1026 >test0a.$pid.tmp
WVPASS $RANDOMDATA 0 1026 >test0b.$pid.tmp

# identical nonzero seeds produce identical results
WVPASSEQ "$(sha1sum <test1a.$pid.tmp)" "$(sha1sum <test1b.$pid.tmp)"
WVPASSEQ "$(sha1sum <test2a.$pid.tmp)" "$(sha1sum <test2b.$pid.tmp)"

# zero seeds produce non-identical results
WVPASSNE "$(sha1sum <test0a.$pid.tmp)" "$(sha1sum <test0b.$pid.tmp)"

# non-identical nonzero seeds produce non-identical results
WVPASSNE "$(sha1sum <test1a.$pid.tmp)" "$(sha1sum <test2a.$pid.tmp)"

# check file sizes
WVPASSEQ $(wc -c <test1a.$pid.tmp) 1025
WVPASSEQ $(wc -c <test2a.$pid.tmp) 1025
WVPASSEQ $(wc -c <test0a.$pid.tmp) 1026

rm -f *.$pid.tmp
