#!/usr/bin/python

"""Utility for controlling WiFi AP and client functionality."""

from __future__ import print_function

import atexit
import glob
import os
import re
import subprocess
import sys
import time

import autochannel
import bandsteering
import configs
import experiment
import iw
import mwifiex
import options
import persist
import qca9880_cal
import quantenna
import utils


_OPTSPEC_FORMAT = """
{bin} set           Enable or modify access points.  Takes all options unless otherwise specified.
{bin} setclient     Enable or modify wifi clients.  Takes -b, -P, -s, --bssid, -S, --wds.
{bin} stop|off      Disable access points and clients.  Takes -b, -P, -S.
{bin} stopap        Disable access points.  Takes -b, -P, -S.
{bin} stopclient    Disable wifi clients.  Takes -b, -P, -S.
{bin} restore       Restore saved client and access point options.  Takes -b, -S.
{bin} show          Print all known parameters.  Takes -b, -S.
{bin} scan          Print 'iw scan' results for a single band.  Takes -b, -S.
--
b,band=                           Wifi band(s) to use (5 GHz and/or 2.4 GHz).  set, setclient, and scan have a default of 2.4 and cannot take multiple-band values.  [2.4 5]
c,channel=                        Channel to use [auto]
a,autotype=                       Autochannel method to use (LOW, HIGH, DFS, NONDFS, ANY,OVERLAP) [NONDFS]
s,ssid=                           SSID to use [{ssid}]
bssid=                            BSSID to use []
e,encryption=                     Encryption type to use (WPA_PSK_AES, WPA2_PSK_AES, WPA12_PSK_AES, WPA_PSK_TKIP, WPA2_PSK_TKIP, WPA12_PSK_TKIP, WEP, or NONE) [WPA2_PSK_AES]
f,force-restart                   Force restart even if already running with these options
C,client-isolation                Enable client isolation, preventing bridging of frames between associated stations.
H,hidden-mode                     Enable hidden mode (disable SSID advertisements)
M,enable-wmm                      Enable wmm extensions (needed for block acks)
G,short-guard-interval            Enable short guard interval
p,protocols=                      802.11 levels to allow, slash-delimited [a/b/g/n/ac]
w,width=                          Channel width to use, in MHz (20, 40, or 80) [20]
B,bridge=                         Bridge device to use [br0]
X,extra-short-timeouts            Use shorter key rotations; 1=rotate PTK, 2=rotate often
Y,yottasecond-timeouts            Don't rotate any keys: PTK, GTK, or GMK
P,persist                         For set commands, persist options so we can restore them with 'wifi restore'.  For stop commands, remove persisted options.
S,interface-suffix=               Interface suffix (defaults to ALL for stop commands; use NONE to specify no suffix) []
W,wds                             Enable WDS mode (nl80211 only)
lock-timeout=                     How long, in seconds, to wait for another /bin/wifi process to finish before giving up. [60]
scan-ap-force                     (Scan only) scan when in AP mode
scan-passive                      (Scan only) do not probe, scan passively
scan-freq=                        (Scan only) limit scan to specific frequencies.
supports-provisioning             Indicate via vendor IE that this AP supports provisioning.  Corresponds to feature ID 01 of OUI f4f5e8 at go/alphabet-ie-registry.
no-band-restriction               For setclient only.  If set, let --band select the wifi radio but do not actually enforce it for multi-band radios.
"""

_FINGERPRINTS_DIRECTORY = '/tmp/wifi/fingerprints'
_LOCKFILE = '/tmp/wifi/wifi'
_PLATFORM_FILE = '/etc/platform'
lockfile_taken = False


# pylint: disable=protected-access
subprocess.call(('mkdir', '-p', utils._CONFIG_DIR))


def _stop_hostapd(interface):
  """Stops hostapd from running on the given interface.

  Also removes the pid file, sets them interface down and deletes the monitor
  interface, if it exists.

  Args:
    interface: The interface on which to stop hostapd.

  Returns:
    Whether hostapd was successfully stopped and cleaned up.
  """
  if not _is_hostapd_running(interface):
    utils.log('hostapd already not running.')
    return True

  config_filename = utils.get_filename(
      'hostapd', utils.FILENAME_KIND.config, interface, tmp=True)
  pid_filename = utils.get_filename(
      'hostapd', utils.FILENAME_KIND.pid, interface, tmp=True)
  if not utils.kill_pid('hostapd .* %s$' % config_filename, pid_filename):
    return False

  # TODO(apenwarr): hostapd doesn't always delete interface mon.$ifc.  Then it
  # gets confused by the fact that it already exists.  Let's help out.  We
  # should really fix this by eliminating the need for hostapd to have a
  # monitor interface at all (which is deprecated anyway) Remove this line when
  # our hostapd no longer needs a monitor interface.
  utils.subprocess_quiet(('iw', 'dev', 'mon.%s' % interface, 'del'))

  subprocess.check_call(('ip', 'link', 'set', interface, 'down'))

  return True


def _stop_wpa_supplicant(interface):
  """Stops wpa_supplicant from running on the given interface.

  Also removes the pid file and sets the interface down.

  Args:
    interface: The interface on which to stop wpa_supplicant.

  Returns:
    Whether wpa_supplicant was successfully stopped.
  """
  if not _is_wpa_supplicant_running(interface):
    utils.log('wpa_supplicant already not running.')
    return True

  pid_filename = utils.get_filename(
      'wpa_supplicant', utils.FILENAME_KIND.pid, interface, tmp=True)
  config_filename = utils.get_filename(
      'wpa_supplicant', utils.FILENAME_KIND.config, interface, tmp=True)
  if not utils.kill_pid('wpa_supplicant .* %s$' % config_filename,
                        pid_filename):
    return False

  try:
    subprocess.check_call(('ip', 'link', 'set', interface, 'down'))
  except subprocess.CalledProcessError:
    return False

  return True


def _set_wifi_broadcom(opt):
  """Set up wifi using wl, for Broadcom chips.

  Args:
    opt: The OptDict parsed from command line options.

  Raises:
    BinWifiException: On various errors.
  """
  def wl(*args):
    utils.log('wl %s', ' '.join(args))
    subprocess.check_call(('wl') + list(args))

  utils.log('Configuring broadcom wifi.')
  wl('radio', 'on')
  wl('down')
  wl('ssid', '')
  band = opt.band
  if opt.channel != 'auto':
    band = 'auto'
  try:
    wl('band', {'2.4': 'b', '5': 'a', 'auto': 'auto'}[band])
  except KeyError:
    raise utils.BinWifiException('Invalid band %s', band)

  wl('ap', '0')
  wl('up')
  if opt.channel == 'auto':
    # We can only run autochannel when ap=0, but setting ap=1 later will wipe
    # the value.  So we have to capture the autochannel setting, then set it
    # later.  'wl autochannel 2' is thus useless.
    wl('autochannel', '1')
    # enough time to scan all the 2.4 or 5 GHz channels at 100ms each
    time.sleep(3)
    utils.log('wl autochannel')
    channel = subprocess.check_output(('wl', 'autochannel')).split()[0]

  wl('ap', '1')
  wl('chanspec', channel)
  wl('auth', '0')
  wl('infra', '1')
  try:
    wl('wsec', {'_AES': '4',
                'TKIP': '2',
                'WEP': '1',
                'NONE': '0'}[opt.encryption[-4:]])
  except KeyError:
    raise utils.BinWifiException('invalid crypto %s', opt.encryption)
  wl('sup_wpa', '1')
  try:
    wl('wpa_auth',
       {'WPA_': '4',
        'WPA2': '128',
        'WEP': '0',
        'NONE': '0'}[opt.encryption[:4]])
  except KeyError:
    raise utils.BinWifiException('invalid crypto %s', opt.encryption)

  wl('up')
  if '_PSK_' in opt.encryption:
    # WPA keys must be added *before* setting the SSID
    wl('set_pmk', os.environ['WIFI_PSK'])
    wl('ssid', opt.ssid)
  elif opt.encryption == 'WEP':
    # WEP keys must be added *after* setting the SSID
    wl('ssid', opt.ssid)
    wl('set_pmk', os.environ['WIFI_PSK'])
  elif opt.encryption == 'NONE':
    wl('ssid', opt.ssid)
  else:
    raise utils.BinWifiException('invalid crypto %s', opt.encryption)


def set_wifi(opt):
  """Set up an access point in response to the 'set' command.

  Args:
    opt: The OptDict parsed from command line options.

  Returns:
    Whether setting up the AP succeeded.

  Raises:
    BinWifiException: On various errors.
  """
  band = opt.band
  width = opt.width
  channel = opt.channel
  autotype = opt.autotype
  protocols = set(opt.protocols.split('/'))

  utils.validate_set_wifi_options(opt)

  psk = None
  if opt.encryption == 'WEP' or '_PSK_' in opt.encryption:
    psk = os.environ['WIFI_PSK']

  if band == '5' and  quantenna.set_wifi(opt):
    return True

  if iw.RUNNABLE_WL() and not iw.RUNNABLE_IW():
    _set_wifi_broadcom(opt)
    return True

  if not iw.RUNNABLE_IW():
    raise utils.BinWifiException("Can't proceed without iw")

  # If this phy is running client mode, we need to use its width/channel.
  phy = iw.find_phy(band, channel)
  if phy is None:
    raise utils.BinWifiException(
        'no wifi phy for band=%s channel=%s', band, channel)

  # Check for calibration errors on ath10k.
  qca9880_cal.qca8990_calibration()
  mwifiex.set_recovery(experiment.enabled('MwifiexFirmwareRecovery'))

  client_interface = iw.find_interface_from_phy(
      phy, iw.INTERFACE_TYPE.client, opt.interface_suffix)
  if (client_interface is not None and
      _is_wpa_supplicant_running(client_interface)):
    # Wait up to ten seconds for interface width and channel to be available
    # (only relevant if wpa_supplicant was started recently).
    # TODO(rofrankel): Consider shortcutting this loop if wpa_cli shows status
    # is SCANNING (and other values)?
    utils.log('Client running on same band; finding its width and channel.')
    for _ in xrange(50):
      client_band = _get_wpa_band(client_interface)
      client_width, client_channel = iw.find_width_and_channel(client_interface)

      sys.stderr.write('.')
      sys.stderr.flush()
      if None not in (client_band, client_width, client_channel):
        band, width, channel = client_band, client_width, client_channel
        utils.log('Using band=%s, channel=%s, width=%s MHz from client',
                  band, channel, width)
        break
      time.sleep(0.2)
    else:
      utils.log("Couldn't find band, width, and channel used by client "
                "(it may not be connected)")

  interface = iw.find_interface_from_phy(
      phy, iw.INTERFACE_TYPE.ap, opt.interface_suffix)
  if interface is None:
    raise utils.BinWifiException(
        'no wifi interface found for band=%s channel=%s suffix=%s',
        band, channel, opt.interface_suffix)

  for ap_interface in iw.find_all_interfaces_from_phy(phy,
                                                      iw.INTERFACE_TYPE.ap):
    if not _is_hostapd_running(ap_interface):
      continue

    if ap_interface == interface:
      continue

    # TODO(rofrankel):  Figure out what to do about width.  Unlike channel,
    # there's no 'auto' default; we don't know if 20 was requested or just
    # defaulted to.  So it's not clear whether to override the other AP's
    # choice.
    _, other_ap_channel = iw.find_width_and_channel(ap_interface)
    if channel == 'auto':
      channel = other_ap_channel
    else:
      _restart_hostapd(ap_interface, '-c', channel)

  utils.log('interface: %s', interface)
  utils.log('Configuring cfg80211 wifi.')

  pid_filename = utils.get_filename(
      'hostapd', utils.FILENAME_KIND.pid, interface, tmp=True)
  utils.log('pidfile: %s', pid_filename)

  autotype_filename = '/tmp/autotype.%s' % interface
  band_filename = '/tmp/band.%s' % interface
  width_filename = '/tmp/width.%s' % interface
  autochan_filename = '/tmp/autochan.%s' % interface

  old_autotype = utils.read_or_empty(autotype_filename)
  old_band = utils.read_or_empty(band_filename)
  old_width = utils.read_or_empty(width_filename)

  # Special case: if autochannel enabled and we've done it before, just use the
  # old autochannel.  The main reason for this is we may not be able to run the
  # autochannel algorithm without stopping hostapd first, which defeats the code
  # that tries not to restart hostapd unnecessarily.
  if (channel == 'auto'
      and (autotype, band, width) == (old_autotype, old_band, old_width)):
    # ...but only if not forced mode.  If it's forced, don't use the old
    # value, but don't wipe it either.
    if not opt.force_restart:
      autochan = utils.read_or_empty(autochan_filename)
      if autochan and int(autochan) > 0:
        utils.log('Reusing old autochannel=%s', autochan)
        channel = autochan
  else:
    # forget old autochannel setting
    if os.path.exists(autochan_filename):
      try:
        os.remove(autochan_filename)
      except OSError:
        utils.log('Failed to remove autochan file.')

  if channel == 'auto':
    utils.atomic_write(autochan_filename, '')
    try:
      channel = autochannel.scan(interface, band, autotype, width)
    except ValueError as e:
      raise utils.BinWifiException('Autochannel scan failed: %s', e)
    utils.atomic_write(autochan_filename, channel)

  utils.atomic_write(autotype_filename, autotype)
  utils.atomic_write(band_filename, band)
  utils.atomic_write(width_filename, width)

  utils.log('using channel=%s', channel)

  try:
    utils.log('getting phy info...')
    with open(os.devnull, 'w') as devnull:
      try:
        phy_info = subprocess.check_output(('iw', 'phy', phy, 'info'),
                                           stderr=devnull)
      except subprocess.CalledProcessError as e:
        raise utils.BinWifiException(
            'Failed to get phy info for phy %s: %s', phy, e)
    hostapd_config = configs.generate_hostapd_config(
        phy_info, interface, band, channel, width, protocols, psk, opt)
  except ValueError as e:
    raise utils.BinWifiException('Invalid option: %s', e)

  return _maybe_restart_hostapd(interface, hostapd_config, opt)


@iw.requires_iw
def stop_wifi(opt):
  return stop_client_wifi(opt) and stop_ap_wifi(opt)


def stop_ap_wifi(opt):
  """Disable an access point in response to the 'setap' command.

  Args:
    opt: The OptDict parsed from command line options.

  Returns:
    Whether disabling the AP succeeded.

  Raises:
    BinWifiException: If an expected interface is not found.
  """
  success = True
  for band in opt.band.split():
    utils.log('stopping AP for %s GHz...', band)

    if band == '5' and quantenna.stop_ap_wifi(opt):
      continue

    interfaces = []
    if opt.interface_suffix == 'ALL':
      interfaces = iw.find_all_interfaces_from_band(band, iw.INTERFACE_TYPE.ap)
    else:
      interface = iw.find_interface_from_band(
          band, iw.INTERFACE_TYPE.ap, opt.interface_suffix)
      if interface:
        interfaces = [interface]
    if not interfaces:
      utils.log('No AP interfaces for %s GHz; nothing to stop', band)
      continue

    for interface in interfaces:
      if _stop_hostapd(interface):
        if opt.persist:
          persist.delete_options('hostapd', interface)
      else:
        utils.log('Failed to stop hostapd on interface %s', interface)
        success = False

  return success


@iw.requires_iw
def _restore_wifi(interface, program):
  """Restore a program from persisted settings.

  Args:
    interface: The interface on which to restore program.
    program: The program to restore (wpa_supplicant or hostapd).

  Returns:
    Whether restoring succeeded.
  """
  argv = persist.load_options(program, interface, False)
  if argv is None:
    utils.log('No persisted options for %s %s, not restoring',
              interface, program)
    return False

  utils.log('Loaded persisted options for %s %s', interface, program)

  if _run(argv):
    utils.log('Restored %s for %s', program, interface)
    return True

  utils.log('Failed to restore %s for %s', program, interface)
  return False


def restore_wifi(opt):
  """Restore hostapd and wpa_supplicant on both bands from persisted settings.

  Nothing happens if persisted settings are not available.

  Args:
    opt: The OptDict parsed from command line options.

  Returns:
    True.
  """
  # If both bands are specified, restore 5 GHz first so that STAs are more
  # likely to join it.
  restored = set()
  for band in sorted(opt.band.split(),
                     reverse=not experiment.enabled('WifiReverseBandsteering')):
    client_interface = iw.find_interface_from_band(band,
                                                   iw.INTERFACE_TYPE.client,
                                                   opt.interface_suffix)
    ap_interface = iw.find_interface_from_band(band, iw.INTERFACE_TYPE.ap,
                                               opt.interface_suffix)
    for interface, program in ((client_interface, 'wpa_supplicant'),
                               (ap_interface, 'hostapd')):
      if interface and interface not in restored:
        restored.add(interface)
        _restore_wifi(interface, program)
  return True


# TODO(apenwarr): Extend this to notice actual running settings.
#  Including whether hostapd is up or down, etc.
# TODO(rofrankel): Extend this to show client interface info.
@iw.requires_iw
def show_wifi(opt):
  """Prints information about wifi interfaces on this device.

  Args:
    opt: The OptDict parsed from command line options.

  Returns:
    True.
  """
  for band in opt.band.split():
    print('Band: %s' % band)
    for tokens in utils.subprocess_line_tokens(('iw', 'reg', 'get')):
      if len(tokens) >= 2 and tokens[0] == 'country':
        print('RegDomain: %s' % tokens[1].strip(':'))
        break

    for prefix, interface_type in (('', iw.INTERFACE_TYPE.ap),
                                   ('Client ', iw.INTERFACE_TYPE.client)):
      interface = iw.find_interface_from_band(
          band, interface_type, opt.interface_suffix)
      if interface is None:
        continue
      print('%sInterface: %s  # %s GHz %s' %
            (prefix, interface, band, 'client' if 'cli' in interface else 'ap'))

      info_parsed = iw.info_parsed(interface)
      for k, label in (('channel', 'Channel'),
                       ('ssid', 'SSID'),
                       ('addr', 'BSSID')):
        v = info_parsed.get(k, None)
        if v is not None:
          print('%s%s: %s' % (prefix, label, v))

      if interface_type == iw.INTERFACE_TYPE.ap:
        print('AutoChannel: %r' %
              os.path.exists('/tmp/autochan.%s' % interface))
        try:
          with open('/tmp/autotype.%s' % interface) as autotype:
            print('AutoType: %s' % autotype.read().strip())
        except IOError:
          pass

        print('Station List for band: %s' % band)
        station_dump = iw.station_dump(interface)
        if station_dump:
          print(station_dump)

      print()

  return True


@iw.requires_iw
def scan_wifi(opt):
  """Prints 'iw scan' results.

  Args:
    opt: The OptDict parsed from command line options.

  Returns:
    True.

  Raises:
    BinWifiException: If an expected interface is not found.
  """
  band = opt.band.split()[0]

  if band == '5' and quantenna.scan_wifi(opt):
    return True

  interface = iw.find_interface_from_band(
      band, iw.INTERFACE_TYPE.ap, opt.interface_suffix)
  if interface is None:
    raise utils.BinWifiException('No client interface for band %s', band)

  scan_args = []
  if opt.scan_freq:
    scan_args += ['freq', str(opt.scan_freq)]
  if opt.scan_ap_force:
    scan_args += ['ap-force']
  if opt.scan_passive:
    scan_args += ['passive']

  print(iw.scan(interface, scan_args))

  return True


def _is_hostapd_running(interface):
  return utils.subprocess_quiet(
      ('hostapd_cli', '-i', interface, 'quit'), no_stdout=True) == 0


def _wpa_cli(program, interface, command):
  return utils.subprocess_quiet(
      (program, '-i', interface, command), no_stdout=True) == 0


def _is_wpa_supplicant_running(interface):
  return _wpa_cli('wpa_cli', interface, 'status')


def _reconfigure_wpa_supplicant(interface):
  if not _wpa_cli('wpa_cli', interface, 'reconfigure'):
    return False

  return _wait_for_wpa_supplicant_to_associate(interface)


def _hostapd_debug_options():
  if experiment.enabled('WifiHostapdDebug'):
    return ['-d']
  else:
    return []


def _get_hostapd_band(interface):
  for line in utils.subprocess_lines(
      ('hostapd_cli', '-i', interface, 'status')):
    tokens = line.split('=')
    if tokens and tokens[0] == 'freq':
      try:
        return {'5': '5', '2': '2.4'}[tokens[1][0]]
      except (IndexError, KeyError):
        return None


def _is_wind_charger():
  try:
    etc_platform = open(_PLATFORM_FILE).read()
    if etc_platform[:-1] == 'GFMN100':
      return True
    else:
      return False
  except IOError as e:
    print('_is_wind_charger: cant open %s: %s' % (_PLATFORM_FILE, e.strerror))
    return False


def _start_hostapd(interface, config_filename, band, ssid):
  """Starts a babysat hostapd.

  Args:
    interface: The interface on which to start hostapd.
    config_filename: The filename of the hostapd configuration.
    band: The band on which hostapd is being started.
    ssid: The SSID with which hostapd is being started.

  Returns:
    Whether hostapd was started successfully.
  """
  aggfiles = glob.glob('/sys/kernel/debug/ieee80211/phy*/' +
                       'netdev:%s/default_agg_timeout' % interface)
  if not aggfiles:
    # This can happen on non-mac80211 interfaces.
    utils.log('agg_timeout: no default_agg_timeout files for %r', interface)
  else:
    if experiment.enabled('WifiShortAggTimeout'):
      utils.log('Using short agg_timeout.')
      agg = 500
    elif experiment.enabled('WifiNoAggTimeout'):
      utils.log('Disabling agg_timeout.')
      agg = 0
    else:
      utils.log('Using default long agg_timeout.')
      agg = 5000
    for aggfile in aggfiles:
      open(aggfile, 'w').write(str(agg))

  pid_filename = utils.get_filename(
      'hostapd', utils.FILENAME_KIND.pid, interface, tmp=True)
  alivemonitor_filename = utils.get_filename(
      'hostapd', utils.FILENAME_KIND.alive, interface, tmp=True)

  # Don't use alivemonitor on Windcharger since no waveguide. b/32376077
  if _is_wind_charger() or experiment.enabled('WifiNoAliveMonitor'):
    alive_monitor = []
  else:
    alive_monitor = ['alivemonitor', alivemonitor_filename, '30', '2', '65']

  utils.log('Starting hostapd.')
  utils.babysit(alive_monitor +
                ['hostapd',
                 '-A', alivemonitor_filename,
                 '-F', _FINGERPRINTS_DIRECTORY] +
                bandsteering.hostapd_options(band, ssid) +
                _hostapd_debug_options() +
                [config_filename],
                'hostapd-%s' % interface, 10, pid_filename)

  # Wait for hostapd to start, and return False if it doesn't.
  i = j = 0
  for i in xrange(100):
    if utils.check_pid(pid_filename):
      break
    sys.stderr.write('.')
    sys.stderr.flush()
    time.sleep(0.1)
  else:
    return False

  # hostapd_cli returns success on command timeouts.  If we time this perfectly
  # and manage to connect but then hostapd dies right after, we'd think it
  # succeeded.  So sleep a bit to try to give hostapd a chance to die from its
  # error before we try to connect to it.
  time.sleep(0.5)
  for j in xrange(100):
    if not utils.check_pid(pid_filename):
      break
    if _is_hostapd_running(interface):
      utils.log('started after %.1fs', (i + j) / 10.0)
      return True
    sys.stderr.write('.')
    sys.stderr.flush()
    time.sleep(0.1)

  utils.log('failed after %.1fs', (i + j) / 10.0)
  return False


def _get_wpa_state(interface):
  for line in utils.subprocess_lines(('wpa_cli', '-i', interface, 'status')):
    tokens = line.split('=')
    if tokens and tokens[0] == 'wpa_state':
      return tokens[1]


def _get_wpa_band(interface):
  for line in utils.subprocess_lines(('wpa_cli', '-i', interface, 'status')):
    tokens = line.split('=')
    if tokens and tokens[0] == 'freq':
      try:
        return {'5': '5', '2': '2.4'}[tokens[1][0]]
      except (IndexError, KeyError):
        return None


def _wait_for_wpa_supplicant_to_associate(interface):
  """Wait for wpa_supplicant to associate.

  If it does not associate within a certain period of time, terminate it.

  Args:
    interface: The interface on which wpa_supplicant is running.

  Raises:
    BinWifiException: if wpa_supplicant fails to associate and
    also cannot be stopped to cleanup after the failure.

  Returns:
    Whether wpa_supplicant associated within the timeout.
  """
  utils.log('Waiting for wpa_supplicant to connect')
  i = 0
  for i in xrange(100):
    if _get_wpa_state(interface) == 'COMPLETED':
      utils.log('wpa_supplicant associated after %.1fs', i / 10.0)
      return True
    sys.stderr.write('.')
    sys.stderr.flush()
    time.sleep(0.1)

  utils.log('wpa_supplicant did not connect after %.1fs.', i / 10.0)
  if not _stop_wpa_supplicant(interface):
    raise utils.BinWifiException(
        "Couldn't stop wpa_supplicant after it failed to connect.  "
        "Consider killing it manually.")

  return False


def _start_wpa_supplicant(interface, config_filename):
  """Starts a babysat wpa_supplicant.

  Args:
    interface: The interface on which to start wpa_supplicant.
    config_filename: The filename of the wpa_supplicant configuration.

  Raises:
    BinWifiException: if wpa_supplicant fails to connect and
    also cannot be stopped to cleanup after the failure.

  Returns:
    Whether wpa_supplicant was started successfully.
  """
  pid_filename = utils.get_filename(
      'wpa_supplicant', utils.FILENAME_KIND.pid, interface, tmp=True)

  utils.log('Starting wpa_supplicant.')
  utils.babysit(['wpa_supplicant',
                 '-Dnl80211',
                 '-i', interface,
                 '-c', config_filename] +
                _hostapd_debug_options(),
                'wpa_supplicant-%s' % interface, 10, pid_filename)

  # Wait for wpa_supplicant to start, and return False if it doesn't.
  i = j = 0
  for i in xrange(100):
    if utils.check_pid(pid_filename):
      break
    sys.stderr.write('.')
    sys.stderr.flush()
    time.sleep(0.1)
  else:
    return False

  # wpa_supplicant_cli returns success on command timeouts.  If we time this
  # perfectly and manage to connect but then wpa_supplicant dies right after,
  # we'd think it succeeded.  So sleep a bit to try to give wpa_supplicant a
  # chance to die from its error before we try to connect to it.
  time.sleep(0.5)

  for j in xrange(50):
    if not utils.check_pid(pid_filename):
      utils.log('wpa_supplicant process died.')
      return False
    if _is_wpa_supplicant_running(interface):
      utils.log('wpa_supplicant process started after %.1fs', (i + j) / 10.0)
      break
    sys.stderr.write('.')
    sys.stderr.flush()
    time.sleep(0.1)
  else:
    utils.log('failed after %.1fs', (i + j) / 10.0)
    return False

  return _wait_for_wpa_supplicant_to_associate(interface)


def _maybe_restart_hostapd(interface, config, opt):
  """Starts or restarts hostapd unless doing so would be a no-op.

  The no-op case (i.e. hostapd is already running with an equivalent config) can
  be overridden with --force-restart.

  Args:
    interface: The interface on which to start hostapd.
    config: A hostapd configuration, as a string.
    opt: The OptDict parsed from command line options.

  Returns:
    Whether hostapd was started successfully.

  Raises:
    BinWifiException: On various errors.
  """
  tmp_config_filename = utils.get_filename(
      'hostapd', utils.FILENAME_KIND.config, interface, tmp=True)
  forced = False
  current_config = None

  try:
    with open(tmp_config_filename) as tmp_config_file:
      current_config = tmp_config_file.read()
  except IOError:
    pass

  if not _is_hostapd_running(interface):
    utils.log('hostapd not running yet, starting.')
  elif current_config != config:
    utils.log('hostapd config changed, restarting.')
  elif opt.force_restart:
    utils.log('Forced restart requested.')
    forced = True
  else:
    utils.log('hostapd-%s already configured and running', interface)
    return True

  if not _stop_hostapd(interface):
    raise utils.BinWifiException("Couldn't stop hostapd")

  # Set or unset 4-address mode.  This has to be done while hostapd is down.
  utils.log('%s 4-address mode', 'Enabling' if opt.wds else 'Disabling')
  iw.set_4address_mode(interface, opt.wds)

  # We don't want to try to rewrite this file if this is just a forced restart.
  if not forced:
    utils.atomic_write(tmp_config_filename, config)

  if not _start_hostapd(interface, tmp_config_filename, opt.band, opt.ssid):
    utils.log('hostapd failed to start.  Look at hostapd logs for details.')
    return False

  return True


def _restart_hostapd(interface, *overrides):
  """Restart hostapd from previous options.

  Only used by _set_wpa_supplicant_config, to restart hostapd after stopping it.

  Args:
    interface: The interface on which to restart hostapd.
    *overrides:  A list of options to override the pre-existing ones.

  Returns:
    Whether hostapd was successfully restarted.

  Raises:
    BinWifiException: If reading previous settings fails.
  """
  argv = persist.load_options('hostapd', interface, True) + list(overrides)

  if argv is None:
    raise utils.BinWifiException('Failed to read previous hostapd config')

  _run(argv)


def _set_wpa_supplicant_config(interface, config, opt):
  """Starts or restarts wpa_supplicant unless doing so would be a no-op.

  The no-op case (i.e. wpa_supplicant is already running with an equivalent
  config) can be overridden with --force-restart.

  Args:
    interface: The interface on which to start wpa_supplicant.
    config: A wpa_supplicant configuration, as a string.
    opt: The OptDict parsed from command line options.

  Returns:
    Whether wpa_supplicant was started successfully.

  Raises:
    BinWifiException: On various errors.
  """
  tmp_config_filename = utils.get_filename(
      'wpa_supplicant', utils.FILENAME_KIND.config, interface, tmp=True)
  forced = False
  current_config = None
  band = opt.band

  try:
    with open(tmp_config_filename) as tmp_config_file:
      current_config = tmp_config_file.read()
  except IOError:
    pass

  already_running = _is_wpa_supplicant_running(interface)
  if not already_running:
    utils.log('wpa_supplicant not running yet, starting.')
  elif current_config != config:
    # TODO(rofrankel): Consider using wpa_cli reconfigure here.
    utils.log('wpa_supplicant config changed, reconfiguring.')
  elif opt.force_restart:
    utils.log('Forced restart requested.')
    forced = True
  else:
    utils.log('wpa_supplicant-%s already configured and running', interface)
    return True

  if not forced:
    utils.atomic_write(tmp_config_filename, config)

  # TODO(rofrankel): Consider removing all the restart hostapd stuff when
  # b/30140131 is resolved.  hostapd seems to keep working without being
  # restarted, at least on Camaro.
  restart_hostapd = False
  ap_interface = iw.find_interface_from_band(band, iw.INTERFACE_TYPE.ap,
                                             opt.interface_suffix)
  if _is_hostapd_running(ap_interface):
    restart_hostapd = True
    opt_without_persist = options.OptDict({})
    opt_without_persist.persist = False
    opt_without_persist.band = opt.band
    opt_without_persist.interface_suffix = opt.interface_suffix
    if not stop_ap_wifi(opt_without_persist):
      raise utils.BinWifiException(
          "Couldn't stop hostapd to start wpa_supplicant.")

  if already_running:
    subprocess.check_call(['ifdown', interface])
    subprocess.check_call(['/etc/ifplugd/ifplugd.action', interface, 'down'])
    if not _reconfigure_wpa_supplicant(interface):
      raise utils.BinWifiException('Failed to reconfigure wpa_supplicant.')
    subprocess.check_call(['ifup', interface])
    subprocess.check_call(['/etc/ifplugd/ifplugd.action', interface, 'up'])
  elif not _start_wpa_supplicant(interface, tmp_config_filename):
    raise utils.BinWifiException(
        'wpa_supplicant failed to start.  Look at wpa_supplicant logs for '
        'details.')

  if restart_hostapd:
    _restart_hostapd(ap_interface)

  return True


@iw.requires_iw
def client_interface_name(phy, interface_suffix):
  ap_interface = iw.find_interface_from_phy(phy, iw.INTERFACE_TYPE.ap,
                                            interface_suffix)
  if ap_interface:
    return 'w%s%s' % (iw.INTERFACE_TYPE.client,
                      re.match(r'[^0-9]+([0-9]+)$', ap_interface).group(1))
  else:
    return None


@iw.requires_iw  # Client mode not supported on Broadcom.
def set_client_wifi(opt):
  """Set up a wifi client in response to the 'setclient' command.

  Args:
    opt: The OptDict parsed from command line options.

  Returns:
    Whether wpa_supplicant successfully started and associated.

  Raises:
    BinWifiException: On various errors.
  """
  if not opt.ssid:
    raise utils.BinWifiException('You must specify an ssid with --ssid')

  band = opt.band
  if band not in ('2.4', '5'):
    raise utils.BinWifiException('You must specify band with -b2.4 or -b5')

  psk = os.environ.get('WIFI_CLIENT_PSK', None)

  if band == '5' and quantenna.set_client_wifi(opt):
    return True

  mwifiex.set_recovery(experiment.enabled('MwifiexFirmwareRecovery'))

  phy = iw.find_phy(band, 'auto')
  if phy is None:
    utils.log("Couldn't find phy for band %s", band)
    return False

  interface = iw.find_interface_from_phy(phy, iw.INTERFACE_TYPE.client,
                                         opt.interface_suffix)

  if interface is None:
    # Create the client interface if it does not exist, using the same number as
    # an existing AP interface, which is stable across driver reloads.
    interface = client_interface_name(phy, opt.interface_suffix)
    if interface is None:
      raise utils.BinWifiException('AP interface not initialized for %s' % phy)

    if not iw.does_interface_exist(interface):
      utils.log('Creating client interface %s', interface)
      utils.subprocess_quiet(
          ('iw', 'phy', phy, 'interface', 'add', interface, 'type', 'station'),
          no_stdout=True)
      ap_mac_address = utils.get_mac_address_for_interface(
          iw.find_interface_from_phy(
              phy, iw.INTERFACE_TYPE.ap,
              opt.interface_suffix))
      mac_address = utils.increment_mac_address(ap_mac_address)
      subprocess.check_call(
          ('ip', 'link', 'set', interface, 'address', mac_address))

  wpa_config = configs.generate_wpa_supplicant_config(opt.ssid, psk, opt)
  if not _set_wpa_supplicant_config(interface, wpa_config, opt):
    return False

  return True


@iw.requires_iw  # Client mode not supported on Broadcom.
def stop_client_wifi(opt):
  """Disable a wifi client in response to the 'stopclient' command.

  Args:
    opt: The OptDict parsed from command line options.

  Returns:
    Whether wpa_supplicant was successfully stopped on all bands specified by
    opt.band.
  """
  success = True
  for band in opt.band.split():
    utils.log('stopping client for %s GHz...', band)

    if band == '5' and quantenna.stop_client_wifi(opt):
      continue

    interfaces = iw.find_interfaces_from_band_and_suffix(
        band, opt.interface_suffix, iw.INTERFACE_TYPE.client)
    if not interfaces:
      utils.log('No client interfaces for %s GHz; nothing to stop', band)
      continue

    for interface in interfaces:
      if _stop_wpa_supplicant(interface):
        if opt.persist:
          persist.delete_options('wpa_supplicant', interface)
      else:
        utils.log('Failed to stop wpa_supplicant on interface %s', interface)
        success = False

  return success


def stringify_options(optdict):
  for option in ('channel', 'width', 'band', 'ssid'):
    optdict.__setitem__(option, str(optdict.__getitem__(option)))


def _run(argv):
  """Runs a wifi command.

  This is the main entry point of the script, and is called by the main function
  and also by commands which need to run other commands (e.g. restore).

  Args:
    argv: A list containing a command and a series of options, e.g.
    sys.argv[1:].

  Returns:
    Whether the command succeeded.

  Raises:
    BinWifiException: On file write failures.
  """
  global lockfile_taken

  serial = 'UNKNOWN'
  try:
    serial = subprocess.check_output(('serial')).strip()
  except subprocess.CalledProcessError:
    utils.log('Could not get serial number')

  optspec = _OPTSPEC_FORMAT.format(
      bin=__file__.split('/')[-1], ssid='%s_TestWifi' % serial)
  parser = options.Options(optspec)
  opt, _, extra = parser.parse(argv)
  stringify_options(opt)

  if not extra:
    parser.fatal('Must specify a command (see usage for details).')
    return 1

  command = extra[0]

  # set and setclient have a different default for -b.
  if command.startswith('set') and ' ' in opt.band:
    opt.band = '2.4'

  if command == 'off' or command.startswith('stop'):
    if not opt.interface_suffix:
      opt.interface_suffix = 'ALL'
    elif opt.interface_suffix == 'NONE':
      opt.interface_suffix = ''

  try:
    function = {
        'set': set_wifi,
        'stop': stop_wifi,
        'off': stop_wifi,
        'restore': restore_wifi,
        'show': show_wifi,
        'setclient': set_client_wifi,
        'stopclient': stop_client_wifi,
        'stopap': stop_ap_wifi,
        'scan': scan_wifi,
    }[command]
  except KeyError:
    parser.fatal('Unrecognized command %s' % command)

  read_only_commands = ('show',)

  if command not in read_only_commands:
    if not lockfile_taken:
      utils.lock(_LOCKFILE, int(opt.lock_timeout))
      atexit.register(utils.unlock, _LOCKFILE)
      lockfile_taken = True

  success = function(opt)

  if success:
    if command in ('set', 'setclient'):
      if command == 'set':
        program = 'hostapd'
        interface_type = iw.INTERFACE_TYPE.ap
      else:
        program = 'wpa_supplicant'
        interface_type = iw.INTERFACE_TYPE.client
      interface = iw.find_interface_from_band(opt.band, interface_type,
                                              opt.interface_suffix)
      if opt.persist:
        # Save in /fiber/config.
        persist.save_options(program, interface, argv, False)
      # Save in /tmp.
      persist.save_options(program, interface, argv, True)

  return success


def main():
  try:
    return 0 if _run(sys.argv[1:]) else 1
  except iw.RequiresIwException as e:
    utils.log('NOOP: %s', e)
    return 0
  except utils.BinWifiException as e:
    utils.log('FATAL: %s', e)
    return 99
  except subprocess.CalledProcessError as e:
    utils.log('FATAL: subprocess failed unexpectedly: %s', e)
    return 99


if __name__ == '__main__':
  sys.stdout = os.fdopen(1, 'w', 1)  # force line buffering even if redirected
  sys.stderr = os.fdopen(2, 'w', 1)  # force line buffering even if redirected
  sys.exit(main())
