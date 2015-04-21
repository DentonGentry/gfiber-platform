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

"""Tests for wifiblaster."""

__author__ = 'mikemu@google.com (Mike Mu)'

import unittest
import wifiblaster


class IwTest(unittest.TestCase):

  def setUp(self):
    self.iw = wifiblaster.Iw('wlan0')
    self.iw._DevStationDump = lambda: (  # pylint: disable=g-long-lambda
        'Station 11:11:11:11:11:11 (on wlan0)\n'
        '\tinactive time:\tx\n'
        '\trx bytes:\tx\n'
        '\trx packets:\tx\n'
        '\ttx bytes:\tx\n'
        '\ttx packets:\tx\n'
        '\ttx retries:\tx\n'
        '\ttx failed:\tx\n'
        '\tsignal:\tx\n'
        '\tsignal avg:\tx\n'
        '\ttx bitrate:\tx\n'
        '\tauthorized:\tx\n'
        '\tauthenticated:\tx\n'
        '\tpreamble:\tx\n'
        '\tWMM/WME:\tx\n'
        '\tMFP:\tx\n'
        '\tTDLS peer:\tx\n'
        'Station 22:22:22:22:22:22 (on wlan0)\n'
        '\tinactive time:\tx\n'
        '\trx bytes:\tx\n'
        '\trx packets:\tx\n'
        '\ttx bytes:\tx\n'
        '\ttx packets:\tx\n'
        '\ttx retries:\tx\n'
        '\ttx failed:\tx\n'
        '\tsignal:\tx\n'
        '\tsignal avg:\tx\n'
        '\ttx bitrate:\tx\n'
        '\tauthorized:\tx\n'
        '\tauthenticated:\tx\n'
        '\tpreamble:\tx\n'
        '\tWMM/WME:\tx\n'
        '\tMFP:\tx\n'
        '\tTDLS peer:\tx\n')
    self.iw._DevInfo = lambda: (  # pylint: disable=g-long-lambda
        'Interface wlan0\n'
        '\tifindex 5\n'
        '\twdev 0x1\n'
        '\taddr 11:11:11:11:11:11\n'
        '\tssid TestWifi\n'
        '\ttype AP\n'
        '\twiphy 0\n'
        '\tchannel 1 (2412 MHz), width: 20 MHz, center1: 2412 MHz\n')

  def testGetClients(self):
    self.assertEquals(self.iw.GetClients(),
                      {'11:11:11:11:11:11', '22:22:22:22:22:22'})

  def testGetPhy(self):
    self.assertEquals(self.iw.GetPhy(), 'phy0')


class Mac80211StatsTest(unittest.TestCase):

  def setUp(self):
    self.mac80211stats = wifiblaster.Mac80211Stats('phy0')
    self.counters = {}
    self.mac80211stats._ReadCounter = lambda counter: self.counters[counter]

  def testGetTransmittedFrameCount(self):
    self.counters['transmitted_frame_count'] = 123
    self.assertEquals(self.mac80211stats.GetTransmittedFrameCount(), 123)


if __name__ == '__main__':
  unittest.main()
