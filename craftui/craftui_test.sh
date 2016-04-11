#! /bin/sh

# some unit tests for the craft UI

# save stdout to 3, dup stdout to a file
log=.testlog.$$
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
	echo "---------------------------------------------------------"
	echo "starting test $test"
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
	testname "process running at exit"
	kill -0 $pid
	check_success

	# cleanup
	kill -9 $pid

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

trap onexit 0 1 2 3

testname "server not running"
curl -s http://localhost:8888/
check_failure

./craftui > /tmp/LOG 2>&1 &
pid=$!

testname "process running"
kill -0 $pid
check_success

sleep 1

testname true
true
check_success

testname false
false
check_failure

testname "main web page"
curl -s http://localhost:8888/ > /dev/null
check_success

testname "404 not found"
curl -s http://localhost:8888/notexist | grep '404: Not Found'
check_success

testname "json"
curl -s http://localhost:8888/content.json | grep '"platform": "GFCH100"'
check_success

