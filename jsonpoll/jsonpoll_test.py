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

"""Tests for jsonpoll."""

__author__ = 'cgibson@google.com (Chris Gibson)'

import json
import os
import tempfile
import unittest
import jsonpoll

JSON_RESPONSE = {
    'firmware': '/foo/bar/modem.fw',
    'network': {
        'rxCounters': {
            'broadcast': 0,
            'bytes': 0,
            'crcErrors': 0,
            'frames': 0,
            'frames1024_1518': 0,
            'frames128_255': 0,
            'frames256_511': 0,
            'frames512_1023': 0,
            'frames64': 0,
            'frames65_127': 0,
            'framesJumbo': 0,
            'framesUndersized': 0,
            'multicast': 0,
            'unicast': 0
        },
    }
}


class FakeJsonPoll(jsonpoll.JsonPoll):
  """Mock JsonPoll."""

  def WriteToStderr(self, unused_msg, unused_is_json=False):
    self.error_count += 1

  def GetHttpResponse(self, unused_url):
    self.get_response_called = True
    if self.generate_empty_response:
      return None
    return self.ParseJSONFromResponse(self.json_response)


class JsonPollTest(unittest.TestCase):

  def CreateTempFile(self):
    # Create a temp file and have that be the target output file.
    fd, self.output_file = tempfile.mkstemp()
    os.close(fd)

  def DeleteTempFile(self):
    if os.path.exists(self.output_file):
      os.unlink(self.output_file)

  def setUp(self):
    self.CreateTempFile()
    self.poller = FakeJsonPoll('fakehost.blah', 31337, 1)
    self.poller.json_response = json.dumps(JSON_RESPONSE)
    self.poller.error_count = 0
    self.poller.generate_empty_response = False

  def tearDown(self):
    self.DeleteTempFile()

  def testRequestStats(self):
    # Create a fake entry in the paths_to_stats map.
    self.poller.paths_to_statfiles = {'fake/url': self.output_file}
    self.poller.RequestStats()
    self.assertEqual(True, self.poller.get_response_called)
    self.assertEqual(0, self.poller.error_count)

    # Read back the contents of the fake output file. It should be the
    # equivalent JSON representation we wrote out from the mock.
    with open(self.output_file, 'r') as f:
      output = ''.join(line.rstrip() for line in f)
      self.assertEqual(JSON_RESPONSE, json.loads(output))

  def testRequestStatsFailureToCreateDirOutput(self):
    self.poller.paths_to_statfiles = {'fake/url': '/root/cannotwrite'}
    self.poller.RequestStats()
    self.assertTrue(self.poller.error_count > 0)

  def testRequestStatsFailedToGetResponse(self):
    self.poller.paths_to_statfiles = {'fake/url': self.output_file}
    self.poller.generate_empty_response = True
    self.poller.RequestStats()
    self.assertEqual(True, self.poller.get_response_called)
    self.assertTrue(self.poller.error_count > 0)

  def testCachedRequestStats(self):
    # Set the "last_response" as our mock output. This should mean we do not
    # write anything to the output file.
    self.poller.last_response = JSON_RESPONSE

    # Create a fake entry in the paths_to_stats map.
    self.poller.paths_to_statfiles = {'fake/url': self.output_file}
    self.poller.RequestStats()
    self.assertEqual(True, self.poller.get_response_called)
    self.assertTrue(self.poller.error_count > 0)

    # Read back the contents of the fake output file: It should be empty.
    with open(self.output_file, 'r') as f:
      output = ''.join(line.rstrip() for line in f)
      self.assertEqual('', output)

  def testFlatObject(self):
    obj = {'key1': 1, 'key2': {'key3': 3, 'key4': 4}}
    got = []
    self.poller._FlatObject('base', obj, got)
    want = ['base/key1=1', 'base/key2/key3=3', 'base/key2/key4=4']
    self.assertEqual(got.sort(), want.sort())

  def testJSONParsing(self):
    # { "key": "value" }
    start_json = ' { "key" : "'
    euro = u'\u20AC'
    end_json = '" }'

    # Test for empty JSON
    self.poller.json_response = ''
    self.assertEquals(self.poller.GetHttpResponse('fake/url'), None)

    # Test for broken JSON
    self.poller.json_response = start_json
    self.assertEquals(self.poller.GetHttpResponse('fake/url'), None)
    self.poller.json_response = end_json
    self.assertEquals(self.poller.GetHttpResponse('fake/url'), None)
    self.poller.json_response = start_json + end_json + end_json
    self.assertEquals(self.poller.GetHttpResponse('fake/url'), None)

    # The json library (dumps/loads) assumes strings as UTF-8
    # Need to fail gracefully when wrong encoding is given

    # Normal ascii
    incoming_json = start_json + 'ascii-value' + end_json
    self.poller.json_response = incoming_json
    self.assertNotEquals(self.poller.GetHttpResponse('fake/url'), None)

    # Unicode utf-8: '\xE2 \x82 \xAC' == euro_sign
    self.poller.json_response = start_json + euro.encode('utf-8') + end_json
    self.assertNotEquals(self.poller.GetHttpResponse('fake/url'), None)

    # Unicode utf-16: '\x20\xAC' == euro_sign, should fail
    self.poller.json_response = start_json + euro.encode('utf-16') + end_json
    self.assertEquals(self.poller.GetHttpResponse('fake/url'), None)

if __name__ == '__main__':
  unittest.main()
