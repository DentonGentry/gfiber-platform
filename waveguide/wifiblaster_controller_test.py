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

import mock
import helpers
import log
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
def LogWifiblasterResultsTest(log_mock):
  manager = mock.MagicMock()
  wc = waveguide.WifiblasterController([manager], '')

  # Stub WlanManager.AnonymizeMAC to use a fake consensus key.
  manager.AnonymizeMAC = lambda mac: log.AnonymizeMAC(16 * 'x', mac)

  # Result should be anonymized and not include "not connected" lines.
  stdout = ('mac=11:11:11:11:11:11 tx_packets=2052 tx_retries=7 '
            'tx_failed=0 throughput=10506134\n'
            'mac=22:22:22:22:22:22 not connected\n'
            'malformed 11:11:11:11:11:11 but has macs 11:11:11:11:11:11\n')
  wc._LogWifiblasterResults(0, stdout, '')
  expected = ('wifiblaster: mac=CYAFVU tx_packets=2052 tx_retries=7 '
              'tx_failed=0 throughput=10506134\n'
              'wifiblaster: malformed CYAFVU but has macs CYAFVU\n')
  result = '\n'.join([s for ((s,), _) in log_mock.call_args_list]) + '\n'
  wvtest.WVPASSEQ(result, expected)


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
                          last_seen=None)],
      arp=None)

  # Stub WifiblasterController._ReadFile to return fake parameter file data.
  files = {'wifiblaster.duration': '.1',
           'wifiblaster.enable': 'False',
           'wifiblaster.interval': '10',
           'wifiblaster.size': '64'}
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


if __name__ == '__main__':
  wvtest.wvtest_main()
