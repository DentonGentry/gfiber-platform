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
  local http https url curl n arg curl_arg

  http=8888
  https=8889
  curl_ca="--cacert tmp-certs/rootCA.pem"

  testname "server not running"
  curl -s http://localhost:8888/
  check_failure

  # add a signed cert (using our fake CA)
  sh HOW.cert
  chmod 750 sim/tmp/ssl/*
  cp tmp-certs/localhost.pem sim/tmp/ssl/certs/craftui.pem
  cp tmp-certs/localhost.key sim/tmp/ssl/private/craftui.key

  ./craftui &
  pid=$!

  testname "process running"
  kill -0 $pid
  check_success

  sleep 1

  curl_noca="curl -v -s -m 1"
  curl="$curl_noca $curl_ca"

  # verify localhost works
  testname https localhost
  $curl https://localhost:$https/status |& grep 'WWW-Authenticate: Digest'
  check_success

  # but needs --cacert
  testname https localhost without CA
  $curl_noca https://localhost:$https/status |&
    grep 'unable to get local issuer certificate'
  check_success

  # and 127.0.0.1 fails due to cert name mismatch
  testname https 127.0.0.1
  $curl https://127.0.0.1:$https/status |&
    grep "certificate subject name 'localhost' does not match target host name"
  check_success

  for url in http://localhost:$http https://localhost:$https; do

    testname "404 not found ($url)"
    $curl $url/notexist |& grep '404: Not Found'
    check_success

    baduser_auth="--digest --user root:admin"
    badpass_auth="--digest --user guest:admin"

    testname "welcome web page ($url)"
    $curl $url/ |& grep welcome.thtml
    check_success

    for auth in "" "$baduser_auth" "$badpass_auth"; do
      for n in status config content.json; do
        testname "page $n bad auth ($url, $auth)"
        $curl $auth $url/$n |& grep 'WWW-Authenticate: Digest'
        check_success
      done
    done

    admin_auth="--digest --user admin:admin"
    guest_auth="--digest --user guest:guest"

    for auth in "$admin_auth" "$guest_auth"; do
      testname "status web page ($url, $auth)"
      $curl $auth $url/status |& grep status.thtml
      check_success

      testname "config web page ($url, $auth)"
      $curl $auth $url/config |& grep config.thtml
      check_success

      testname "json ($url, $auth)"
      $curl $auth $url/content.json |& grep '"platform": "GFCH100"'
      check_success
    done

    testname "bad json to config page ($url)"
    $curl $admin_auth -d 'duck' $url/content.json | grep "json format error"
    check_success

    testname "good json config ($url)"
    d='{"config":[{"peer_ipaddr":"192.168.99.99/24"}]}'
    $curl $admin_auth -d $d $url/content.json |& grep '"error": 0}'
    check_success

    testname "good json config, bad value ($url)"
    d='{"config":[{"peer_ipaddr":"192.168.99.99/240"}]}'
    $curl $admin_auth -d $d $url/content.json |& grep '"error": 1}'
    check_success

    testname "good json config, guest access ($url)"
    d='{"config":[{"peer_ipaddr":"192.168.99.99/24"}]}'
    $curl $guest_auth -d $d $url/content.json |& grep '401 Unauthorized'
    check_success

    testname "good json config, no auth ($url)"
    d='{"config":[{"peer_ipaddr":"192.168.99.99/24"}]}'
    $curl -d $d $url/content.json |& grep '401 Unauthorized'
    check_success

    testname "password is base64 ($url)"
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
    testname "password not base64 ($url)"
    new=ducky
    d='{ "config": [ { "password_guest": {
          "admin": "'"$admin"'",
          "new": "'"$new"'",
          "confirm": "'"$new"'"
        } } ] }'
    $curl $admin_auth -d "$d" $url/content.json |& grep '"error": 1}'
    check_success

  done

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

# run test suite
run_tests

# If there's a syntax error in this script, trap 0 will call onexit,
# so indicate we really hit the end of the script.
eos=1
# end of script, add more tests before this section
