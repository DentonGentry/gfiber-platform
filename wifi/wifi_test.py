#!/usr/bin/python -S

"""Tests for wifi.py."""

import options
import utils
import wifi

from wvtest import wvtest


@wvtest.wvtest
def stop_ap_wifi_dies_with_invalid_band():
  opts = options.OptDict({})
  opts.band = '42'
  opts.interface_suffix = ''
  wvtest.WVEXCEPT(utils.BinWifiException, wifi.stop_ap_wifi, opts)


if __name__ == '__main__':
  wvtest.wvtest_main()
