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

  def DevStationGet(self, client):
    if client == '11:11:11:11:11:11':
      return ('Station 11:11:11:11:11:11 (on wlan0)\n'
              '\tinactive time:\t100 ms\n'
              '\trx bytes:\tx\n'
              '\trx packets:\tx\n'
              '\ttx bytes:\tx\n'
              '\ttx packets:\tx\n'
              '\ttx retries:\tx\n'
              '\ttx failed:\tx\n'
              '\tsignal:\t-38 [-48, -56, -39] dBm\n'
              '\tsignal avg:\tx\n'
              '\ttx bitrate:\tx\n'
              '\tauthorized:\tx\n'
              '\tauthenticated:\tx\n'
              '\tpreamble:\tx\n'
              '\tWMM/WME:\tx\n'
              '\tMFP:\tx\n'
              '\tTDLS peer:\tx\n')
    raise wifiblaster.NotAssociatedError

  def setUp(self):
    self.iw = wifiblaster.Iw('wlan0')
    self.iw._DevInfo = lambda: (  # pylint: disable=g-long-lambda
        'Interface wlan0\n'
        '\tifindex 5\n'
        '\twdev 0x1\n'
        '\taddr 11:11:11:11:11:11\n'
        '\tssid TestWifi\n'
        '\ttype AP\n'
        '\twiphy 0\n'
        '\tchannel 1 (2412 MHz), width: 20 MHz, center1: 2412 MHz\n')
    self.iw._DevStationDump = lambda: (  # pylint: disable=g-long-lambda
        'Station 11:11:11:11:11:11 (on wlan0)\n'
        '\tinactive time:\t100 ms\n'
        '\trx bytes:\tx\n'
        '\trx packets:\tx\n'
        '\ttx bytes:\tx\n'
        '\ttx packets:\tx\n'
        '\ttx retries:\tx\n'
        '\ttx failed:\tx\n'
        '\tsignal:\t-38 [-48, -56, -39] dBm\n'
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
    self.iw._DevStationGet = self.DevStationGet

  def testGetFrequency(self):
    self.assertEquals(self.iw.GetFrequency(), 2412)

  def testGetPhy(self):
    self.assertEquals(self.iw.GetPhy(), 'phy0')

  def testGetClients(self):
    self.assertEquals(self.iw.GetClients(),
                      {'11:11:11:11:11:11', '22:22:22:22:22:22'})

  def testGetInactiveTime(self):
    self.assertEquals(self.iw.GetInactiveTime('11:11:11:11:11:11'), .1)

  def testGetRssi(self):
    self.assertEquals(self.iw.GetRssi('11:11:11:11:11:11'), -38)

  def testGetRssiNotAssociated(self):
    self.assertRaises(wifiblaster.NotAssociatedError,
                      self.iw.GetRssi, '33:33:33:33:33:33')


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
