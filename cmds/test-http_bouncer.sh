#!/bin/bash
#
# Copyright 2015 Google Inc. All Rights Reserved.
#

. ./wvtest/wvtest.sh

HTTP_BOUNCER=./host-http_bouncer
PORT="1337"
URL="example.com"

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

# requires bash 4+
declare -A INPUTS OUTPUTS STRIP_HEADER

INPUTS[0]=$(printf "GET / HTTP/1.0\r\nHost: google.com\r\n\r\n"; printf "$SENTINEL")
OUTPUTS[0]=$(printf "HTTP/1.0 302 Found\r\nLocation: $URL\r\n\r\n"; printf "$SENTINEL")

INPUTS[1]=$(printf "GET /path?arg HTTP/1.1\r\n\r\n"; printf "$SENTINEL")
OUTPUTS[1]=$(printf "HTTP/1.0 302 Found\r\nLocation: $URL\r\n\r\n"; printf "$SENTINEL")

INPUTS[2]=$(printf "GET / HTTP/1.0\nHost: google.com\n\n"; printf "$SENTINEL")
OUTPUTS[2]=$(printf "HTTP/1.0 302 Found\r\nLocation: $URL\r\n\r\n"; printf "$SENTINEL")

INPUTS[3]=$(printf "\n\n"; printf "$SENTINEL")
OUTPUTS[3]=$(printf "HTTP/1.0 302 Found\r\nLocation: $URL\r\n\r\n"; printf "$SENTINEL")

WVSTART "http_bouncer test"

# fail with no arguments
WVFAIL $HTTP_BOUNCER
# fail with extra arguments
WVFAIL $HTTP_BOUNCER -u $URL -p $PORT --EXTRA_ARGUMENT
# fail with invalid port
WVFAIL $HTTP_BOUNCER -p $URL -u $PORT

run_http_bouncer
wait_for_socket

i=0
while [ $i -lt ${#INPUTS[@]} ]; do
  output=$(echo -n "${INPUTS[$i]}" | nc localhost $PORT; printf "$SENTINEL")
  WVPASSEQ "$output" "${OUTPUTS[$i]}"
  i=$(expr $i + 1)
done

# Make sure we can download a CRL even through the bouncer.
# Some Internet Explorer versions will refuse to connect if we can't.
WVPASS printf "GET /GIAG2.crl HTTP/1.0\r\nHost: pki.google.com\r\n\r\n" |\
  nc localhost $PORT |\
  sed '1,/^\r$/d' |\
  openssl crl -inform DER
