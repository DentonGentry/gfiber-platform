#!/usr/bin/python

"""Manages a device's wired and wireless connections to the LAN."""

import collections
import errno
import glob
import json
import logging
import os
import re
import subprocess
import time

import pyinotify

import cycler
import interface
import iw

GFIBER_OUIS = ['f4:f5:e8']
VENDOR_IE_FEATURE_ID_AUTOPROVISIONING = '01'


class FileChangeHandler(pyinotify.ProcessEvent):
  """Connects pyinotify events to ConnectionManager."""

  def __init__(self, connection_manager, *args, **kwargs):
    self._connection_manager = connection_manager
    super(FileChangeHandler, self).__init__(*args, **kwargs)

  def process_IN_DELETE(self, event):
    self._connection_manager.handle_event(event.path, event.name, True)

  def process_IN_CLOSE_WRITE(self, event):
    self._connection_manager.handle_event(event.path, event.name, False)

  def process_IN_MOVED_TO(self, event):
    self.process_IN_CLOSE_WRITE(event)

  def process_IN_MOVED_FROM(self, event):
    self.process_IN_DELETE(event)

  def check_events(self, *args, **kwargs):
    result = super(FileChangeHandler, self).check_events(*args, **kwargs)
    return result


class WLANConfiguration(object):
  """Represents a WLAN configuration from cwmpd."""

  WIFI_STOPAP = ['wifi', 'stopap']
  WIFI_SETCLIENT = ['wifi', 'setclient']
  WIFI_STOPCLIENT = ['wifi', 'stopclient']

  def __init__(self, band, wifi, command_lines):
    self.band = band
    self.wifi = wifi
    self.command = command_lines.splitlines()
    self.access_point_up = False
    self.client_up = False
    self.ssid = None
    self.passphrase = None
    self.interface_suffix = None
    self.access_point = None

    binwifi_option_attrs = {
        '-s': 'ssid',
        '--ssid': 'ssid',
        '-S': 'interface_suffix',
        '--interface_suffix': 'interface_suffix',
    }
    attr = None
    for line in self.command:
      if attr is not None:
        setattr(self, attr, line)
        attr = None
        continue

      attr = binwifi_option_attrs.get(line, None)

      if line.startswith('WIFI_PSK='):
        self.passphrase = line.split('WIFI_PSK=')[-1]

    if self.ssid is None:
      raise ValueError('Command file does not specify SSID')

  def start_access_point(self):
    """Start an access point."""

    if not self.access_point or self.access_point_up:
      return

    # Since we only run an access point if we have a wired connection anyway, we
    # don't want to constrain the AP's channel to the channel of the AP we are
    # connected to.  This line should go away when we have wireless repeaters.
    self.stop_client()

    try:
      subprocess.check_call(self.command, stderr=subprocess.STDOUT)
      self.access_point_up = True
    except subprocess.CalledProcessError as e:
      logging.error('Failed to start access point: %s', e.output)

  def stop_access_point(self):
    if not self.access_point_up:
      return

    command = self.WIFI_STOPAP + ['--band', self.band]
    if self.interface_suffix:
      command += ['--interface_suffix', self.interface_suffix]

    try:
      subprocess.check_call(command, stderr=subprocess.STDOUT)
      self.access_point_up = False
    except subprocess.CalledProcessError as e:
      logging.error('Failed to stop access point: %s', e.output)
      return

  def start_client(self):
    """Join the WLAN as a client."""
    if self.client_up:
      return

    self.wifi.detach_wpa_control()

    command = self.WIFI_SETCLIENT + ['--ssid', self.ssid, '--band', self.band]
    env = dict(os.environ)
    if self.passphrase:
      env['WIFI_CLIENT_PSK'] = self.passphrase
    try:
      subprocess.check_call(command, stderr=subprocess.STDOUT, env=env)
      self.client_up = True
      logging.info('Successfully joined WLAN')
    except subprocess.CalledProcessError as e:
      logging.error('Failed to start wifi client: %s', e.output)

  def stop_client(self):
    if not self.client_up:
      return

    self.wifi.detach_wpa_control()

    try:
      subprocess.check_call(self.WIFI_STOPCLIENT + ['-b', self.band],
                            stderr=subprocess.STDOUT)
      self.client_up = False
    except subprocess.CalledProcessError as e:
      logging.error('Failed to stop wifi client: %s', e.output)


class ConnectionManager(object):
  """Manages wired and wireless connections to the LAN."""

  # pylint: disable=invalid-name
  Bridge = interface.Bridge
  Wifi = interface.Wifi
  WLANConfiguration = WLANConfiguration

  ETHERNET_STATUS_FILE = 'eth0'
  WLAN_FILE_REGEXP_FMT = r'^%s((?P<interface_suffix>.*)\.)?(?P<band>2\.4|5)$'
  COMMAND_FILE_PREFIX = 'command.'
  ACCESS_POINT_FILE_PREFIX = 'access_point.'
  COMMAND_FILE_REGEXP = WLAN_FILE_REGEXP_FMT % COMMAND_FILE_PREFIX
  ACCESS_POINT_FILE_REGEXP = WLAN_FILE_REGEXP_FMT % ACCESS_POINT_FILE_PREFIX
  GATEWAY_FILE_PREFIX = 'gateway.'
  MOCA_NODE_FILE_PREFIX = 'node'
  WIFI_SETCLIENT = ['wifi', 'setclient']
  IFUP = ['ifup']
  IP_LINK = ['ip', 'link']
  IFPLUGD_ACTION = ['/etc/ifplugd/ifplugd.action']

  def __init__(self,
               bridge_interface='br0',
               status_dir='/tmp/conman',
               moca_status_dir='/tmp/cwmp/monitoring/moca2',
               wpa_control_interface='/var/run/wpa_supplicant',
               run_duration_s=1, interface_update_period=5,
               wifi_scan_period_s=120, wlan_retry_s=15, acs_update_wait_s=10):

    self.bridge = self.Bridge(bridge_interface, '10')

    # If we have multiple wcli interfaces, 5 GHz-only < both < 2.4 GHz-only.
    def metric_for_bands(bands):
      if '5' in bands:
        if '2.4' in bands:
          return interface.METRIC_24GHZ_5GHZ
        return interface.METRIC_5GHZ
      return interface.METRIC_24GHZ

    self.wifi = sorted([self.Wifi(interface_name, metric_for_bands(bands),
                                  # Prioritize 5 GHz over 2.4.
                                  bands=sorted(bands, reverse=True))
                        for interface_name, bands
                        in get_client_interfaces().iteritems()],
                       key=lambda w: w.metric)

    self._status_dir = status_dir
    self._interface_status_dir = os.path.join(status_dir, 'interfaces')
    self._moca_status_dir = moca_status_dir
    self._wpa_control_interface = wpa_control_interface
    self._run_duration_s = run_duration_s
    self._interface_update_period = interface_update_period
    self._wifi_scan_period_s = wifi_scan_period_s
    for wifi in self.wifi:
      wifi.last_wifi_scan_time = -self._wifi_scan_period_s
    self._wlan_retry_s = wlan_retry_s
    self._acs_update_wait_s = acs_update_wait_s

    self._wlan_configuration = {}

    # It is very important that we know whether ethernet is up.  So if the
    # ethernet file doesn't exist for any reason when conman starts, check
    # explicitly.
    if os.path.exists(os.path.join(self._interface_status_dir,
                                   self.ETHERNET_STATUS_FILE)):
      self._process_file(self._interface_status_dir, self.ETHERNET_STATUS_FILE)
    else:
      ethernet_up = self.is_ethernet_up()
      self.bridge.ethernet = ethernet_up
      self.ifplugd_action('eth0', ethernet_up)

    for path, prefix in ((self._moca_status_dir, self.MOCA_NODE_FILE_PREFIX),
                         (self._status_dir, self.COMMAND_FILE_PREFIX),
                         (self._status_dir, self.GATEWAY_FILE_PREFIX)):
      for filepath in glob.glob(os.path.join(path, prefix + '*')):
        self._process_file(path, os.path.split(filepath)[-1])

    wm = pyinotify.WatchManager()
    wm.add_watch(self._status_dir,
                 pyinotify.IN_CLOSE_WRITE | pyinotify.IN_MOVED_TO |
                 pyinotify.IN_DELETE | pyinotify.IN_MOVED_FROM)
    wm.add_watch(self._interface_status_dir,
                 pyinotify.IN_CLOSE_WRITE | pyinotify.IN_MOVED_TO)
    wm.add_watch(self._moca_status_dir,
                 pyinotify.IN_CLOSE_WRITE | pyinotify.IN_MOVED_TO)
    self.notifier = pyinotify.Notifier(wm, FileChangeHandler(self), timeout=0)

    self._interface_update_counter = 0
    self._try_wlan_after = {'5': 0, '2.4': 0}

  def is_ethernet_up(self):
    """Explicitly check whether ethernet is up.

    Only used on startup, and only if ifplugd file is missing.

    Returns:
      Whether the ethernet link is up.
    """
    try:
      lines = subprocess.check_output(self.IP_LINK).splitlines()
    except subprocess.CalledProcessError as e:
      raise EnvironmentError('Failed to call "ip link": %r', e.message)

    for line in lines:
      if 'eth0' in line and 'LOWER_UP' in line:
        return True

    return False

  def run(self):
    """Run the main loop, and clean up when done."""
    try:
      while True:
        self.run_once()
    finally:
      for wifi in self.wifi:
        wifi.detach_wpa_control()
      self.notifier.stop()

  def run_once(self):
    """Run one iteration of the main loop.

    This includes the following:

    1. Process any changes in watched files.
    2. Check interfaces for changed connectivity, if
       update_interfaces_and_routes is true.
    3. Start, stop, or restart access points as appropriate.  If running an
       access point, skip all remaining wifi steps for that band.
    3. Handle any wpa_supplicant events.
    4. Periodically, perform a wifi scan.
    5. If not connected to the WLAN or to the ACS, try to connect to something.
    6. If connected to the ACS but not the WLAN, and enough time has passed
       since connecting that we should expect a current WLAN configuration, try
       to join the WLAN again.
    7. Sleep for the rest of the duration of _run_duration_s.
    """
    start_time = time.time()
    self.notifier.process_events()
    while self.notifier.check_events():
      self.notifier.read_events()
      self.notifier.process_events()

    self._interface_update_counter += 1
    if self._interface_update_counter == self._interface_update_period:
      self._interface_update_counter = 0
      self._update_interfaces_and_routes()

    for wifi in self.wifi:
      continue_wifi = False
      if not wifi.attached():
        logging.debug('Attempting to attach to wpa control interface for %s',
                      wifi.name)
        wifi.attach_wpa_control(self._wpa_control_interface)
      wifi.handle_wpa_events()

      # Only one wlan_configuration per interface will have access_point ==
      # True.  Try 5 GHz first, then 2.4 GHz.  If both bands are supported by
      # the same radio, and the enabled band is changed, then hostapd will be
      # updated by the wifi command, but we should tell any other
      # wlan_configurations that their APs have been disabled.
      for band in wifi.bands:
        wlan_configuration = self._wlan_configuration.get(band, None)
        if continue_wifi:
          if wlan_configuration:
            wlan_configuration.access_point_up = False
          continue

        # Make sure access point is running iff it should be.
        if wlan_configuration:
          self._update_access_point(wlan_configuration)
          # If we are running an AP on this interface, we don't want to make any
          # client connections on it.
          if wlan_configuration.access_point_up:
            continue_wifi = True

      if continue_wifi:
        logging.debug('Running AP on %s, nothing else to do.', wifi.name)
        continue

      # If this interface is connected to the user's WLAN, there is nothing else
      # to do.
      if self._connected_to_wlan(wifi):
        logging.debug('Connected to WLAN on %s, nothing else to do.', wifi.name)
        return

      # This interface is not connected to the WLAN, so scan for potential
      # routes to the ACS for provisioning.
      if (not self.acs() and
          time.time() > wifi.last_wifi_scan_time + self._wifi_scan_period_s):
        logging.debug('Performing scan on %s.', wifi.name)
        self._wifi_scan(wifi)

      # Periodically retry rejoining the WLAN.  If the WLAN configuration is
      # updated then _update_wlan_configuration will do this immediately, but
      # maybe the credentials didn't change and we were just waiting for the AP
      # to restart or something.  Try both bands, so we can fall back to 2.4 in
      # case 5 is unavailable for some reason.
      for band in wifi.bands:
        wlan_configuration = self._wlan_configuration.get(band, None)
        if wlan_configuration and time.time() > self._try_wlan_after[band]:
          logging.debug('Trying to join WLAN on %s.', wifi.name)
          wlan_configuration.start_client()
          if self._connected_to_wlan(wifi):
            logging.debug('Joined WLAN on %s.', wifi.name)
            self._try_wlan_after[band] = 0
            break
          else:
            logging.error('Failed to connect to WLAN on %s.', wifi.name)
            self._try_wlan_after[band] = time.time() + self._wlan_retry_s
      else:
        # If we are aren't on the WLAN, can ping the ACS, and haven't gotten a
        # new WLAN configuration yet, there are two possibilities:
        #
        # 1) The configuration didn't change, and we should retry connecting.
        # 2) cwmpd isn't writing a configuration, possibly because the device
        #    isn't registered to any accounts.
        logging.debug('Unable to join WLAN on %s', wifi.name)
        if self.acs():
          logging.debug('Connected to ACS on %s', wifi.name)
          now = time.time()
          if (self._wlan_configuration and
              hasattr(wifi, 'waiting_for_acs_since')):
            if now - wifi.waiting_for_acs_since > self._acs_update_wait_s:
              logging.info('ACS has not updated WLAN configuration; will retry '
                           ' with old config.')
              for w in self.wifi:
                for b in w.bands:
                  self._try_wlan_after[b] = now
              continue
          # We don't want to want to log this spammily, so do exponential
          # backoff.
          elif (hasattr(wifi, 'complain_about_acs_at')
                and now >= wifi.complain_about_acs_at):
            wait = wifi.complain_about_acs_at - wifi.waiting_for_acs_since
            logging.info('Can ping ACS, but no WLAN configuration for %ds.',
                         wait)
            wifi.complain_about_acs_at += wait
        # If we didn't manage to join the WLAN and we don't have an ACS
        # connection, we should try to establish one.
        else:
          logging.debug('Not connected to ACS on %s', wifi.name)
          self._try_next_bssid(wifi)

    time.sleep(max(0, self._run_duration_s - (time.time() - start_time)))

  def acs(self):
    return self.bridge.acs() or any(wifi.acs() for wifi in self.wifi)

  def internet(self):
    return self.bridge.internet() or any(wifi.internet() for wifi in self.wifi)

  def _update_interfaces_and_routes(self):
    self.bridge.update_routes()
    for wifi in self.wifi:
      wifi.update_routes()

  def handle_event(self, path, filename, deleted):
    if deleted:
      self._handle_deleted_file(path, filename)
    else:
      self._process_file(path, filename)

  def _handle_deleted_file(self, path, filename):
    if path == self._status_dir:
      match = re.match(self.COMMAND_FILE_REGEXP, filename)
      if match:
        band = match.group('band')
        if band in self._wlan_configuration:
          self._remove_wlan_configuration(band)
          return

      match = re.match(self.ACCESS_POINT_FILE_REGEXP, filename)
      if match:
        band = match.group('band')
        if band in self._wlan_configuration:
          self._wlan_configuration[band].access_point = False
          return

  def _remove_wlan_configuration(self, band):
    config = self._wlan_configuration[band]
    config.stop_client()
    config.stop_access_point()
    del self._wlan_configuration[band]

  def _process_file(self, path, filename):
    """Process or ignore an updated file in a watched directory."""
    filepath = os.path.join(path, filename)
    try:
      contents = open(filepath).read().strip()
    except IOError as e:
      if e.errno == errno.ENOENT:
        # Logging about failing to open .tmp files results in spammy logs.
        if not filename.endswith('.tmp'):
          logging.error('Not a file: %s', filepath)
        return
      else:
        raise

    if path == self._interface_status_dir:
      if filename == self.ETHERNET_STATUS_FILE:
        try:
          self.bridge.ethernet = bool(int(contents))
        except ValueError:
          logging.error('Status file contents should be 0 or 1, not %s',
                        contents)
          return

    if path == self._status_dir:
      if filename.startswith(self.COMMAND_FILE_PREFIX):
        match = re.match(self.COMMAND_FILE_REGEXP, filename)
        if match:
          band = match.group('band')
          wifi = next(w for w in self.wifi if band in w.bands)
          self._update_wlan_configuration(
              self.WLANConfiguration(band, wifi, contents))
      elif filename.startswith(self.ACCESS_POINT_FILE_PREFIX):
        match = re.match(self.ACCESS_POINT_FILE_REGEXP, filename)
        if match:
          band = match.group('band')
          wifi = next(w for w in self.wifi if band in w.bands)
          if band in self._wlan_configuration:
            self._wlan_configuration[band].access_point = True
      elif filename.startswith(self.GATEWAY_FILE_PREFIX):
        interface_name = filename.split(self.GATEWAY_FILE_PREFIX)[-1]
        ifc = self.interface_by_name(interface_name)
        if ifc:
          ifc.set_gateway_ip(contents)
          logging.info('Received gateway %r for interface %s', contents,
                       ifc.name)

    elif path == self._moca_status_dir:
      match = re.match(r'^%s\d+$' % self.MOCA_NODE_FILE_PREFIX, filename)
      if match:
        try:
          json_contents = json.loads(contents)
        except ValueError:
          logging.error('Cannot parse %s as JSON.', filepath)
          return
        node = json_contents['NodeId']
        had_moca = self.bridge.moca
        if json_contents.get('RxNBAS', 0) == 0:
          self.bridge.remove_moca_station(node)
        else:
          self.bridge.add_moca_station(node)
        has_moca = self.bridge.moca
        if had_moca != has_moca:
          self.ifplugd_action('moca0', has_moca)

  def interface_by_name(self, interface_name):
    for ifc in [self.bridge] + self.wifi:
      if ifc.name == interface_name:
        return ifc

  def ifplugd_action(self, interface_name, up):
    subprocess.call(self.IFPLUGD_ACTION + [interface_name,
                                           'up' if up else 'down'])

  def _wifi_scan(self, wifi):
    """Perform a wifi scan and update wifi.cycler."""
    logging.info('Scanning on %s...', wifi.name)
    wifi.last_wifi_scan_time = time.time()
    subprocess.call(self.IFUP + [wifi.name])
    with_ie, without_ie = self._find_bssids(wifi.name)
    logging.info('Done scanning on %s', wifi.name)
    items = [(bss_info, 3) for bss_info in with_ie]
    items += [(bss_info, 1) for bss_info in without_ie]
    wifi.cycler = cycler.AgingPriorityCycler(cycle_length_s=30, items=items)

  def _find_bssids(self, wcli):
    def supports_autoprovisioning(oui, vendor_ie):
      if oui not in GFIBER_OUIS:
        return False

      return vendor_ie.startswith(VENDOR_IE_FEATURE_ID_AUTOPROVISIONING)

    return iw.find_bssids(wcli, supports_autoprovisioning, False)

  def _try_next_bssid(self, wifi):
    """Attempt to connect to the next BSSID in wifi's BSSID cycler.

    Args:
      wifi: The wifi interface to use.

    Returns:
      Whether connecting to the network succeeded.
    """
    if not hasattr(wifi, 'cycler'):
      return False

    bss_info = wifi.cycler.next()
    if bss_info is not None:
      connected = subprocess.call(self.WIFI_SETCLIENT +
                                  ['--ssid', bss_info.ssid,
                                   '--band', wifi.bands[0],
                                   '--bssid', bss_info.bssid]) == 0
      if connected:
        now = time.time()
        wifi.waiting_for_acs_since = now
        wifi.complain_about_acs_at = now + 5
        logging.info('Attempting to provision via SSID %s', bss_info.ssid)
      return connected

    return False

  def _connected_to_wlan(self, wifi):
    return (wifi.wpa_supplicant and
            any(config.client_up for band, config
                in self._wlan_configuration.iteritems()
                if band in wifi.bands))

  def _update_wlan_configuration(self, wlan_configuration):
    band = wlan_configuration.band
    current = self._wlan_configuration.get(band, None)
    if current is None or wlan_configuration.command != current.command:
      if current is not None:
        wlan_configuration.access_point = current.access_point
      else:
        ap_file = os.path.join(
            self._status_dir, self.ACCESS_POINT_FILE_PREFIX +
            (('%s.' % wlan_configuration.interface_suffix)
             if wlan_configuration.interface_suffix else '') + band)
        wlan_configuration.access_point = os.path.exists(ap_file)
      self._wlan_configuration[band] = wlan_configuration
      self._update_access_point(wlan_configuration)

  def _update_access_point(self, wlan_configuration):
    if self.bridge.internet() and wlan_configuration.access_point:
      wlan_configuration.start_access_point()
    else:
      wlan_configuration.stop_access_point()
      wlan_configuration.start_client()


def _wifi_show():
  try:
    return subprocess.check_output(['wifi', 'show'])
  except subprocess.CalledProcessError as e:
    logging.error('Failed to call "wifi show": %s', e)


def get_client_interfaces():
  current_band = None
  result = collections.defaultdict(set)
  for line in _wifi_show().splitlines():
    if line.startswith('Band:'):
      current_band = line.split()[1]
    elif line.startswith('Client Interface:'):
      result[line.split()[2]].add(current_band)

  return result

