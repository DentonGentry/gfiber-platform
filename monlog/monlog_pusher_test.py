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
import unittest
import urllib2

import mock
from mock import Mock
from mock import patch

import monlog_pusher
from monlog_pusher import LogCollector
from monlog_pusher import LogPusher


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
    self.assertEquals(sorted(expected_logs, key=lambda x: x[0]),
                      sorted(collected_logs, key=lambda x: x[0]))
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
      mock_open.side_effect = IOError()
      self.assertRaises(monlog_pusher.ExeException, log_collector.CollectLogs)
      mock_open.assert_called_once()


class LogPusherTest(unittest.TestCase):
  """Test LogPusher using mock connection to the cloud server."""

  def GenerateDeviceRegInfoTempFile(self):
    """Helper function to generate dev_reg_info temp file."""
    self._dev_reg_info = tempfile.NamedTemporaryFile()
    self._dev_reg_info.write(json.dumps({'client_secret': 'ClientSecret',
                                         'client_id': 'ClientId',
                                         'refresh_token': 'RefreshToken',
                                         'oauth_url': 'OAuthURL/',
                                         'device_id': 'DeviceId',
                                         'random_att': 'RandomAtt'
                                        }))
    self._dev_reg_info.seek(0)

  @mock.patch('monlog_pusher.urllib2')
  def testGetAccessTokenOk(self, mock_urllib2):
    """Test successful flow of GetAccessToken."""

    self.GenerateDeviceRegInfoTempFile()

    # Mock return from authentication server.
    resp = Mock()
    resp.read.side_effect = [
        json.dumps({'access_token': 'AccessToken', 'token_type': 'TokenType',
                    'other_att': 'OtherAtt'})]
    mock_urllib2.urlopen.return_value = resp

    # Call GetAccessToken and verify return value.
    log_pusher = LogPusher('TestServer', self._dev_reg_info.name)
    self.assertEqual(('DeviceId', 'TokenType', 'AccessToken'),
                     (log_pusher._device_id, log_pusher._token_type,
                      log_pusher._access_token))

    # Verify expected request data built from the test environment.
    expected_req_data = ('client_secret=ClientSecret&grant_type=refresh_token&'
                         'refresh_token=RefreshToken&client_id=ClientId')
    mock_urllib2.Request.assert_called_once_with(
        'OAuthURL/token', expected_req_data)
    mock_urllib2.urlopen.assert_called_once()

  @mock.patch('monlog_pusher.urllib2.urlopen')
  def testGetAccessTokenException(self, mock_urlopen):
    """Test different bad scenarios which GetAccessToken raises exception."""

    # 1. Bad dev_reg_info NOT in json format.
    tmp_file = tempfile.NamedTemporaryFile()
    tmp_file.write('This file is NOT in json format.')
    tmp_file.seek(0)
    self.assertRaises(monlog_pusher.ExeException, LogPusher,
                      'TestServer/', tmp_file.name)

    self.GenerateDeviceRegInfoTempFile()

    # 2. Mock bad authentication request raises HTTPError.
    mock_urlopen.side_effect = Mock(spec=urllib2.HTTPError)
    self.assertRaises(monlog_pusher.ExeException, LogPusher,
                      'TestServer/', self._dev_reg_info.name)
    mock_urlopen.assert_called_once()

    # 3. Mock bad authentication request raises URLError.
    mock_urlopen.side_effect = Mock(spec=urllib2.URLError)
    self.assertRaises(monlog_pusher.ExeException, LogPusher,
                      'TestServer/', self._dev_reg_info.name)
    mock_urlopen.assert_called_once()

    # 4. Mock bad authentication request raises HTTPException.
    mock_urlopen.side_effect = Mock(spec=httplib.HTTPException)
    self.assertRaises(monlog_pusher.ExeException, LogPusher,
                      'TestServer/', self._dev_reg_info.name)
    mock_urlopen.assert_called_once()

    # 5. Mock bad authentication request raises general exception.
    mock_urlopen.side_effect = Mock(spec=Exception)
    self.assertRaises(monlog_pusher.ExeException, LogPusher,
                      'TestServer/', self._dev_reg_info.name)
    mock_urlopen.assert_called_once()

    # 6. Mock bad authentication response NOT in json format.
    resp = Mock()
    resp.read.side_effect = ['Not_json_format']
    mock_urlopen.return_value = resp
    self.assertRaises(monlog_pusher.ExeException, LogPusher,
                      'TestServer', self._dev_reg_info.name)
    mock_urlopen.assert_called_once()

  @mock.patch('monlog_pusher.urllib2')
  @mock.patch('monlog_pusher.LogPusher._GetAccessToken')
  def testPushLogOk(self, mock_access_token, mock_urllib2):
    """Test successful flow of LogPusher mocking out GetAccessToken."""
    # Return mock access token used to authorize the log server.
    mock_access_token.return_value = ('device_id', 'type', 'access_token')

    # Mock connection to the server.
    mock_urllib2.urlopen.return_value = Mock()
    log_pusher = LogPusher('TestServer/')

    # Verify PushLog good flow.
    self.assertTrue(log_pusher.PushLog({'data': 'Example'}, 'metrics'))

    # Verify expected request data built from the test environment.
    expected_req_data = ('{"data":"Example","deviceId":"device_id"}')
    mock_urllib2.Request.assert_called_once_with(
        'TestServer/device_id/metrics:batchCreatePoints', expected_req_data)
    mock_access_token.assert_called_once()
    mock_urllib2.urlopen.assert_called_once()

  @mock.patch('monlog_pusher.urllib2.urlopen')
  @mock.patch('monlog_pusher.LogPusher._GetAccessToken')
  def testPushLogException(self, mock_access_token, mock_urlopen):
    """Test different bad push requests which raises exception."""
    # Return mock access token used to authorize log server.
    mock_access_token.return_value = ('device_id', 'type', 'access_token')

    # 1. Unsupported log_type raises ExeException.
    log_pusher = LogPusher('TestServer/')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'structuredLogs')

    # 2. Bad data i.e. not a mapping object or valid non-string sequence.
    log_pusher = LogPusher('TestServer/')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      'data', 'metrics')
    mock_access_token.assert_called_once()

    # 3. Mock bad request raises HTTPError.
    mock_urlopen.side_effect = Mock(spec=urllib2.HTTPError)
    log_pusher = LogPusher('TestServer/')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'metrics')
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()

    # 4. Mock bad request raises URLError.
    mock_urlopen.side_effect = Mock(spec=urllib2.URLError)
    log_pusher = LogPusher('TestServer/')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'metrics')
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()

    # 5. Mock bad request raises HTTPException.
    mock_urlopen.side_effect = Mock(spec=httplib.HTTPException)
    log_pusher = LogPusher('TestServer/')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'metrics')
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()

    # 6. Mock bad request raises general Exception.
    mock_urlopen.side_effect = Mock(spec=Exception)
    log_pusher = LogPusher('TestServer/')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'metrics')
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()


if __name__ == '__main__':
  unittest.main()
