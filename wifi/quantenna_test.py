#!/usr/bin/python -S

"""Tests for quantenna.py."""

import os
from configs_test import FakeOptDict
import quantenna
from wvtest import wvtest


calls = []


def fake_qcsapi(*args):
  calls.append(list(args))
  if args[0] == 'is_startprod_done':
    return '1\n' if ['startprod', 'wifi0'] in calls else '0\n'


def set_fakes(interface='wlan1', qcsapi='qcsapi_pcie_static'):
  del calls[:]
  os.environ['WIFI_PSK'] = 'wifi_psk'
  os.environ['WIFI_CLIENT_PSK'] = 'wifi_client_psk'
  quantenna._get_interface = lambda: interface
  quantenna._get_qcsapi = lambda: qcsapi
  quantenna._get_mac_address = lambda: '00:11:22:33:44:55'
  quantenna._qcsapi = fake_qcsapi


def matching_calls_indices(accept):
  return [i for i, c in enumerate(calls) if c[0] in accept]


@wvtest.wvtest
def not_5ghz_test():
  opt = FakeOptDict()
  opt.band = '2.4'
  set_fakes()
  wvtest.WVFAIL(quantenna.set_wifi(opt))
  wvtest.WVFAIL(quantenna.set_client_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_ap_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_client_wifi(opt))
  wvtest.WVPASSEQ(calls, [])


@wvtest.wvtest
def not_quantenna_test():
  opt = FakeOptDict()
  opt.band = '5'
  set_fakes(interface='')
  wvtest.WVFAIL(quantenna.set_wifi(opt))
  wvtest.WVFAIL(quantenna.set_client_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_ap_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_client_wifi(opt))
  wvtest.WVPASSEQ(calls, [])
  set_fakes(qcsapi='')
  wvtest.WVFAIL(quantenna.set_wifi(opt))
  wvtest.WVFAIL(quantenna.set_client_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_ap_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_client_wifi(opt))
  wvtest.WVPASSEQ(calls, [])
  set_fakes(interface='', qcsapi='')
  wvtest.WVFAIL(quantenna.set_wifi(opt))
  wvtest.WVFAIL(quantenna.set_client_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_ap_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_client_wifi(opt))
  wvtest.WVPASSEQ(calls, [])


@wvtest.wvtest
def set_wifi_test():
  opt = FakeOptDict()
  opt.band = '5'
  set_fakes()

  # Run set_wifi for the first time.
  wvtest.WVPASS(quantenna.set_wifi(opt))

  # 'rfenable 0' must be run first so that a live interface is not being
  # modified.
  wvtest.WVPASSEQ(calls[0], ['rfenable', '0'])

  # 'restore_default_config noreboot' must be run before any configuration so
  # that old configuration is cleared.
  wvtest.WVPASSEQ(calls[1], ['restore_default_config', 'noreboot'])

  # Check that 'reload_in_mode' is not run.
  wvtest.WVPASS(['reload_in_mode', 'wifi0'] not in calls)

  # Check that configs are written.
  wvtest.WVPASS(['update_config_param', 'wifi0', 'bw', '20'] in calls)
  wvtest.WVPASS(['update_config_param', 'wifi0', 'channel', '149'] in calls)
  wvtest.WVPASS(['update_config_param', 'wifi0', 'mode', 'ap'] in calls)
  wvtest.WVPASS(['update_config_param', 'wifi0', 'pmf', '0'] in calls)
  wvtest.WVPASS(['update_config_param', 'wifi0', 'scs', '0'] in calls)
  wvtest.WVPASS(['set_mac_addr', 'wifi0', '00:11:22:33:44:55'] in calls)
  wvtest.WVPASS(['set_ssid', 'wifi0', 'TEST_SSID'] in calls)
  wvtest.WVPASS(['set_passphrase', 'wifi0', '0', 'wifi_psk'] in calls)
  wvtest.WVPASS(['set_option', 'wifi0', 'ssid_broadcast', '1'] in calls)

  # 'update_config_param' and 'set_mac_addr' must be run before 'startprod',
  # since 'startprod' runs scripts that read these configs.
  sp = calls.index(['startprod', 'wifi0'])
  i = matching_calls_indices(['update_config_param', 'set_mac_addr'])
  wvtest.WVPASSLT(i[-1], sp)

  # 'startprod' must be followed by 'is_startprod_done'.
  wvtest.WVPASSEQ(calls[sp + 1], ['is_startprod_done'])

  # Other configs must be written after 'startprod' and before 'rfenable 1'.
  i = matching_calls_indices(['set_ssid', 'set_passphrase', 'set_option'])
  wvtest.WVPASSLT(sp, i[0])
  wvtest.WVPASSLT(i[-1], calls.index(['rfenable', '1']))

  # Run set_wifi again in client mode with new options.
  opt.channel = '147'
  opt.ssid = 'TEST_SSID2'
  opt.width = '80'
  new_calls_start = len(calls)
  wvtest.WVPASS(quantenna.set_client_wifi(opt))

  # Clear old calls.
  del calls[:new_calls_start]

  # 'rfenable 0' must be run first so that a live interface is not being
  # modified.
  wvtest.WVPASSEQ(calls[0], ['rfenable', '0'])

  # 'restore_default_config noreboot' must be run before any configuration so
  # that old configuration is cleared.
  wvtest.WVPASSEQ(calls[1], ['restore_default_config', 'noreboot'])

  # Check that 'startprod' is not run.
  wvtest.WVPASS(['startprod', 'wifi0'] not in calls)

  # Check that configs are written.
  wvtest.WVPASS(['update_config_param', 'wifi0', 'bw', '80'] in calls)
  wvtest.WVPASS(['update_config_param', 'wifi0', 'channel', '147'] in calls)
  wvtest.WVPASS(['update_config_param', 'wifi0', 'mode', 'sta'] in calls)
  wvtest.WVPASS(['update_config_param', 'wifi0', 'pmf', '0'] in calls)
  wvtest.WVPASS(['update_config_param', 'wifi0', 'scs', '0'] in calls)
  wvtest.WVPASS(['set_mac_addr', 'wifi0', '00:11:22:33:44:55'] in calls)
  wvtest.WVPASS(['create_ssid', 'wifi0', 'TEST_SSID2'] in calls)
  wvtest.WVPASS(['ssid_set_passphrase', 'wifi0', 'TEST_SSID2', '0',
                 'wifi_client_psk'] in calls)

  # 'update_config_param' and 'set_mac_addr' must be run before
  # 'reload_in_mode', since 'reload_in_mode' runs scripts that read these
  # configs.
  rim = calls.index(['reload_in_mode', 'wifi0', 'sta'])
  i = matching_calls_indices(['update_config_param', 'set_mac_addr'])
  wvtest.WVPASSLT(i[-1], rim)

  # Other configs must be written after 'reload_in_mode' and before
  # 'apply_security_config'.
  i = matching_calls_indices(['create_ssid', 'ssid_set_passphrase'])
  wvtest.WVPASSLT(rim, i[0])
  wvtest.WVPASSLT(i[-1], calls.index(['apply_security_config', 'wifi0']))


if __name__ == '__main__':
  wvtest.wvtest_main()
