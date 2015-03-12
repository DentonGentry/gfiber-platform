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

CONTENT_TYPE = 'Content-Type'
APP_JSON = 'application/json'
APP_FORM_URL_ENCODED = 'application/x-www-form-urlencoded'
AUTHORIZATION = 'Authorization'
DEVICE_REG_INFO = '/chroot/chromeos/var/lib/buffet/device_reg_info'
LOG_DIR = '/tmp/applogs/'
LOG_TYPE_STRUCTURED = 'structuredLogs'
LOG_SERVER_PATH = 'https://www.googleapis.com/devicestats/v1/devices/'
POLL_SEC = 300


class Error(Exception):
  """Base class for all exceptions in this module."""


class ExeException(Error):
  """Empty Exception Class just to raise an Error on bad execution."""


class LogCollector(object):
  """LogCollector class.

  LogCollector is responsible for collecting the logs from the well-known place
  and build its internal log collection for future retrieval.
  """

  def __init__(self, log_dir):
    self._log_dir = log_dir
    self._logs = []
    self._CollectLogs()

  def _CollectLogs(self):
    """Collects the logs and constructs the log collection.

    Raises:
      ExeException: if there is any error.
    """
    # Filter the files and loop through each file in timestamp order.
    try:
      log_dir_contents = os.listdir(self._log_dir)
    except OSError as e:
      raise ExeException('Failed to access directory %s. Error: %r'
                         % (self._log_dir, e.errno))
    else:
      log_files = [os.path.join(self._log_dir, f) for f in log_dir_contents
                   if os.path.isfile(os.path.join(self._log_dir, f))]
      log_files.sort(key=os.path.getmtime)
    for log_file in log_files:
      try:
        with open(log_file) as f:
          self._logs.append(f.read())
      except IOError as e:
        raise ExeException('Failed to open file %s. Error: %r'
                           % (log_file, e.errno))
      finally:
        # Remove the log to clean up log_dir.
        os.remove(log_file)

  def IsEmpty(self):
    """Checks if there is any pending log to push."""
    return not self._logs

  def GetNextLog(self):
    """Returns the next log data and type in the collection."""
    # Pop will return and erase the first log data in the collection.
    # TODO(hunguyen): Only support structuredLogs in the first phase.
    if not self._logs:
      return None, None
    return self._logs.pop(0), LOG_TYPE_STRUCTURED


class LogPusher(object):
  """LogPusher class.

  LogPusher is responsible for establishing a secure connection and sending the
  logs to the cloud server.
  """

  def __init__(self, log_server_path, dev_reg_info=DEVICE_REG_INFO):
    self._log_server_path = log_server_path
    self._dev_reg_info = dev_reg_info

  def PushLog(self, log_data, log_type):
    """Sends log data to the log server endpoint.

    Args:
      log_data: the log content to be sent.
      log_type: 'structured', 'unstructured', or 'metrics'.
    Returns:
      true if log was sent successfully.
    Raises:
      ExeException: if there is any URLError.
    """
    # MonLog server authorization requires access_token and device_id.
    device_id, token_type, access_token = self.GetAccessToken()

    # Prepare the connection to the log server.
    try:
      # Log data should be in json format.
      data = urllib.urlencode(log_data)
    except TypeError as e:
      raise ExeException('Failed to encode data %r. Error: %r'
                         % (log_data, e))

    req = urllib2.Request(self._log_server_path + device_id + '/' + log_type, data)
    req.add_header(CONTENT_TYPE, APP_JSON)
    req.add_header(AUTHORIZATION, token_type + ' ' + access_token)

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

  def GetAccessToken(self):
    """Gets authorization info i.e. device_id, access_token, and token_type.

    Returns:
      Tuple (device_id, token_type, access_token)
    """

    # Since MonLog shares registration info with GCD server, we will get the
    # access_token and token_type via GCD registration info, which is stored
    # in dev_reg_info file.
    with open(self._dev_reg_info) as f:
      # device_reg_info should be in json format
      try:
        dev_reg_info_json = json.load(f)
      except ValueError as e:
        raise ExeException('Failed to load json in file %s. %r'
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
      raise ExeException('Failed to encode data %r. %r'
                         % (oauth_data, e))

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
  return log_dir, log_server, poll_interval


def main():
  log_dir, log_server_path, poll_interval = GetArgs()
  log_collector = LogCollector(log_dir)
  log_pusher = LogPusher(log_server_path)

  while True:
    time.sleep(poll_interval)
    # Loop through the log collection and send out logs.
    while not log_collector.IsEmpty():
      log_data, log_type = log_collector.GetNextLog()
      log_pusher.PushLog(log_data, log_type)


if __name__ == '__main__':
  main()
