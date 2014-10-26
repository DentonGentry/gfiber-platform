#!/bin/sh
. wvtest/wvtest.sh


_wait_file() {
  local i
  for i in $(seq 50); do
    [ -e "$1" ] && return 0
    sleep 0.1
  done
  return 1
}


wait_files() {
  local fname
  rm -f "$@"
  for fname in "$@"; do
    _wait_file "$fname" || return 1
  done
  return 0
}


high_powered_competitor() {
  WVSTART "high powered competitor"
  echo "11:22:33:44:55:66,2412,-20,50" \
      >fake/scanresults.wlan-22:22:22:22:22:22
  # wait for current half-done scan to finish
  WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned

  # need to let this try a few times.  We configure waveguide with very
  # short timeouts to try to make the test run fast, but that means on a
  # heavily loaded CPU, there might be a race where wg1 thinks wg2 has timed
  # out even though it's just slow at delivering a packet.  That should
  # happen very rarely, and certainly not so many times in a row.
  for i in $(seq 15); do
    WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
    # wg2 is a high powered AP, so wg1 should shut down
    [ -e wg1.tmp/wlan-22:22:22:22:22:22.disabled ] && return 0
  done
  WVFAIL
  return 1
}


export PATH=$PWD/fake:$PATH
rm -rf wg*.tmp
WVPASS mkdir wg1.tmp


WVSTART "startup"
rm -f fake/scanresults.wlan-22:22:22:22:22:22
WVPASS touch wg1.tmp/wlan-22:22:22:22:22:22.disabled
./waveguide --status-dir=wg1.tmp --watch-pid=$$ \
    --fake=22:22:22:22:22:22 --tx-interval=0.3 --scan-interval=0.3 &
pid1=$!
WVPASS wait_files wg1.tmp/ready
WVFAIL [ -e wg1.tmp/wlan-22:22:22:22:22:22.disabled ]


WVSTART "no visible competitors"
./waveguide --status-dir=wg2.tmp --watch-pid=$$ \
    --fake=11:22:33:44:55:66 --tx-interval=0.3 --scan-interval=0.3 \
    --high-power &
pid2=$!
WVPASS wait_files wg2.tmp/sentpacket wg1.tmp/gotpacket \
    wg1.tmp/wlan-22:22:22:22:22:22.scanned
# wg2 is not visible in scan, so wg1 should stay up
WVFAIL [ -e wg1.tmp/wlan-22:22:22:22:22:22.disabled ]


high_powered_competitor


WVSTART "far away competitor"
echo "11:22:33:44:55:66,2412,-70,50" \
	>fake/scanresults.wlan-22:22:22:22:22:22
# wait for the current scan to finish, plus another full scan
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
# wg2 is too far away, so wg1 should stay up
WVFAIL [ -e wg1.tmp/wlan-22:22:22:22:22:22.disabled ]


high_powered_competitor


WVSTART "no longer visible competitor"
rm -f fake/scanresults.wlan-22:22:22:22:22:22
# results only time out after several full scans with nothing visible
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
# should be back up again
WVFAIL [ -e wg1.tmp/wlan-22:22:22:22:22:22.disabled ]


high_powered_competitor


WVSTART "obsolete scan results"
echo "11:22:33:44:55:66,2412,-20,15000" \
	>fake/scanresults.wlan-22:22:22:22:22:22
# wait for the current scan to finish, plus another full scan
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
WVPASS wait_files wg1.tmp/wlan-22:22:22:22:22:22.scanned
# wg2 last_seen is ancient, should be ignored
WVFAIL [ -e wg1.tmp/wlan-22:22:22:22:22:22.disabled ]


high_powered_competitor


WVSTART "dead competitor"
kill $pid2
wait $pid2
WVPASS wait_files wg1.tmp/sentpacket
WVPASS wait_files wg1.tmp/sentpacket
WVPASS wait_files wg1.tmp/sentpacket
WVPASS wait_files wg1.tmp/sentpacket
WVPASS wait_files wg1.tmp/sentpacket
WVPASS wait_files wg1.tmp/ready
# no packets received from wg2 for several tx periods, so wg1 should start
WVFAIL [ -e wg1.tmp/wlan-22:22:22:22:22:22.disabled ]


# don't run high_powered_competitor here: we just killed its daemon!


WVSTART "low powered competitor"
echo "11:22:33:44:55:66,2412,-20,50" \
    >fake/scanresults.wlan-22:22:22:22:22:22
rm -rf wg2.tmp
./waveguide --status-dir=wg2.tmp --watch-pid=$$ \
    --fake=11:22:33:44:55:66 --tx-interval=0.3 --scan-interval=0.3 &
pid2=$!
WVPASS wait_files wg2.tmp/sentpacket wg1.tmp/gotpacket
# wg1 and wg2 are both low powered, so wg1 should stay up
WVFAIL [ -e wg1.tmp/wlan-22:22:22:22:22:22.disabled ]


kill $pid1
kill $pid2
wait
