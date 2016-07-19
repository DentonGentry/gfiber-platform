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

import glob
import os
import random
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
  old_wifiblaster_dir = waveguide.WIFIBLASTER_DIR
  waveguide.WIFIBLASTER_DIR = tempfile.mkdtemp()
  try:
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

  finally:
    shutil.rmtree(waveguide.WIFIBLASTER_DIR)
    waveguide.WIFIBLASTER_DIR = old_wifiblaster_dir


class Empty(object):
  pass


@wvtest.wvtest
def PollTest():
  d = tempfile.mkdtemp()
  old_wifiblaster_dir = waveguide.WIFIBLASTER_DIR
  waveguide.WIFIBLASTER_DIR = tempfile.mkdtemp()
  oldexpovariate = random.expovariate
  oldpath = os.environ['PATH']
  oldtime = time.time

  def FakeTime():
    faketime[0] += 1
    return faketime[0]

  try:
    random.expovariate = lambda lambd: random.uniform(0, 2 * 1.0 / lambd)
    time.time = FakeTime
    os.environ['PATH'] = 'fake:' + os.environ['PATH']
    sys.path.insert(0, 'fake')
    waveguide.opt = Empty()
    waveguide.opt.status_dir = d

    for flags in [{'phyname': 'phy-22:22', 'vdevname': 'wlan-22:22',
                   'high_power': True, 'tv_box': False},
                  {'phyname': 'phy-22:22', 'vdevname': 'wlan-22:22',
                   'high_power': False, 'tv_box': True}]:
      # Reset time and config directory before every test run.
      faketime = [-1]
      for f in glob.glob(os.path.join(d, '*')):
        os.remove(f)

      managers = []
      wc = waveguide.WifiblasterController(managers, d)
      manager = waveguide.WlanManager(wifiblaster_controller=wc, **flags)
      managers.append(manager)
      manager.UpdateStationInfo()

      def WriteConfig(k, v):
        open(os.path.join(d, 'wifiblaster.%s' % k), 'w').write(v)

      WriteConfig('duration', '.1')
      WriteConfig('enable', 'False')
      WriteConfig('fraction', '10')
      WriteConfig('interval', '10')
      WriteConfig('measureall', '0')
      WriteConfig('size', '1470')

      def CountRuns():
        try:
          v = open('fake/wifiblaster.out').readlines()
        except IOError:
          return 0
        else:
          os.unlink('fake/wifiblaster.out')
          return len(v)

      # Get rid of any leftovers.
      CountRuns()

      # Disabled.
      # No measurements should be run.
      print manager.GetState()
      for t in xrange(0, 100):
        wc.Poll(t)
      wvtest.WVPASSEQ(CountRuns(), 0)

      # Enabled.
      # The first measurement should be one cycle later than the start time.
      # This is not an implementation detail: it prevents multiple APs from
      # running simultaneous measurements if measurements are enabled at the
      # same time.
      WriteConfig('enable', 'True')
      wc.Poll(100)
      wvtest.WVPASSGT(wc.NextMeasurement(), 100)
      for t in xrange(101, 200):
        wc.Poll(t)
      wvtest.WVPASSGE(CountRuns(), 1)

      # Invalid parameter.
      # Disabled. No measurements should be run.
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

      # Next poll should be in at most one second regardless of interval.
      wvtest.WVPASSLE(wc.NextTimeout(), 400)

      # Enabled with shorter average interval.  The change in interval should
      # trigger a change in next poll timeout.
      WriteConfig('interval', '0.5')
      old_to = wc.NextMeasurement()
      wc.Poll(400)
      wvtest.WVPASSNE(old_to, wc.NextMeasurement())
      for t in xrange(401, 500):
        wc.Poll(t)
      wvtest.WVPASSGE(CountRuns(), 1)

      # Request all clients to be measured and make sure it only happens once.
      # Disable automated measurement so they are not counted.
      WriteConfig('interval', '0')
      WriteConfig('measureall', str(faketime[0]))
      wc.Poll(500)
      wvtest.WVPASSEQ(CountRuns(), 1)
      wc.Poll(501)
      wvtest.WVPASSEQ(CountRuns(), 0)

      # Measure on association only if enabled.
      wc.MeasureOnAssociation(manager.vdevname,
                              manager.GetState().assoc[0].mac)
      wvtest.WVPASSEQ(CountRuns(), 0)
      WriteConfig('onassociation', 'True')
      wc.MeasureOnAssociation(manager.vdevname,
                              manager.GetState().assoc[0].mac)
      wvtest.WVPASSEQ(CountRuns(), 1)
  finally:
    random.expovariate = oldexpovariate
    time.time = oldtime
    shutil.rmtree(d)
    os.environ['PATH'] = oldpath
    shutil.rmtree(waveguide.WIFIBLASTER_DIR)
    waveguide.WIFIBLASTER_DIR = old_wifiblaster_dir


if __name__ == '__main__':
  wvtest.wvtest_main()
