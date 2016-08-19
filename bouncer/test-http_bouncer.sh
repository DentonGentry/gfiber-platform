#!/bin/bash
#
# Copyright 2015 Google Inc. All Rights Reserved.
#

. ./wvtest/wvtest.sh

HTTP_BOUNCER=./host-http_bouncer
PORT="1337"
URL="http://example.com"

# command substition strips off trailing newlines, so we add a one-character
# sentinel to the command's output
SENTINEL="X"

function run_http_bouncer() {
  $HTTP_BOUNCER -u $URL -p $PORT &
  pid=$!
  trap 'kill $pid' EXIT
}

function wait_for_socket() {
  i=0
  retries=100
  while ! nc -z localhost $PORT && [ $i -lt $retries ] ; do sleep 0.1; i=$(expr $i + 1); done
}

WVSTART "http_bouncer test"

# fail with no arguments
WVFAIL $HTTP_BOUNCER
# fail with extra arguments
WVFAIL $HTTP_BOUNCER -u $URL -p $PORT --EXTRA_ARGUMENT
# fail with invalid port
WVFAIL $HTTP_BOUNCER -p $URL -u $PORT

run_http_bouncer
wait_for_socket

redirect0=$(printf "< HTTP/1.0 302 Found\r\n< Location: $URL\r")
redirect1=$(printf "< HTTP/1.1 302 Found\r\n< Location: $URL\r")

WVPASSEQ "$(curl -0vH 'Host: google.com' "localhost:$PORT" 2>&1 |\
  egrep '< HTTP|< Location')" "$redirect0"

WVPASSEQ "$(curl -vH 'Host: google.com' "localhost:$PORT/path?arg" 2>&1 |\
  egrep '< HTTP|< Location')" "$redirect1"

WVPASSEQ "$(curl -0vH '' "localhost:$PORT" 2>&1 |\
  egrep '< HTTP|< Location')" "$redirect0"

# Make sure we can download a CRL even through the bouncer.
# Some Internet Explorer versions will refuse to connect if we can't.
WVPASS curl -H 'Host: pki.google.com' 'http://localhost:1337/GIAG2.crl' |\
  openssl crl -inform DER
