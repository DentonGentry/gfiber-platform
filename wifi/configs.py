#!/usr/bin/python -S

"""Utils for hostapd and wpa_supplicant configuration."""

import subprocess

import Crypto.Protocol.KDF

# pylint: disable=g-bad-import-order
import autochannel
import experiment
import utils


EXPERIMENTS = [
    'NoSwapWifiPrimaryChannel',  # checked by hostapd itself
    'NoAutoNarrowWifiChannel',  # checked by hostapd itself
    'Wifi80211k',
    'WifiBandsteering',
    'WifiReverseBandsteering',
    'WifiHostapdLogging',
    'WifiHostapdDebug',
    'WifiShortAggTimeout',
    'WifiNoAggTimeout',
]
for _i in EXPERIMENTS:
  experiment.register(_i)


# From http://go/alphabet-ie-registry, OUI f4f5e8.
# The properties of this class are hex string representations of varints.
# pylint: disable=invalid-name
class VENDOR_IE_FEATURE_ID(object):
  SUPPORTS_PROVISIONING = '01'
  PROVISIONING_SSID = '03'


# Recommended HT40/VHT80 settings for given primary channels.
# HT40 channels can fall back to 20 MHz, and VHT80 can fall back to 40 or 20.
# So we configure using a "primary" 20 MHz channel, then allow wider
# transmissions if $width says to do so.  These tables are the extra
# information needed to locate the "wider" channels.
# (It might be nicer to use frequencies instead of channels here, but
# unfortunately iw makes conversion back and forth complicated, and the
# math is easier with channel numbers.  TODO(apenwarr): Make iw less lame.)
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
{ap_isolate}
{vendor_elements}

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

_EXTRA_SHORT_TIMEOUTS1_TPL = """
# Enable PTK rekeying (ie. rotate per-station keys, not just group keys).
# People seem to rarely use this but it might improve security.
wpa_ptk_rekey=333
"""

_EXTRA_SHORT_TIMEOUTS2_TPL = """
# Obnoxiously short rekey intervals to maximize the chance of discovering bugs
# caused by rekeying at inopportune times.
wep_rekey_period=10
wpa_group_rekey=10
wpa_strict_rekey=1
wpa_gmk_rekey=9
wpa_ptk_rekey=10
"""

_YOTTASECOND_TIMEOUTS_TPL = """
# Disable all rekeying.  This may slightly reduce security but might be
# useful if there are rekeying bugs.
wpa_ptk_rekey=0
wep_rekey_period=0
wpa_group_rekey=0
wpa_strict_rekey=0
wpa_gmk_rekey=0
wpa_ptk_rekey=0
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
    if subprocess.call(('is-network-box')) == 0:
      mac_addr_hnvram = (
          'MAC_ADDR_WIFI' + ('' if interface.startswith('wlan0') else '2'))
      bssid = utils.subprocess_output_or_none(
          ('hnvram', '-qr', mac_addr_hnvram))

    if bssid is None:
      bssid = utils.subprocess_output_or_none(('hnvram', '-rq', 'MAC_ADDR'))
      if bssid is None:
        raise utils.BinWifiException(
            'Box has no MAC_ADDR_WIFI, MAC_ADDR_WIFI2, or MAC_ADDR.  You can '
            'set these with e.g. '
            "'# hnvram -w MAC_ADDR_WIFI=00:00:00:00:00:00'")
  except OSError:
    pass

  enable_wmm = 'wmm_enabled=1' if opt.enable_wmm else ''
  hidden = 'ignore_broadcast_ssid=1' if opt.hidden_mode else ''
  bridge = 'bridge=%s' % opt.bridge if opt.bridge else ''
  ap_isolate = 'ap_isolate=1' if opt.client_isolation else ''
  hostapd_conf_parts = [_HOSTCONF_TPL.format(
      interface=interface, band=band, channel=channel, width=width,
      protocols=protocols, hostapd_band=hostapd_band,
      enable_80211n=enable_80211n, enable_80211ac=enable_80211ac,
      require_ht=require_ht, require_vht=require_vht, ht20=ht20, ht40=ht40,
      ht_rxstbc=ht_rxstbc, vht_settings=vht_settings,
      guard_interval=guard_interval, enable_wmm=enable_wmm, hidden=hidden,
      ap_isolate=ap_isolate, auth_algs=auth_algs, bridge=bridge,
      ssid=utils.sanitize_ssid(opt.ssid),
      vendor_elements=get_vendor_elements(opt))]

  if opt.encryption != 'NONE':
    hostapd_conf_parts.append(_HOSTCONF_WPA_TPL.format(
        psk=utils.validate_and_sanitize_psk(psk), wpa=wpa,
        wpa_pairwise=wpa_pairwise))

  if experiment.enabled('Wifi80211k'):
    hostapd_conf_parts.append(
        _EXPERIMENT_80211K_TPL.format(interface=interface))

  if opt.yottasecond_timeouts:
    hostapd_conf_parts.append(_YOTTASECOND_TIMEOUTS_TPL)
  elif opt.extra_short_timeouts >= 2:
    hostapd_conf_parts.append(_EXTRA_SHORT_TIMEOUTS2_TPL)
  elif opt.extra_short_timeouts >= 1:
    hostapd_conf_parts.append(_EXTRA_SHORT_TIMEOUTS1_TPL)

  # Track the active experiments the last time hostapd was started:
  #  - for easier examination of the state
  #  - to make sure the config counts as changed whenever the set of
  #    experiments changes.
  active_experiments = [i for i in EXPERIMENTS
                        if experiment.enabled(i)]
  hostapd_conf_parts.append('# Experiments: (%s)\n'
                            % ','.join(active_experiments))

  utils.log('configuration ready.')

  return '\n'.join(hostapd_conf_parts)


def make_network_block(network_block_lines):
  return 'network={\n%s\n}\n' % '\n'.join(network_block_lines)


def open_network_lines(ssid):
  return ['\tssid="%s"' % utils.sanitize_ssid(ssid),
          '\tkey_mgmt=NONE']


def wpa_network_lines(ssid, passphrase):
  """Like `wpa_passphrase "$ssid" "$passphrase"`, but more convenient output.

  This generates raw config lines, so we can update the config when the defaults
  don't make sense for us without doing parsing.

  N.b. wpa_passphrase double quotes provided SSID and passphrase arguments, and
  does not escape quotes or backslashes.

  Args:
    ssid: a wifi network SSID
    passphrase: a wifi network PSK
  Returns:
    lines of a network block that will let wpa_supplicant join this network
  """
  clean_ssid = utils.sanitize_ssid(ssid)
  network_lines = ['\tssid="%s"' % clean_ssid]
  clean_passphrase = utils.validate_and_sanitize_psk(passphrase)
  if len(clean_passphrase) == 64:
    network_lines += ['\tpsk=%s' % clean_passphrase]
  else:
    raw_psk = Crypto.Protocol.KDF.PBKDF2(clean_passphrase, clean_ssid, 32, 4096)
    hex_psk = ''.join(ch.encode('hex') for ch in raw_psk)
    network_lines += ['\t#psk="%s"' % clean_passphrase, '\tpsk=%s' % hex_psk]

  return network_lines


def generate_wpa_supplicant_config(ssid, passphrase, opt):
  """Generate a wpa_supplicant config from the provided arguments."""
  if passphrase is None:
    network_block_lines = open_network_lines(ssid)
  else:
    network_block_lines = wpa_network_lines(ssid, passphrase)

  network_block_lines.append('\tscan_ssid=1')
  if opt.bssid:
    network_block_lines.append('\tbssid=%s' %
                               utils.validate_and_sanitize_bssid(opt.bssid))
  network_block = make_network_block(network_block_lines)

  freq_list = ' '.join(autochannel.get_all_frequencies(opt.band))

  lines = [
      'ctrl_interface=/var/run/wpa_supplicant',
      'ap_scan=1',
      'autoscan=exponential:1:30',
      'freq_list=' + freq_list,
      network_block
  ]
  return '\n'.join(lines)


def create_vendor_ie(feature_id, payload=''):
  """Create a vendor IE in hostapd config format.

  Args:
    feature_id:  The go/alphabet-ie-registry feature ID for OUI f4f5e8.
    payload:  A string payload (must be ASCII), or none.

  Returns:
    The vendor IE, as a string.
  """
  length = '%02x' % (3 + (len(feature_id)/2) + len(payload))
  oui = 'f4f5e8'
  return 'dd%s%s%s%s' % (length, oui, feature_id, payload.encode('hex'))


def get_vendor_elements(opt):
  """Get vendor_elements value hostapd config.

  The way to specify multiple vendor IEs in hostapd is to concatenate them, e.g.

    vendor_elements=dd0411223301dd051122330203

  Args:
    opt:  The optdict containing user-specified options.

  Returns:
    The vendor_elements string (including that prefix, or empty if there are no
    vendor IEs.)
  """
  vendor_ies = []

  if opt.supports_provisioning:
    vendor_ies.append(
        create_vendor_ie(VENDOR_IE_FEATURE_ID.SUPPORTS_PROVISIONING))

  if opt.hidden_mode:
    vendor_ies.append(
        create_vendor_ie(VENDOR_IE_FEATURE_ID.PROVISIONING_SSID, opt.ssid))

  if vendor_ies:
    return 'vendor_elements=%s' % ''.join(vendor_ies)

  return ''
