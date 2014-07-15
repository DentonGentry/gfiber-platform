#!/usr/bin/python2

from wifipacket import Packetize, Struct, ZOpen
import unittest
from collections import OrderedDict

__author__ = 'shantanuj@google.com (Shantanu Jain)'


class TestPacketizeBasic(unittest.TestCase):
  """Packetize() test of ProbeReq, Auth, and AssocReq packets only."""
  
  def setUp(self):
    stream = ZOpen('./Probe_Auth_Asso_test.pcap')
    self.packets = list(Packetize(stream))

  def test_ProbeReq(self):
    ProbeReq = Struct({
        'bad': 0,
        'channel_flags': 256,
        'dot11_mgmt_frame_attrs': OrderedDict([
            (0, [['0x47', '0x45', '0x45', '0x4b', '0x48', '0x4f', '0x4c', '0x44', '0x2d', '0x41']]),
            (1, [['0xc', '0x12', '0x18', '0x24', '0x30', '0x48', '0x60', '0x6c']]),
            (45, [['0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (221, [['0x0', '0x90', '0x4c', '0x33', '0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 0,
        'flags': 2,
        'frag': 0,
        'freq': 5805,
        'incl_len': 131,
        'mac_usecs': 946316,
        'order': 0,
        'orig_len': 131,
        'pcap_secs': 1402754454.634853,
        'powerman': 0,
        'ra': 'ff:ff:ff:ff:ff:ff',
        'rate': 12,
        'retry': 0,
        'seq': 0,
        'ta': '28:cf:e9:16:67:4f',
        'type': 4,
        'typestr': 'ProbeReq',
        'xa': 'ff:ff:ff:ff:ff:ff'
    })
    self.assertEqual(ProbeReq, self.packets[0][0])

  def test_Auth(self):
    Auth = Struct({
        'bad': 0,
        'channel_flags': 256,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('AUTHENTICATION_ALGORITHM_NUMBER', [['0x0', '0x0']]),
            ('AUTHENTICATION_TRANSACTION_SEQUENCE_NUMBER', [['0x1', '0x0']]),
            ('STATUS_CODE', [['0x0', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 2,
        'frag': 0,
        'freq': 5240,
        'incl_len': 55,
        'mac_usecs': 1092934,
        'order': 0,
        'orig_len': 55,
        'pcap_secs': 1402754454.781483,
        'powerman': 0,
        'ra': 'c4:3d:c7:9d:a5:dd',
        'rate': 12,
        'retry': 0,
        'seq': 0,
        'ta': '28:cf:e9:16:67:4f',
        'type': 11,
        'typestr': 'Auth',
        'xa': 'c4:3d:c7:9d:a5:dd'
    })
    self.assertEqual(Auth, self.packets[1][0])

  def test_AssocReq(self):
    AssocReq = Struct({
        'bad': 0,
        'channel_flags': 256,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('CAPABILITY_INFO', [['0x11', '0x0']]),
            ('LISTEN_INTERVAL', [['0xa', '0x0']]),
            (0, [['0x47', '0x45', '0x45', '0x4b', '0x48', '0x4f', '0x4c', '0x44', '0x2d', '0x41']]),
            (1, [['0x8c', '0x12', '0x98', '0x24', '0xb0', '0x48', '0x60', '0x6c']]),
            (33, [['0x5', '0x0']]),
            (36, [['0x24', '0x4', '0x34', '0x4', '0x64', '0xb', '0x95', '0x4', '0xa5', '0x1']]),
            (48, [['0x1', '0x0', '0x0', '0xf', '0xac', '0x2', '0x1', '0x0', '0x0', '0xf', '0xac', '0x4', '0x1', '0x0', '0x0', '0xf', '0xac', '0x2', '0x0', '0x0']]),
            (45, [['0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (221, [['0x0', '0x90', '0x4c', '0x33', '0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0'], ['0x0', '0x50', '0xf2', '0x2', '0x0', '0x1', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 2,
        'frag': 0,
        'freq': 5240,
        'incl_len': 182,
        'mac_usecs': 1093954,
        'order': 0,
        'orig_len': 182,
        'pcap_secs': 1402754454.782498,
        'powerman': 0,
        'ra': 'c4:3d:c7:9d:a5:dd',
        'rate': 12,
        'retry': 0,
        'seq': 0,
        'ta': '28:cf:e9:16:67:4f',
        'type': 0,
        'typestr': 'AssocReq',
        'xa': 'c4:3d:c7:9d:a5:dd'
    })
    self.assertEqual(AssocReq, self.packets[2][0])


class TestPacketizeAdvanced(unittest.TestCase):

  def setUp(self):
    self.packets = list(Packetize(ZOpen('./MacOS Wifi Fingerprint.pcap')))

  def test_ProbeReq_1(self):
    ProbeReq = Struct({
        'bad': 0,
        'channel_flags': 256,
        'dot11_mgmt_frame_attrs': OrderedDict([
            (0, [['0x47', '0x45', '0x45', '0x4b', '0x48', '0x4f', '0x4c', '0x44', '0x2d', '0x41']]),
            (1, [['0xc', '0x12', '0x18', '0x24', '0x30', '0x48', '0x60', '0x6c']]),
            (45, [['0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (221, [['0x0', '0x90', '0x4c', '0x33', '0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 0,
        'flags': 2,
        'frag': 0,
        'freq': 5805,
        'incl_len': 131,
        'mac_usecs': 946316,
        'order': 0,
        'orig_len': 131,
        'pcap_secs': 1402754454.634853,
        'powerman': 0,
        'ra': 'ff:ff:ff:ff:ff:ff',
        'rate': 12,
        'retry': 0,
        'seq': 0,
        'ta': '28:cf:e9:16:67:4f',
        'type': 4,
        'typestr': 'ProbeReq',
        'xa': 'ff:ff:ff:ff:ff:ff'
    })
    self.assertEqual(ProbeReq, self.packets[0][0])
    # self.assertTrue('dot11_mgmt_frame_attrs' not in self.packets[0][0])

  def test_ProbeReq_2(self):
    """Tests another ProbeReq to check SSID parsing when SSID is Broadcast (blank)"""
    ProbeReq = Struct({
        'bad': 0,
        'channel_flags': 256,
        'dot11_mgmt_frame_attrs': OrderedDict([
            (0, [[]]),
            (1, [['0xc', '0x12', '0x18', '0x24', '0x30', '0x48', '0x60', '0x6c']]),
            (45, [['0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (221, [['0x0', '0x90', '0x4c', '0x33', '0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 0,
        'flags': 2,
        'frag': 0,
        'freq': 5805,
        'incl_len': 121,
        'mac_usecs': 948733,
        'order': 0,
        'orig_len': 121,
        'pcap_secs': 1402754454.637267,
        'powerman': 0,
        'ra': 'ff:ff:ff:ff:ff:ff',
        'rate': 12,
        'retry': 0,
        'seq': 0,
        'ta': '28:cf:e9:16:67:4f',
        'type': 4,
        'typestr': 'ProbeReq',
        'xa': 'ff:ff:ff:ff:ff:ff'
    })
    self.assertEqual(ProbeReq, self.packets[13][0])

  def test_Beacon(self):
    Beacon = Struct({
        'antenna': 1,
        'bad': 0,
        'channel_flags': 320,
        'dbm_antnoise': -90,
        'dbm_antsignal': -80,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('TIMESTAMP', [['0x3b', '0x88', '0x4a', '0x6e', '0x31', '0x0', '0x0', '0x0']]),
            ('BEACON_INTERVAL', [['0x64', '0x0']]), ('CAPABILITY_INFO', [['0x11', '0x0']]),
            (0, [['0x47', '0x45', '0x45', '0x4b', '0x48', '0x4f', '0x4c', '0x44', '0x2d', '0x41']]),
            (1, [['0x8c', '0x12', '0x98', '0x24', '0xb0', '0x48', '0x60', '0x6c']]),
            (3, [['0x30']]), (5, [['0x0', '0x2', '0x1', '0x0']]),
            (7, [['0x55', '0x53', '0x20', '0x24', '0x4', '0x11', '0x95', '0x5', '0x14', '0x0']]),
            (48, [['0x1', '0x0', '0x0', '0xf', '0xac', '0x2', '0x2', '0x0', '0x0', '0xf', '0xac', '0x4', '0x0', '0xf', '0xac', '0x2', '0x1', '0x0', '0x0', '0xf', '0xac', '0x2', '0xc', '0x0']]),
            (221, [['0x0', '0x50', '0xf2', '0x1', '0x1', '0x0', '0x0', '0x50', '0xf2', '0x2', '0x2', '0x0', '0x0', '0x50', '0xf2', '0x4', '0x0', '0x50', '0xf2', '0x2', '0x1', '0x0', '0x0', '0x50', '0xf2', '0x2'], ['0x0', '0x50', '0xf2', '0x2', '0x1', '0x1', '0x0', '0x0', '0x3', '0xa4', '0x0', '0x0', '0x27', '0xa4', '0x0', '0x0', '0x42', '0x43', '0x5e', '0x0', '0x62', '0x32', '0x2f', '0x0']]),
            (45, [['0xcc', '0x11', '0x1b', '0xff', '0xff', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (61, [['0x30', '0x0', '0x4', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 0,
        'flags': 18,
        'frag': 0,
        'freq': 5240,
        'incl_len': 240,
        'mac_usecs': 983273,
        'order': 0,
        'orig_len': 240,
        'pcap_secs': 1402754454.672181,
        'powerman': 0,
        'ra': 'ff:ff:ff:ff:ff:ff',
        'rate': 12,
        'retry': 0,
        'seq': 4035,
        'ta': 'c4:3d:c7:9d:a5:dd',
        'type': 8,
        'typestr': 'Beacon',
        'xa': 'c4:3d:c7:9d:a5:dd'
    })
    self.assertEqual(Beacon, self.packets[14][0])

  def test_Auth(self):
    Auth = Struct({
        'bad': 0,
        'channel_flags': 256,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('AUTHENTICATION_ALGORITHM_NUMBER', [['0x0', '0x0']]),
            ('AUTHENTICATION_TRANSACTION_SEQUENCE_NUMBER', [['0x1', '0x0']]),
            ('STATUS_CODE', [['0x0', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 2,
        'frag': 0,
        'freq': 5240,
        'incl_len': 55,
        'mac_usecs': 1092934,
        'order': 0,
        'orig_len': 55,
        'pcap_secs': 1402754454.781483,
        'powerman': 0,
        'ra': 'c4:3d:c7:9d:a5:dd',
        'rate': 12,
        'retry': 0,
        'seq': 0,
        'ta': '28:cf:e9:16:67:4f',
        'type': 11,
        'typestr': 'Auth',
        'xa': 'c4:3d:c7:9d:a5:dd'
    })
    self.assertEqual(Auth, self.packets[16][0])

  def test_AssocReq(self):
    AssocReq = Struct({
        'bad': 0,
        'channel_flags': 256,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('CAPABILITY_INFO', [['0x11', '0x0']]),
            ('LISTEN_INTERVAL', [['0xa', '0x0']]),
            (0, [['0x47', '0x45', '0x45', '0x4b', '0x48', '0x4f', '0x4c', '0x44', '0x2d', '0x41']]),
            (1, [['0x8c', '0x12', '0x98', '0x24', '0xb0', '0x48', '0x60', '0x6c']]),
            (33, [['0x5', '0x0']]),
            (36, [['0x24', '0x4', '0x34', '0x4', '0x64', '0xb', '0x95', '0x4', '0xa5', '0x1']]),
            (48, [['0x1', '0x0', '0x0', '0xf', '0xac', '0x2', '0x1', '0x0', '0x0', '0xf', '0xac', '0x4', '0x1', '0x0', '0x0', '0xf', '0xac', '0x2', '0x0', '0x0']]),
            (45, [['0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (221, [['0x0', '0x90', '0x4c', '0x33', '0xef', '0x9', '0x1b', '0xff', '0xff', '0xff', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0'], ['0x0', '0x50', '0xf2', '0x2', '0x0', '0x1', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 2,
        'frag': 0,
        'freq': 5240,
        'incl_len': 182,
        'mac_usecs': 1093954,
        'order': 0,
        'orig_len': 182,
        'pcap_secs': 1402754454.782498,
        'powerman': 0,
        'ra': 'c4:3d:c7:9d:a5:dd',
        'rate': 12,
        'retry': 0,
        'seq': 0,
        'ta': '28:cf:e9:16:67:4f',
        'type': 0,
        'typestr': 'AssocReq',
        'xa': 'c4:3d:c7:9d:a5:dd'
    })
    self.assertEqual(AssocReq, self.packets[19][0])

  def test_AssocResp(self):
    AssocResp = Struct({
        'antenna': 0,
        'bad': 0,
        'channel_flags': 320,
        'dbm_antnoise': -90,
        'dbm_antsignal': -79,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('CAPABILITY_INFO', [['0x11', '0x0']]),
            ('STATUS_CODE', [['0x0', '0x0']]),
            ('AID', [['0x3', '0xc0']]),
            (1, [['0x8c', '0x12', '0x98', '0x24', '0xb0', '0x48', '0x60', '0x6c']]),
            (45, [['0xcc', '0x11', '0x1b', '0xff', '0xff', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (61, [['0x30', '0x0', '0x4', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (221, [['0x0', '0x50', '0xf2', '0x2', '0x1', '0x1', '0x0', '0x0', '0x3', '0xa4', '0x0', '0x0', '0x27', '0xa4', '0x0', '0x0', '0x42', '0x43', '0x5e', '0x0', '0x62', '0x32', '0x2f', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 18,
        'frag': 0,
        'freq': 5240,
        'incl_len': 147,
        'mac_usecs': 1094437,
        'order': 0,
        'orig_len': 147,
        'pcap_secs': 1402754454.783172,
        'powerman': 0,
        'ra': '28:cf:e9:16:67:4f',
        'rate': 12,
        'retry': 0,
        'seq': 4039,
        'ta': 'c4:3d:c7:9d:a5:dd',
        'type': 1,
        'typestr': 'AssocResp',
        'xa': 'c4:3d:c7:9d:a5:dd'
    })
    self.assertEqual(AssocResp, self.packets[21][0])


class TestPacketizeExtensionPackets(unittest.TestCase):

  def setUp(self):
    self.packets = list(Packetize(ZOpen('./Deauth ProbeReq Auth Reassoc.pcap')))

  def test_Deauth(self):
    Deauth = Struct({
        'antenna': 0,
        'bad': 0,
        'channel_flags': 320,
        'dbm_antsignal': -29,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('REASON_CODE', [['0x3', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 0,
        'frag': 0,
        'freq': 5200,
        'incl_len': 44,
        'order': 0,
        'orig_len': 44,
        'pcap_secs': 1404771727.796671,
        'powerman': 0,
        'ra': '40:16:7e:a3:0c:f4',
        'rate': 12,
        'retry': 0,
        'rx_flags': 0,
        'seq': 10,
        'ta': '7c:7a:91:a5:d1:d3',
        'type': 12,
        'typestr': 'Deauth',
        'xa': '40:16:7e:a3:0c:f4'
    })
    self.assertEqual(Deauth, self.packets[0][0])

  def test_ProbeResp(self):
    ProbeResp = Struct({
        'antenna': 0,
        'bad': 0,
        'channel_flags': 320,
        'dbm_antsignal': -33,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('TIMESTAMP', [['0xd1', '0x86', '0x32', '0xe9', '0x4f', '0x0', '0x0', '0x0']]),
            ('BEACON_INTERVAL', [['0x64', '0x0']]),
            ('CAPABILITY_INFO', [['0x11', '0x10']]),
            (0, [['0x41', '0x53', '0x55', '0x53', '0x5f', '0x35', '0x30', '0x47']]),
            (1, [['0x8c', '0x12', '0x98', '0x24', '0xb0', '0x48', '0x60', '0x6c']]),
            (48, [['0x1', '0x0', '0x0', '0xf', '0xac', '0x4', '0x1', '0x0', '0x0', '0xf', '0xac', '0x4', '0x1', '0x0', '0x0', '0xf', '0xac', '0x2', '0xc', '0x0']]),
            (45, [['0xef', '0x9', '0x17', '0xff', '0xff', '0xff', '0x0', '0x1', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (61, [['0x28', '0xf', '0x11', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0']]),
            (127, [['0x4', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x40']]),
            (191, [['0xb2', '0x59', '0x82', '0xf', '0xea', '0xff', '0x0', '0x0', '0xea', '0xff', '0x0', '0x0']]),
            (192, [['0x0', '0x26', '0x0', '0x0', '0x0']]),
            (195, [['0x1', '0x2', '0x2']]),
            (221, [['0x0', '0x10', '0x18', '0x2', '0x0', '0x0', '0x1c', '0x0', '0x0'], ['0x0', '0x50', '0xf2', '0x2', '0x1', '0x1', '0x84', '0x0', '0x3', '0xa4', '0x0', '0x0', '0x27', '0xa4', '0x0', '0x0', '0x42', '0x43', '0x5e', '0x0', '0x62', '0x32', '0x2f', '0x0']]),
            (70, [['0x72', '0x8', '0x1', '0x0', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 0,
        'frag': 0,
        'freq': 5200,
        'incl_len': 228,
        'order': 0,
        'orig_len': 228,
        'pcap_secs': 1404771733.563038,
        'powerman': 0,
        'ra': '7c:7a:91:a5:d1:d3',
        'rate': 12,
        'retry': 0,
        'rx_flags': 0,
        'seq': 3145,
        'ta': '40:16:7e:a3:0c:f4',
        'type': 5,
        'typestr': 'ProbeResp',
        'xa': '40:16:7e:a3:0c:f4'
    })
    self.assertEqual(ProbeResp, self.packets[1][0])

  def test_ReassocReq(self):
    ReassocReq = Struct({
        'antenna': 0,
        'bad': 0,
        'channel_flags': 320,
        'dbm_antsignal': -30,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('CAPABILITY_INFO', [['0x11', '0x0']]),
            ('LISTEN_INTERVAL', [['0xa', '0x0']]),
            ('CURRENT_AP_ADDRESS', [['0x40', '0x16', '0x7e', '0xa3', '0xc', '0xf4']]),
            (0, [['0x41', '0x53', '0x55', '0x53', '0x5f', '0x35', '0x30', '0x47']]),
            (1, [['0xc', '0x12', '0x18', '0x24', '0x30', '0x48', '0x60', '0x6c']]),
            (48, [['0x1', '0x0', '0x0', '0xf', '0xac', '0x4', '0x1', '0x0', '0x0', '0xf', '0xac', '0x4', '0x1', '0x0', '0x0', '0xf', '0xac', '0x2', '0x0', '0x0']]),
            (221, [['0x0', '0x50', '0xf2', '0x2', '0x0', '0x1', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 0,
        'frag': 0,
        'freq': 5200,
        'incl_len': 103,
        'order': 0,
        'orig_len': 103,
        'pcap_secs': 1404771736.180159,
        'powerman': 0,
        'ra': '40:16:7e:a3:0c:f4',
        'rate': 12,
        'retry': 0,
        'rx_flags': 0,
        'seq': 12,
        'ta': '7c:7a:91:a5:d1:d3',
        'type': 2,
        'typestr': 'ReassocReq',
        'xa': '40:16:7e:a3:0c:f4'
    })
    self.assertEqual(ReassocReq, self.packets[3][0])

  def test_ReassocResp(self):
    ReassocResp = Struct({
        'antenna': 0,
        'bad': 0,
        'channel_flags': 320,
        'dbm_antsignal': -33,
        'dot11_mgmt_frame_attrs': OrderedDict([
            ('CAPABILITY_INFO', [['0x11', '0x10']]),
            ('STATUS_CODE', [['0x0', '0x0']]),
            ('AID', [['0x1', '0xc0']]),
            (1, [['0x8c', '0x12', '0x98', '0x24', '0xb0', '0x48', '0x60', '0x6c']]),
            (53, [['0x0']]),
            (65, [['0x0']]),
            (70, [['0x72', '0x8', '0x1', '0x0', '0x0']]),
            (127, [['0x4', '0x0', '0x0', '0x0', '0x0', '0x0', '0x0', '0x40']]),
            (221, [['0x0', '0x10', '0x18', '0x2', '0x0', '0x0', '0x1c', '0x0', '0x0'], ['0x0', '0x50', '0xf2', '0x2', '0x1', '0x1', '0x84', '0x0', '0x3', '0xa4', '0x0', '0x0', '0x27', '0xa4', '0x0', '0x0', '0x42', '0x43', '0x5e', '0x0', '0x62', '0x32', '0x2f', '0x0']])
        ]),
        'dsmode': 0,
        'duration': 60,
        'flags': 0,
        'frag': 0,
        'freq': 5200,
        'incl_len': 118,
        'order': 0,
        'orig_len': 118,
        'pcap_secs': 1404771736.180516,
        'powerman': 0,
        'ra': '7c:7a:91:a5:d1:d3',
        'rate': 12,
        'retry': 0,
        'rx_flags': 0,
        'seq': 3182,
        'ta': '40:16:7e:a3:0c:f4',
        'type': 3,
        'typestr': 'ReassocResp',
        'xa': '40:16:7e:a3:0c:f4'
    })


if __name__ == '__main__':
  unittest.main()
