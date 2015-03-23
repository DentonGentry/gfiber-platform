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

"""monlog_pusher is a daemon responsible for pushing device logs to the cloud.

Applications run in the device drop their logs to a well-known directory where
monlog_pusher daemon can collect and push them to the monlog server. This daemon
is also responsible for establishing a secure connection to the server and clean
up the logs after use.

Usage:
  monlog_pusher [--log_dir <log_dir>] [--log_server_path <server_url>]
                [--poll_interval <seconds>]
"""

__author__ = 'hunguyen@google.com (Huy Nguyen)'

import argparse
import httplib
import json
import os
import time
import traceback
import urllib
import urllib2
from urllib2 import HTTPError
from urllib2 import URLError

APP_JSON = 'application/json'
APP_FORM_URL_ENCODED = 'application/x-www-form-urlencoded'
AUTHORIZATION = 'Authorization'
CONTENT_TYPE = 'Content-Type'
COMPLETE_SPACECAST_LOG_PATTERN = 'spacecast_log'
DEVICE_REG_INFO = '/chroot/chromeos/var/lib/buffet/device_reg_info'
LOG_DIR = '/tmp/applogs/'
LOG_TYPE_METRICS = 'metrics'
LOG_SERVER_PATH = 'https://www.googleapis.com/devicestats/v1/devices/'
METRIC_BATCH_CREATE_POINTS = 'metrics:batchCreatePoints'
POLL_SEC = 300


class Error(Exception):
  """Base class for all exceptions in this module."""


class ExeException(Error):
  """Empty Exception Class just to raise an Error on bad execution."""

  def __init__(self, errormsg):
    super(ExeException, self).__init__(errormsg)
    self.errormsg = errormsg


class LogCollector(object):
  """LogCollector class.

  LogCollector is responsible for collecting the logs from the well-known place
  and build its internal log collection for future retrieval.
  """

  def __init__(self, log_dir):
    self._log_dir = log_dir
    # Log collection is the map from log_file to log_data.
    self._log_collection = {}

  def CollectLogs(self):
    """Collects the logs and constructs the log collection.

    Raises:
      ExeException: if there is any error.
    """
    # The complete log files must have the format: <application>_log.<timestamp>
    # Filter the complete logs and loop through each file in timestamp order.
    try:
      log_dir_contents = os.listdir(self._log_dir)
    except OSError as e:
      raise ExeException('Failed to access directory %s. Error: %r'
                         % (self._log_dir, e.errno))
    else:
      log_files = [os.path.join(self._log_dir, f) for f in log_dir_contents
                   if os.path.isfile(os.path.join(self._log_dir, f)) and
                   f.startswith(COMPLETE_SPACECAST_LOG_PATTERN)]
    for log_file in log_files:
      try:
        with open(log_file) as f:
          # Log file should be in json format. Raises exception if it is not.
          self._log_collection[log_file] = json.load(f)
      except IOError as e:
        # Remove the bad log file.
        self.RemoveLogFile(log_file)
        raise ExeException('Failed to open file %s. Error: %r'
                           % (log_file, e.errno))
      except ValueError as e:
        # Remove the bad log file.
        self.RemoveLogFile(log_file)
        raise ExeException('Failed to load json in file %s. Error: %r'
                           % (log_file, e))

  def IsEmpty(self):
    """Checks if there is any pending log to push."""
    return not self._log_collection

  def GetAvailableLog(self):
    """Gets an available log_file, log_data and log_type in the collection."""
    # Pop will return and erase one log data in the collection.
    # TODO(hunguyen): Only support log_type=metricPoints in the first phase.
    if not self._log_collection:
      return None, None, None

    log_file, log_data = self._log_collection.popitem()
    return log_file, log_data, LOG_TYPE_METRICS

  def RemoveLogFile(self, log_file):
    try:
      os.remove(log_file)
    except IOError as e:
      raise ExeException('Failed to remove file %s. Error: %r'
                         % (log_file, e.errno))


class LogPusher(object):
  """LogPusher class.

  LogPusher is responsible for establishing a secure connection and sending the
  logs to the cloud server.
  """

  def __init__(self, log_server_path, dev_reg_info=DEVICE_REG_INFO):
    self._log_server_path = log_server_path
    self._dev_reg_info = dev_reg_info
    self._device_id, self._token_type, self._access_token = (
        self._GetAccessToken())

  def PushLog(self, log_data, log_type):
    """Sends log data to the log server endpoint.

    Args:
      log_data: the log content to be sent.
      log_type: 'structured', 'unstructured', or 'metrics'.
    Returns:
      True if log was sent successfully.
    Raises:
      ExeException: if there is any error.
    """

    # TODO(hunguyen): Only support log_type=metrics in phase 1.
    if log_type != LOG_TYPE_METRICS:
      raise ExeException('Unsupported log_type=%s' % log_type)

    # Prepare the connection to the log server.
    try:
      # Log data should be in json format. Add deviceid field to the log data.
      log_data['deviceId'] = self._device_id
      # TODO(hunguyen): Make sure there is no space in log_data added to URL.
      data = json.dumps(log_data, separators=(',', ':'))
    except TypeError as e:
      raise ExeException('Failed to encode data %r. Error: %r'
                         % (log_data, e))

    req = urllib2.Request(self._log_server_path + self._device_id + '/' +
                          METRIC_BATCH_CREATE_POINTS, data)
    req.add_header(CONTENT_TYPE, APP_JSON)
    req.add_header(AUTHORIZATION, self._token_type + ' ' + self._access_token)

    try:
      urllib2.urlopen(req)
    except HTTPError as e:
      raise ExeException('HTTPError = %r' % str(e.code))
    except URLError as e:
      raise ExeException('URLError = %r' % str(e.reason))
    except httplib.HTTPException as e:
      raise ExeException('HTTPException')
    except Exception:
      raise ExeException('Generic exception: %r' % traceback.format_exc())

    return True

  def _GetAccessToken(self):
    """Gets authorization info i.e. device_id, access_token, and token_type.

    Returns:
      Tuple (device_id, token_type, access_token)
    Raises:
      ExeException: if there is any error.
    """

    # Since MonLog shares registration info with GCD server, we will get the
    # access_token and token_type via GCD registration info, which is stored
    # in dev_reg_info file.
    try:
      with open(self._dev_reg_info) as f:
        # device_reg_info should be in json format
        dev_reg_info_json = json.load(f)
    except IOError as e:
      raise ExeException('Failed to open file %s. Error: %r'
                         % (self._dev_reg_info, e))
    except ValueError as e:
      raise ExeException('Failed to load json in file %s. Error: %r'
                         % (self._dev_reg_info, e))

    # Get URL to the OAuth2 server.
    oauth_url = dev_reg_info_json['oauth_url'] + 'token'

    # Prepare the data fields to send to the OAuth2 server.
    # Construct the data fields map by compressing device_reg_info to include
    # only necessary fields.
    oauth_data_dict = {
        key: dev_reg_info_json[key] for key in [
            'client_secret', 'client_id', 'refresh_token'
        ]
    }
    oauth_data_dict['grant_type'] = 'refresh_token'

    try:
      oauth_data = urllib.urlencode(oauth_data_dict)
    except TypeError as e:
      raise ExeException('Failed to encode data %r. %r' % (oauth_data, e))

    req = urllib2.Request(oauth_url, oauth_data)
    req.add_header(CONTENT_TYPE, APP_FORM_URL_ENCODED)
    try:
      response = urllib2.urlopen(req)
    except HTTPError as e:
      raise ExeException('HTTPError = %r' % str(e.code))
    except URLError as e:
      raise ExeException('URLError = %r' % str(e.reason))
    except httplib.HTTPException as e:
      raise ExeException('HTTPException')
    except Exception:
      raise ExeException('Generic exception: %r' % traceback.format_exc())

    else:
      json_resp = response.read()

    try:
      # response should be in json format.
      resp_dict = json.loads(json_resp)
    except Error as e:
      raise ExeException('Failed to load json from response %r. Error: %r'
                         % (json_resp, e))

    return (dev_reg_info_json['device_id'], resp_dict['token_type'],
            resp_dict['access_token'])


def GetArgs():
  """Parses and returns arguments passed in."""

  parser = argparse.ArgumentParser(prog='logpush')
  parser.add_argument('--log_dir', nargs='?', help='Location to collect logs',
                      default=LOG_DIR)
  parser.add_argument('--log_server_path', nargs='?',
                      help='URL path to the log server.',
                      default=LOG_SERVER_PATH)
  parser.add_argument('--poll_interval', nargs='?',
                      help='Polling interval in seconds.', default=POLL_SEC)
  args = parser.parse_args()
  log_dir = args.log_dir
  log_server_path = args.log_server_path
  poll_interval = args.poll_interval
  return log_dir, log_server_path, poll_interval


def main():
  log_dir, log_server_path, poll_interval = GetArgs()

  while True:
    try:
      time.sleep(poll_interval)
      log_collector = LogCollector(log_dir)
      log_pusher = LogPusher(log_server_path)
      log_collector.CollectLogs()
      # Loop through the log collection and send out logs.
      while not log_collector.IsEmpty():
        log_file, log_data, log_type = log_collector.GetAvailableLog()
        print 'Pushing log_file=', log_file, ' to MonLog server.'
        log_pusher.PushLog(log_data, log_type)
        log_collector.RemoveLogFile(log_file)
    except ExeException as e:
      print 'Exception caught: ', e.errormsg


if __name__ == '__main__':
  main()
