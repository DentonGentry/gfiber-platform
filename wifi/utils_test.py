#!/usr/bin/python -S

"""Tests for utils.py."""

import collections
import multiprocessing
import os
import shutil
import sys
import tempfile

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
  """Test increment_mac_address."""

  wvtest.WVPASSEQ('12:34:56:78:90:13',
                  utils.increment_mac_address('12:34:56:78:90:12'))
  wvtest.WVPASSEQ('12:34:56:78:91:00',
                  utils.increment_mac_address('12:34:56:78:90:FF'))

  # b/34050122
  wvtest.WVPASSEQ('00:0b:6b:ed:eb:ad',
                  utils.increment_mac_address('00:0b:6b:ed:eb:ac'))

  # b/34050122 (initial misunderstanding of bug, but still worth testing)
  wvtest.WVPASSEQ('00:00:00:00:00:00',
                  utils.increment_mac_address('ff:ff:ff:ff:ff:ff'))


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
def lock_test():
  """Test utils.lock and utils._lockfile_create_retries."""
  wvtest.WVPASSEQ(utils._lockfile_create_retries(0), 1)
  wvtest.WVPASSEQ(utils._lockfile_create_retries(4), 1)
  wvtest.WVPASSEQ(utils._lockfile_create_retries(5), 2)
  wvtest.WVPASSEQ(utils._lockfile_create_retries(14), 2)
  wvtest.WVPASSEQ(utils._lockfile_create_retries(15), 3)
  wvtest.WVPASSEQ(utils._lockfile_create_retries(60), 5)

  try:
    temp_dir = tempfile.mkdtemp()
    lockfile = os.path.join(temp_dir, 'lock_and_unlock_test')

    def lock_until_qi_nonempty(qi, qo, timeout_sec):
      try:
        utils.lock(lockfile, timeout_sec)
      except utils.BinWifiException:
        qo.put('timed out')
        return
      qo.put('acquired')
      wvtest.WVPASSEQ(qi.get(), 'release')
      qo.put('released')
      sys.exit(0)

    # Use multiprocessing because we're using lockfile-create with --use-pid, so
    # we need separate PIDs.
    q1i = multiprocessing.Queue()
    q1o = multiprocessing.Queue()
    # The timeout here is 5 because occasionally it takes more than one second
    # to acquire the lock, causing the test to hang.  Five seconds is enough to
    # prevent this.
    p1 = multiprocessing.Process(target=lock_until_qi_nonempty,
                                 args=(q1i, q1o, 1))

    q2i = multiprocessing.Queue()
    q2o = multiprocessing.Queue()
    p2 = multiprocessing.Process(target=lock_until_qi_nonempty,
                                 args=(q2i, q2o, 10))

    p1.start()
    wvtest.WVPASSEQ(q1o.get(), 'acquired')

    p2.start()
    wvtest.WVPASS(q2o.empty())

    q1i.put('release')
    wvtest.WVPASSEQ(q1o.get(), 'released')
    p1.join()
    wvtest.WVPASSEQ(q2o.get(), 'acquired')

    q2i.put('release')
    wvtest.WVPASSEQ(q2o.get(), 'released')
    p2.join()

    # Now test that the timeout works.
    q3i = multiprocessing.Queue()
    q3o = multiprocessing.Queue()
    p3 = multiprocessing.Process(target=lock_until_qi_nonempty,
                                 args=(q3i, q3o, 1))

    q4i = multiprocessing.Queue()
    q4o = multiprocessing.Queue()
    p4 = multiprocessing.Process(target=lock_until_qi_nonempty,
                                 args=(q4i, q4o, 1))

    p3.start()
    wvtest.WVPASSEQ(q3o.get(), 'acquired')
    p4.start()
    wvtest.WVPASSEQ(q4o.get(), 'timed out')
    p4.join()

    q3i.put('release')
    p3.join()

  finally:
    shutil.rmtree(temp_dir)


if __name__ == '__main__':
  wvtest.wvtest_main()
