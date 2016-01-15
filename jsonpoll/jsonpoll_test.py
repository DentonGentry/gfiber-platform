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

"""Tests for jsonpoll."""

__author__ = 'cgibson@google.com (Chris Gibson)'

import json
import os
import tempfile
import unittest
import jsonpoll

JSON_RESPONSE = {
    'report': {
        'abs_mse_db': '-3276.8',
        'adc_count': '3359',
        'external_agc_ind': '9.2',
        'in_power_rssi_dbc': '-12.0',
        'inband_power_rssi_dbc': '-67.2',
        'internal_agc_db': '55.2',
        'msr_pwr_rssi_dbm': '-20.0',
        'norm_mse_db': '-3270.9',
        'num_samples': 'Undefined',
        'rad_mse_db': '-3267.9',
        'rx_lock_loss_events': 'Undefined',
        'rx_lock_loss_time_ms': 'Undefined',
        'rx_locked': '0',
        'sample_end_tstamp_ms': '1444839743297',
        'sample_start_tstamp_ms': '1444839743287'
    },
    'result': {
        'err_code': 0,
        'err_msg': 'None',
        'status': 'SUCCESS'
    },
    'running_config': {
        'acmb_enable': True,
        'bgt_tx_vga_gain_ind': 0,
        'heartbeat_rate': 60,
        'ip_addr': '10.0.0.40',
        'ip_gateway': '10.0.0.1',
        'ip_netmask': '255.255.255.0',
        'ipv6_addr': 'fe80::7230:d5ff:fe00:1418',
        'manual_acmb_profile_indx': 0,
        'modem_cfg_file': 'default.bin',
        'modem_on': True,
        'pa_lna_enable': True,
        'radio_heater_on': False,
        'radio_on': True,
        'report_avg_window_ms': 10,
        'report_dest_ip': '192.168.1.1',
        'report_dest_port': 4950,
        'report_enable': True,
        'report_interval_hz': 1,
        'rx_khz': 85500000,
        'tx_khz': 75500000
    },
}


class FakeJsonPoll(jsonpoll.JsonPoll):
  """Mock JsonPoll."""

  def GetHttpResponse(self, unused_url):
    self.get_response_called = True
    return json.dumps(JSON_RESPONSE)


class JsonPollTest(unittest.TestCase):

  def setUp(self):
    self.CreateTempFile()
    self.poller = FakeJsonPoll('fakehost.blah', 31337, 1)

  def tearDown(self):
    self.DeleteTempFile()

  def CreateTempFile(self):
    # Create a temp file and have that be the target output file.
    fd, self.output_file = tempfile.mkstemp()
    os.close(fd)

  def DeleteTempFile(self):
    if os.path.exists(self.output_file):
      os.unlink(self.output_file)

  def testRequestStats(self):
    # Create a fake entry in the paths_to_stats map.
    self.poller.paths_to_statfiles = {'fake/url': self.output_file}
    self.poller.RequestStats()
    self.assertEqual(True, self.poller.get_response_called)

    # Read back the contents of the fake output file. It should be the
    # equivalent JSON representation we wrote out from the mock.
    with open(self.output_file, 'r') as f:
      output = ''.join(line.rstrip() for line in f)
      self.assertEqual(json.dumps(JSON_RESPONSE), output)

  def testRequestStatsFailureToCreateOutputFile(self):
    self.poller.paths_to_statfiles = {'fake/url': '/root/cannotwrite'}
    result = self.poller.RequestStats()
    self.assertEqual(False, result)

  def testCachedRequestStats(self):
    # Set the "last_response" as our mock output. This should mean we do not
    # write anything to the output file.
    self.poller.last_response = json.dumps(JSON_RESPONSE)

    # Create a fake entry in the paths_to_stats map.
    self.poller.paths_to_statfiles = {'fake/url': self.output_file}
    result = self.poller.RequestStats()
    self.assertEqual(True, self.poller.get_response_called)
    self.assertEqual(True, result)

    # Read back the contents of the fake output file: It should be empty.
    with open(self.output_file, 'r') as f:
      output = ''.join(line.rstrip() for line in f)
      self.assertEqual('', output)


if __name__ == '__main__':
  unittest.main()
