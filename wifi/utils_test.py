#!/usr/bin/python -S

"""Tests for utils.py."""

import collections
import multiprocessing
import os
import shutil
import tempfile
import time

import utils
from wvtest import wvtest


_VALIDATION_PASS = (
    {},
    {'band': '5'},
    {'width': '40'},
    {'autotype': 'ANY'},
    {'protocols': ('a', 'b', 'ac')},
    {'encryption': 'NONE'},
)

_VALIDATION_FAIL = (
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
)


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
      wvtest.WVFAIL('Test failed.')

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


@wvtest.wvtest
def sanitize_ssid_test():
  """Tests utils.sanitize_ssid."""
  wvtest.WVPASSEQ('foo', utils.sanitize_ssid('foo'))
  wvtest.WVPASSEQ('foo', utils.sanitize_ssid('foo\n\0'))
  hebrew = ('\xd7\xa0\xd6\xb0\xd7\xa7\xd6\xbb\xd7\x93\xd6\xbc\xd7\x95\xd6\xb9'
            '\xd7\xaa')
  unicode_control_char = u'\u200e'.encode('utf-8')
  non_utf8 = '\x97'
  wvtest.WVPASSEQ(hebrew,
                  utils.sanitize_ssid(''.join((hebrew, unicode_control_char,
                                               non_utf8))))


@wvtest.wvtest
def validate_and_sanitize_psk_test():
  """Tests utils.validate_and_sanitize_psk."""
  # Too short.
  wvtest.WVEXCEPT(utils.BinWifiException,
                  utils.validate_and_sanitize_psk, 'foo')
  # Too long.
  wvtest.WVEXCEPT(utils.BinWifiException,
                  utils.validate_and_sanitize_psk, '0' * 65)
  # Not ASCII.
  wvtest.WVEXCEPT(utils.BinWifiException,
                  utils.validate_and_sanitize_psk, 'abcdefgh\xd7\xa0')
  # Not hex.
  wvtest.WVEXCEPT(utils.BinWifiException,
                  utils.validate_and_sanitize_psk, 'g' * 64)
  # Too short after control characters removed.
  wvtest.WVEXCEPT(utils.BinWifiException,
                  utils.validate_and_sanitize_psk, 'foobar\n\0')

  wvtest.WVPASSEQ('foobarba', utils.validate_and_sanitize_psk('foobarba\n\0'))
  wvtest.WVPASSEQ('0' * 64, utils.validate_and_sanitize_psk('0' * 64))
  wvtest.WVPASSEQ('g' * 63, utils.validate_and_sanitize_psk('g' * 63))


@wvtest.wvtest
def lock_and_unlock_test():
  """Test utils.lock and utils.unlock."""
  try:
    temp_dir = tempfile.mkdtemp()
    lockfile = open(os.path.join(temp_dir, 'test.lock'), 'w')

    def lock_until_queue_nonempty(q, timeout_sec):
      try:
        utils.lock(lockfile, timeout_sec)
      except utils.BinWifiException:
        q.put('timed out')
        return
      q.get()
      q.put('releasing')
      utils.unlock(lockfile)

    q1 = multiprocessing.Queue()
    p1 = multiprocessing.Process(target=lock_until_queue_nonempty, args=(q1, 1))

    q2 = multiprocessing.Queue()
    p2 = multiprocessing.Process(target=lock_until_queue_nonempty, args=(q2, 3))

    p1.start()
    p2.start()

    wvtest.WVPASS(q1.empty())
    wvtest.WVPASS(q2.empty())

    q1.put('release')
    # Wait for p1 to take this off the queue and relase the lock.
    time.sleep(0.5)
    wvtest.WVFAIL(q1.empty())
    wvtest.WVPASSEQ(q1.get(), 'releasing')
    wvtest.WVPASS(q2.empty())

    q2.put('release')
    # Wait for p2 to take this off the queue and relase the lock.
    time.sleep(0.5)
    wvtest.WVPASS(q1.empty())
    wvtest.WVFAIL(q2.empty())
    wvtest.WVPASSEQ(q2.get(), 'releasing')

    # Now test that the timeout works.
    q3 = multiprocessing.Queue()
    p3 = multiprocessing.Process(target=lock_until_queue_nonempty, args=(q3, 1))

    q4 = multiprocessing.Queue()
    p4 = multiprocessing.Process(target=lock_until_queue_nonempty, args=(q4, 1))

    p3.start()
    p4.start()
    wvtest.WVPASSEQ(q4.get(), 'timed out')

    q3.put('release')

  finally:
    for p in [p1, p2, p3, p4]:
      p.join()
    shutil.rmtree(temp_dir)


if __name__ == '__main__':
  wvtest.wvtest_main()
