# Copyright 2013 Google Inc. All Rights Reserved.
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

"""Tests for tvstat."""

import unittest
import tvstat


class TvstatTest(unittest.TestCase):

  def setUp(self):
    tvstat.PROC_NET_DEV = 'testdata/proc_net_dev'
    tvstat.PROC_NET_UDP = 'testdata/proc_net_udp'
    tvstat.TS_JSON = 'testdata/tr_135_total_tsstats%d.json'

  def testGetUdpDrops(self):
    self.assertEqual(tvstat.GetUDPDrops(), 66 + 77 + 88)

  def testGetTsErrors(self):
    errs = tvstat.GetTsErrors()
    self.assertEqual(errs['PacketDiscontinuityCounter'], 70 + 60)
    self.assertEqual(errs['DropBytes'], 7000 + 6000)
    self.assertEqual(errs['DropPackets'], 701 + 601)
    self.assertEqual(errs['PacketErrorCount'], 7 + 6)

  def testDelta(self):
    new = {'foo': 12}
    old = {}
    self.assertEqual(tvstat.Delta(new, old, 'foo'), 12)
    old = {'foo': 7}
    self.assertEqual(tvstat.Delta(new, old, 'foo'), 5)

  def testGetIfErrors(self):
    errs = tvstat.GetIfErrors('foo0')
    self.assertEqual(errs, (3, 4, 5, 6))


if __name__ == '__main__':
  unittest.main()
