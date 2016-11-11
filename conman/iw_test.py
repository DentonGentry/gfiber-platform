#!/usr/bin/python

"""Tests for iw.py."""

import subprocess

import iw
import test_common
from wvtest import wvtest


SCAN_RESULTS = (
    {'rssi': -60, 'ssid': 'short scan result', 'bssid': '00:23:97:57:f4:d8',
     'vendor_ies': [('00:11:22', '01 23 45 67')], 'security': 'WEP'},
    {'rssi': -54, 'ssid': 'Google', 'bssid': '94:b4:0f:f1:02:a0',
     'vendor_ies': [], 'security': 'WPA2'},
    {'rssi': -39, 'ssid': 'Google', 'bssid': '94:b4:0f:f1:35:60',
     'vendor_ies': [], 'security': 'WPA2'},
    {'rssi': -38, 'ssid': 'GoogleGuest', 'bssid': '94:b4:0f:f1:35:61',
     'vendor_ies': [], 'security': None},
    {'rssi': -55, 'ssid': 'Google', 'bssid': '94:b4:0f:f1:3a:e0',
     'vendor_ies': [], 'security': 'WPA2'},
    {'rssi': -65, 'ssid': 'GoogleGuest', 'bssid': '94:b4:0f:f1:3a:e1',
     'vendor_ies': [], 'security': None},
    {'rssi': -67, 'ssid': 'GoogleGuest', 'bssid': '94:b4:0f:f1:36:41',
     'vendor_ies': [], 'security': None},
    {'rssi': -66, 'ssid': 'Google', 'bssid': '94:b4:0f:f1:36:40',
     'vendor_ies': [], 'security': 'WPA2'},
    {'rssi': -66, 'ssid': '', 'bssid': '94:b4:0f:f1:36:42',
     'vendor_ies': [('00:11:22', '01 23 45 67'), ('f4:f5:e8', '01'),
                    ('f4:f5:e8', '03 47 46 69 62 65 72 53 65 74 75 70 41 75 74 '
                     '6f 6d 61 74 69 6f 6e')], 'security': None},
    {'rssi': -66, 'ssid': '', 'bssid': '00:1a:11:f1:36:43',
     'vendor_ies': [], 'security': None},
)


@test_common.wvtest
def find_bssids_test():
  """Test iw.find_bssids."""
  subprocess.mock('wifi', 'interfaces',
                  subprocess.wifi.MockInterface(phynum='0', bands=['2.4', '5'],
                                                driver='cfg80211'))
  subprocess.call(['ifup', 'wcli0'])
  for scan_result in SCAN_RESULTS:
    subprocess.mock('wifi', 'remote_ap', band='5', **scan_result)

  test_ie = ('00:11:22', '01 23 45 67')
  provisioning_ie = ('f4:f5:e8', '01')
  ssid_ie = (
      'f4:f5:e8',
      '03 47 46 69 62 65 72 53 65 74 75 70 41 75 74 6f 6d 61 74 69 6f 6e',
  )
  short_scan_result = iw.BssInfo(ssid='short scan result',
                                 bssid='00:23:97:57:f4:d8',
                                 band='5',
                                 rssi=-60,
                                 security=['WEP'],
                                 vendor_ies=[test_ie])
  provisioning_bss_info = iw.BssInfo(ssid=iw.DEFAULT_GFIBERSETUP_SSID,
                                     bssid='94:b4:0f:f1:36:42',
                                     band='5',
                                     rssi=-66,
                                     vendor_ies=[test_ie, provisioning_ie,
                                                 ssid_ie])
  provisioning_bss_info_frenzy = iw.BssInfo(ssid=iw.DEFAULT_GFIBERSETUP_SSID,
                                            bssid='00:1a:11:f1:36:43',
                                            band='5',
                                            rssi=-66)

  wvtest.WVPASSEQ(
      set(iw.find_bssids('2.4', True)),
      set([(short_scan_result, 2.4),
           (provisioning_bss_info, 5.34),
           (provisioning_bss_info_frenzy, 4.34),
           (iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:36:41',
                       band='5', rssi=-67), 2.33),
           (iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:3a:e1',
                       band='5', rssi=-65), 2.35),
           (iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:35:61',
                       band='5', rssi=-38), 2.62),
           (iw.BssInfo(ssid='Google', bssid='94:b4:0f:f1:36:40', band='5',
                       rssi=-66, security=['WPA2']), 2.34),
           (iw.BssInfo(ssid='Google', bssid='94:b4:0f:f1:3a:e0', band='5',
                       rssi=-55, security=['WPA2']), 2.45),
           (iw.BssInfo(ssid='Google', bssid='94:b4:0f:f1:35:60', band='5',
                       rssi=-39, security=['WPA2']), 2.61),
           (iw.BssInfo(ssid='Google', bssid='94:b4:0f:f1:02:a0', band='5',
                       rssi=-54, security=['WPA2']), 2.46)]))

  wvtest.WVPASSEQ(
      set(iw.find_bssids('2.4', False)),
      set([(provisioning_bss_info, 5.34),
           (provisioning_bss_info_frenzy, 4.34),
           (iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:36:41', band='5',
                       rssi=-67), 2.33),
           (iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:3a:e1', band='5',
                       rssi=-65), 2.35),
           (iw.BssInfo(ssid='GoogleGuest', bssid='94:b4:0f:f1:35:61', band='5',
                       rssi=-38), 2.62)]))

if __name__ == '__main__':
  wvtest.wvtest_main()
