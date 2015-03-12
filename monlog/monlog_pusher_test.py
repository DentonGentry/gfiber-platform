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
from monlog_pusher import LogPusher


class LogCollectorTest(unittest.TestCase):
  """Tests LogCollector functionality."""

  def setUp(self):
    self.tmpdir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.tmpdir)

  def GenerateTempFile(self, num_files):
    """Helper function to generate temp files used for testing."""
    for i in range(num_files):
      with tempfile.NamedTemporaryFile(dir=self.tmpdir, delete=False) as temp:
        temp.write('LogData%d' % i)
        temp.flush()
        # Force sleep for 0.1s to differentiate between the files.
        time.sleep(0.1)

  def testGoodLogCollector(self):
    """Tests successful flow of LogCollector."""

    # Empty log collection returns None.
    log_collector = LogCollector(self.tmpdir)
    self.assertTrue(log_collector.IsEmpty())
    self.assertEqual((None, None), log_collector.GetNextLog())

    # Initialize log collector again with good log files and verifies contents
    # are correct.
    self.GenerateTempFile(3)
    log_collector = LogCollector(self.tmpdir)
    self.assertFalse(log_collector.IsEmpty())
    self.assertEqual(('LogData0', 'structuredLogs'), log_collector.GetNextLog())
    self.assertFalse(log_collector.IsEmpty())
    self.assertEqual(('LogData1', 'structuredLogs'), log_collector.GetNextLog())
    self.assertFalse(log_collector.IsEmpty())
    self.assertEqual(('LogData2', 'structuredLogs'), log_collector.GetNextLog())
    self.assertTrue(log_collector.IsEmpty())
    self.assertEqual((None, None), log_collector.GetNextLog())

    # Verify log files are cleaned up properly after use.
    self.assertEqual([], os.listdir(self.tmpdir))

  def testBadLogCollector(self):
    """Tests different bad scenarios which raises exception."""

    # 1. Mock non-existed log dir to raise OSError.
    with patch('monlog_pusher.os') as mock_os:
      mock_os.listdir.side_effect = OSError()
      self.assertRaises(monlog_pusher.ExeException, LogCollector, self.tmpdir)
      mock_os.listdir.assert_called_once_with(self.tmpdir)

    # 2. Mock failed to open files to raise IOError.
    self.GenerateTempFile(2)
    with patch('__builtin__.open') as mock_open:
      mock_open.side_effect = IOError()
      self.assertRaises(monlog_pusher.ExeException, LogCollector, self.tmpdir)
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
                     log_pusher.GetAccessToken())

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
    log_pusher = LogPusher('TestServer', tmp_file.name)
    self.assertRaises(monlog_pusher.ExeException, log_pusher.GetAccessToken)

    self.GenerateDeviceRegInfoTempFile()

    # 2. Mock bad authentication request raises HTTPError.
    mock_urlopen.side_effect = Mock(spec=urllib2.HTTPError)
    log_pusher = LogPusher('TestServer', self._dev_reg_info.name)
    self.assertRaises(monlog_pusher.ExeException, log_pusher.GetAccessToken)
    mock_urlopen.assert_called_once()

    # 3. Mock bad authentication request raises URLError.
    mock_urlopen.side_effect = Mock(spec=urllib2.URLError)
    log_pusher = LogPusher('TestServer', self._dev_reg_info.name)
    self.assertRaises(monlog_pusher.ExeException, log_pusher.GetAccessToken)
    mock_urlopen.assert_called_once()

    # 4. Mock bad authentication request raises HTTPException.
    mock_urlopen.side_effect = Mock(spec=httplib.HTTPException)
    log_pusher = LogPusher('TestServer', self._dev_reg_info.name)
    self.assertRaises(monlog_pusher.ExeException, log_pusher.GetAccessToken)
    mock_urlopen.assert_called_once()

    # 5. Mock bad authentication request raises general exception.
    mock_urlopen.side_effect = Mock(spec=Exception)
    log_pusher = LogPusher('TestServer', self._dev_reg_info.name)
    self.assertRaises(monlog_pusher.ExeException, log_pusher.GetAccessToken)
    mock_urlopen.assert_called_once()

    # 6. Mock bad authentication response NOT in json format.
    resp = Mock()
    resp.read.side_effect = ['Not_json_format']
    mock_urlopen.return_value = resp
    log_pusher = LogPusher('TestServer', self._dev_reg_info.name)
    self.assertRaises(monlog_pusher.ExeException, log_pusher.GetAccessToken)
    mock_urlopen.assert_called_once()

  @mock.patch('monlog_pusher.urllib2.urlopen')
  @mock.patch('monlog_pusher.LogPusher.GetAccessToken')
  def testPushLogOk(self, mock_access_token, mock_urlopen):
    """Test successful flow of LogPusher mocking out GetAccessToken."""
    # Return mock access token used to authorize the log server.
    mock_access_token.return_value = ('device_id', 'type', 'access_token')
    # Mock connection to the log server.
    mock_urlopen.return_value = Mock()
    log_pusher = LogPusher('TestServer')

    # Verify PushLog good flow.
    self.assertTrue(log_pusher.PushLog({'data': 'Example'}, 'structuredLogs'))
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()

  @mock.patch('monlog_pusher.urllib2.urlopen')
  @mock.patch('monlog_pusher.LogPusher.GetAccessToken')
  def testPushLogException(self, mock_access_token, mock_urlopen):
    """Test different bad push requests which raises exception."""
    # Return mock access token used to authorize log server.
    mock_access_token.return_value = ('device_id', 'type', 'access_token')

    # 1. Bad data i.e. not a mapping object or valid non-string sequence.
    log_pusher = LogPusher('TestServer')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      'data', 'structuredLogs')
    mock_access_token.assert_called_once()

    # 2. Mock bad request raises HTTPError.
    mock_urlopen.side_effect = Mock(spec=urllib2.HTTPError)
    log_pusher = LogPusher('TestServer')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'structuredLogs')
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()

    # 3. Mock bad request raises URLError.
    mock_urlopen.side_effect = Mock(spec=urllib2.URLError)
    log_pusher = LogPusher('TestServer')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'structuredLogs')
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()

    # 4. Mock bad request raises HTTPException.
    mock_urlopen.side_effect = Mock(spec=httplib.HTTPException)
    log_pusher = LogPusher('TestServer')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'structuredLogs')
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()

    # 5. Mock bad request raises general Exception.
    mock_urlopen.side_effect = Mock(spec=Exception)
    log_pusher = LogPusher('TestServer')
    self.assertRaises(monlog_pusher.ExeException, log_pusher.PushLog,
                      {'data': 'Example'}, 'structuredLogs')
    mock_access_token.assert_called_once()
    mock_urlopen.assert_called_once()


if __name__ == '__main__':
  unittest.main()
