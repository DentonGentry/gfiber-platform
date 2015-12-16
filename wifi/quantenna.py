#!/usr/bin/python

"""Wifi commands for Quantenna using QCSAPI."""

import os
import subprocess
import time

import utils


def _get_qcsapi():
  """Detect Quantenna device."""
  if not hasattr(_get_qcsapi, 'qcsapi'):
    _get_qcsapi.qcsapi = None
    if (utils.subprocess_quiet(['runnable', 'qcsapi_pcie_static']) == 0 and
        utils.subprocess_quiet(['modinfo', 'qdpc-host'], no_stdout=True) == 0):
      # qcsapi_pcie_static runs on PCIe hosts, e.g. GFRG250. qdpc-host is only
      # loaded if a Quantenna device is present.
      _get_qcsapi.qcsapi = 'qcsapi_pcie_static'
    elif utils.subprocess_quiet(['runnable', 'call_qcsapi']) == 0:
      # call_qcsapi runs on the LHOST, e.g. GFEX250.
      _get_qcsapi.qcsapi = 'call_qcsapi'
  return _get_qcsapi.qcsapi


def _qcsapi(*args):
  return subprocess.check_output([_get_qcsapi()] + list(args))


def _set(mode, opt):
  """Enable wifi."""
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

  mac = subprocess.check_output(['hnvram', '-rq', 'MAC_ADDR_WIFI2']).strip()
  _qcsapi('set_mac_addr', 'wifi0', mac)

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
    _qcsapi('set_ssid', 'wifi0', opt.ssid)
    _qcsapi('set_passphrase', 'wifi0', '0', os.environ['WIFI_PSK'])
    _qcsapi('set_option', 'wifi0', 'ssid_broadcast',
            '0' if opt.hidden_mode else '1')
    _qcsapi('rfenable', '1')
  else:
    _qcsapi('create_ssid', 'wifi0', opt.ssid)
    _qcsapi('ssid_set_passphrase', 'wifi0', opt.ssid, '0',
            os.environ['WIFI_CLIENT_PSK'])
    # In STA mode, 'rfenable 1' is already done by 'startprod'/'reload_in_mode'.
    # 'apply_security_config' must be called instead.
    _qcsapi('apply_security_config', 'wifi0')

  return True


def _stop(_):
  """Disable wifi."""
  _qcsapi('rfenable', '0')
  return True


def has_quantenna():
  return _get_qcsapi() is not None


def set_wifi(opt):
  return _set('ap', opt)


def set_client_wifi(opt):
  return _set('sta', opt)


def stop_ap_wifi(opt):
  return _stop(opt)


def stop_client_wifi(opt):
  return _stop(opt)
