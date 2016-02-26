#!/usr/bin/python -S

"""Wifi commands for Quantenna using QCSAPI."""

import os
import subprocess
import time

import utils


ALREADY_MEMBER_FMT = ('device %s is already a member of a bridge; '
                      "can't enslave it to bridge %s.")
NOT_MEMBER_FMT = 'device %s is not a slave of %s'


def _get_interface():
  return subprocess.check_output(['get-quantenna-interface']).strip()


def _get_mac_address(interface):
  try:
    var = {'wlan0': 'MAC_ADDR_WIFI', 'wlan1': 'MAC_ADDR_WIFI2'}[interface]
  except KeyError:
    raise utils.BinWifiException('no MAC address for %s in hnvram' % interface)
  return subprocess.check_output(['hnvram', '-rq', var]).strip()


def _qcsapi(*args):
  return subprocess.check_output(['qcsapi'] + list(args)).strip()


def _brctl(*args):
  return subprocess.check_output(['brctl'] + list(args),
                                 stderr=subprocess.STDOUT).strip()


def _set_interface_in_bridge(bridge, interface, want_in_bridge):
  """Add/remove Quantenna interface from/to the bridge."""
  if want_in_bridge:
    command = 'addif'
    error_fmt = ALREADY_MEMBER_FMT
  else:
    command = 'delif'
    error_fmt = NOT_MEMBER_FMT

  try:
    _brctl(command, bridge, interface)
  except subprocess.CalledProcessError as e:
    if error_fmt % (interface, bridge) not in e.output:
      raise utils.BinWifiException(e.output)


def _set(mode, opt):
  """Enable wifi."""
  interface = _get_interface()
  if not interface:
    return False

  _qcsapi('rfenable', '0')
  _qcsapi('restore_default_config', 'noreboot')

  config = {
      'bw': opt.width,
      'channel': '149' if opt.channel == 'auto' else opt.channel,
      'mode': mode,
      'pmf': '0',
      'scs': '0',
  }
  for param, value in config.iteritems():
    _qcsapi('update_config_param', 'wifi0', param, value)

  _qcsapi('set_mac_addr', 'wifi0', _get_mac_address(interface))

  if int(_qcsapi('is_startprod_done')):
    _qcsapi('reload_in_mode', 'wifi0', mode)
  else:
    _qcsapi('startprod', 'wifi0')
    for _ in xrange(30):
      if int(_qcsapi('is_startprod_done')):
        break
      time.sleep(1)
    else:
      raise utils.BinWifiException('startprod timed out')

  if mode == 'ap':
    _set_interface_in_bridge(opt.bridge, interface, True)
    _qcsapi('set_ssid', 'wifi0', opt.ssid)
    _qcsapi('set_passphrase', 'wifi0', '0', os.environ['WIFI_PSK'])
    _qcsapi('set_option', 'wifi0', 'ssid_broadcast',
            '0' if opt.hidden_mode else '1')
    _qcsapi('rfenable', '1')
  elif mode == 'sta':
    _set_interface_in_bridge(opt.bridge, interface, False)
    _qcsapi('create_ssid', 'wifi0', opt.ssid)
    _qcsapi('ssid_set_passphrase', 'wifi0', opt.ssid, '0',
            os.environ['WIFI_CLIENT_PSK'])
    # In STA mode, 'rfenable 1' is already done by 'startprod'/'reload_in_mode'.
    # 'apply_security_config' must be called instead.
    _qcsapi('apply_security_config', 'wifi0')

  return True


def _stop(_):
  """Disable wifi."""
  if not _get_interface():
    return False

  _qcsapi('rfenable', '0')
  return True


def set_wifi(opt):
  return _set('ap', opt)


def set_client_wifi(opt):
  return _set('sta', opt)


def stop_ap_wifi(opt):
  return _stop(opt)


def stop_client_wifi(opt):
  return _stop(opt)
