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

"""JsonPoll reads a response from a socket and writes it to disk."""

__author__ = 'cgibson@google.com (Chris Gibson)'

import errno
import json
import os
import socket
import sys
import tempfile
import time
import urllib2
import options


optspec = """
jsonpoll [options]
--
host=            host to connect to [localhost]
port=            port to connect to [8080]
i,interval=      poll interval in seconds [15]
"""


class JsonPoll(object):
  """Periodically poll a web server to request stats."""

  # The directory that JSON files will be written to. Note that changing this
  # path to another filesystem other than /tmp will affect the atomicity of the
  # os.rename() when moving files into the final destination.
  OUTPUT_DIR = '/tmp/glaukus/'

  # The time to wait before giving up on blocking connection operations.
  _SOCKET_TIMEOUT_SECS = 15

  def __init__(self, host, port, interval):
    self.hostport = 'http://%s:%d' % (host, port)

    # The time to wait between requests in seconds.
    self.poll_interval_secs = interval

    self.api_modem_output_file = os.path.join(self.OUTPUT_DIR, 'modem.json')
    self.api_radio_output_file = os.path.join(self.OUTPUT_DIR, 'radio.json')
    self.paths_to_statfiles = {'api/modem': self.api_modem_output_file,
                               'api/radio': self.api_radio_output_file}
    self.last_response = None

  def _FlatObject(self, base, obj, out):
    """Open json object to a list of strings.

    Args:
      base: is a string that has a base name for a key.
      obj: is a JSON object that will be flatten.
      out: is an output where strings will be appended.

    Example:
         data = {"a": 1, "b": { "c": 2, "d": 3}}
         out = []
         _FlatObject('', data, out)

         out will be equal to
           [u'/a=1', u'/b/c=2', u'/b/d=3']
    """
    for k, v in obj.items():
      name = base + '/' + k
      if isinstance(v, dict):
        self._FlatObject(name, v, out)
      else:
        val = '%s=' % name
        out.append(val + str(v))

  def WriteToStderr(self, msg, is_json=False):
    """Write a message to stderr."""
    if is_json:
      flat_data = []
      self._FlatObject('', msg, flat_data)
      # Make the json easier to parse from the logs.
      for s in flat_data:
        sys.stderr.write('%s\n' % s)
      sys.stderr.flush()
    else:
      sys.stderr.write(msg)
      sys.stderr.flush()

  def RequestStats(self):
    """Sends a request via HTTP GET to the specified web server."""
    for path, output_file in self.paths_to_statfiles.iteritems():
      url = '%s/%s' % (self.hostport, path)
      tmpfile = ''
      try:
        response = self.GetHttpResponse(url)
        if not response:
          self.WriteToStderr('Failed to get response from glaukus: %s\n' % url)
          continue
        elif self.last_response == response:
          self.WriteToStderr('Skip file write as content has not changed.\n')
          continue
        self.last_response = response
        with tempfile.NamedTemporaryFile(delete=False) as fd:
          if not self.CreateDirs(os.path.dirname(output_file)):
            self.WriteToStderr('Failed to create output directory: %s\n' %
                               os.path.dirname(output_file))
            continue
          tmpfile = fd.name
          fd.write(json.dumps(response))
          fd.flush()
          os.fsync(fd.fileno())
          try:
            os.rename(tmpfile, output_file)
          except OSError as ex:
            self.WriteToStderr('Failed to move %s to %s: %s\n' % (
                tmpfile, output_file, ex))
            continue
      finally:
        if os.path.exists(tmpfile):
          os.unlink(tmpfile)

  def ParseJSONFromResponse(self, response):
    try:
      json_resp = json.loads(response)
    except UnicodeDecodeError as ex:
      self.WriteToStderr('Non-UTF8 character in HTTP response: %s', ex)
      return None
    except ValueError as ex:
      self.WriteToStderr('Failed to parse JSON from HTTP response: %s', ex)
      return None
    return json_resp

  def GetHttpResponse(self, url):
    """Creates a request and retrieves the response from a web server."""
    try:
      handle = urllib2.urlopen(url, timeout=self._SOCKET_TIMEOUT_SECS)
      response = self.ParseJSONFromResponse(handle.read())
    except socket.timeout as ex:
      self.WriteToStderr('Connection to %s timed out after %d seconds: %s\n'
                         % (url, self._SOCKET_TIMEOUT_SECS, ex))
      return None
    except urllib2.URLError as ex:
      self.WriteToStderr('Connection to %s failed: %s\n' % (url, ex.reason))
      return None

    # Write the response to stderr so it will be uploaded with the other system
    # log files. This will allow turbogrinder to alert on the radio subsystem.
    if response is not None:
      self.WriteToStderr(response, is_json=True)
    return response

  def CreateDirs(self, dir_to_create):
    """Recursively creates directories."""
    try:
      os.makedirs(dir_to_create)
    except os.error as ex:
      if ex.errno == errno.EEXIST:
        return True
      self.WriteToStderr('Failed to create directory: %s\n' % ex)
      return False
    return True

  def RunForever(self):
    while True:
      self.RequestStats()
      time.sleep(self.poll_interval_secs)


def main():
  o = options.Options(optspec)
  (opt, unused_flags, unused_extra) = o.parse(sys.argv[1:])
  poller = JsonPoll(opt.host, opt.port, opt.interval)
  poller.RunForever()


if __name__ == '__main__':
  main()
