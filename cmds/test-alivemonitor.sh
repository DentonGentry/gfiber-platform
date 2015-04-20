#!/bin/bash
. ./wvtest/wvtest.sh

pid=$$
TICK=tick.$pid.tmp
AM=./host-alivemonitor

wait_until_contains()
{
  for i in $(seq 100); do
    [ "$(cat $1)" = "$2" ] && break
    echo 'checking'
    sleep 0.1
  done
  WVPASSEQ "$(cat $1)" "$2"
}


WVSTART "alivemonitor test"

# make sure return values are passed through correctly
WVPASS $AM $TICK 1 1 10 true
WVFAIL $AM $TICK 1 1 10 false

# catch invalid parameters
WVPASS $AM $TICK 10 1 10 true
WVFAIL $AM $TICK 10.01 1 10 true
WVPASS $AM $TICK 1 1 1 true
WVFAIL $AM $TICK -1 1 1 true
WVFAIL $AM $TICK 1 -1 1 true
WVFAIL $AM $TICK 1 1 -1 true

# make sure it kills after a timeout
YESLOG=killed.$pid.tmp
WVFAIL $AM $TICK 0.1 0.1 0.2 sleep 10 2>&1 | tee $YESLOG
# I don't usually like to look for specific output messages in tests,
# but since we had an actual bug where the output was lying, we should
# probably check that out.
WVPASS fgrep -q 'Timeout!' $YESLOG

# make sure it sleeps for less than incr_timeout if it would go past timeout.
# We know it slept for a shorter interval if it kills the subtask.
WVFAIL $AM $TICK 0.1 100 0.2 sleep 10

# make sure it doesn't kill if no timeout occurs.
do_ticks()
{
  while kill -0 $pid 2>/dev/null; do
    echo "tick" >&2 &&
    touch $TICK &&
    sleep $1 ||
    break
  done
  rm -rf $TICK
}
do_ticks 0.1 &
dtpid=$!
NOTLOG=notkilled.$pid.tmp
WVPASS $AM $TICK 0.05 0.05 10 sleep 0.5 2>&1 | tee $NOTLOG
WVFAIL fgrep -q 'Timeout!' $NOTLOG
kill $dtpid

# make sure log message doesn't talk about timeouts if the task
# is killed.
NOTLOG2=notkilled2.$pid.tmp
$AM $TICK 0.05 0.05 100 sh -c 'kill 0' 2>&1 | tee $NOTLOG2
WVFAIL fgrep -q 'Timeout!' $NOTLOG2
WVPASS grep -q 'signal [0-9]* received' $NOTLOG2

# test traplog.sh that we will use for testing prekill
TMP=trap.$pid.tmp
: >$TMP
./traplog.sh $TMP &
tlpid=$!
wait_until_contains $TMP "START "
kill -HUP $tlpid
wait_until_contains $TMP "START HUP "
kill -HUP $tlpid
wait_until_contains $TMP "START HUP HUP "
kill -TERM $tlpid
wait_until_contains $TMP "START HUP HUP TERM "
WVPASS wait $tlpid

# check prekill behaviour
# traplog.sh does exit(0) on SIGTERM
: >$TMP
WVPASS $AM -S 15 -T 5 $TICK 0.1 0.1 1 ./traplog.sh $TMP
WVPASSEQ "$(cat $TMP)" "START TERM "
# but it doesn't exit on SIGHUP, so we'll end up killing it
: >$TMP
WVFAIL $AM -S 1 -T 0.5 $TICK 0.1 0.1 1 ./traplog.sh $TMP
WVPASSEQ "$(cat $TMP)" "START HUP "

rm -f *.$pid.tmp
