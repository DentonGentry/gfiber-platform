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

__author__ = 'mikemu@google.com (Mike Mu)'

import os
import shutil
import tempfile
import mock
import helpers
import waveguide
import wgdata
from wvtest import wvtest


@wvtest.wvtest
def ReadParameterTest():
  wc = waveguide.WifiblasterController([], '')

  # Stub WifiblasterController._ReadFile to return fake parameter file data.
  files = {}
  wc._ReadFile = lambda filename: files[filename]

  # Result should be equal in value and type (unless None).
  testcases = [('1\n', int, 1),
               ('1.5\n', float, 1.5),
               ('True\n', str, 'True\n'),
               ('x\n', int, None)]
  for (read_data, typecast, expected) in testcases:
    files['wifiblaster.name'] = read_data
    result = wc._ReadParameter('name', typecast)
    wvtest.WVPASSEQ(result, expected)
    if result:
      wvtest.WVPASS(isinstance(result, typecast))


@wvtest.wvtest
def ReadParameterIOErrorTest():
  wc = waveguide.WifiblasterController([], '')

  # Stub WifiblasterController._ReadFile to raise an IOError.
  def RaiseIOError(_):
    raise IOError
  wc._ReadFile = RaiseIOError

  # IOError should be caught.
  wvtest.WVPASSEQ(wc._ReadParameter('name', int), None)


@wvtest.wvtest
@mock.patch('log.Log')
@mock.patch('waveguide.WifiblasterController._SaveWifiblasterResult')
def LogWifiblasterResultsTest(save_mock, log_mock):
  manager = mock.MagicMock()
  wc = waveguide.WifiblasterController([manager], '')

  # Set the consensus key to a known value.
  waveguide.consensus_key = 16 * 'x'

  # Result should be anonymized and not include "not connected" lines.
  stdout = ('version=1 mac=11:11:11:11:11:11 throughput=10000000 '
            'samples=5000000,15000000\n'
            'malformed 11:11:11:11:11:11 but has macs 11:11:11:11:11:11\n')
  wc._LogWifiblasterResults(0, stdout, '')
  expected = ('wifiblaster: version=1 mac=CYAFVU throughput=10000000 '
              'samples=5000000,15000000\n'
              'wifiblaster: malformed CYAFVU but has macs CYAFVU\n')
  result = '\n'.join([s for ((s,), _) in log_mock.call_args_list]) + '\n'
  wvtest.WVPASSEQ(result, expected)
  expected = [('version=1 mac=11:11:11:11:11:11 throughput=10000000 '
               'samples=5000000,15000000'),
              'malformed 11:11:11:11:11:11 but has macs 11:11:11:11:11:11']
  result = [s for ((s, _), _) in save_mock.call_args_list]
  wvtest.WVPASSEQ(result, expected)


@wvtest.wvtest
@mock.patch('time.time')
def SaveWifiblasterResultTest(unused_time_mock):
  oldwifiblasterdir = waveguide.WIFIBLASTER_DIR
  waveguide.WIFIBLASTER_DIR = tempfile.mkdtemp()
  manager = mock.MagicMock()
  wc = waveguide.WifiblasterController([manager], '')

  wc._SaveWifiblasterResult('j 00:00:00:00:00:00\n', '00:00:00:00:00:00')
  with open(os.path.join(waveguide.WIFIBLASTER_DIR, '00:00:00:00:00:00')) as f:
    contents = f.read()
  wvtest.WVPASSEQ('1 j 00:00:00:00:00:00\n', contents)
  wc._SaveWifiblasterResult('k 00:00:00:00:00:00\n', '00:00:00:00:00:00')
  wc._SaveWifiblasterResult('l 00:00:00:00:00:00\n', '00:00:00:00:00:00')
  with open(os.path.join(waveguide.WIFIBLASTER_DIR, '00:00:00:00:00:00')) as f:
    contents = f.read()
  expected = ('1 j 00:00:00:00:00:00\n'
              '1 k 00:00:00:00:00:00\n'
              '1 l 00:00:00:00:00:00\n')
  wvtest.WVPASSEQ(expected, contents)

  for _ in range(256):
    wc._SaveWifiblasterResult('x 00:00:00:00:00:00\n', '00:00:00:00:00:00')
  with open(os.path.join(waveguide.WIFIBLASTER_DIR, '00:00:00:00:00:00')) as f:
    lines = f.readlines()
    wvtest.WVPASSEQ(128, len(lines))

  shutil.rmtree(waveguide.WIFIBLASTER_DIR)
  waveguide.WIFIBLASTER_DIR = oldwifiblasterdir


@wvtest.wvtest
@mock.patch('waveguide.RunProc')
@mock.patch('random.expovariate')
def PollTest(expovariate_mock, run_proc_mock):
  manager = mock.MagicMock()
  wc = waveguide.WifiblasterController([manager], '')

  # Stub random.expovariate to return the average.
  expovariate_mock.side_effect = lambda lambd: 1.0 / lambd

  # Stub WlanManager.GetState to return a single client.
  manager.GetState = lambda: wgdata.State(  # pylint: disable=g-long-lambda
      me=None,
      seen_bss=None,
      channel_survey=None,
      assoc=[wgdata.Assoc(mac=helpers.EncodeMAC('11:11:11:11:11:11'),
                          rssi=None,
                          last_seen=None,
                          can5G=None)],
      arp=None)

  # Stub WifiblasterController._ReadFile to return fake parameter file data.
  files = {'wifiblaster.duration': '.1',
           'wifiblaster.enable': 'False',
           'wifiblaster.fraction': '10',
           'wifiblaster.interval': '10',
           'wifiblaster.rapidpolling': '0',
           'wifiblaster.size': '1470'}
  wc._ReadFile = lambda filename: files[filename]

  # Disabled. No packet blasts should be run.
  for t in xrange(0, 100):
    wc.Poll(t)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 0)

  # Enable.
  files['wifiblaster.enable'] = 'True'

  # Enabled with 10 second interval. 9 packet blasts should be run. The
  # expected number is not 10 because the first packet blast should be one
  # cycle later than the start time. This is not an implementation detail: it
  # prevents multiple APs from running simultaneous packet blasts if packet
  # blasts are enabled at the same time.
  for t in xrange(100, 200):
    wc.Poll(t)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 9)

  # Invalid parameter.
  files['wifiblaster.duration'] = '-1'

  # Disabled. No packet blasts should be run.
  for t in xrange(200, 300):
    wc.Poll(t)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 9)

  # Fix invalid parameter.
  files['wifiblaster.duration'] = '.1'

  # Enabled with 10 second interval. 9 packet blasts should be run.
  for t in xrange(300, 400):
    wc.Poll(t)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 18)

  # Run the packet blast at t=400 to restart the timer.
  wc.Poll(400)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 19)

  # Next poll should be in at most one second regardless of interval.
  wvtest.WVPASSLE(wc.NextTimeout(), 401)

  # Interval changed.
  files['wifiblaster.interval'] = '1'

  # Enabled with 1 second interval. 8 packet blasts should be run without
  # waiting for the previous 10 second interval to finish.
  for t in xrange(401, 410):
    wc.Poll(t)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 27)


@wvtest.wvtest
@mock.patch('waveguide.RunProc')
@mock.patch('random.expovariate')
def RapidPollingTest(expovariate_mock, run_proc_mock):
  manager = mock.MagicMock()
  wc = waveguide.WifiblasterController([manager], '')

  # Stub random.expovariate to return the average.
  expovariate_mock.side_effect = lambda lambd: 1.0 / lambd

  # Stub WlanManager.GetState to return a single client.
  manager.GetState = lambda: wgdata.State(  # pylint: disable=g-long-lambda
      me=None,
      seen_bss=None,
      channel_survey=None,
      assoc=[wgdata.Assoc(mac=helpers.EncodeMAC('11:11:11:11:11:11'),
                          rssi=None,
                          last_seen=None,
                          can5G=None)],
      arp=None)

  # Stub WifiblasterController._ReadFile to return fake parameter file data.
  files = {'wifiblaster.duration': '.1',
           'wifiblaster.enable': 'True',
           'wifiblaster.fraction': '10',
           'wifiblaster.interval': '3600',
           'wifiblaster.rapidpolling': '0',
           'wifiblaster.size': '1470'}
  wc._ReadFile = lambda filename: files[filename]

  # Enabled with 3600 second interval. No packet blasts should be run.
  for t in xrange(0, 100):
    wc.Poll(t)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 0)

  files['wifiblaster.rapidpolling'] = 200

  # Rapid polling with 10 second interval. 9 packet blasts should be run.
  for t in xrange(100, 200):
    wc.Poll(t)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 9)

  # Rapid polling expired, packet blasts should now run at wifiblaster.interval
  for t in xrange(200, 300):
    wc.Poll(t)
  wvtest.WVPASSEQ(run_proc_mock.call_count, 9)


if __name__ == '__main__':
  wvtest.wvtest_main()
