#!/bin/bash
. ./wvtest/wvtest.sh

GREP=./host-grep

WVSTART "grep test"

echo test-string | WVPASS $GREP test-
echo test-string | WVFAIL $GREP best-
echo test-string | WVPASS $GREP ''
WVPASSEQ "$($GREP line1 test-grep.txt)" "line1"

WVPASSEQ "$($GREP line1 test-grep.txt test-grep.txt)" \
"test-grep.txt:line1
test-grep.txt:line1"

WVPASSEQ "$($GREP line test-grep.txt test-grep.txt)" \
"test-grep.txt:line1
test-grep.txt:line2
test-grep.txt:line1
test-grep.txt:line2"
