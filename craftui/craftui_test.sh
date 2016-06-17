#! /bin/sh

# some unit tests for the craft UI

# save stdout to 3, dup stdout to a file
log=.testlog.$$
ln -sf LOG $log
exec 3>&1
exec >$log 2>&1

failcount=0
passcount=0

fail() {
  echo "FAIL: $*" >&3
  echo "FAIL: $*"
  ((failcount++))
}

pass() {
  echo "PASS: $*" >&3
  echo "PASS: $*"
  ((passcount++))
}

testname() {
  test="$*"
  echo ""
  echo "---------------------------------------------------------"
  echo "starting test '$test'"
}

check_success() {
  status=$?
  echo "check_success: last return code was $status, wanted 0"
  if [ $status = 0 ]; then
    pass $test
  else
    fail $test
  fi
}

check_failure() {
  status=$?
  echo "check_failure: last return code was $status, wanted not-0"
  if [ $status != 0 ]; then
    pass $test
  else
    fail $test
  fi
}

onexit() {
  testname "process not running at exit"
  kill -0 $pid
  check_failure

  testname "end of script reached"
  test "$eos" = 1
  check_success

  exec 1>&3
  echo "SUMMARY: pass=$passcount fail=$failcount"
  if [ $failcount -eq 0 ]; then
    echo "SUCCESS: $passcount tests passed."
  else
    echo "FAILURE: $failcount tests failed."
    echo "details follow:"
    cat $log
  fi
  rm -f $log

  exit $failcount
}

run_tests() {
  local use_https http https url curl n arg secure_arg curl_arg
  use_https=$1

  http=8888
  https=8889
  url=http://localhost:$http

  if [ "$use_https" = 1 ]; then
    url=https://localhost:$https
    secure_arg=-S
    curl_arg=-k

    # not really testing here, just showing the mode change
    testname "INFO: https mode"
    true
    check_success
  else
    # not really testing here, just showing the mode change
    testname "INFO: http mode"
    true
    check_success
  fi

  testname "server not running"
  curl -s http://localhost:8888/
  check_failure

  ./craftui $secure_arg &
  pid=$!

  testname "process running"
  kill -0 $pid
  check_success

  sleep 1

  curl="curl -v -s -m 1 $curl_arg"

  if [ "$use_https" = 1 ]; then
    for n in localhost 127.0.0.1; do
      testname "redirect web page ($n)"
      $curl "http://$n:8888/anything" |& grep "Location: https://$n:8889/"
      check_success
    done
  fi

  testname "404 not found"
  $curl $url/notexist |& grep '404: Not Found'
  check_success

  baduser_auth="--digest --user root:admin"
  badpass_auth="--digest --user guest:admin"

  for auth in "" "$baduser_auth" "$badpass_auth"; do
    for n in / /config /content.json; do
      testname "page $n bad auth ($auth)"
      $curl -v $auth $url/ |& grep 'WWW-Authenticate: Digest'
      check_success
    done
  done

  admin_auth="--digest --user admin:admin"
  guest_auth="--digest --user guest:guest"

  for auth in "$admin_auth" "$guest_auth"; do
    testname "main web page ($auth)"
    $curl $auth $url/ |& grep index.thtml
    check_success

    testname "config web page ($auth)"
    $curl $auth $url/config |& grep config.thtml
    check_success

    testname "json ($auth)"
    $curl $auth $url/content.json |& grep '"platform": "GFCH100"'
    check_success
  done

  testname "bad json to config page"
  $curl $admin_auth -d 'duck' $url/content.json | grep "json format error"
  check_success

  testname "good json config"
  d='{"config":[{"peer_ipaddr":"192.168.99.99/24"}]}'
  $curl $admin_auth -d $d $url/content.json |& grep '"error": 0}'
  check_success

  testname "good json config, bad value"
  d='{"config":[{"peer_ipaddr":"192.168.99.99/240"}]}'
  $curl $admin_auth -d $d $url/content.json |& grep '"error": 1}'
  check_success

  testname "good json config, guest access"
  d='{"config":[{"peer_ipaddr":"192.168.99.99/24"}]}'
  $curl $guest_auth -d $d $url/content.json |& grep '401 Unauthorized'
  check_success

  testname "good json config, no auth"
  d='{"config":[{"peer_ipaddr":"192.168.99.99/24"}]}'
  $curl -d $d $url/content.json |& grep '401 Unauthorized'
  check_success

  testname "password is base64"
  admin=$(echo -n admin | base64)
  new=$(echo -n ducky | base64)
  d='{ "config": [ { "password_guest": {
        "admin": "'"$admin"'",
        "new": "'"$new"'",
        "confirm": "'"$new"'"
      } } ] }'
  $curl $admin_auth -d "$d" $url/content.json |& grep '"error": 0}'
  check_success

  # TODO(edjames): duckduck does not fail.  Need to catch that.
  testname "password not base64"
  new=ducky
  d='{ "config": [ { "password_guest": {
        "admin": "'"$admin"'",
        "new": "'"$new"'",
        "confirm": "'"$new"'"
      } } ] }'
  $curl $admin_auth -d "$d" $url/content.json |& grep '"error": 1}'
  check_success

  testname "process still running at end of test sequence"
  kill -0 $pid
  check_success

  # cleanup
  t0=$(date +%s)
  kill $pid
  wait
  t1=$(date +%s)
  dt=$((t1 - t0))

  testname "process stopped on TERM reasonably fast"
  echo "process stopped in $dt seconds"
  test "$dt" -lt 3
  check_success
}

#
# main()
#
trap onexit 0 1 2 3

# sanity tests
testname true
true
check_success

testname false
false
check_failure

# run without https
run_tests 0

# run with https
run_tests 1

# If there's a syntax error in this script, trap 0 will call onexit,
# so indicate we really hit the end of the script.
eos=1
# end of script, add more tests before this section
