#!/usr/bin/python

"""Utility for controlling WiFi AP and client functionality."""

from __future__ import print_function

import os
import re
import subprocess
import sys
import time

import options

import autochannel
import bandsteering
import configs
import experiment
import iw
import persist
import utils


_OPTSPEC = """
{bin} set           Enable or modify access points.  Takes all options unless otherwise specified.
{bin} setclient     Enable or modify wifi clients.  Takes -b, -P, -s, -S.
{bin} stop|off      Disable access points and clients.  Takes -b, -P, -S.
{bin} stopap        Disable access points.  Takes -b, -P, -S.
{bin} stopclient    Disable wifi clients.  Takes -b, -P, -S.
{bin} restore       Restore saved client and access point options.  Takes -b, -S.
{bin} show          Print all known parameters.  Takes -b, -S.
--
b,band=                           Wifi band(s) to use (5 GHz and/or 2.4 GHz).  set commands have a default of 2.4 and cannot take multiple-band values.  [2.4 5]
c,channel=                        Channel to use [auto]
a,autotype=                       Autochannel method to use (LOW, HIGH, DFS, NONDFS, ANY,OVERLAP) [NONDFS]
s,ssid=                           SSID to use [{ssid}]
e,encryption=                     Encryption type to use (WPA_PSK_AES, WPA2_PSK_AES, WPA12_PSK_AES, WPA_PSK_TKIP, WPA2_PSK_TKIP, WPA12_PSK_TKIP, WEP, or NONE) [WPA2_PSK_AES]
f,force-restart                   Force restart even if already running with these options
H,hidden-mode                     Enable hidden mode (disable SSID advertisements)
M,enable-wmm                      Enable wmm extensions (needed for block acks)
G,short-guard-interval            Enable short guard interval
p,protocols=                      802.11 levels to allow, slash-delimited [a/b/g/n/ac]
w,width=                          Channel width to use, in MHz (20, 40, or 80) [20]
B,bridge=                         Bridge device to use [br0]
X,extra-short-timeout-intervals   Use extra short timeout intervals for stress testing
P,persist                         For set commands, persist options so we can restore them with 'wifi restore'.  For stop commands, remove persisted options.
S,interface-suffix=               Interface suffix []
""".format(bin=__file__.split('/')[-1],
           ssid='%s_TestWifi' % subprocess.check_output(('serial')).strip())

_FINGERPRINTS_DIRECTORY = '/tmp/wifi/fingerprints'

experiment.register('NoSwapWifiPrimaryChannel')  # checked by hostapd itself
experiment.register('NoAutoNarrowWifiChannel')  # checked by hostapd itself
experiment.register('Wifi80211k')
experiment.register('WifiBandsteering')
experiment.register('WifiReverseBandsteering')
experiment.register('WifiHostapdLogging')


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

  utils.validate_set_wifi_options(
      band, width, autotype, protocols, opt.encryption)

  psk = None
  if opt.encryption == 'WEP' or '_PSK_' in opt.encryption:
    psk = os.environ['WIFI_PSK']

  if iw.RUNNABLE_WL() and not iw.RUNNABLE_IW():
    _set_wifi_broadcom(opt)
    return True

  if not iw.RUNNABLE_IW():
    raise utils.BinWifiException('Can\'t proceed without iw')

  # If this phy is running client mode, we need to use its width/channel.
  phy = iw.find_phy(band, channel)
  if phy is None:
    raise utils.BinWifiException(
        'no wifi phy for band=%s channel=%s', band, channel)

  client_interface = iw.find_interface_from_phy(
      phy, iw.INTERFACE_TYPE.client, opt.interface_suffix)
  if (client_interface is not None and
      _is_wpa_supplicant_running(client_interface)):
    # Wait up to ten seconds for client width and channel to be available (only
    # relevant if client was started recently).
    # TODO(rofrankel): Consider shortcutting this loop if wpa_cli shows status
    # is SCANNING (and other values)?
    utils.log('Client running on same band; finding its width and channel.')
    for _ in xrange(50):
      client_band = _get_wpa_band(client_interface)
      client_width, client_channel = iw.find_width_and_channel(client_interface)
      sys.stderr.write('.')
      if None not in (client_band, client_width, client_channel):
        band, width, channel = client_band, client_width, client_channel
        utils.log('Using band=%s, channel=%s, width=%s MHz from client',
                  band, channel, width)
        break
      time.sleep(0.2)
    else:
      utils.log('Couldn\'t find band, width, and channel used by client '
                '(it may not be connected)')

  interface = iw.find_interface_from_phy(
      phy, iw.INTERFACE_TYPE.ap, opt.interface_suffix)
  if interface is None:
    raise utils.BinWifiException(
        'no wifi interface for band=%s channel=%s', band, channel)

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

    interface = iw.find_interface_from_band(
        band, iw.INTERFACE_TYPE.ap, opt.interface_suffix)
    if interface is None:
      raise utils.BinWifiException('No AP interface for band=\'%s\'', band)

    if opt.persist:
      persist.delete_options('hostapd', band)

    success &= _stop_hostapd(interface)

  return success


@iw.requires_iw
def _restore_wifi(band, program):
  """Restore a program from persisted settings.

  Args:
    band: The band on which to restore program.
    program: The program to restore (wpa_supplicant or hostapd).

  Returns:
    Whether whether restoring succeeded, but may die.
  """
  argv = persist.load_options(program, band, False)
  if argv is None:
    utils.log('No persisted options for %s GHz %s, not restoring',
              band, program)
    return False

  utils.log('Loaded persisted options for %s GHz %s', band, program)

  if _run(argv):
    utils.log('Restored %s for %s GHz', program, band)
    return True

  utils.log('Failed to restore %s for %s GHz', program, band)
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
  for band in sorted(opt.band.split(),
                     reverse=not experiment.enabled('WifiReverseBandsteering')):
    _restore_wifi(band, 'wpa_supplicant')
    _restore_wifi(band, 'hostapd')

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
    interface = iw.find_interface_from_band(
        band, iw.INTERFACE_TYPE.ap, opt.interface_suffix)
    if interface is None:
      continue

    print('Band: %s' % band)
    for tokens in utils.subprocess_line_tokens(('iw', 'reg', 'get')):
      if len(tokens) >= 2 and tokens[0] == 'country':
        print('RegDomain: %s' % tokens[1].strip(':'))
        break

    info_parsed = iw.info_parsed(interface)
    for k, label in (('channel', 'Channel'),
                     ('ssid', 'SSID'),
                     ('addr', 'BSSID')):
      v = info_parsed.get(k, None)
      if v is not None:
        print('%s: %s' % (label, v))

    print('AutoChannel: %r' % os.path.exists('/tmp/autochan.%s' % interface))
    try:
      with open('/tmp/autotype.%s' % interface) as autotype:
        print('AutoType: %s' % autotype.read().strip())
    except IOError:
      pass

    print('Station List for band: %s' % band)
    station_dump = iw.station_dump(interface)
    if station_dump:
      print(station_dump)

  return True


def _is_hostapd_running(interface):
  return utils.subprocess_quiet(
      ('hostapd_cli', '-i', interface, 'status'), no_stdout=True) == 0


def _is_wpa_supplicant_running(interface):
  return utils.subprocess_quiet(
      ('wpa_cli', '-i', interface, 'status'), no_stdout=True) == 0


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
  pid_filename = utils.get_filename(
      'hostapd', utils.FILENAME_KIND.pid, interface, tmp=True)
  alivemonitor_filename = utils.get_filename(
      'hostapd', utils.FILENAME_KIND.alive, interface, tmp=True)

  utils.log('Starting hostapd.')
  utils.babysit(('alivemonitor', alivemonitor_filename, '30', '2', '65',
                 'hostapd',
                 '-A', alivemonitor_filename,
                 '-F', _FINGERPRINTS_DIRECTORY) +
                bandsteering.hostapd_options(band, ssid) +
                (config_filename,),
                'hostapd-%s' % interface, 10, pid_filename)

  # Wait for hostapd to start, and return False if it doesn't.
  for _ in xrange(40):
    if utils.check_pid(pid_filename):
      break
    sys.stderr.write('.')
    time.sleep(0.1)
  else:
    return False

  # hostapd_cli returns success on command timeouts.  If we time this
  # perfectly and manage to connect but then wpa_supplicant dies right after,
  # we'd think it succeeded.  So sleep a bit to try to give wpa_supplicant a
  # chance to die from its error before we try to connect to it.
  time.sleep(0.5)
  for _ in xrange(10):
    if not utils.check_pid(pid_filename):
      break
    if _is_hostapd_running(interface):
      utils.log('ok')
      return True
    sys.stderr.write('.')
    time.sleep(0.1)

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
      except KeyError:
        return None


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
  utils.babysit(('wpa_supplicant', '-Dnl80211', '-i', interface,
                 '-c', config_filename),
                'wpa_supplicant-%s' % interface, 10, pid_filename)

  # Wait for wpa_supplicant to start, and return False if it doesn't.
  for _ in xrange(10):
    if utils.check_pid(pid_filename):
      break
    sys.stderr.write('.')
    time.sleep(0.1)
  else:
    return False

  # wpa_supplicant_cli returns success on command timeouts.  If we time this
  # perfectly and manage to connect but then wpa_supplicant dies right after,
  # we'd think it succeeded.  So sleep a bit to try to give wpa_supplicant a
  # chance to die from its error before we try to connect to it.
  time.sleep(0.5)

  for _ in xrange(50):
    if not utils.check_pid(pid_filename):
      utils.log('wpa_supplicant process died.')
      return False
    if _is_wpa_supplicant_running(interface):
      break
    sys.stderr.write('.')
    time.sleep(0.1)
  else:
    return False

  utils.log('Waiting for wpa_supplicant to connect')
  for _ in xrange(100):
    if _get_wpa_state(interface) == 'COMPLETED':
      utils.log('ok')
      return True
    sys.stderr.write('.')
    time.sleep(0.1)

  utils.log('wpa_supplicant did not connect.')
  if not _stop_wpa_supplicant(interface):
    raise utils.BinWifiException(
        "Couldn't stop wpa_supplicant after it failed to connect.  "
        "Consider killing it manually.")

  return False


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

  # We don't want to try to rewrite this file if this is just a forced restart.
  if not forced:
    utils.atomic_write(tmp_config_filename, config)

  if not _start_hostapd(interface, tmp_config_filename, opt.band, opt.ssid):
    utils.log('hostapd failed to start.  Look at hostapd logs for details.')
    return False

  return True


def _restart_hostapd(band):
  """Restart hostapd from previous options.

  Only used by _maybe_restart_wpa_supplicant, to restart hostapd after stopping
  it.

  Args:
    band: The band on which to restart hostapd.

  Returns:
    Whether hostapd was successfully restarted.

  Raises:
    BinWifiException: If reading previous settings fails.
  """
  argv = persist.load_options('hostapd', band, True)

  if argv is None:
    raise utils.BinWifiException('Failed to read previous hostapd config')

  _run(argv)


def _maybe_restart_wpa_supplicant(interface, config, opt):
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

  if not _is_wpa_supplicant_running(interface):
    utils.log('wpa_supplicant not running yet, starting.')
  elif current_config != config:
    # TODO(rofrankel): Consider using wpa_cli reconfigure here.
    utils.log('wpa_supplicant config changed, restarting.')
  elif opt.force_restart:
    utils.log('Forced restart requested.')
    forced = True
  else:
    utils.log('wpa_supplicant-%s already configured and running', interface)
    return True

  if not _stop_wpa_supplicant(interface):
    raise utils.BinWifiException("Couldn't stop wpa_supplicant")

  if not forced:
    utils.atomic_write(tmp_config_filename, config)

  restart_hostapd = False
  ap_interface = iw.find_interface_from_band(band, iw.INTERFACE_TYPE.ap,
                                             opt.interface_suffix)
  if _is_hostapd_running(ap_interface):
    restart_hostapd = True
    opt_without_persist = options.OptDict({})
    opt_without_persist.persist = False
    opt_without_persist.band = opt.band
    # Code review: Will AP and client always have the same suffix?
    opt_without_persist.interface_suffix = opt.interface_suffix
    if not stop_ap_wifi(opt_without_persist):
      raise utils.BinWifiException(
          'Couldn\'t stop hostapd to start wpa_supplicant.')

  if not _start_wpa_supplicant(interface, tmp_config_filename):
    raise utils.BinWifiException(
        'wpa_supplicant failed to start.  Look at wpa_supplicant logs for '
        'details.')

  if restart_hostapd:
    _restart_hostapd(band)

  return True


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

  if 'WIFI_CLIENT_PSK' not in os.environ:
    raise utils.BinWifiException('WIFI_CLIENT_PSK must be in the environment')

  phy = iw.find_phy(band, 'auto')
  if phy is None:
    utils.log('Couldn\'t find phy for band %s', band)
    return False

  interface = iw.find_interface_from_phy(phy, iw.INTERFACE_TYPE.client,
                                         opt.interface_suffix)

  if interface is None:
    # Create the client interface if it does not exist.
    # TODO(rofrankel):  Create client interfaces outside this script, so that
    # they don't depend on the phynum, which occasionally changes.
    interface = 'w%s%s' % (iw.INTERFACE_TYPE.client,
                           filter(None, re.split(r'(\d+)', phy))[-1])
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

  wpa_config = configs.generate_wpa_supplicant_config(
      opt.ssid, os.environ['WIFI_CLIENT_PSK'])
  if not _maybe_restart_wpa_supplicant(interface, wpa_config, opt):
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

    interface = iw.find_interface_from_band(
        band, iw.INTERFACE_TYPE.client, opt.interface_suffix)
    if interface is not None:
      if not _stop_wpa_supplicant(interface):
        utils.log('Failed to stop wpa_supplicant on interface %s', interface)
        success = False
    else:
      utils.log('No client interface for %s GHz; nothing to stop', band)

    if opt.persist:
      persist.delete_options('wpa_supplicant', band)

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
  parser = options.Options(_OPTSPEC)
  opt, _, extra = parser.parse(argv)
  stringify_options(opt)

  if not extra:
    parser.fatal('Must specify a command (see usage for details).')
    return 1

  # set and setclient have a different default for -b.
  if extra[0].startswith('set') and ' ' in opt.band:
    opt.band = '2.4'

  if extra[0] == 'set':
    if opt.persist:
      persist.save_options('hostapd', opt.band, argv)
    persist.save_options('hostapd', opt.band, argv, tmp=True)

  if extra[0] == 'setclient' and opt.persist:
    persist.save_options('wpa_supplicant', opt.band, argv)

  try:
    return {
        'set': set_wifi,
        'stop': stop_wifi,
        'off': stop_wifi,
        'restore': restore_wifi,
        'show': show_wifi,
        'setclient': set_client_wifi,
        'stopclient': stop_client_wifi,
        'stopap': stop_ap_wifi,
    }[extra[0]](opt)
  except KeyError:
    parser.fatal('Unrecognized command %s' % extra[0])


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
