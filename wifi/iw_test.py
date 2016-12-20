#!/usr/bin/python -S

"""Tests for iw.py."""

import iw
from wvtest import wvtest


PHY_OUTPUT = """Wiphy phy1
  max # scan SSIDs: 16
  max scan IEs length: 199 bytes
  Retry short limit: 7
  Retry long limit: 4
  Coverage class: 0 (up to 0m)
  Device supports AP-side u-APSD.
  Supported Ciphers:
    * WEP40 (00-0f-ac:1)
    * WEP104 (00-0f-ac:5)
    * TKIP (00-0f-ac:2)
    * CCMP (00-0f-ac:4)
    * CMAC (00-0f-ac:6)
  Available Antennas: TX 0x7 RX 0x7
  Configured Antennas: TX 0x7 RX 0x7
  Supported interface modes:
     * managed
     * AP
     * AP/VLAN
     * monitor
  Band 2:
    Capabilities: 0x19e3
      RX LDPC
      HT20/HT40
      Static SM Power Save
      RX HT20 SGI
      RX HT40 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 7935 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 8 usec (0x06)
    HT TX/RX MCS rate indexes supported: 0-23
    VHT Capabilities (0x338001b2):
      Max MPDU length: 11454
      Supported Channel Width: neither 160 nor 80+80
      RX LDPC
      short GI (80 MHz)
      TX STBC
      RX antenna pattern consistency
      TX antenna pattern consistency
    VHT RX MCS set:
      1 streams: MCS 0-9
      2 streams: MCS 0-9
      3 streams: MCS 0-9
      4 streams: not supported
      5 streams: not supported
      6 streams: not supported
      7 streams: not supported
      8 streams: not supported
    VHT RX highest supported: 0 Mbps
    VHT TX MCS set:
      1 streams: MCS 0-9
      2 streams: MCS 0-9
      3 streams: MCS 0-9
      4 streams: not supported
      5 streams: not supported
      6 streams: not supported
      7 streams: not supported
      8 streams: not supported
    VHT TX highest supported: 0 Mbps
    Bitrates (non-HT):
      * 6.0 Mbps
      * 9.0 Mbps
      * 12.0 Mbps
      * 18.0 Mbps
      * 24.0 Mbps
      * 36.0 Mbps
      * 48.0 Mbps
      * 54.0 Mbps
    Frequencies:
      * 5180 MHz [36] (17.0 dBm)
      * 5200 MHz [40] (17.0 dBm)
      * 5220 MHz [44] (17.0 dBm)
      * 5240 MHz [48] (17.0 dBm)
      * 5260 MHz [52] (23.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5280 MHz [56] (23.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5300 MHz [60] (23.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5320 MHz [64] (23.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5500 MHz [100] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5520 MHz [104] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5540 MHz [108] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5560 MHz [112] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5580 MHz [116] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5600 MHz [120] (disabled)
      * 5620 MHz [124] (disabled)
      * 5640 MHz [128] (disabled)
      * 5660 MHz [132] (30.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5680 MHz [136] (30.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5700 MHz [140] (30.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5745 MHz [149] (30.0 dBm)
      * 5765 MHz [153] (30.0 dBm)
      * 5785 MHz [157] (30.0 dBm)
      * 5805 MHz [161] (30.0 dBm)
      * 5825 MHz [165] (30.0 dBm)
  Supported commands:
     * new_interface
     * set_interface
     * new_key
     * start_ap
     * new_station
     * set_bss
     * authenticate
     * associate
     * deauthenticate
     * disassociate
     * join_ibss
     * remain_on_channel
     * set_tx_bitrate_mask
     * frame
     * frame_wait_cancel
     * set_wiphy_netns
     * set_channel
     * set_wds_peer
     * probe_client
     * set_noack_map
     * register_beacons
     * start_p2p_device
     * set_mcast_rate
     * channel_switch
     * Unknown command (104)
     * connect
     * disconnect
  Supported TX frame types:
     * IBSS: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * managed: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * AP: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * AP/VLAN: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * mesh point: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * P2P-client: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * P2P-GO: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * P2P-device: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
  Supported RX frame types:
     * IBSS: 0x40 0xb0 0xc0 0xd0
     * managed: 0x40 0xd0
     * AP: 0x00 0x20 0x40 0xa0 0xb0 0xc0 0xd0
     * AP/VLAN: 0x00 0x20 0x40 0xa0 0xb0 0xc0 0xd0
     * mesh point: 0xb0 0xc0 0xd0
     * P2P-client: 0x40 0xd0
     * P2P-GO: 0x00 0x20 0x40 0xa0 0xb0 0xc0 0xd0
     * P2P-device: 0x40 0xd0
  software interface modes (can always be added):
     * AP/VLAN
     * monitor
  valid interface combinations:
     * #{ AP } <= 8,
       total <= 8, #channels <= 1, STA/AP BI must match, radar detect widths: { 20 MHz (no HT), 20 MHz, 40 MHz, 80 MHz }

  HT Capability overrides:
     * MCS: ff ff ff ff ff ff ff ff ff ff
     * maximum A-MSDU length
     * supported channel width
     * short GI for 40 MHz
     * max A-MPDU length exponent
     * min MPDU start spacing
  Device supports TX status socket option.
  Device supports HT-IBSS.
  Device supports SAE with AUTHENTICATE command
  Device supports scan flush.
  Device supports per-vif TX power setting
  Driver supports a userspace MPM
  Driver/device bandwidth changes during BSS lifetime (AP/GO mode)
  Device supports static SMPS
Wiphy phy0
  max # scan SSIDs: 4
  max scan IEs length: 2257 bytes
  Retry short limit: 7
  Retry long limit: 4
  Coverage class: 0 (up to 0m)
  Device supports RSN-IBSS.
  Device supports AP-side u-APSD.
  Device supports T-DLS.
  Supported Ciphers:
    * WEP40 (00-0f-ac:1)
    * WEP104 (00-0f-ac:5)
    * TKIP (00-0f-ac:2)
    * CCMP (00-0f-ac:4)
    * CMAC (00-0f-ac:6)
  Available Antennas: TX 0x7 RX 0x7
  Configured Antennas: TX 0x7 RX 0x7
  Supported interface modes:
     * IBSS
     * managed
     * AP
     * AP/VLAN
     * WDS
     * monitor
     * P2P-client
     * P2P-GO
  Band 1:
    Capabilities: 0x11ef
      RX LDPC
      HT20/HT40
      SM Power Save disabled
      RX HT20 SGI
      RX HT40 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 3839 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 8 usec (0x06)
    HT TX/RX MCS rate indexes supported: 0-23
    Bitrates (non-HT):
      * 1.0 Mbps
      * 2.0 Mbps (short preamble supported)
      * 5.5 Mbps (short preamble supported)
      * 11.0 Mbps (short preamble supported)
      * 6.0 Mbps
      * 9.0 Mbps
      * 12.0 Mbps
      * 18.0 Mbps
      * 24.0 Mbps
      * 36.0 Mbps
      * 48.0 Mbps
      * 54.0 Mbps
    Frequencies:
      * 2412 MHz [1] (30.0 dBm)
      * 2417 MHz [2] (30.0 dBm)
      * 2422 MHz [3] (30.0 dBm)
      * 2427 MHz [4] (30.0 dBm)
      * 2432 MHz [5] (30.0 dBm)
      * 2437 MHz [6] (30.0 dBm)
      * 2442 MHz [7] (30.0 dBm)
      * 2447 MHz [8] (30.0 dBm)
      * 2452 MHz [9] (30.0 dBm)
      * 2457 MHz [10] (30.0 dBm)
      * 2462 MHz [11] (30.0 dBm)
      * 2467 MHz [12] (disabled)
      * 2472 MHz [13] (disabled)
      * 2484 MHz [14] (disabled)
  Band 2:
    Capabilities: 0x11ef
      RX LDPC
      HT20/HT40
      SM Power Save disabled
      RX HT20 SGI
      RX HT40 SGI
      TX STBC
      RX STBC 1-stream
      Max AMSDU length: 3839 bytes
      DSSS/CCK HT40
    Maximum RX AMPDU length 65535 bytes (exponent: 0x003)
    Minimum RX AMPDU time spacing: 8 usec (0x06)
    HT TX/RX MCS rate indexes supported: 0-23
    Bitrates (non-HT):
      * 6.0 Mbps
      * 9.0 Mbps
      * 12.0 Mbps
      * 18.0 Mbps
      * 24.0 Mbps
      * 36.0 Mbps
      * 48.0 Mbps
      * 54.0 Mbps
    Frequencies:
      * 5180 MHz [36] (17.0 dBm)
      * 5200 MHz [40] (17.0 dBm)
      * 5220 MHz [44] (17.0 dBm)
      * 5240 MHz [48] (17.0 dBm)
      * 5260 MHz [52] (23.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5280 MHz [56] (23.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5300 MHz [60] (23.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5320 MHz [64] (23.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5500 MHz [100] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5520 MHz [104] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5540 MHz [108] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5560 MHz [112] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5580 MHz [116] (20.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5600 MHz [120] (disabled)
      * 5620 MHz [124] (disabled)
      * 5640 MHz [128] (disabled)
      * 5660 MHz [132] (30.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5680 MHz [136] (30.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5700 MHz [140] (30.0 dBm) (no IR, radar detection)
        DFS state: usable (for 7965 sec)
        DFS CAC time: 60000 ms
      * 5745 MHz [149] (30.0 dBm)
      * 5765 MHz [153] (30.0 dBm)
      * 5785 MHz [157] (30.0 dBm)
      * 5805 MHz [161] (30.0 dBm)
      * 5825 MHz [165] (30.0 dBm)
  Supported commands:
     * new_interface
     * set_interface
     * new_key
     * start_ap
     * new_station
     * set_bss
     * authenticate
     * associate
     * deauthenticate
     * disassociate
     * join_ibss
     * remain_on_channel
     * set_tx_bitrate_mask
     * frame
     * frame_wait_cancel
     * set_wiphy_netns
     * set_channel
     * set_wds_peer
     * tdls_mgmt
     * tdls_oper
     * probe_client
     * set_noack_map
     * register_beacons
     * start_p2p_device
     * set_mcast_rate
     * channel_switch
     * Unknown command (104)
     * connect
     * disconnect
  Supported TX frame types:
     * IBSS: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * managed: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * AP: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * AP/VLAN: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * mesh point: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * P2P-client: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * P2P-GO: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
     * P2P-device: 0x00 0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0 0xf0
  Supported RX frame types:
     * IBSS: 0x40 0xb0 0xc0 0xd0
     * managed: 0x40 0xd0
     * AP: 0x00 0x20 0x40 0xa0 0xb0 0xc0 0xd0
     * AP/VLAN: 0x00 0x20 0x40 0xa0 0xb0 0xc0 0xd0
     * mesh point: 0xb0 0xc0 0xd0
     * P2P-client: 0x40 0xd0
     * P2P-GO: 0x00 0x20 0x40 0xa0 0xb0 0xc0 0xd0
     * P2P-device: 0x40 0xd0
  software interface modes (can always be added):
     * AP/VLAN
     * monitor
  valid interface combinations:
     * #{ managed } <= 2048, #{ AP } <= 8, #{ P2P-client, P2P-GO } <= 1,
       total <= 2048, #channels <= 1, STA/AP BI must match
     * #{ WDS } <= 2048,
       total <= 2048, #channels <= 1, STA/AP BI must match
     * #{ IBSS, AP } <= 1,
       total <= 1, #channels <= 1, STA/AP BI must match, radar detect widths: { 20 MHz (no HT), 20 MHz }

  HT Capability overrides:
     * MCS: ff ff ff ff ff ff ff ff ff ff
     * maximum A-MSDU length
     * supported channel width
     * short GI for 40 MHz
     * max A-MPDU length exponent
     * min MPDU start spacing
  Device supports TX status socket option.
  Device supports HT-IBSS.
  Device supports SAE with AUTHENTICATE command
  Device supports low priority scan.
  Device supports scan flush.
  Device supports AP scan.
  Device supports per-vif TX power setting
  P2P GO supports CT window setting
  Driver supports a userspace MPM
  Device supports active monitor (which will ACK incoming frames)
  Driver/device bandwidth changes during BSS lifetime (AP/GO mode)
"""
DEV_OUTPUT = """phy#1
  Interface wlan1_portal
    ifindex 11
    wdev 0x100000002
    addr 8a:dc:96:0c:8d:bb
    type managed
  Interface wlan1
    ifindex 9
    wdev 0x100000001
    addr 88:dc:96:0c:8d:bb
    type AP
phy#0
  Interface wcli0
    ifindex 20
    wdev 0x3
    addr 88:dc:96:08:60:2d
    type managed
  Interface wlan0_portal
    ifindex 10
    wdev 0x2
    addr 8a:dc:96:08:60:2c
    type managed
  Interface wlan0
    ifindex 5
    wdev 0x1
    addr 88:dc:96:08:60:2c
    type AP
"""
INTERFACE_INFO_OUTPUT = """Interface wcli0
  ifindex 20
  wdev 0x3
  addr 88:dc:96:08:60:2d
  type managed
  wiphy 0
  channel 6 (2437 MHz), width: 20 MHz, center1: 2437 MHz
"""
INTERFACE_LINK_OUTPUT = """SSID: some_ssid
  freq: 2437
  RX: 56110 bytes (537 packets)
  TX: 926 bytes (10 packets)
  signal: -36 dBm
  tx bitrate: 43.3 MBit/s MCS 4 short GI

  bss flags:  short-preamble short-slot-time
  dtim period:  2
  beacon int: 100
"""


# Monkey-patch the stuff that actually depends on IW being available.
iw.RUNNABLE_IW = lambda: True


# pylint: disable=unused-argument,protected-access
def fake_phy(*args, **kwargs):
  return PHY_OUTPUT
iw._phy = fake_phy


# pylint: disable=unused-argument,protected-access
def fake_dev(*args, **kwargs):
  return DEV_OUTPUT
iw._dev = fake_dev


# pylint: disable=unused-argument,protected-access
def fake_info(*args, **kwargs):
  return INTERFACE_INFO_OUTPUT
iw._info = fake_info


# pylint: disable=unused-argument,protected-access
def fake_link(*args, **kwargs):
  return INTERFACE_LINK_OUTPUT
iw._link = fake_link


@wvtest.wvtest
def find_phy_test():
  wvtest.WVPASSEQ('phy0', iw.find_phy('2.4', 'auto'))
  wvtest.WVPASSEQ('phy0', iw.find_phy('2.4', '11'))
  wvtest.WVPASSEQ('phy1', iw.find_phy('5', 'auto'))
  wvtest.WVPASSEQ('phy1', iw.find_phy('5', '165'))
  wvtest.WVPASSEQ(None, iw.find_phy('asdf', '544312'))


@wvtest.wvtest
def find_interface_from_phy_test():
  wvtest.WVPASSEQ('wlan0',
                  iw.find_interface_from_phy('phy0', iw.INTERFACE_TYPE.ap, ''))
  wvtest.WVPASSEQ('wlan1',
                  iw.find_interface_from_phy('phy1', iw.INTERFACE_TYPE.ap, ''))
  wvtest.WVPASSEQ('wcli0',
                  iw.find_interface_from_phy('phy0', iw.INTERFACE_TYPE.client,
                                             ''))
  # The output is from a device with no client interface on phy1.
  wvtest.WVPASSEQ(None,
                  iw.find_interface_from_phy('phy1', iw.INTERFACE_TYPE.client,
                                             ''))


@wvtest.wvtest
def find_all_interfaces_from_phy_test():
  wvtest.WVPASSEQ(set(['wlan0', 'wlan0_portal', 'wcli0']),
                  iw.find_all_interfaces_from_phy('phy0'))
  wvtest.WVPASSEQ(set(['wlan0', 'wlan0_portal']),
                  iw.find_all_interfaces_from_phy('phy0', iw.INTERFACE_TYPE.ap))
  wvtest.WVPASSEQ(set(['wcli0']),
                  iw.find_all_interfaces_from_phy('phy0',
                                                  iw.INTERFACE_TYPE.client))
  wvtest.WVPASSEQ(set(['wlan1', 'wlan1_portal']),
                  iw.find_all_interfaces_from_phy('phy1'))


@wvtest.wvtest
def find_interface_from_band_test():
  wvtest.WVPASSEQ('wlan0',
                  iw.find_interface_from_band('2.4', iw.INTERFACE_TYPE.ap, ''))
  wvtest.WVPASSEQ('wlan1',
                  iw.find_interface_from_band('5', iw.INTERFACE_TYPE.ap, ''))
  wvtest.WVPASSEQ('wcli0',
                  iw.find_interface_from_band('2.4', iw.INTERFACE_TYPE.client,
                                              ''))
  # The output is from a device with no client interface on phy1.
  wvtest.WVPASSEQ(None,
                  iw.find_interface_from_band('5', iw.INTERFACE_TYPE.client,
                                              ''))


@wvtest.wvtest
def find_all_interfaces_from_band_test():
  wvtest.WVPASSEQ(set(['wlan0', 'wlan0_portal', 'wcli0']),
                  iw.find_all_interfaces_from_band('2.4'))
  wvtest.WVPASSEQ(set(['wlan0', 'wlan0_portal']),
                  iw.find_all_interfaces_from_band('2.4', iw.INTERFACE_TYPE.ap))
  wvtest.WVPASSEQ(set(['wcli0']),
                  iw.find_all_interfaces_from_band('2.4',
                                                   iw.INTERFACE_TYPE.client))
  wvtest.WVPASSEQ(set(['wlan1', 'wlan1_portal']),
                  iw.find_all_interfaces_from_band('5'))


@wvtest.wvtest
def find_interfaces_from_band_and_suffix_test():
  """Test find_interfaces_from_band_and_suffix."""
  wvtest.WVPASSEQ(set(['wlan0', 'wlan0_portal', 'wcli0']),
                  iw.find_interfaces_from_band_and_suffix('2.4', 'ALL'))
  wvtest.WVPASSEQ(set(['wlan0', 'wcli0']),
                  iw.find_interfaces_from_band_and_suffix('2.4', ''))
  wvtest.WVPASSEQ(set(['wlan0_portal']),
                  iw.find_interfaces_from_band_and_suffix('2.4', '_portal'))
  wvtest.WVPASSEQ(set([]),
                  iw.find_interfaces_from_band_and_suffix('2.4', 'fake_suffix'))

  wvtest.WVPASSEQ(set(['wlan0', 'wlan0_portal']),
                  iw.find_interfaces_from_band_and_suffix('2.4', 'ALL',
                                                          iw.INTERFACE_TYPE.ap))
  wvtest.WVPASSEQ(set(['wcli0']),
                  iw.find_interfaces_from_band_and_suffix(
                      '2.4', 'ALL', iw.INTERFACE_TYPE.client))
  wvtest.WVPASSEQ(set(['wlan1', 'wlan1_portal']),
                  iw.find_interfaces_from_band_and_suffix('5', 'ALL'))


@wvtest.wvtest
def info_parsed_test():
  wvtest.WVPASSEQ({
      'wdev': '0x3',
      'wiphy': '0',
      'addr': '88:dc:96:08:60:2d',
      'width': '20',
      'ifindex': '20',
      'type': 'managed',
      'channel': '6'
  }, iw.info_parsed('wcli0'))


@wvtest.wvtest
def find_width_and_channel_test():
  wvtest.WVPASSEQ(('20', '6'), iw.find_width_and_channel('wcli0'))

  # Now, test the 'iw link' fallback.
  global INTERFACE_INFO_OUTPUT
  hold = INTERFACE_INFO_OUTPUT
  INTERFACE_INFO_OUTPUT = ''
  wvtest.WVPASSEQ(('20', '6'), iw.find_width_and_channel('wcli0'))
  INTERFACE_INFO_OUTPUT = hold


@wvtest.wvtest
def phy_bands_test():
  # phy0 claims to support 5 GHz, but phy1 only supports 5 GHz and so it
  # supercedes it.
  wvtest.WVPASSEQ(set(['2.4']), iw.phy_bands('phy0'))
  wvtest.WVPASSEQ(set(['5']), iw.phy_bands('phy1'))

  # Now remove phy1 from the 'iw phy' output and see that phy0 gets both bands.
  global PHY_OUTPUT
  hold = PHY_OUTPUT
  PHY_OUTPUT = PHY_OUTPUT[PHY_OUTPUT.find('Wiphy phy0'):]
  wvtest.WVPASSEQ(set(['2.4', '5']), iw.phy_bands('phy0'))
  PHY_OUTPUT = hold


if __name__ == '__main__':
  wvtest.wvtest_main()
