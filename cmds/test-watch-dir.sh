#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
# Author: irinams@google.com (Irina Stanescu)

. ./wvtest/wvtest.sh

WD=./host-watch-dir
TEST_DIR=test_dir
TEST_FILE=test_file
OUTPUT_FILE=output_file

WVSTART "watch-dir test"

# Fails if no arguments are provided.
WVFAIL $WD

# Fails if a file is provided instead of a directory.
rm -f $TEST_FILE
echo "test" > $TEST_FILE
WVFAIL $WD $TEST_FILE

# Test on a directory that doesn't have access rights.
rm -rf $TEST_DIR
mkdir $TEST_DIR
chmod 000 $TEST_DIR
WVFAIL $WD $TEST_DIR

# Make sure the test directory doesn't exist.
rm -rf $TEST_DIR
WVFAIL stat $TEST_DIR
#if the directory doesn't exist, it should create it
WVPASSEQ "$(($WD $TEST_DIR) & (sleep 0.05; killall $WD))" ""
WVPASS stat $TEST_DIR

# Test what happens if the directory is removed while it's being watched.
rm -rf $TEST_DIR
WVFAIL "$(($WD $TEST_DIR) & (sleep 0.05; rmdir $TEST_DIR))"

# Run watch-dir in parallel with some operations on the directory.
rm -rf $TEST_DIR
mkdir $TEST_DIR
touch $TEST_DIR"/file"
WVPASSEQ "$(($WD $TEST_DIR) & (sleep 0.05; echo blabla >> $TEST_DIR/file; \
sleep 0.05; killall $WD))" "file"

# It will trigger inotify twice if trying to echo into an existing file,
# because the file is accessed once for cleaning, and once for writing.
WVPASSEQ "$(($WD $TEST_DIR) & (sleep 0.05; echo blabla > $TEST_DIR/file; \
sleep 0.05; killall $WD))" "file"$'\n'"file"
