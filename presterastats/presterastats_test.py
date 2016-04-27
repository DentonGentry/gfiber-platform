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

"""Tests for presterastats."""

__author__ = 'poist@google.com (Gregory Poist)'

import json
import os
import subprocess
import time
import unittest

import presterastats

VALID_JSON_RESPONSE = """
garbage here
JSONSTART
{ "valid": {
  "0/0": {
    "unicast_packets_sent": 19
  }
}}
JSONEND
garbage there
"""

VALID_JSON_CONTENT = """
{ "valid": {
  "0/0": {
    "unicast_packets_sent": 19
  }
}}
"""

NO_START_BLOCK_RESPONSE = """
blah
blah
blah
no json here
"""


class FakePresteraStats(presterastats.PresteraStats):
  """Mock PresteraStats."""

  def StartCpssSubprocess(self):
    return subprocess.Popen(self.command.split(),
                            stdin=self.cpss_in, stdout=self.cpss_out,
                            preexec_fn=os.setsid)


class PresteraStatsTest(unittest.TestCase):

  def setUp(self):
    self.poller = FakePresteraStats('0/0,0/4', 1)
    self.poller.command = ''
    self.poller.cpss_in = subprocess.PIPE
    self.poller.cpss_out = subprocess.PIPE

  def testRequestStatsTimeout(self):
    start_time = time.time()
    self.poller.command = '/bin/sleep 30'
    self.poller.subproc_response_fd = subprocess.PIPE
    result = self.poller.GetMibStats()
    end_time = time.time()

    self.assertIsNone(result)
    self.assertTrue(end_time - start_time < 30)

  def testValidJsonBlock(self):
    self.poller.command = '/bin/cat'
    self.poller.cpss_in, out_fd = os.pipe()
    os.write(out_fd, VALID_JSON_RESPONSE)
    os.close(out_fd)
    result = self.poller.GetMibStats()
    os.close(self.poller.cpss_in)

    self.assertEquals(result, json.loads(VALID_JSON_CONTENT))

  def testNoJsonBlock(self):
    self.poller.command = '/bin/cat'
    self.poller.cpss_in, out_fd = os.pipe()
    os.write(out_fd, NO_START_BLOCK_RESPONSE)
    os.close(out_fd)
    result = self.poller.GetMibStats()
    os.close(self.poller.cpss_in)

    self.assertIsNone(result)

  def testBogusCommand(self):
    self.poller.command = 'aintgottimeforthat'
    try:
      _ = self.poller.GetMibStats()
      self.fail('Should explode')
    except OSError:
      pass


if __name__ == '__main__':
  unittest.main()
