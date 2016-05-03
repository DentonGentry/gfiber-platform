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

"""Tests for prestera_periodic."""

__author__ = 'poist@google.com (Gregory Poist)'

import errno
import os
import subprocess
import tempfile
import unittest
import prestera_periodic

STATS_JSON = """
{
  "port-interface-statistics": {
    "0/0": {
      "broadcast_packets_received": 8739,
      "broadcast_packets_sent": 3,
      "bytes_received": 32061162,
      "bytes_sent": 10145704,
      "multicast_packets_received": 35484,
      "multicast_packets_sent": 20471,
      "unicast_packets_received": 22875,
      "unicast_packets_sent": 20737
    }
  }
}
"""


class FakePresteraPeriodic(prestera_periodic.PresteraPeriodic):
  """Mock PresteraPeriodic."""

  def WriteToStderr(self, msg):
    self.error_count += 1

  def RunPresteraStats(self):
    self.get_stats_called = True
    if self.raise_os_error:
      raise OSError(errno.ENOENT, 'raise an exception')
    if self.raise_subprocess:
      raise subprocess.CalledProcessError(cmd='durp', returncode=1)
    return self.stats_response


class PresteraPeriodicTest(unittest.TestCase):

  def CreateTempFile(self):
    # Create a temp file and have that be the target output file.
    fd, self.output_file = tempfile.mkstemp()
    os.close(fd)

  def DeleteTempFile(self):
    if os.path.exists(self.output_file):
      os.unlink(self.output_file)

  def setUp(self):
    self.CreateTempFile()
    self.periodic = FakePresteraPeriodic(1000)
    self.periodic.raise_os_error = False
    self.periodic.raise_subprocess = False
    self.periodic.stats_response = STATS_JSON
    self.periodic.ports_output_file = self.output_file
    self.periodic.error_count = 0

  def tearDown(self):
    self.DeleteTempFile()

  def testAcquireStats(self):
    self.periodic.AcquireStats()

    self.assertEquals(True, self.periodic.get_stats_called)
    with open(self.output_file, 'r') as f:
      output = ''.join(line for line in f)
      self.assertEqual(self.periodic.stats_response, output)

  def testAcquireStatsFailureToCreateOutputDir(self):
    self.periodic.ports_output_file = '/root/nope/cant/write/this'

    self.periodic.AcquireStats()
    self.assertTrue(self.periodic.error_count > 0)

  def testSubsequentEmptyDataNoOverwrite(self):
    self.periodic.AcquireStats()

    self.periodic.stats_response = ''
    self.periodic.AcquireStats()

    with open(self.output_file, 'r') as f:
      output = ''.join(line for line in f)
      self.assertEqual(STATS_JSON, output)

  def testSubsequentExecError(self):
    self.periodic.AcquireStats()

    self.periodic.raise_os_error = True
    self.periodic.AcquireStats()

    self.assertTrue(self.periodic.error_count > 0)
    with open(self.output_file, 'r') as f:
      output = ''.join(line for line in f)
      self.assertEqual(STATS_JSON, output)

  def testExecError(self):
    self.periodic.raise_subprocess = True
    self.periodic.AcquireStats()

    self.assertTrue(self.periodic.error_count > 0)
    with open(self.output_file, 'r') as f:
      output = ''.join(line for line in f)
      self.assertEqual('', output)


if __name__ == '__main__':
  unittest.main()
