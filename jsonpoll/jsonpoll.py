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
port=            port to connect to [8000]
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

    # TODO(cgibson): Support more request types once Glaukus Manager's JSON spec
    # is more stable.
    self.api_modem_output_file = os.path.join(self.OUTPUT_DIR, 'modem.json')
    self.paths_to_statfiles = {'api/modem': self.api_modem_output_file}
    self.last_response = None

  def RequestStats(self):
    """Sends a request via HTTP GET to the specified web server."""
    for path, output_file in self.paths_to_statfiles.iteritems():
      url = '%s/%s' % (self.hostport, path)
      tmpfile = ''
      try:
        response = self.GetHttpResponse(url)
        if not response:
          return False
        elif self.last_response == response:
          print 'Skipping file write as content has not changed.'
          return True
        self.last_response = response
        with tempfile.NamedTemporaryFile(delete=False) as fd:
          if not self.CreateDirs(os.path.dirname(output_file)):
            print ('Failed to create output directory: %s' %
                   os.path.dirname(output_file))
            return False
          tmpfile = fd.name
          fd.write(response)
          fd.flush()
          os.fsync(fd.fileno())
          print 'Wrote %d bytes to %s' % (len(response), tmpfile)
          try:
            os.rename(tmpfile, output_file)
          except OSError as ex:
            print 'Failed to move %s to %s: %s' % (tmpfile, output_file, ex)
            return False
          print 'Moved %s to %s' % (tmpfile, output_file)
        return True
      finally:
        if os.path.exists(tmpfile):
          os.unlink(tmpfile)

  def GetHttpResponse(self, url):
    """Creates a request and retrieves the response from a web server."""
    print 'Connecting to %s' % url
    try:
      handle = urllib2.urlopen(url, timeout=self._SOCKET_TIMEOUT_SECS)
      response = handle.read()
    except socket.timeout as ex:
      print ('Connection to %s timed out after %d seconds: %s'
             % (url, self._SOCKET_TIMEOUT_SECS, ex))
      return None
    except urllib2.URLError as ex:
      print 'Connection to %s failed: %s' % (url, ex.reason)
      return None
    return response

  def CreateDirs(self, dir_to_create):
    """Recursively creates directories."""
    try:
      os.makedirs(dir_to_create)
    except os.error as ex:
      if ex.errno == errno.EEXIST:
        return True
      print 'Failed to create directory: %s' % ex
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
