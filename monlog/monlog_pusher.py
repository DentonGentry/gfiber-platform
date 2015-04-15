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
import urllib2
from urllib2 import HTTPError
from urllib2 import URLError

APP_JSON = 'application/json'
APP_FORM_URL_ENCODED = 'application/x-www-form-urlencoded'
AUTHORIZATION = 'Authorization'
CONTENT_TYPE = 'Content-Type'
COMPLETE_SPACECAST_LOG_PATTERN = 'spacecast_log'
LOG_DIR = '/tmp/applogs/'
METRIC_BATCH_CREATE_POINTS = 'metrics:batchCreatePoints'
MONLOG_TYPE_METRICS = 'metrics'
MONLOG_SPACECAST_SERVER_PATH = ('https://www.googleapis.com/devicestats/'
                                'v1alpha/types/SPACECAST_CACHE/devices/')
MONLOG_REG_INFO = '/tmp/monlog_reg_info'
POLL_SEC = 300
SLASH = '/'


class Error(Exception):
  """Base class for all exceptions in this module."""


class ExeException(Error):
  """Empty Exception Class just to raise an Error on bad execution."""

  def __init__(self, errormsg):
    super(ExeException, self).__init__(errormsg)
    self.errormsg = errormsg


def RemoveLogFile(log_file):
  try:
    os.remove(log_file)
  except IOError as e:
    raise ExeException('Failed to remove file %s. Error: %r'
                       % (log_file, e.errno))


class LogCollector(object):
  """LogCollector class.

  LogCollector is responsible for collecting the logs from the well-known place
  and build its internal log collection for future retrieval.
  """

  def __init__(self, log_dir):
    self._log_dir = log_dir
    # Log collection is the list of (log_file, log_data) tuples sorted by
    # timestamp.
    self._log_collection = []

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
      log_files.sort(key=os.path.getmtime)

    # Collect good log files and keep a list of bad files to raise exception at
    # the end.
    # Bad files are kept in a list of tuple of the bad file name and its error.
    bad_files = []
    for log_file in log_files:
      try:
        with open(log_file) as f:
          # Log file should be in json format. Raises exception if it is not.
          self._log_collection.append((log_file, json.load(f)))
      except IOError as e:
        bad_files.append((log_file, e.strerror))
      except ValueError as e:
        bad_files.append((log_file, e))

    # Loop through the bad_files list to remove the bad file and create an error
    # message to raise exception.
    error_msg = ''
    for bad_file in bad_files:
      RemoveLogFile(bad_file[0])
      error_msg += ('Bad file: ' + bad_file[0] +
                    '. Error: ' + str(bad_file[1]) + '\n')

    # Raise ExeException if there is any bad file.
    if bad_files:
      raise ExeException(error_msg)

  def IsEmpty(self):
    """Checks if there is any pending log to push."""
    return not self._log_collection

  def GetAvailableLog(self):
    """Gets the next log_file, log_data, and log_type in the collection."""
    # TODO(hunguyen): Only support log_type=metricPoints in the first phase.
    if not self._log_collection:
      return None, None, None

    # Pop will return and erase the first (log_file, log_data) tuple in the
    # collection.
    log_file, log_data = self._log_collection.pop(0)
    return log_file, log_data, MONLOG_TYPE_METRICS


class MonlogPusher(object):
  """MonlogPusher class.

  MonlogPusher is responsible for establishing a secure connection and sending
  the logs to the cloud server.
  """

  def __init__(self, monlog_server_path, monlog_reg_info=MONLOG_REG_INFO):
    self._monlog_server_path = monlog_server_path
    self._monlog_reg_info = monlog_reg_info
    self._device_id, self._token_type, self._access_token = (
        self._GetAccessToken())

  def PushLog(self, log_data, log_type):
    """Sends log data to the monlog server endpoint and remove the log file.

    Args:
      log_data: the log content to be sent.
      log_type: 'structured', 'unstructured', or 'metrics'.
    Returns:
      True if log was sent successfully.
    Raises:
      ExeException: if there is any error.
    """

    # TODO(hunguyen): Only support log_type=metrics in phase 1.
    if log_type != MONLOG_TYPE_METRICS:
      raise ExeException('Unsupported log_type=%s' % log_type)

    # Prepare the connection to the monlog server.
    try:
      # Add deviceId to the 'id' field of the log data.
      # Example:
      #   log_data = {"id":{"type":"spacecast","deviceId":""},"metricPoints"...}
      log_data['id']['deviceId'] = self._device_id
      # TODO(hunguyen): Make sure there is no space in log_data added to URL.
      data = json.dumps(log_data, separators=(',', ':'))
    except TypeError as e:
      raise ExeException('Failed to encode data %r. Error: %r'
                         % (log_data, e))

    # Construct complete MonLog URL e.g.
    #   https://www-googleapis-staging.sandbox.google.com/devicestats/v1alpha/
    # types/SPACECAST_CACHE/devices/abc123/metrics:batchCreatePoints
    req = urllib2.Request(self._monlog_server_path + self._device_id + SLASH +
                          METRIC_BATCH_CREATE_POINTS, data)
    req.add_header(CONTENT_TYPE, APP_JSON)
    req.add_header(AUTHORIZATION, self._token_type + ' ' + self._access_token)

    try:
      urllib2.urlopen(req)
    except HTTPError as e:
      raise ExeException('HTTPError = %r' % e.read())
    except URLError as e:
      raise ExeException('URLError = %r' % str(e.reason))
    except httplib.HTTPException as e:
      raise ExeException('HTTPException')
    except Exception:
      raise ExeException('Generic exception: %r' % traceback.format_exc())

    return True

  def _GetAccessToken(self):
    """Gets authorization info i.e. device_id, token_type, and access_token.

    Returns:
      Tuple (device_id, token_type, access_token)
    Raises:
      ExeException: if there is any error.
    """
    # SpaceCast periodically refreshes access_token for MonLog pusher and store
    # in monlog_reg_info. Read that file to retrieve the access_token.
    try:
      with open(self._monlog_reg_info) as f:
        # monlog_reg_info should be in json format
        monlog_reg_info_json = json.load(f)
    except IOError as e:
      raise ExeException('Failed to open file %s. Error: %r'
                         % (self._monlog_reg_info, e))
    except ValueError as e:
      raise ExeException('Failed to load json in file %s. Error: %r'
                         % (self._monlog_reg_info, e))

    # Raise exception if missing registration info.
    if not all(k in monlog_reg_info_json.keys()
               for k in ('device_id', 'token_type', 'access_token')):
      raise ExeException('Missing monlog registration info in file %s'
                         % self._monlog_reg_info)

    return (monlog_reg_info_json['device_id'],
            monlog_reg_info_json['token_type'],
            monlog_reg_info_json['access_token'])


def GetArgs():
  """Parses and returns arguments passed in."""

  parser = argparse.ArgumentParser(prog='logpush')
  parser.add_argument('--log_dir', nargs='?', help='Location to collect logs',
                      default=LOG_DIR)
  parser.add_argument('--monlog_server_path', nargs='?',
                      help='URL path to the log server.',
                      default=MONLOG_SPACECAST_SERVER_PATH)
  parser.add_argument('--poll_interval', nargs='?',
                      help='Polling interval in seconds.', default=POLL_SEC)

  args = parser.parse_args()
  log_dir = args.log_dir
  monlog_server_path = args.monlog_server_path
  poll_interval = float(args.poll_interval)
  return log_dir, monlog_server_path, poll_interval


def main():
  log_dir, monlog_server_path, poll_interval = GetArgs()

  while True:
    time.sleep(poll_interval)
    try:
      log_collector = LogCollector(log_dir)
      log_collector.CollectLogs()
    except ExeException as e:
      print 'Error on collecting logs: ', e.errormsg

    try:
      monlog_pusher = MonlogPusher(monlog_server_path)
    except ExeException as e:
      print 'Failed to get access token for monlog pusher: ', e.errormsg
    else:
      # Loop through the log collection and send out logs.
      while not log_collector.IsEmpty():
        log_file, log_data, log_type = log_collector.GetAvailableLog()
        try:
          monlog_pusher.PushLog(log_data, log_type)
        except ExeException as e:
          print 'Failed to push log file ', log_file, '. Error: ', e.errormsg
        else:
          print 'Successfully pushed log file ', log_file, ' to MonLog server.'
        finally:
          RemoveLogFile(log_file)


if __name__ == '__main__':
  main()
