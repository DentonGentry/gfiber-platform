#!/usr/bin/python

"""Tests for wifi.py."""

import iw_test
import wifi

from wvtest import wvtest


@wvtest.wvtest
def client_interface_name_test():
  wvtest.WVPASSEQ('wcli0', wifi.client_interface_name('phy0', ''))

  try:
    hold = iw_test.DEV_OUTPUT
    iw_test.DEV_OUTPUT = """phy#1
      Interface wlan0
        ifindex 5
        wdev 0x1
        addr 88:dc:96:08:60:2c
        type AP
    """
    wvtest.WVPASSEQ('wcli0', wifi.client_interface_name('phy1', ''))
  finally:
    iw_test.DEV_OUTPUT = hold


if __name__ == '__main__':
  wvtest.wvtest_main()
