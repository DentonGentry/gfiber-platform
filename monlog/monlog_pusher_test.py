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

"""Unittests for monlog_pusher."""

__author__ = 'hunguyen@google.com (Huy Nguyen)'

import httplib
import json

import os
import shutil
import tempfile
import time
import unittest
import urllib2

import mock
from mock import Mock
from mock import patch

import monlog_pusher
from monlog_pusher import LogCollector
from monlog_pusher import MonlogPusher


class LogCollectorTest(unittest.TestCase):
  """Tests LogCollector functionality."""

  def setUp(self):
    self.tmpdir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.tmpdir)

  def GenerateTempFile(self, prefix='spacecast_log', num_files=3):
    """Helper function to generate temp files used for testing.

    Args:
      prefix: prefix of temp file name.
      num_files: number of temp files to be generated.
    Returns:
      A list contains temp file names in order of creation.
    """
    temp_files = []
    for i in range(num_files):
      with tempfile.NamedTemporaryFile(prefix=prefix, dir=self.tmpdir,
                                       delete=False) as temp:
        log_data = {('LogData%d' %i): ('Example%d' %i)}
        temp.write(json.dumps(log_data))
        temp.flush()
        temp_files.append(temp.name)
        # Force sleep for 0.1s to differentiate between the files.
        time.sleep(0.1)
    return temp_files

  def testGoodLogCollector(self):
    """Tests successful flow of LogCollector."""

    # Empty log collection returns None.
    log_collector = LogCollector(self.tmpdir)
    log_collector.CollectLogs()
    self.assertTrue(log_collector.IsEmpty())
    self.assertEqual((None, None, None), log_collector.GetAvailableLog())

    # Generate *incomplete* log files and verifies no file is collected.
    self.GenerateTempFile(prefix='temp_spacecast_log')
    log_collector.CollectLogs()
    self.assertTrue(log_collector.IsEmpty())
    self.assertEqual((None, None, None), log_collector.GetAvailableLog())

    # Generate *complete* log files and verifies contents are correct.
    temp_files = self.GenerateTempFile(num_files=4)
    log_collector.CollectLogs()

    # Loop to collect all log files.
    collected_logs = []
    for dummy_i in range(4):
      self.assertFalse(log_collector.IsEmpty())
      collected_logs.append(log_collector.GetAvailableLog())

    expected_logs = [(temp_files[0], {'LogData0': 'Example0'}, 'metrics'),
                     (temp_files[1], {'LogData1': 'Example1'}, 'metrics'),
                     (temp_files[2], {'LogData2': 'Example2'}, 'metrics'),
                     (temp_files[3], {'LogData3': 'Example3'}, 'metrics')]
    self.assertEquals(expected_logs, collected_logs)
    for temp_file in temp_files:
      os.remove(temp_file)

    self.assertTrue(log_collector.IsEmpty())
    self.assertEqual((None, None, None), log_collector.GetAvailableLog())

    # Verify *complete* log files are cleaned up after use.
    log_files = [f for f in os.listdir(self.tmpdir)
                 if f.startswith('spacecast_log')]
    self.assertEqual([], log_files)

  def testBadLogCollector(self):
    """Tests different bad scenarios which raises exception."""

    log_collector = LogCollector(self.tmpdir)
    # 1. Mock non-existed log dir to raise OSError.
    with patch('monlog_pusher.os') as mock_os:
      mock_os.listdir.side_effect = OSError()
      self.assertRaises(monlog_pusher.ExeException, log_collector.CollectLogs)
      mock_os.listdir.assert_called_once_with(self.tmpdir)

    # 2. Mock failed to open files to raise IOError.
    self.GenerateTempFile()
    with patch('__builtin__.open') as mock_open:
      mock_open.side_effect = IOError(1, 'SuperFail')
      self.assertRaisesRegexp(monlog_pusher.ExeException,
                              'Bad file:.*. Error: SuperFail$',
                              log_collector.CollectLogs)
      self.assertEqual(3, mock_open.call_count)

    # 3. Mock failed to load json file to raise ValueError.
    self.GenerateTempFile()
    with patch('monlog_pusher.json') as mock_json:
      mock_json.load.side_effect = ValueError('NoJson')
      self.assertRaisesRegexp(monlog_pusher.ExeException,
                              'Bad file:.*. Error: NoJson',
                              log_collector.CollectLogs)
      self.assertEqual(3, mock_json.load.call_count)


class MonlogPusherTest(unittest.TestCase):
  """Test MonlogPusher using mock connection to the cloud server."""

  def GenerateMonlogRegInfoTempFile(
      self, has_device_id=True, has_token_type=True, has_access_token=True):
    """Helper function to generate dev_reg_info temp file.

    Args:
      has_device_id: whether the file contains device_id.
      has_token_type: whether the file contains token_type.
      has_access_token: whether the file contains access_token.
    """
    self._monlog_reg_info = tempfile.NamedTemporaryFile()
    reg_info_dict = {'random_att': 'RandomAtt'}
    if has_device_id: reg_info_dict['device_id'] = 'DeviceId'
    if has_token_type: reg_info_dict['token_type'] = 'TokenType'
    if has_access_token: reg_info_dict['access_token'] = 'AccessToken'
    self._monlog_reg_info.write(json.dumps(reg_info_dict))
    self._monlog_reg_info.seek(0)

  def testGetAccessTokenOk(self):
    """Test successful flow of GetAccessToken."""

    self.GenerateMonlogRegInfoTempFile()
    # Call GetAccessToken and verify return value.
    log_pusher = MonlogPusher('TestServer', self._monlog_reg_info.name)
    self.assertEqual(('DeviceId', 'TokenType', 'AccessToken'),
                     (log_pusher._device_id, log_pusher._token_type,
                      log_pusher._access_token))

  def testGetAccessTokenException(self):
    """Test different bad scenarios which GetAccessToken raises exception."""

    # 1. Bad monlog_reg_info NOT in json format.
    tmp_file = tempfile.NamedTemporaryFile()
    tmp_file.write('This file is NOT in json format.')
    tmp_file.seek(0)
    self.assertRaisesRegexp(monlog_pusher.ExeException,
                            'Failed to load json .*',
                            MonlogPusher, 'TestServer/', tmp_file.name)

    # 2. Failed to read monlog_reg_info.
    self.GenerateMonlogRegInfoTempFile()
    with patch('__builtin__.open') as mock_open:
      mock_open.side_effect = IOError()
      self.assertRaisesRegexp(monlog_pusher.ExeException,
                              'Failed to open file .*$',
                              MonlogPusher, 'TestServer/',
                              self._monlog_reg_info.name)
      self.assertEqual(1, mock_open.call_count)

    # 3. monlog_reg_info lacks 'device_id' field.
    self.GenerateMonlogRegInfoTempFile(has_device_id=False)
    self.assertRaisesRegexp(monlog_pusher.ExeException,
                            'Missing monlog registration info .*',
                            MonlogPusher, 'TestServer/',
                            self._monlog_reg_info.name)

    # 4. monlog_reg_info lacks 'token_type' field.
    self.GenerateMonlogRegInfoTempFile(has_token_type=False)
    self.assertRaisesRegexp(monlog_pusher.ExeException,
                            'Missing monlog registration info .*',
                            MonlogPusher, 'TestServer/',
                            self._monlog_reg_info.name)

    # 5. monlog_reg_info lacks 'access_token' field.
    self.GenerateMonlogRegInfoTempFile(has_access_token=False)
    self.assertRaisesRegexp(monlog_pusher.ExeException,
                            'Missing monlog registration info .*',
                            MonlogPusher, 'TestServer/',
                            self._monlog_reg_info.name)

  @mock.patch('monlog_pusher.urllib2')
  @mock.patch('monlog_pusher.MonlogPusher._GetAccessToken')
  def testPushLogOk(self, mock_access_token, mock_urllib2):
    """Test successful flow of MonlogPusher mocking out GetAccessToken."""
    # Return mock access token used to authorize the log server.
    mock_access_token.return_value = ('device_id', 'type', 'access_token')

    # Mock connection to the server.
    mock_urllib2.urlopen.return_value = Mock()
    log_pusher = MonlogPusher('TestServer/')

    # Verify PushLog good flow.
    self.assertTrue(log_pusher.PushLog(
        {'id': {'type': 'Type', 'deviceId': ''}, 'data': 'Example'}, 'metrics'))

    # Verify expected request data built from the test environment.
    expected_req_data = (
        '{"data":"Example","id":{"type":"Type","deviceId":"device_id"}}')
    mock_urllib2.Request.assert_called_once_with(
        'TestServer/device_id/metrics:batchCreatePoints', expected_req_data)
    mock_access_token.assert_called_once_with()
    self.assertEqual(1, mock_urllib2.urlopen.call_count)

  @mock.patch('monlog_pusher.urllib2.urlopen')
  @mock.patch('monlog_pusher.MonlogPusher._GetAccessToken')
  def testPushLogException(self, mock_access_token, mock_urlopen):
    """Test different bad push requests which raises exception."""
    # Return mock access token used to authorize log server.
    mock_access_token.return_value = ('device_id', 'type', 'access_token')

    # 1. Unsupported log_type raises ExeException.
    log_pusher = MonlogPusher('TestServer/')
    self.assertRaises(
        monlog_pusher.ExeException, log_pusher.PushLog,
        {'id': {'type': 'Type', 'deviceId': ''}, 'data': 'Example'},
        'structuredLogs')
    self.assertEqual(1, mock_access_token.call_count)

    # 2. Bad data i.e. not a mapping object or valid non-string sequence.
    log_pusher = MonlogPusher('TestServer/')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      'data', 'metrics')
    self.assertEqual(2, mock_access_token.call_count)

    # 3. Mock bad request raises HTTPError.
    mock_urlopen.side_effect = Mock(spec=urllib2.HTTPError)
    log_pusher = MonlogPusher('TestServer/')
    self.assertRaises(
        monlog_pusher.ExeException, log_pusher.PushLog,
        {'id': {'type': 'Type', 'deviceId': ''}, 'data': 'Example'}, 'metrics')
    self.assertEqual(3, mock_access_token.call_count)
    self.assertEqual(1, mock_urlopen.call_count)

    # 4. Mock bad request raises URLError.
    mock_urlopen.side_effect = Mock(spec=urllib2.URLError)
    log_pusher = MonlogPusher('TestServer/')
    self.assertRaises(
        monlog_pusher.ExeException, log_pusher.PushLog,
        {'id': {'type': 'Type', 'deviceId': ''}, 'data': 'Example'}, 'metrics')
    self.assertEqual(4, mock_access_token.call_count)
    self.assertEqual(2, mock_urlopen.call_count)

    # 5. Mock bad request raises HTTPException.
    mock_urlopen.side_effect = Mock(spec=httplib.HTTPException)
    log_pusher = MonlogPusher('TestServer/')
    self.assertRaises(
        monlog_pusher.ExeException, log_pusher.PushLog,
        {'id': {'type': 'Type', 'deviceId': ''}, 'data': 'Example'}, 'metrics')
    self.assertEqual(5, mock_access_token.call_count)
    self.assertEqual(3, mock_urlopen.call_count)

    # 6. Mock bad request raises general Exception.
    mock_urlopen.side_effect = Mock(spec=Exception)
    log_pusher = MonlogPusher('TestServer/')
    self.assertRaises(
        monlog_pusher.ExeException, log_pusher.PushLog,
        {'id': {'type': 'Type', 'deviceId': ''}, 'data': 'Example'}, 'metrics')
    self.assertEqual(6, mock_access_token.call_count)
    self.assertEqual(4, mock_urlopen.call_count)


if __name__ == '__main__':
  unittest.main()
