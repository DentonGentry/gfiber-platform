#!/usr/bin/python -S

"""Utils for hostapd and wpa_supplicant configuration."""

import subprocess

import experiment
import utils


# Recommended HT40/VHT80 settings for given primary channels.
# HT40 channels can fall back to 20 MHz, and VHT80 can fall back to 40 or 20.
# So we configure using a "primary" 20 MHz channel, then allow wider
# transmissions if $width says to do so.  These tables are the extra
# information needed to locate the "wider" channels.
# (It might be nicer to use frequencies instead of channels here, but
# unfortunately iw makes conversion back and forth complicated, and the
# math is easier with channel numbers.  TODO(apenwarr): make iw less lame.)
_HT_DIRECTIONS = """
    1+ 2+ 3+ 4+ 5- 6- 7+ 8- 9- 10- 11-
    36+ 40- 44+ 48-
    149+ 153- 157+ 161-
    52+ 56- 60+ 64-
    100+ 104- 108+ 112-
    132+ 136-
"""

_VHT_BASES = """
    36=36 40=36 44=36 48=36
    149=149 153=149 157=149 161=149
    52=52 56=52 60=52 64=52
    100=100 104=100 108=100 112=100
"""


_VHT_SETTINGS_TPL = """vht_capab={ampdu}{vht_guard_interval}{ldpc}
vht_oper_chwidth=1

# Wifi channel numbers define the center of a 20 MHz
# channel.  40 MHz channels are defined as one channel plus
# the next one 4 up.  80 MHz channels go back to defining by
# center.  If you think about it long enough, you eventually
# discover that the center is 6 channels up from the base 20
# MHz channel, although this isn't very intuitive.
vht_oper_centr_freq_seg0_idx={vht_base}
"""

_HOSTCONF_TPL = """ctrl_interface=/var/run/hostapd
interface={interface}
{bridge}
ssid={ssid}
utf8_ssid=1
auth_algs={auth_algs}
hw_mode={hostapd_band}
channel={channel}
country_code=US
ieee80211d=1
ieee80211h=1
{enable_80211n}
{enable_80211ac}
{enable_wmm}
{require_ht}
{require_vht}
{hidden}

ht_capab={ht20}{ht40}{guard_interval}{ht_rxstbc}
{vht_settings}
"""

_HOSTCONF_WPA_TPL = """wpa={wpa}
wpa_passphrase={psk}
wpa_key_mgmt=WPA-PSK
wpa_pairwise={wpa_pairwise}
"""

_EXPERIMENT_80211K_TPL = """
# 802.11k support
radio_measurements=1
local_pwr_constraint=3
spectrum_mgmt_required=1
neighbor_ap_list_file=/tmp/waveguide/APs.{interface}
"""


_EXTRA_SHORT_TIMEOUT_INTERVALS_TPL = """
# Obnoxiously short rekey intervals to maximize the chance of discovering bugs
# caused by rekeying at inopportune times.
wep_rekey_period=10
wpa_group_rekey=10
wpa_strict_rekey=1
wpa_gmk_rekey=9
wpa_ptk_rekey=10
"""


def generate_hostapd_config(
    phy_info, interface, band, channel, width, protocols, psk, opt):
  """Generates a hostpad config from the given arguments.

  Args:
    phy_info: The result of running 'iw phy <phy> info' where <phy> is the phy
      on which hostapd will run.
    interface: The interface on which hostapd will run.
    band: The band on which hostapd will run.
    channel: The channel on which hostapd will run.
    width: The channel width with which hostapd will run.
    protocols: The supported 802.11 protocols, as a collection of
      single-character strings (e.g. ['a', 'g', 'n'])
    psk: The PSK to use for the AP.
    opt: The OptDict parsed from command line options.

  Returns:
    The generated hostapd config, as a string.

  Raises:
    ValueError: For certain invalid combinations of arguments.
  """
  utils.log('generating configuration...')

  if band == '2.4':
    hostapd_band = 'g' if set(('n', 'g')) & protocols else 'b'
  else:
    hostapd_band = 'a'

  ampdu = ''
  enable_80211n = ''
  enable_80211ac = ''
  require_ht = ''
  require_vht = ''
  ht20 = ''
  ht40 = ''
  ht_rxstbc = ''
  vht_settings = ''

  guard_interval = (
      '[SHORT-GI-20][SHORT-GI-40]' if opt.short_guard_interval else '')
  vht_guard_interval = '[SHORT-GI-80]' if opt.short_guard_interval else ''

  if 'RX STBC 3-stream' in phy_info:
    ht_rxstbc = '[RX-STBC123]'
  if 'RX STBC 2-stream' in phy_info:
    ht_rxstbc = '[RX-STBC12]'
  if 'RX STBC 1-stream' in phy_info:
    ht_rxstbc = '[RX-STBC1]'

  if 'n' in protocols:
    enable_80211n = 'ieee80211n=1'
    ht20 = '[HT20]'

  if 'ac' in protocols:
    if width == '80':
      enable_80211ac = 'ieee80211ac=1'

    if 'Maximum RX AMPDU length 16383 bytes' in phy_info:
      ampdu = '[MAX-A-MPDU-LEN-EXP1]'
    if 'Maximum RX AMPDU length 32767 bytes' in phy_info:
      ampdu = '[MAX-A-MPDU-LEN-EXP2]'
    if 'Maximum RX AMPDU length 65535 bytes' in phy_info:
      ampdu = '[MAX-A-MPDU-LEN-EXP3]'
    if 'Maximum RX AMPDU length 131071 bytes' in phy_info:
      ampdu = '[MAX-A-MPDU-LEN-EXP4]'
    if 'Maximum RX AMPDU length 262143 bytes' in phy_info:
      ampdu = '[MAX-A-MPDU-LEN-EXP5]'
    if 'Maximum RX AMPDU length 524287 bytes' in phy_info:
      ampdu = '[MAX-A-MPDU-LEN-EXP6]'
    if 'Maximum RX AMPDU length 1048575 bytes' in phy_info:
      ampdu = '[MAX-A-MPDU-LEN-EXP7]'

  if not set(('a', 'b', 'ab', 'g')) & protocols:
    require_ht = 'require_ht=1'
  if not set(('a', 'b', 'ab', 'g', 'n')) & protocols:
    require_vht = 'require_vht=1'

  if opt.encryption.startswith('WPA_PSK_'):
    auth_algs, wpa = 1, 1
  elif opt.encryption.startswith('WPA2_PSK_'):
    auth_algs, wpa = 1, 2
  elif opt.encryption.startswith('WPA12_PSK_'):
    auth_algs, wpa = 1, 3
  elif opt.encryption.startswith('WEP'):
    auth_algs, wpa = 3, 0
  elif opt.encryption.startswith('NONE'):
    auth_algs, wpa = 1, 0
  else:
    raise ValueError('Invalid crypto protocol: %s' % opt.encryption)

  if opt.encryption[-4:] in ('_AES', 'WEP', 'NONE'):
    wpa_pairwise = 'CCMP'
  elif opt.encryption.endswith('_TKIP'):
    wpa_pairwise = 'TKIP'
  else:
    raise ValueError('Invalid crypto protocol: %s' % opt.encryption)

  if int(width) >= 40:
    if '%s+' % channel in _HT_DIRECTIONS.split():
      ht40 = '[HT40+]'
    elif '%s-' % channel in _HT_DIRECTIONS.split():
      ht40 = '[HT40-]'
    else:
      raise ValueError(
          'HT40 requested but not available on channel %s.' % channel)

  if width == '80':
    ldpc = '[RXLDPC]'
    vht_base = ''
    for base in _VHT_BASES.split():
      if base.startswith('%s=' % channel):
        vht_base = base.split('=')[1]
        break

    if vht_base:
      vht_settings = _VHT_SETTINGS_TPL.format(
          ampdu=ampdu,
          vht_guard_interval=vht_guard_interval,
          ldpc=ldpc, vht_base=int(vht_base) + 6)
    else:
      raise ValueError(
          'VHT80 requested but not available on channel %s' % channel)

  try:
    bssid = None
    if subprocess.call(['is-network-box']) == 0:
      mac_addr_hnvram = (
          'MAC_ADDR_WIFI' + ('' if interface.startswith('wlan0') else '2'))
      bssid = utils.subprocess_output_or_none(
          ['hnvram', '-qr', mac_addr_hnvram])

    if bssid is None:
      bssid = utils.subprocess_output_or_none(['hnvram', '-rq', 'MAC_ADDR'])
      if bssid is None:
        raise utils.BinWifiException(
            'Box has no MAC_ADDR_WIFI, MAC_ADDR_WIFI2, or MAC_ADDR.  You can '
            'set these with e.g. '
            '\'# hnvram -w MAC_ADDR_WIFI="00:00:00:00:00:00"\'')
  except OSError:
    pass

  enable_wmm = 'wmm_enabled=1' if opt.enable_wmm else ''
  hidden = 'ignore_broadcast_ssid=1' if opt.hidden_mode else ''
  bridge = 'bridge=%s' % opt.bridge if opt.bridge else ''
  hostapd_conf_parts = [_HOSTCONF_TPL.format(
      interface=interface, band=band, channel=channel, width=width,
      protocols=protocols, hostapd_band=hostapd_band,
      enable_80211n=enable_80211n, enable_80211ac=enable_80211ac,
      require_ht=require_ht, require_vht=require_vht, ht20=ht20, ht40=ht40,
      ht_rxstbc=ht_rxstbc, vht_settings=vht_settings,
      guard_interval=guard_interval, enable_wmm=enable_wmm, hidden=hidden,
      auth_algs=auth_algs, bridge=bridge, ssid=utils.sanitize_ssid(opt.ssid))]

  if opt.encryption != 'NONE':
    hostapd_conf_parts.append(_HOSTCONF_WPA_TPL.format(
        psk=utils.validate_and_sanitize_psk(psk), wpa=wpa,
        wpa_pairwise=wpa_pairwise))

  if experiment.enabled('Wifi80211k'):
    hostapd_conf_parts.append(
        _EXPERIMENT_80211K_TPL.format(interface=interface))

  if opt.extra_short_timeout_intervals:
    hostapd_conf_parts.append(_EXTRA_SHORT_TIMEOUT_INTERVALS_TPL)

  utils.log('configuration ready.')

  return '\n'.join(hostapd_conf_parts)


def generate_wpa_supplicant_config(ssid, passphrase):
  return '\n'.join(
      ('ctrl_interface=/var/run/wpa_supplicant',
       'ap_scan=1',
       subprocess.check_output(['wpa_passphrase',
                                utils.sanitize_ssid(ssid),
                                utils.validate_and_sanitize_psk(passphrase)])))
