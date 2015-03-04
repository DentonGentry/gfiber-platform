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

  def testGetClients(self):
    self.iw._StationDump = lambda: (
        'Station 11:11:11:11:11:11 (on wlan0)\n'
        '\tinactive time:\tx\n'
        '\trx bytes:\tx\n'
        '\trx packets:\tx\n'
        '\ttx bytes:\tx\n'
        '\ttx packets:\t111\n'
        '\ttx retries:\t11\n'
        '\ttx failed:\t1\n'
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
        '\ttx packets:\t222\n'
        '\ttx retries:\t22\n'
        '\ttx failed:\t2\n'
        '\tsignal:\tx\n'
        '\tsignal avg:\tx\n'
        '\ttx bitrate:\tx\n'
        '\tauthorized:\tx\n'
        '\tauthenticated:\tx\n'
        '\tpreamble:\tx\n'
        '\tWMM/WME:\tx\n'
        '\tMFP:\tx\n'
        '\tTDLS peer:\tx\n')
    self.assertEquals(self.iw.GetClients(), {
        '11:11:11:11:11:11': wifiblaster.IwStat(tx_packets=111,
                                                tx_retries=11,
                                                tx_failed=1),
        '22:22:22:22:22:22': wifiblaster.IwStat(tx_packets=222,
                                                tx_retries=22,
                                                tx_failed=2)})

  def testGetClients_MissingCounter(self):
    self.iw._StationDump = lambda: (
        'Station 11:11:11:11:11:11 (on wlan0)\n'
        '\tinactive time:\tx\n'
        '\trx bytes:\tx\n'
        '\trx packets:\tx\n'
        '\ttx bytes:\tx\n'
        '\ttx packets:\t111\n'
        '\ttx retries:\t11\n'
        # '\ttx failed:\t1\n'
        '\tsignal:\tx\n'
        '\tsignal avg:\tx\n'
        '\ttx bitrate:\tx\n'
        '\tauthorized:\tx\n'
        '\tauthenticated:\tx\n'
        '\tpreamble:\tx\n'
        '\tWMM/WME:\tx\n'
        '\tMFP:\tx\n'
        '\tTDLS peer:\tx\n')
    self.assertEquals(self.iw.GetClients(), {
        '11:11:11:11:11:11': wifiblaster.IwStat(tx_packets=111,
                                                tx_retries=11,
                                                tx_failed=0)})


class PktgenTest(unittest.TestCase):

  def setUp(self):
    self.pktgen = wifiblaster.Pktgen('wlan0')
    self.files = {}
    self.pktgen._ReadFile = lambda filename: self.files[filename]
    self.pktgen._WriteFile = lambda filename, s: None

  def testPacketBlast(self):
    self.files['/proc/net/pktgen/wlan0'] = (
        'Params: count 1000  min_pkt_size: 64  max_pkt_size: 64\n'
        '     frags: 0  delay: 0  clone_skb: 0  ifname: wlan0\n'
        '     flows: 0 flowlen: 0\n'
        '     queue_map_min: 0  queue_map_max: 0\n'
        '     duration: 0\n'
        '     dst_min:   dst_max: \n'
        '        src_min:   src_max: \n'
        '     src_mac: 11:11:11:11:11:11 dst_mac: 22:22:22:22:22:22\n'
        '     udp_src_min: 9  udp_src_max: 9  udp_dst_min: 9  udp_dst_max: 9\n'
        '     src_mac_count: 0  dst_mac_count: 0\n'
        '     Flags: \n'
        'Current:\n'
        '     pkts-sofar: 1000  errors: 0\n'
        '     started: 764372888089us  stopped: 764373283285us idle: 212us\n'
        '     seq_num: 1001  cur_dst_mac_offset: 0  cur_src_mac_offset: 0\n'
        '     cur_saddr: 0.0.0.0  cur_daddr: 0.0.0.0\n'
        '     cur_udp_dst: 9  cur_udp_src: 9\n'
        '     cur_queue_map: 0\n'
        '     flows: 0\n'
        'Result: OK: 395196(c394984+d212) usec, 1000 (64byte,0frags)\n'
        '  2530pps 1Mb/sec (1295360bps) errors: 0\n')
    self.assertEquals(self.pktgen.PacketBlast('22:22:22:22:22:22', 1000, 0, 64),
                      .395196)

  def testPacketBlast_PacketBlastFailed(self):
    self.files['/proc/net/pktgen/wlan0'] = (
        'Params: count 1000  min_pkt_size: 0  max_pkt_size: 0\n'
        '     frags: 0  delay: 0  clone_skb: 0  ifname: wlan0\n'
        '     flows: 0 flowlen: 0\n'
        '     queue_map_min: 0  queue_map_max: 0\n'
        '     duration: 0\n'
        '     dst_min:   dst_max: \n'
        '        src_min:   src_max: \n'
        '     src_mac: 11:11:11:11:11:11 dst_mac: 22:22:22:22:22:22\n'
        '     udp_src_min: 9  udp_src_max: 9  udp_dst_min: 9  udp_dst_max: 9\n'
        '     src_mac_count: 0  dst_mac_count: 0\n'
        '     Flags: \n'
        'Current:\n'
        '     pkts-sofar: 0  errors: 0\n'
        '     started: 0us  stopped: 0us idle: 0us\n'
        '     seq_num: 0  cur_dst_mac_offset: 0  cur_src_mac_offset: 0\n'
        '     cur_saddr: 0.0.0.0  cur_daddr: 0.0.0.0\n'
        '     cur_udp_dst: 0  cur_udp_src: 0\n'
        '     cur_queue_map: 0\n'
        '     flows: 0\n'
        'Result: Idle\n')
    self.assertRaisesRegexp(Exception, r'Packet blast failed',
                            self.pktgen.PacketBlast,
                            '22:22:22:22:22:22', 1000, 0, 64)


class WifiblasterTest(unittest.TestCase):

  def setUp(self):
    def GetClients():
      return {
          '11:11:11:11:11:11':
              wifiblaster.IwStat(tx_packets=self.tx_packets,
                                 tx_retries=self.tx_packets / 100,
                                 tx_failed=self.tx_packets / 1000)
      }

    def PacketBlast(client, count, duration, _):
      if client == '11:11:11:11:11:11':
        if count != 0:
          self.tx_packets += count
        elif duration != 0:
          self.tx_packets += 1000 * duration
      return duration

    self.iw = wifiblaster.Iw('wlan0')
    self.iw.GetClients = GetClients
    self.pktgen = wifiblaster.Pktgen('wlan0')
    self.pktgen.PacketBlast = PacketBlast
    self.tx_packets = 0

  def testPacketBlast(self):
    self.assertEquals(
        wifiblaster._PacketBlast(self.iw, self.pktgen,
                                 '11:11:11:11:11:11', 1, 64),
        'mac=11:11:11:11:11:11 tx_packets=1000 tx_retries=10 tx_failed=1 '
        'throughput=512000')

  def testPacketBlast_ClientDisconnected(self):
    self.assertEquals(
        wifiblaster._PacketBlast(self.iw, self.pktgen,
                                 '22:22:22:22:22:22', 1, 64),
        'mac=22:22:22:22:22:22 not connected')


if __name__ == '__main__':
  unittest.main()
