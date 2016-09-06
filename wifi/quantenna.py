#!/usr/bin/python -S

"""Wifi commands for Quantenna using QCSAPI."""

import os
import re
import subprocess
import time

import utils


def _get_quantenna_interfaces():
  return subprocess.check_output(['get-quantenna-interfaces']).split()


def _qcsapi(*args):
  return subprocess.check_output(['qcsapi'] + [str(x) for x in args]).strip()


def _get_external_mac(hif):
  # The MAC of the LHOST interface is equal to the MAC of the host interface
  # with the locally administered bit cleared.
  mac = utils.get_mac_address_for_interface(hif)
  octets = mac.split(':')
  octets[0] = '%02x' % (int(octets[0], 16) & ~(1 << 1))
  return ':'.join(octets)


def _get_vlan(hif):
  m = re.search(r'VID: (\d+)', utils.read_or_empty('/proc/net/vlan/%s' % hif))
  if m:
    return int(m.group(1))
  raise utils.BinWifiException('no VLAN ID for interface %s' % hif)


def _get_interface(mode, suffix):
  # Each host interface (hif) maps to exactly one LHOST interface (lif) based on
  # the VLAN ID as follows: the lif is wifiX where X is the VLAN ID - 2 (VLAN
  # IDs start at 2). The client interface must map to wifi0, so it must have
  # VLAN ID 2.
  prefix = 'wlan' if mode == 'ap' else 'wcli'
  for hif in _get_quantenna_interfaces():
    if re.match(prefix + r'\d*' + suffix, hif):
      vlan = _get_vlan(hif)
      lif = 'wifi%d' % (vlan - 2)
      mac = _get_external_mac(hif)
      return hif, lif, mac, vlan
  return None, None, None, None


def _ifplugd_action(hif, state):
  subprocess.check_output(['/etc/ifplugd/ifplugd.action', hif, state])


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
  return sp[0][1:-1], sp[1], int(sp[2]), -float(sp[3]), int(sp[4]), int(sp[5])


def _ensure_initialized(mode):
  """Ensure that the device is in a state suitable for the given mode."""
  if int(_qcsapi('is_startprod_done')):
    if (mode == 'scan' or
        mode == 'ap' and _qcsapi('get_mode', 'wifi0') == 'Access point' or
        mode == 'sta' and _qcsapi('get_mode', 'wifi0') == 'Station'):
      return
    _qcsapi('restore_default_config', 'noreboot', mode)
    _qcsapi('reload_in_mode', 'wifi0', mode)
    _qcsapi('rfenable', 1)
  else:
    _qcsapi('restore_default_config', 'noreboot',
            'sta' if mode == 'scan' else mode)

    _, _, mac, _ = _get_interface('sta', '')
    if mac:
      _qcsapi('set_mac_addr', 'wifi0', mac)

    _qcsapi('startprod')
    for _ in xrange(30):
      if int(_qcsapi('is_startprod_done')):
        break
      time.sleep(1)
    else:
      raise utils.BinWifiException('startprod timed out')

    _qcsapi('rfenable', 1)


def set_wifi(opt):
  """Enable AP."""
  hif, lif, mac, vlan = _get_interface('ap', opt.interface_suffix)
  if not hif:
    return False

  if opt.encryption == 'WEP':
    raise utils.BinWifiException('WEP not supported')

  stop_ap_wifi(opt)

  try:
    _ensure_initialized('ap')

    _qcsapi('wifi_create_bss', lif, mac)
    _qcsapi('set_ssid', lif, opt.ssid)
    _qcsapi('set_bw', 'wifi0', opt.width)
    _qcsapi('set_channel', 'wifi0',
            149 if opt.channel == 'auto' else opt.channel)

    if opt.encryption == 'NONE':
      _qcsapi('set_beacon_type', lif, 'Basic')
    else:
      protocol, authentication, encryption = opt.encryption.split('_')
      protocol = {'WPA': 'WPA', 'WPA2': '11i', 'WPA12': 'WPAand11i'}[protocol]
      authentication += 'Authentication'
      encryption += 'Encryption'
      _qcsapi('set_beacon_type', lif, protocol)
      _qcsapi('set_wpa_authentication_mode', lif, authentication)
      _qcsapi('set_wpa_encryption_modes', lif, encryption)
      _qcsapi('set_passphrase', lif, 0, os.environ['WIFI_PSK'])
    _qcsapi('set_option', lif, 'ssid_broadcast', int(not opt.hidden_mode))

    _qcsapi('vlan_config', lif, 'enable')
    _qcsapi('vlan_config', lif, 'access', vlan)
    _qcsapi('vlan_config', 'pcie0', 'enable')
    _qcsapi('vlan_config', 'pcie0', 'trunk', vlan)

    _qcsapi('block_bss', lif, 0)
    _ifplugd_action(hif, 'up')
  except:
    stop_ap_wifi(opt)
    raise

  return True


def set_client_wifi(opt):
  """Enable client."""
  hif, lif, _, vlan = _get_interface('sta', opt.interface_suffix)
  if not hif:
    return False

  stop_client_wifi(opt)

  try:
    _ensure_initialized('sta')

    _qcsapi('create_ssid', lif, opt.ssid)
    _qcsapi('set_bw', 'wifi0', 80)

    if opt.bssid:
      _qcsapi('set_ssid_bssid', lif, opt.ssid, opt.bssid)
    if opt.encryption == 'NONE' or not os.environ.get('WIFI_CLIENT_PSK'):
      _qcsapi('ssid_set_authentication_mode', lif, opt.ssid, 'NONE')
    else:
      _qcsapi('ssid_set_passphrase', lif, opt.ssid, 0,
              os.environ['WIFI_CLIENT_PSK'])
    _qcsapi('apply_security_config', lif)

    for _ in xrange(10):
      if _qcsapi('get_ssid', lif):
        break
      time.sleep(1)
    else:
      raise utils.BinWifiException('wpa_supplicant failed to connect')

    _qcsapi('vlan_config', lif, 'enable')
    _qcsapi('vlan_config', lif, 'access', vlan)
    _qcsapi('vlan_config', 'pcie0', 'enable')
    _qcsapi('vlan_config', 'pcie0', 'trunk', vlan)

    _ifplugd_action(hif, 'up')
  except:
    stop_client_wifi(opt)
    raise

  return True


def stop_ap_wifi(opt):
  """Disable AP."""
  hif, lif, _, _ = _get_interface('ap', opt.interface_suffix)
  if not hif:
    return False

  try:
    _qcsapi('wifi_remove_bss', lif)
  except subprocess.CalledProcessError:
    pass

  _ifplugd_action(hif, 'down')

  return True


def stop_client_wifi(opt):
  """Disable client."""
  hif, lif, _, _ = _get_interface('sta', opt.interface_suffix)
  if not hif:
    return False

  try:
    _qcsapi('remove_ssid', lif, _qcsapi('get_ssid_list', lif, 1))
  except subprocess.CalledProcessError:
    pass

  _ifplugd_action(hif, 'down')

  return True


def scan_wifi(_):
  """Scan for APs."""
  hif, _, _, _ = _get_interface('ap', '')
  if not hif:
    return False

  _ensure_initialized('scan')

  _qcsapi('start_scan', 'wifi0')
  for _ in xrange(30):
    if not int(_qcsapi('get_scanstatus', 'wifi0')):
      break
    time.sleep(1)
  else:
    raise utils.BinWifiException('scan timed out')

  for i in xrange(int(_qcsapi('get_results_ap_scan', 'wifi0'))):
    ssid, mac, channel, rssi, flags, protocols = _parse_scan_result(
        _qcsapi('get_properties_ap', 'wifi0', i))
    print 'BSS %s(on %s)' % (mac, hif)
    print '\tfreq: %d' % (5000 + 5 * channel)
    print '\tsignal: %.2f' % rssi
    print '\tSSID: %s' % ssid
    if flags & 0x1:
      if protocols & 0x1:
        print '\tWPA:'
      if protocols & 0x2:
        print '\tRSN:'

  return True
