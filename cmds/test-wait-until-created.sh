#!/bin/bash
. ./wvtest/wvtest.sh

WUC=./host-wait-until-created

WVSTART "wait-until-created test"

rm -f f1 f2

try_timeout()
{
  local timeout=$1 pid= pid2= rv=
  shift
  "$@" &
  pid=$!
  (
    sleep $timeout &
    pid3=$!
    trap 'kill $pid3' SIGTERM
    wait
    kill $pid 2>/dev/null
  ) &
  pid2=$!
  echo "waiting for '$*'..."
  wait $pid
  rv=$?
  kill $pid2 2>/dev/null
  wait $pid2
  return $rv
}

# fails instantly if no files given
WVFAIL $WUC

# returns instantly if file exists
touch f1
WVPASS $WUC f1
WVPASS $WUC "$PWD/f1"

# returns instantly if any one file exists
WVPASS $WUC f1 ../file-does-not-exist

# returns instantly if the file does exist (verifying try_timeout)
WVPASS try_timeout 5 $WUC f1

# does *not* return if the file doesn't exist
time WVFAIL try_timeout 0.1 $WUC will-never-exist

# does return if file is created during test
echo 'starting wait for f2'
WVPASS try_timeout 5 $WUC f2 &
echo 'bg process running'
echo 'creating f2'
touch f2
echo 'waiting'
wait

# does return if directory is created during test
echo 'starting wait for d2/f2'
WVPASS try_timeout 5 $WUC d2/f2 &
echo 'bg process running'
echo 'creating d2'
mkdir d2
echo 'creating d2/f2'
touch d2/f2
echo 'waiting'
wait


rm -f f1 f2
