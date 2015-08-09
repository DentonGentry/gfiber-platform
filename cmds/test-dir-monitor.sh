#!/bin/bash
#
# Copyright 2015 Google Inc. All Rights Reserved.

. ./wvtest/wvtest.sh

DM=./host-dir-monitor
TEST_DIR=test_dir

WVSTART "dir-monitor test"

# Fails if no arguments are provided.
WVFAIL "$DM"

# Run dir-monitor in parallel with some operations on the directory.
rm -rf "$TEST_DIR"
mkdir "$TEST_DIR"
touch "$TEST_DIR/file"

# Run without arguments, modifications of existing files won't be noticed.
WVPASSEQ "$((($DM $TEST_DIR) & (sleep 0.05; echo blabla > $TEST_DIR/file; \
sleep 0.05; killall $DM)) | wc -l)" "2"

# with -m, modification events should be printed
WVPASSEQ "$((($DM -m $TEST_DIR) & (sleep 0.05; echo blabla > $TEST_DIR/file; \
sleep 0.05; killall $DM)) | wc -l)" "6"

rm -rf "$TEST_DIR"
