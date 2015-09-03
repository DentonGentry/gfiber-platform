#!/usr/bin/python
# Copyright 2015 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for WifiblasterController."""

import os
import shutil
import sys
import tempfile
import time
import waveguide
from wvtest import wvtest


@wvtest.wvtest
def ReadParameterTest():
  d = tempfile.mkdtemp()
  try:
    wc = waveguide.WifiblasterController([], d)
    testfile = os.path.join(d, 'wifiblaster.name')

    # Result should be equal in value and type (unless None).
    testcases = [('1\n', int, 1),
                 ('1.5\n', float, 1.5),
                 ('True\n', str, 'True\n'),
                 ('x\n', int, None)]
    for (read_data, typecast, expected) in testcases:
      open(testfile, 'w').write(read_data)
      result = wc._ReadParameter('name', typecast)
      wvtest.WVPASSEQ(result, expected)
      if result:
        wvtest.WVPASS(isinstance(result, typecast))

    # Make sure it works if the file isn't readable
    os.chmod(testfile, 0)
    result = wc._ReadParameter('name', int)
    wvtest.WVPASSEQ(result, None)

    # and if it generally doesn't exist
    os.unlink(testfile)
    result = wc._ReadParameter('name', str)
    wvtest.WVPASSEQ(result, None)
  finally:
    os.rmdir(d)


@wvtest.wvtest
def LogWifiblasterResultsTest():
  wc = waveguide.WifiblasterController([], '')

  # Set the consensus key to a known value.
  waveguide.consensus_key = 16 * 'x'

  stdout = ('version=1 mac=11:11:11:11:11:11 throughput=10000000 '
            'samples=5000000,15000000\n'
            'malformed 11:11:11:11:11:11 but has macs 11:11:11:11:11:11\n')

  result = wc._AnonymizeResult(stdout)
  expected = ('version=1 mac=CYAFVU throughput=10000000 '
              'samples=5000000,15000000\n'
              'malformed CYAFVU but has macs CYAFVU\n')
  wvtest.WVPASSEQ(result, expected)

  expected = [('version=1 mac=11:11:11:11:11:11 throughput=10000000 '
               'samples=5000000,15000000'),
              'malformed 11:11:11:11:11:11 but has macs 11:11:11:11:11:11']
  filename = os.path.join(waveguide.WIFIBLASTER_DIR, '11:11:11:11:11:11')
  # Should save only the second line, with leading timestamp
  wc._HandleResults(2, stdout, 'error')
  wvtest.WVPASSEQ(open(filename).read().split(' ', 1)[1], expected[1])
  # Should save the first line, with leading timestamp
  wc._HandleResults(0, stdout.split('\n')[0], 'error 2')
  wvtest.WVPASSEQ(open(filename).read().split(' ', 1)[1], expected[0])


class Empty(object):
  pass


@wvtest.wvtest
def PollTest():
  d = tempfile.mkdtemp()
  oldpath = os.environ['PATH']
  oldtime = time.time
  faketime = [-1]

  def FakeTime():
    faketime[0] += 1
    return faketime[0]

  try:
    time.time = FakeTime
    os.environ['PATH'] = 'fake:' + os.environ['PATH']
    sys.path.insert(0, 'fake')
    waveguide.opt = Empty()
    waveguide.opt.status_dir = d
    manager = waveguide.WlanManager(phyname='phy-22:22', vdevname='wlan-22:22',
                                    high_power=True)
    manager.UpdateStationInfo()
    wc = waveguide.WifiblasterController([manager], d)

    def WriteConfig(k, v):
      open(os.path.join(d, 'wifiblaster.%s' % k), 'w').write(v)

    WriteConfig('duration', '.1')
    WriteConfig('enable', 'False')
    WriteConfig('fraction', '10')
    WriteConfig('interval', '10')
    WriteConfig('rapidpolling', '10')
    WriteConfig('size', '1470')

    # Disabled. No packet blasts should be run.
    print manager.GetState()
    for t in xrange(0, 100):
      wc.Poll(t)

    def CountRuns():
      try:
        v = open('fake/wifiblaster.out').readlines()
      except IOError:
        return 0
      else:
        os.unlink('fake/wifiblaster.out')
        return len(v)

    CountRuns()  # get rid of any leftovers
    wvtest.WVPASSEQ(CountRuns(), 0)

    # The first packet blast should be one
    # cycle later than the start time. This is not an implementation detail:
    # it prevents multiple APs from running simultaneous packet blasts if
    # packet blasts are enabled at the same time.
    WriteConfig('enable', 'True')
    wc.Poll(100)
    wvtest.WVPASSGE(wc.NextBlast(), 100)
    for t in xrange(101, 200):
      wc.Poll(t)
    wvtest.WVPASSGE(CountRuns(), 1)

    # Invalid parameter.
    # Disabled. No packet blasts should be run.
    WriteConfig('duration', '-1')
    for t in xrange(200, 300):
      wc.Poll(t)
    wvtest.WVPASSEQ(CountRuns(), 0)

    # Fix invalid parameter.
    # Enabled again with 10 second average interval.
    WriteConfig('duration', '.1')
    for t in xrange(300, 400):
      wc.Poll(t)
    wvtest.WVPASSGE(CountRuns(), 1)

    # Run the packet blast at t=400 to restart the timer.
    wc.Poll(400)
    wvtest.WVPASSGE(CountRuns(), 0)

    # Next poll should be in at most one second regardless of interval.
    wvtest.WVPASSLE(wc.NextTimeout(), 401)

    # Enabled with longer average interval.  The change in interval should
    # trigger a change in next poll timeout.
    WriteConfig('interval', '0.5')
    old_to = wc.NextBlast()
    wc.Poll(401)
    wvtest.WVPASSNE(old_to, wc.NextBlast())
    for t in xrange(402, 410):
      wc.Poll(t)
    wvtest.WVPASSGE(CountRuns(), 1)

    # Switch back to a longer poll interval.
    WriteConfig('interval', '36000')
    ok = False
    for t in xrange(410, 600):
      wc.Poll(t)
      if wc.NextBlast() > t + 200:
        ok = True
    wvtest.WVPASS(ok)

    # And then try rapid polling for a limited time
    WriteConfig('rapidpolling', '800')
    ok = False
    for t in xrange(600, 700):
      wc.Poll(t)
      if wc.NextBlast() < t + 20:
        ok = True
    wvtest.WVPASS(ok)

    # Make sure rapid polling auto-disables
    ok = False
    for t in xrange(700, 900):
      wc.Poll(t)
      if wc.NextBlast() > t + 200:
        ok = True
    wvtest.WVPASS(ok)

  finally:
    time.time = oldtime
    shutil.rmtree(d)
    os.environ['PATH'] = oldpath


if __name__ == '__main__':
  wvtest.wvtest_main()
