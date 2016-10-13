#!/usr/bin/python

"""Fake /bin/wifi implementation."""

import collections
import os
import random

import connection_check
import get_quantenna_interfaces
import ifplugd_action
import ifup
import qcsapi
import wpa_cli


MockInterface = collections.namedtuple('MockInterface',
                                       ['phynum', 'bands', 'driver'])


# A randomly selceted wifi scan result with the interesting stuff templated.
WIFI_SCAN_TPL = '''BSS {bssid}(on wcli0)
  TSF: 1269828266773 usec (14d, 16:43:48)
  freq: {freq}
  beacon interval: 100 TUs
  capability: ESS Privacy ShortSlotTime (0x0411)
  signal: {rssi}
  last seen: 2190 ms ago
  Information elements from Probe Response frame:
  {vendor_ies}
  SSID: {ssid}
  Supported rates: 1.0* 2.0* 5.5* 11.0* 18.0 24.0 36.0 54.0
  DS Parameter set: channel 6
  ERP: <no flags>
  ERP D4.0: <no flags>
  {security}
  Extended supported rates: 6.0 9.0 12.0 48.0
'''

VENDOR_IE_TPL = '  Vendor specific: OUI {oui}, data: {data}'


WIFI_SHOW_TPL = '''Band: {band}
RegDomain: US
Interface: wlan{phynum}  # {band} GHz ap
BSSID: f4:f5:e8:81:1b:a0
AutoChannel: True
AutoType: NONDFS
Station List for band: {band}

Client Interface: wcli{phynum}  # {band} GHz client
Client BSSID: f4:f5:e8:81:1b:a1
'''

WIFI_SHOW_NO_RADIO_TPL = '''Band: {band}
RegDomain: 00
'''

WPA_PATH = None
REMOTE_ACCESS_POINTS = collections.defaultdict(dict)
INTERFACE_FOR_BAND = collections.defaultdict(lambda: None)
INTERFACE_EVENTS = collections.defaultdict(list)
LOCAL_ACCESS_POINTS = {}
CLIENT_ASSOCIATIONS = {}


class AccessPoint(object):

  def __init__(self, **kwargs):
    for attr in ('ssid', 'psk', 'band', 'bssid', 'security', 'rssi',
                 'vendor_ies', 'connection_check_result', 'hidden'):
      setattr(self, attr, kwargs.get(attr, None))

  def scan_str(self):
    security_strs = {
        'WEP': '  Privacy:  WEP',
        'WPA': '  WPA:',
        'WPA2': '  RSN:   * Version: 1',
    }
    return WIFI_SCAN_TPL.format(
        ssid=self.ssid if not self.hidden else '',
        freq='2437' if self.band == '2.4' else '5160',
        bssid=self.bssid,
        vendor_ies='\n'.join(VENDOR_IE_TPL.format(oui=oui, data=data)
                             for oui, data in (self.vendor_ies or [])),
        rssi='%.2f dBm' % (self.rssi or 0),
        security=security_strs.get(self.security, ''))


def call(*args, **kwargs):
  wifi_commands = {
      'scan': _scan,
      'set': _set,
      'stopap': _stopap,
      'setclient': _setclient,
      'stopclient': _stopclient,
      'stop': _stop,
      'show': _show,
  }

  if WPA_PATH is None and args[0].endswith('client'):
    raise ValueError('Set subprocess.wifi.WPA_PATH before calling a fake '
                     '"wifi *client" command')

  if args[0] in wifi_commands:
    return wifi_commands[args[0]](args[1:], env=kwargs.get('env', {}))

  return 99, 'unrecognized command %s' % args[0]


def _set(args, env=None):
  band = _get_flag(args, ('-b', '--band'))
  LOCAL_ACCESS_POINTS[band] = args, env
  return 0, ''


def _stopap(args, env=None):
  bands = _get_flag(args, ('-b', '--band')) or '2.4 5'
  for band in bands.split():
    if band in LOCAL_ACCESS_POINTS:
      del LOCAL_ACCESS_POINTS[band]

  return 0, ''


def _setclient(args, env=None):
  env = env or {}

  band = _get_flag(args, ('-b', '--band'))
  bssid = _get_flag(args, ('--bssid',))
  ssid = _get_flag(args, ('S', '--ssid',))

  if band not in INTERFACE_FOR_BAND:
    raise ValueError('No interface for band %r' % band)

  interface = INTERFACE_FOR_BAND[band]
  interface_name = 'wcli%s' % interface.phynum

  if bssid:
    ap = REMOTE_ACCESS_POINTS[band].get(bssid, None)
    if not ap or ap.ssid != ssid:
      _setclient_error_not_found(interface_name, ssid, interface.driver)
      return 1, ('AP with band %r and BSSID %r and ssid %s not found'
                 % (band, bssid, ssid))
  elif ssid:
    candidates = [ap for ap in REMOTE_ACCESS_POINTS[band].itervalues()
                  if ap.ssid == ssid]
    if not candidates:
      _setclient_error_not_found(interface_name, ssid, interface.driver)
      return 1, 'AP with SSID %r not found' % ssid
    ap = random.choice(candidates)
  else:
    raise ValueError('Did not specify BSSID or SSID in %r' % args)

  psk = env.get('WIFI_CLIENT_PSK', None)
  if psk != ap.psk:
    _setclient_error_auth(interface_name, ssid, interface.driver)
    return 1, 'Wrong PSK, got %r, expected %r' % (psk, ap.psk)

  _setclient_success(interface_name, ssid, bssid, psk, interface.driver, ap,
                     band)

  return 0, ''


def _setclient_error_not_found(interface_name, ssid, driver):
  if driver == 'cfg80211':
    wpa_cli.mock(interface_name, wpa_state='SCANNING')
  elif driver == 'frenzy':
    qcsapi.mock('get_mode', 'wifi0', value='Station')
    qcsapi.mock('get_ssid', 'wifi0', value='')
    qcsapi.mock('ssid_get_authentication_mode', 'wifi0', ssid, value='')
    qcsapi.mock('get_status', 'wifi0', value='Error')

  CLIENT_ASSOCIATIONS[interface_name] = None


def _setclient_error_auth(interface_name, ssid, driver):
  if driver == 'cfg80211':
    # This is what our version of wpa_supplicant does for auth failures.
    INTERFACE_EVENTS[interface_name].append('<2>CTRL-EVENT-SSID-TEMP-DISABLED')
    wpa_cli.mock(interface_name, wpa_state='SCANNING')
  elif driver == 'frenzy':
    qcsapi.mock('get_mode', 'wifi0', value='Station')
    qcsapi.mock('get_ssid', 'wifi0', value='')
    qcsapi.mock('ssid_get_authentication_mode', 'wifi0', ssid, value='')
    qcsapi.mock('get_status', 'wifi0', value='Error')

  CLIENT_ASSOCIATIONS[interface_name] = None


def _setclient_success(interface_name, ssid, bssid, psk, driver, ap, band):
  if CLIENT_ASSOCIATIONS.get(interface_name, None):
    _disconnected_event(band)
  if driver == 'cfg80211':
    # Make sure the wpa_supplicant socket exists.
    open(os.path.join(WPA_PATH, interface_name), 'w')

    # Tell wpa_cli what to return.
    key_mgmt = 'WPA2-PSK' if psk else 'NONE'
    wpa_cli.mock(interface_name, wpa_state='COMPLETED', ssid=ssid, bssid=bssid,
                 key_mgmt=key_mgmt)

    # Send the CONNECTED event.
    INTERFACE_EVENTS[interface_name].append('<2>CTRL-EVENT-CONNECTED')

  elif driver == 'frenzy':
    qcsapi.mock('get_mode', 'wifi0', value='Station')
    qcsapi.mock('get_ssid', 'wifi0', value=ssid)
    qcsapi.mock('ssid_get_authentication_mode', 'wifi0', ssid,
                value='PSKAuthentication' if psk else 'NONE')
    qcsapi.mock('get_status', 'wifi0', value='')

  CLIENT_ASSOCIATIONS[interface_name] = ap
  connection_check.mock(interface_name, ap.connection_check_result or 'succeed')

  # Call ifplugd.action for the interface coming up (wifi/quantenna.py does this
  # manually).
  ifplugd_action.call(interface_name, 'up')


def _disconnected_event(band):
  interface = INTERFACE_FOR_BAND[band]
  interface_name = 'wcli%s' % interface.phynum
  if interface.driver == 'cfg80211':
    INTERFACE_EVENTS[interface_name].append('<2>CTRL-EVENT-DISCONNECTED')
    wpa_cli.mock(interface_name, wpa_state='SCANNING')
  else:
    qcsapi.mock('get_ssid', 'wifi0', value='')
    qcsapi.mock('get_status', 'wifi0', value='Error')

  CLIENT_ASSOCIATIONS[interface_name] = None


def _stopclient(args, env=None):
  bands = _get_flag(args, ('-b', '--band')) or '2.4 5'
  for band in bands.split():
    interface = INTERFACE_FOR_BAND[band]
    interface_name = 'wcli%s' % interface.phynum

    if interface.driver == 'cfg80211':
      # Send the DISCONNECTED and TERMINATING events.
      INTERFACE_EVENTS[interface_name].append('<2>CTRL-EVENT-DISCONNECTED')
      INTERFACE_EVENTS[interface_name].append('<2>CTRL-EVENT-TERMINATING')

      # Clear the wpa_cli status response.
      wpa_cli.mock(interface_name)

      # Make sure the wpa_supplicant socket does not.
      if os.path.exists(os.path.join(WPA_PATH, interface_name)):
        os.unlink(os.path.join(WPA_PATH, interface_name))

    elif interface.driver == 'frenzy':
      qcsapi.mock('get_ssid', 'wifi0', value='')
      qcsapi.mock('get_status', 'wifi0', value='Error')

    CLIENT_ASSOCIATIONS[interface_name] = None

  # Call ifplugd.action for the interface going down (wifi/quantenna.py does this
  # manually).
  ifplugd_action.call(interface_name, 'down')

  return 0, ''


def _stop(*args, **kwargs):
  _stopap(*args, **kwargs)
  _stopclient(*args, **kwargs)
  return 0, ''


def _kill_wpa_supplicant(band):
  # From conman's perspective, there's no difference between someone running
  # 'wifi stopclient' and the process dying for some other reason.
  _stopclient(['--band', band])


def _scan(args, **unused_kwargs):
  band_flag = _get_flag(args, ('-b', '--band'))
  interface = INTERFACE_FOR_BAND[band_flag]
  interface_name = 'wcli%s' % interface.phynum
  if not ifup.INTERFACE_STATE.get(interface_name, False):
    return 1, 'interface down'

  return 0, '\n'.join(ap.scan_str()
                      for band in interface.bands
                      for ap in REMOTE_ACCESS_POINTS[band].itervalues())


def _show(unused_args, **unused_kwargs):
  return 0, '\n\n'.join(WIFI_SHOW_TPL.format(band=band, **interface._asdict()) if interface
                        else WIFI_SHOW_NO_RADIO_TPL.format(band)
                        for band, interface in INTERFACE_FOR_BAND.iteritems())


def _get_flag(args, flags):
  for flag in flags:
    if flag in args:
      return args[args.index(flag) + 1]


def mock(command, *args, **kwargs):
  if command == 'remote_ap':
    remote_ap = AccessPoint(**kwargs)
    REMOTE_ACCESS_POINTS[kwargs['band']][kwargs['bssid']] = remote_ap
  elif command == 'remote_ap_remove':
    del REMOTE_ACCESS_POINTS[kwargs['band']][kwargs['bssid']]
  elif command == 'interfaces':
    INTERFACE_FOR_BAND.clear()
    for interface in args:
      for band in interface.bands:
        INTERFACE_FOR_BAND[band] = interface
      if interface.driver == 'frenzy':
        get_quantenna_interfaces.mock(
            fmt % interface.phynum
            for fmt in ('wlan%s', 'wlan%s_portal', 'wcli%s'))
  elif command == 'wpa_path':
    global WPA_PATH
    WPA_PATH = args[0]
  elif command == 'disconnected_event':
    _disconnected_event(args[0])
  elif command == 'kill_wpa_supplicant':
    _kill_wpa_supplicant(args[0])
