#!/usr/bin/python -S

"""Tests for wifi.py."""

import wifi

from wvtest import wvtest


@wvtest.wvtest
def a_placeholder_because_all_we_really_care_is_that_import_wifi_works():
  wvtest.WVPASS(wifi)


if __name__ == '__main__':
  wvtest.wvtest_main()
