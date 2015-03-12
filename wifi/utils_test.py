#!/usr/bin/python -S

"""Tests for utils.py."""

import collections
import os

import utils
from wvtest import wvtest


_VALIDATION_PASS = [
    {},
    {'band': '5'},
    {'width': '40'},
    {'autotype': 'ANY'},
    {'protocols': ['a', 'b', 'ac']},
    {'encryption': 'NONE'},
]

_VALIDATION_FAIL = [
    # Invalid bands
    {'band': 2.4},
    {'band': '2.5'},
    {'band': ''},
    # Specific invalid combinations
    {'band': '2.4', 'width': '80'},
    {'band': '2.4', 'autotype': 'DFS'},
    {'band': '5', 'autotype': 'OVERLAP'},
    # Invalid protocols
    {'protocols': set('abc')},
    {'protocols': set()},
    # Invalid width
    {'width': '25'},
    # Invalid width/protocols
    {'width': '40', 'protocols': set('abg')},
    {'width': '80', 'protocols': set('abgn')},
]


_DEFAULTS = collections.OrderedDict((('band', '2.4'), ('width', '20'),
                                     ('autotype', 'NONDFS'),
                                     ('protocols', ('a', 'b', 'g', 'n', 'ac')),
                                     ('encryption', 'WPA2_PSK_AES')))


def modify_defaults(**kwargs):
  result = collections.OrderedDict(_DEFAULTS)
  result.update(kwargs)
  return result


# pylint: disable=protected-access
@wvtest.wvtest
def validate_set_options_test():
  """Tests utils.validate_set_options."""
  os.environ['WIFI_PSK'] = 'NOT_USED'

  for case in _VALIDATION_PASS:
    try:
      utils.validate_set_wifi_options(*modify_defaults(**case).values())
    except utils.BinWifiException:
      wvtest.WVFAIL('Test failed')

  for case in _VALIDATION_FAIL:
    wvtest.WVEXCEPT(
        utils.BinWifiException, utils.validate_set_wifi_options,
        *modify_defaults(**case).values())

  # Test failure when WIFI_PSK is missing
  del os.environ['WIFI_PSK']
  wvtest.WVEXCEPT(
      utils.BinWifiException, utils.validate_set_wifi_options,
      *_DEFAULTS.values())
  wvtest.WVEXCEPT(
      utils.BinWifiException, utils.validate_set_wifi_options,
      *modify_defaults(encryption='WEP').values())


@wvtest.wvtest
def increment_mac_address_test():
  wvtest.WVPASSEQ('12:34:56:78:91',
                  utils.increment_mac_address('12:34:56:78:90'))
  wvtest.WVPASSEQ('12:34:56:79:00',
                  utils.increment_mac_address('12:34:56:78:FF'))


if __name__ == '__main__':
  wvtest.wvtest_main()
