#!/usr/bin/python -S

"""Tests for quantenna.py."""

import json
import os
import shutil
from subprocess import CalledProcessError
import tempfile

from configs_test import FakeOptDict
import quantenna
from wvtest import wvtest


calls = []
ifplugd_action_calls = []


def fake_qcsapi(*args):
  calls.append(list(args))
  if args[0] == 'is_startprod_done':
    return '1' if ['startprod', 'wifi0'] in calls else '0'
  if args[0] == 'get_bssid':
    return '00:11:22:33:44:55'
  if args[0] == 'get_mode':
    i = [c for c in matching_calls_indices(['update_config_param'])
         if calls[c][2] == 'mode']
    return 'Access point' if calls[i[-1]][3] == 'ap' else 'Station'


bridge_interfaces = set()


def fake_brctl(*args):
  bridge = args[-2]
  wvtest.WVPASS(bridge == 'br0')
  interface = args[-1]
  if 'addif' in args:
    if interface in bridge_interfaces:
      raise CalledProcessError(
          returncode=1, cmd=['brctl'] + list(args),
          output=quantenna.ALREADY_MEMBER_FMT % (interface, bridge))
    bridge_interfaces.add(interface)
    return

  if 'delif' in args:
    if interface not in bridge_interfaces:
      raise CalledProcessError(
          returncode=1, cmd=['brctl'] + list(args),
          output=quantenna.NOT_MEMBER_FMT % (interface, bridge))
    bridge_interfaces.remove(interface)
    return


def set_fakes(interface='wlan1'):
  del calls[:]
  del ifplugd_action_calls[:]
  bridge_interfaces.clear()
  os.environ['WIFI_PSK'] = 'wifi_psk'
  os.environ['WIFI_CLIENT_PSK'] = 'wifi_client_psk'
  quantenna._get_interface = lambda: interface
  quantenna._get_mac_address = lambda _: '00:11:22:33:44:55'
  quantenna._qcsapi = fake_qcsapi
  quantenna._brctl = fake_brctl
  quantenna._ifplugd_action = lambda *args: ifplugd_action_calls.append(args)


def matching_calls_indices(accept):
  return [i for i, c in enumerate(calls) if c[0] in accept]


@wvtest.wvtest
def not_quantenna_test():
  opt = FakeOptDict()
  set_fakes(interface='')
  wvtest.WVFAIL(quantenna.set_wifi(opt))
  wvtest.WVFAIL(quantenna.set_client_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_ap_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_client_wifi(opt))
  wvtest.WVPASSEQ(calls, [])
  wvtest.WVFAIL(quantenna.set_wifi(opt))
  wvtest.WVFAIL(quantenna.set_client_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_ap_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_client_wifi(opt))
  wvtest.WVPASSEQ(calls, [])
  set_fakes(interface='')
  wvtest.WVFAIL(quantenna.set_wifi(opt))
  wvtest.WVFAIL(quantenna.set_client_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_ap_wifi(opt))
  wvtest.WVFAIL(quantenna.stop_client_wifi(opt))
  wvtest.WVPASSEQ(calls, [])


@wvtest.wvtest
def set_wifi_test():
  opt = FakeOptDict()
  opt.bridge = 'br0'
  set_fakes()

  # Run set_wifi for the first time.
  wvtest.WVPASS(quantenna.set_wifi(opt))
  wvtest.WVPASS('wlan1' in bridge_interfaces)

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

  # We shouldn't touch ifplugd in AP mode.
  wvtest.WVPASSEQ(len(ifplugd_action_calls), 0)

  # Run set_wifi again in client mode with new options.
  opt.channel = '147'
  opt.ssid = 'TEST_SSID2'
  opt.width = '80'
  new_calls_start = len(calls)
  wvtest.WVPASS(quantenna.set_client_wifi(opt))
  wvtest.WVFAIL('wlan1' in bridge_interfaces)

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

  # We should have called ipflugd.action after setclient.
  wvtest.WVPASSEQ(len(ifplugd_action_calls), 1)
  wvtest.WVPASSEQ(ifplugd_action_calls[0], ('wlan1', 'up'))

  # Make sure subsequent equivalent calls don't fail despite the redundant
  # bridge changes.
  wvtest.WVPASS(quantenna.set_client_wifi(opt))
  wvtest.WVPASS(quantenna.set_client_wifi(opt))
  wvtest.WVPASS(quantenna.set_wifi(opt))
  wvtest.WVPASS(quantenna.set_wifi(opt))


@wvtest.wvtest
def stop_wifi_test():
  opt = FakeOptDict()
  opt.bridge = 'br0'
  set_fakes()
  wvtest.WVPASS(quantenna.set_wifi(opt))
  new_calls_start = len(calls)
  wvtest.WVPASS(quantenna.stop_ap_wifi(opt))
  wvtest.WVPASS(['rfenable', '0'] in calls[new_calls_start:])
  new_calls_start = len(calls)
  wvtest.WVPASS(quantenna.stop_client_wifi(opt))
  wvtest.WVPASS(['rfenable', '0'] not in calls[new_calls_start:])


@wvtest.wvtest
def info_parsed_test():
  set_fakes()

  try:
    quantenna.WIFIINFO_PATH = tempfile.mkdtemp()
    json.dump({
        'Channel': '64',
        'SSID': 'my ssid',
        'BSSID': '00:00:00:00:00:00',
    }, open(os.path.join(quantenna.WIFIINFO_PATH, 'wlan0'), 'w'))

    wvtest.WVPASSEQ(quantenna.info_parsed('wlan0'), {
        'ssid': 'my ssid',
        'addr': '00:00:00:00:00:00',
        'channel': '64',
    })
  finally:
    shutil.rmtree(quantenna.WIFIINFO_PATH)

if __name__ == '__main__':
  wvtest.wvtest_main()
