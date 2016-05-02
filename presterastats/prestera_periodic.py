#!/usr/bin/python
# Copyright 2016 Google Inc. All Rights Reserved.
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

"""Periodically call presterastats and save results to filesystem."""

__author__ = 'poist@google.com (Gregory Poist)'

import errno
import os
import subprocess
import sys
import tempfile
import time

import options


optspec = """
presterastats [options]
--
startup_delay=    wait this many seconds before first query [60]
interval=         interval to read statistics [15]
"""


class PresteraPeriodic(object):
  """Class wrapping a cpss command to request stats."""

  OUTPUT_DIR = '/tmp/prestera'

  def __init__(self, interval):
    self.interval = interval
    self.ports_output_file = os.path.join(self.OUTPUT_DIR, 'ports.json')

  def WriteToStderr(self, msg):
    """Write a message to stderr."""

    sys.stderr.write(msg)
    sys.stderr.flush()

  def RunPresteraStats(self):
    """Run presterastats, return command output."""
    return subprocess.check_output(['presterastats'])

  def AcquireStats(self):
    """Call the child process and get stats."""

    # Output goes to a temporary file, which is renamed to the destination
    tmpfile = ''
    ports_stats = ''
    try:
      ports_stats = self.RunPresteraStats()
    except OSError as ex:
      self.WriteToStderr('Failed to run presterastats: %s\n' % ex)
    except subprocess.CalledProcessError as ex:
      self.WriteToStderr('presterastats exited non-zero: %s\n' % ex)

    if not ports_stats:
      self.WriteToStderr('Failed to get data from presterastats\n')
      return

    try:
      with tempfile.NamedTemporaryFile(delete=False) as fd:
        if not self.CreateDirs(os.path.dirname(self.ports_output_file)):
          self.WriteToStderr('Failed to create output directory: %s\n' %
                             os.path.dirname(self.ports_output_file))
          return
        tmpfile = fd.name
        fd.write(ports_stats)
        fd.flush()
        os.fsync(fd.fileno())
        try:
          os.rename(tmpfile, self.ports_output_file)
        except OSError as ex:
          self.WriteToStderr('Failed to move %s to %s: %s\n' % (
              tmpfile, self.ports_output_file, ex))
          return
    finally:
      if tmpfile and os.path.exists(tmpfile):
        os.unlink(tmpfile)

  def CreateDirs(self, dir_to_create):
    """Recursively creates directories."""
    try:
      os.makedirs(dir_to_create)
    except os.error as ex:
      if ex.errno == errno.EEXIST:
        return True
      self.WriteToStderr('Failed to create directory: %s' % ex)
      return False
    return True

  def RunForever(self):
    while True:
      self.AcquireStats()
      time.sleep(self.interval)


def main():
  o = options.Options(optspec)
  (opt, unused_flags, unused_extra) = o.parse(sys.argv[1:])
  if opt.startup_delay:
    time.sleep(opt.startup_delay)
  prestera = PresteraPeriodic(opt.interval)
  prestera.RunForever()


if __name__ == '__main__':
  main()
