#!/usr/bin/python -S

"""Tests for quantenna.py."""

import quantenna
import utils
from wvtest import wvtest


@wvtest.wvtest
def get_external_mac_test():
  old_get_mac_address_for_interface = utils.get_mac_address_for_interface
  utils.get_mac_address_for_interface = lambda _: '02:00:00:00:00:00'
  wvtest.WVPASSEQ(quantenna._get_external_mac('wlan0'), '00:00:00:00:00:00')
  utils.get_mac_address_for_interface = old_get_mac_address_for_interface


@wvtest.wvtest
def get_interface_test():
  old_get_quantenna_interfaces = quantenna._get_quantenna_interfaces
  old_get_external_mac = quantenna._get_external_mac
  quantenna._get_quantenna_interfaces = lambda: 'wlan0 3\nwlan0_portal 4\n'
  quantenna._get_external_mac = lambda _: '00:00:00:00:00:00'
  wvtest.WVPASSEQ(quantenna._get_interface('ap', ''),
                  ('wlan0', 'wifi1', '00:00:00:00:00:00', 3))
  wvtest.WVPASSEQ(quantenna._get_interface('ap', '_portal'),
                  ('wlan0_portal', 'wifi2', '00:00:00:00:00:00', 4))
  wvtest.WVPASSEQ(quantenna._get_interface('sta', ''),
                  (None, None, None, None))
  quantenna._get_external_mac = old_get_external_mac
  quantenna._get_quantenna_interfaces = old_get_quantenna_interfaces


@wvtest.wvtest
def parse_scan_result_test():
  result = ('  " ssid with "quotes" " 00:11:22:33:44:55 40 25 0 0 0 0 0 1 40 '
            '100 1 Infrastructure')
  wvtest.WVPASSEQ(quantenna._parse_scan_result(result),
                  (' ssid with "quotes" ', '00:11:22:33:44:55', 40, -25, 0, 0))


if __name__ == '__main__':
  wvtest.wvtest_main()
