#!/usr/bin/python -S

"""Wifi commands for Quantenna using QCSAPI."""

import json
import os
import subprocess
import time

import utils


WIFIINFO_PATH = '/tmp/wifi/wifiinfo'


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
  return subprocess.check_output(['qcsapi'] + [str(x) for x in args]).strip()


def _brctl(*args):
  return subprocess.check_output(['brctl'] + list(args),
                                 stderr=subprocess.STDOUT).strip()


def _ifplugd_action(*args):
  return subprocess.check_output(['/etc/ifplugd/ifplugd.action'] + list(args),
                                 stderr=subprocess.STDOUT).strip()


def info_parsed(interface):
  """Fake version of iw.info_parsed."""
  wifiinfo_filename = os.path.join(WIFIINFO_PATH, interface)

  if not os.path.exists(wifiinfo_filename):
    return {}

  wifiinfo = json.load(open(wifiinfo_filename))
  return {'addr' if k == 'BSSID' else k.lower(): v
          for k, v in wifiinfo.iteritems()}


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

  if opt.encryption == 'WEP':
    raise utils.BinWifiException('WEP not supported')

  if mode == 'scan':
    mode = 'sta'
    scan = True
  else:
    scan = False

  _qcsapi('rfenable', 0)
  _qcsapi('restore_default_config', 'noreboot')

  config = {
      'bw': opt.width,
      'channel': 149 if opt.channel == 'auto' else opt.channel,
      'mode': mode,
      'pmf': 0,
      'scs': 0,
  }
  for param, value in config.iteritems():
    _qcsapi('update_config_param', 'wifi0', param, value)

  _qcsapi('set_mac_addr', 'wifi0', _get_mac_address(interface))

  if int(_qcsapi('is_startprod_done')):
    _qcsapi('reload_in_mode', 'wifi0', mode)
  else:
    _qcsapi('startprod')
    for _ in xrange(30):
      if int(_qcsapi('is_startprod_done')):
        break
      time.sleep(1)
    else:
      raise utils.BinWifiException('startprod timed out')

  if mode == 'ap':
    _set_interface_in_bridge(opt.bridge, interface, True)
    _qcsapi('set_ssid', 'wifi0', opt.ssid)
    if opt.encryption == 'NONE':
      _qcsapi('set_beacon_type', 'wifi0', 'Basic')
    else:
      protocol, authentication, encryption = opt.encryption.split('_')
      protocol = {'WPA': 'WPA', 'WPA2': '11i', 'WPA12': 'WPAand11i'}[protocol]
      authentication += 'Authentication'
      encryption += 'Encryption'
      _qcsapi('set_beacon_type', 'wifi0', protocol)
      _qcsapi('set_wpa_authentication_mode', 'wifi0', authentication)
      _qcsapi('set_wpa_encryption_modes', 'wifi0', encryption)
      _qcsapi('set_passphrase', 'wifi0', 0, os.environ['WIFI_PSK'])
    _qcsapi('set_option', 'wifi0', 'ssid_broadcast', int(not opt.hidden_mode))
    _qcsapi('rfenable', 1)
  elif mode == 'sta' and not scan:
    _set_interface_in_bridge(opt.bridge, interface, False)
    _qcsapi('create_ssid', 'wifi0', opt.ssid)
    if opt.bssid:
      _qcsapi('set_ssid_bssid', 'wifi0', opt.ssid, opt.bssid)
    if opt.encryption == 'NONE' or not os.environ.get('WIFI_CLIENT_PSK'):
      _qcsapi('ssid_set_authentication_mode', 'wifi0', opt.ssid, 'NONE')
    else:
      _qcsapi('ssid_set_passphrase', 'wifi0', opt.ssid, 0,
              os.environ['WIFI_CLIENT_PSK'])
    # In STA mode, 'rfenable 1' is already done by 'startprod'/'reload_in_mode'.
    # 'apply_security_config' must be called instead.
    _qcsapi('apply_security_config', 'wifi0')

    for _ in xrange(10):
      if _qcsapi('get_ssid', 'wifi0'):
        break
      time.sleep(1)
    else:
      raise utils.BinWifiException('wpa_supplicant failed to connect')

    try:
      _ifplugd_action(interface, 'up')
    except subprocess.CalledProcessError:
      utils.log('Failed to call ifplugd.action.  %s may not get an IP address.'
                % interface)

  return True


def _parse_scan_result(line):
  # Scan result format:
  #
  # "Quantenna1" 00:26:86:00:11:5f 60 56 1 2 1 2 0 15 80
  # |            |                 |  |  | | | | | |  |
  # |            |                 |  |  | | | | | |  Maximum bandwidth
  # |            |                 |  |  | | | | | WPS flags
  # |            |                 |  |  | | | | Qhop flags
  # |            |                 |  |  | | | Encryption modes
  # |            |                 |  |  | | Authentication modes
  # |            |                 |  |  | Security protocols
  # |            |                 |  |  Security enabled
  # |            |                 |  RSSI
  # |            |                 Channel
  # |            MAC
  # SSID
  #
  # The SSID may contain quotes and spaces. Split on whitespace from the right,
  # making at most 10 splits, to preserve spaces in the SSID.
  sp = line.strip().rsplit(None, 10)
  return sp[0][1:-1], sp[1], int(sp[2]), float(sp[3]), int(sp[4]), int(sp[5])


def set_wifi(opt):
  return _set('ap', opt)


def set_client_wifi(opt):
  return _set('sta', opt)


def stop_ap_wifi(_):
  """Disable AP."""
  if not _get_interface():
    return False

  if (int(_qcsapi('is_startprod_done')) and
      _qcsapi('get_mode', 'wifi0') == 'Access point'):
    _qcsapi('rfenable', 0)

  return True


def stop_client_wifi(_):
  """Disable client."""
  if not _get_interface():
    return False

  if (int(_qcsapi('is_startprod_done')) and
      _qcsapi('get_mode', 'wifi0') == 'Station'):
    _qcsapi('rfenable', 0)

  return True


def scan_wifi(opt):
  """Scan for APs."""
  interface = _get_interface()
  if not interface:
    return False

  if _qcsapi('rfstatus') == 'Off':
    _set('scan', opt)

  _qcsapi('start_scan', 'wifi0')
  for _ in xrange(30):
    if not int(_qcsapi('get_scanstatus', 'wifi0')):
      break
    time.sleep(1)
  else:
    raise utils.BinWifiException('start_scan timed out')

  for i in xrange(int(_qcsapi('get_results_ap_scan', 'wifi0'))):
    ssid, mac, channel, rssi, flags, protocols = _parse_scan_result(
        _qcsapi('get_properties_ap', 'wifi0', i))
    print 'BSS %s(on %s)' % (mac, interface)
    print '\tfreq: %d' % (5000 + 5 * channel)
    print '\tsignal: %.2f' % -rssi
    print '\tSSID: %s' % ssid
    if flags & 0x1:
      if protocols & 0x1:
        print '\tWPA:'
      if protocols & 0x2:
        print '\tRSN:'

  return True
