#!/usr/bin/python

"""Manages a device's wired and wireless connections to the LAN."""

import collections
import errno
import glob
import json
import logging
import os
import random
import re
import socket
import subprocess
import time

# This is in site-packages on the device, but not when running tests, and so
# raises lint errors.
# pylint: disable=g-bad-import-order
import pyinotify

import cycler
import experiment
import interface
import iw
import ratchet
import status


HOSTNAME = socket.gethostname()
TMP_HOSTS = '/tmp/hosts'
CWMP_PATH = '/tmp/cwmp'

experiment.register('WifiNo2GClient')


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

  WIFI_STOPAP = ['wifi', 'stopap', '--persist']
  WIFI_SETCLIENT = ['wifi', 'setclient', '--persist']
  WIFI_STOPCLIENT = ['wifi', 'stopclient', '--persist']

  def __init__(self, band, wifi, command_lines, wpa_control_interface):
    self.band = band
    self.wifi = wifi
    self.command = command_lines.splitlines()
    self.access_point_up = False
    self.ssid = None
    self.passphrase = None
    self.interface_suffix = None
    self.access_point = None
    self._wpa_control_interface = wpa_control_interface

    binwifi_option_attrs = {
        '-s': 'ssid',
        '--ssid': 'ssid',
        '-S': 'interface_suffix',
        '--interface-suffix': 'interface_suffix',
    }
    attr = None
    for line in self.command:
      if attr is not None:
        setattr(self, attr, line)
        attr = None
        continue

      attr = binwifi_option_attrs.get(line, None)

      if line.startswith('WIFI_PSK='):
        self.passphrase = line[len('WIFI_PSK='):]

    if self.ssid is None:
      raise ValueError('Command file does not specify SSID')

    if self.wifi.initial_ssid == self.ssid:
      logging.info('Connected to WLAN at startup')

  @property
  def client_up(self):
    wpa_status = self.wifi.wpa_status()
    return (wpa_status.get('wpa_state') == 'COMPLETED'
            # NONE indicates we're on a provisioning network; anything else
            # suggests we're already on the WLAN.
            and wpa_status.get('key_mgmt') != 'NONE')

  def start_access_point(self):
    """Start an access point."""

    if not self.access_point or self.access_point_up:
      return

    # Since we only run an access point if we have a wired connection anyway, we
    # don't want to constrain the AP's channel to the channel of the AP we are
    # connected to.  This line should go away when we have wireless repeaters.
    self.stop_client()

    try:
      subprocess.check_output(self.command, stderr=subprocess.STDOUT)
      self.access_point_up = True
      logging.info('Started %s GHz AP', self.band)
    except subprocess.CalledProcessError as e:
      logging.error('Failed to start access point: %s', e.output)

  def stop_access_point(self):
    if not self.access_point_up:
      return

    command = self.WIFI_STOPAP + ['--band', self.band]
    if self.interface_suffix:
      command += ['--interface_suffix', self.interface_suffix]

    try:
      subprocess.check_output(command, stderr=subprocess.STDOUT)
      self.access_point_up = False
      logging.info('Stopped %s GHz AP', self.band)
    except subprocess.CalledProcessError as e:
      logging.error('Failed to stop access point: %s', e.output)
      return

  def start_client(self):
    """Join the WLAN as a client."""
    if experiment.enabled('WifiNo2GClient') and self.band == '2.4':
      logging.info('WifiNo2GClient enabled; not starting 2.4 GHz client.')
      return

    up = self.client_up
    if up:
      logging.debug('Wifi client already started on %s GHz', self.band)
      return

    if self._actually_start_client():
      self._post_start_client()

  def _actually_start_client(self):
    """Actually run wifi setclient.

    Returns:
      Whether the command succeeded.
    """
    self.wifi.set_gateway_ip(None)
    self.wifi.set_subnet(None)
    command = self.WIFI_SETCLIENT + ['--ssid', self.ssid, '--band', self.band]
    env = dict(os.environ)
    if self.passphrase:
      env['WIFI_CLIENT_PSK'] = self.passphrase
    try:
      self.wifi.status.trying_wlan = True
      subprocess.check_output(command, stderr=subprocess.STDOUT, env=env)
    except subprocess.CalledProcessError as e:
      logging.error('Failed to start wifi client: %s', e.output)
      self.wifi.status.wlan_failed = True
      return False

    return True

  def _post_start_client(self):
    self.wifi.status.connected_to_wlan = True
    logging.info('Started wifi client on %s GHz', self.band)
    self.wifi.attach_wpa_control(self._wpa_control_interface)

  def stop_client(self):
    if not self.client_up:
      logging.debug('Wifi client already stopped on %s GHz', self.band)
      return

    self.wifi.detach_wpa_control()

    try:
      subprocess.check_output(self.WIFI_STOPCLIENT + ['-b', self.band],
                              stderr=subprocess.STDOUT)
      # TODO(rofrankel): Make this work for dual-radio devices.
      self.wifi.status.connected_to_wlan = False
      logging.info('Stopped wifi client on %s GHz', self.band)
    except subprocess.CalledProcessError as e:
      logging.error('Failed to stop wifi client: %s', e.output)


class ConnectionManager(object):
  """Manages wired and wireless connections to the LAN."""

  # pylint: disable=invalid-name
  Bridge = interface.Bridge
  Wifi = interface.Wifi
  FrenzyWifi = interface.FrenzyWifi
  WLANConfiguration = WLANConfiguration

  ETHERNET_STATUS_FILE = 'eth0'
  WLAN_FILE_REGEXP_FMT = r'^%s((?P<interface_suffix>.*)\.)?(?P<band>2\.4|5)$'
  COMMAND_FILE_PREFIX = 'command.'
  ACCESS_POINT_FILE_PREFIX = 'access_point.'
  COMMAND_FILE_REGEXP = WLAN_FILE_REGEXP_FMT % COMMAND_FILE_PREFIX
  ACCESS_POINT_FILE_REGEXP = WLAN_FILE_REGEXP_FMT % ACCESS_POINT_FILE_PREFIX
  GATEWAY_FILE_PREFIX = 'gateway.'
  SUBNET_FILE_PREFIX = 'subnet.'
  MOCA_NODE_FILE_PREFIX = 'node'
  WIFI_SETCLIENT = ['wifi', 'setclient']
  IFUP = ['ifup']
  IP_LINK = ['ip', 'link']
  IFPLUGD_ACTION = ['/etc/ifplugd/ifplugd.action']
  BINWIFI = ['wifi']
  UPLOAD_LOGS_AND_WAIT = ['timeout', '60', 'upload-logs-and-wait']
  CWMP_WAKEUP = ['cwmp', 'wakeup']

  def __init__(self,
               bridge_interface='br0',
               tmp_dir='/tmp/conman',
               config_dir='/config/conman',
               moca_tmp_dir='/tmp/cwmp/monitoring/moca2',
               wpa_control_interface='/var/run/wpa_supplicant',
               run_duration_s=1, interface_update_period=5,
               wifi_scan_period_s=120, wlan_retry_s=15, associate_wait_s=15,
               dhcp_wait_s=10, acs_start_wait_s=20, acs_finish_wait_s=120,
               bssid_cycle_length_s=30):

    self._tmp_dir = tmp_dir
    self._config_dir = config_dir
    self._interface_status_dir = os.path.join(tmp_dir, 'interfaces')
    self._status_dir = os.path.join(tmp_dir, 'status')
    self._moca_tmp_dir = moca_tmp_dir
    self._wpa_control_interface = wpa_control_interface
    self._run_duration_s = run_duration_s
    self._interface_update_period = interface_update_period
    self._wifi_scan_period_s = wifi_scan_period_s
    self._wlan_retry_s = wlan_retry_s
    self._associate_wait_s = associate_wait_s
    self._dhcp_wait_s = dhcp_wait_s
    self._acs_start_wait_s = acs_start_wait_s
    self._acs_finish_wait_s = acs_finish_wait_s
    self._bssid_cycle_length_s = bssid_cycle_length_s
    self._wlan_configuration = {}
    self._try_to_upload_logs = False

    # Make sure all necessary directories exist.
    for directory in (self._tmp_dir, self._config_dir, self._moca_tmp_dir,
                      self._interface_status_dir, self._moca_tmp_dir):
      if not os.path.exists(directory):
        os.makedirs(directory)
        logging.info('Created monitored directory: %s', directory)

    acs_autoprov_filepath = os.path.join(self._tmp_dir,
                                         'acs_autoprovisioning')
    self.bridge = self.Bridge(
        bridge_interface, '10',
        acs_autoprovisioning_filepath=acs_autoprov_filepath)

    self.create_wifi_interfaces()

    for ifc in self.interfaces():
      status_dir = os.path.join(self._status_dir, ifc.name)
      if not os.path.exists(status_dir):
        os.makedirs(status_dir)
      ifc.status = status.Status(status_dir)
    self._status = status.CompositeStatus(self._status_dir,
                                          [i.status for i in self.interfaces()])

    wm = pyinotify.WatchManager()
    wm.add_watch(self._config_dir,
                 pyinotify.IN_CLOSE_WRITE | pyinotify.IN_MOVED_TO |
                 pyinotify.IN_DELETE | pyinotify.IN_MOVED_FROM)
    wm.add_watch(self._tmp_dir,
                 pyinotify.IN_CLOSE_WRITE | pyinotify.IN_MOVED_TO)
    wm.add_watch(self._interface_status_dir,
                 pyinotify.IN_CLOSE_WRITE | pyinotify.IN_MOVED_TO)
    wm.add_watch(self._moca_tmp_dir,
                 pyinotify.IN_CLOSE_WRITE | pyinotify.IN_MOVED_TO)
    self.notifier = pyinotify.Notifier(wm, FileChangeHandler(self), timeout=0)

    # If the ethernet file doesn't exist for any reason when conman starts,
    # check explicitly and run ifplugd.action to create the file.
    if not os.path.exists(os.path.join(self._interface_status_dir, 'eth0')):
      ethernet_up = self.is_interface_up('eth0')
      self.ifplugd_action('eth0', ethernet_up)
      self.bridge.ethernet = ethernet_up

    # Do the same for wifi interfaces , but rather than explicitly setting that
    # the wpa_supplicant link is up, attempt to attach to the wpa_supplicant
    # control interface.
    for wifi in self.wifi:
      if not os.path.exists(
          os.path.join(self._interface_status_dir, wifi.name)):
        wifi_up = self.is_interface_up(wifi.name)
        self.ifplugd_action(wifi.name, wifi_up)
        if wifi_up:
          wifi.status.attached_to_wpa_supplicant = wifi.attach_wpa_control(
              self._wpa_control_interface)

    for path, prefix in ((self._tmp_dir, self.GATEWAY_FILE_PREFIX),
                         (self._tmp_dir, self.SUBNET_FILE_PREFIX),
                         (self._interface_status_dir, ''),
                         (self._moca_tmp_dir, self.MOCA_NODE_FILE_PREFIX),
                         (self._config_dir, self.COMMAND_FILE_PREFIX)):
      for filepath in glob.glob(os.path.join(path, prefix + '*')):
        self._process_file(path, os.path.split(filepath)[-1])

    # Now that we've read any existing state, it's okay to let interfaces touch
    # the routing table.
    for ifc in self.interfaces():
      ifc.initialize()
      logging.info('%s initialized', ifc.name)

    # Make sure no unwanted APs or clients are running.
    for wifi in self.wifi:
      for band in wifi.bands:
        config = self._wlan_configuration.get(band, None)
        if config:
          if config.access_point and self.bridge.internet():
            # If we have a config and want an AP, we don't want a client.
            logging.info('Stopping pre-existing %s client on %s',
                         band, wifi.name)
            self._stop_wifi(band, False, True)
          else:
            # If we have a config but don't want an AP, make sure we aren't
            # running one.
            logging.info('Stopping pre-existing %s AP on %s', band, wifi.name)
            self._stop_wifi(band, True, False)
          break
      else:
        # If we have no config for this radio, neither a client nor an AP should
        # be running.
        logging.info('Stopping pre-existing %s AP and clienton %s',
                     band, wifi.name)
        self._stop_wifi(wifi.bands[0], True, True)

    self._interface_update_counter = 0
    self._try_wlan_after = {'5': 0, '2.4': 0}

    for wifi in self.wifi:
      ratchet_name = '%s provisioning' % wifi.name
      wifi.provisioning_ratchet = ratchet.Ratchet(ratchet_name, [
          ratchet.Condition('trying_open', wifi.connected_to_open,
                            self._associate_wait_s,
                            callback=wifi.expire_connection_status_cache),
          ratchet.Condition('waiting_for_dhcp', wifi.gateway, self._dhcp_wait_s,
                            callback=self.cwmp_wakeup),
          ratchet.FileTouchedCondition('waiting_for_cwmp_wakeup',
                                       os.path.join(CWMP_PATH, 'acscontact'),
                                       self._acs_start_wait_s),
          ratchet.FileTouchedCondition('waiting_for_acs_session',
                                       os.path.join(CWMP_PATH, 'acsconnected'),
                                       self._acs_finish_wait_s),
      ], wifi.status)

  def interfaces(self):
    return [self.bridge] + self.wifi

  def create_wifi_interfaces(self):
    """Create Wifi interfaces."""

    # If we have multiple client interfaces, 5 GHz-only < both < 2.4 GHz-only.
    def metric_for_bands(bands):
      if '5' in bands:
        if '2.4' in bands:
          return interface.METRIC_24GHZ_5GHZ
        return interface.METRIC_5GHZ
      return interface.METRIC_24GHZ

    def wifi_class(attrs):
      return self.FrenzyWifi if 'frenzy' in attrs else self.Wifi

    self.wifi = sorted([
        wifi_class(attrs)(interface_name,
                          metric_for_bands(attrs['bands']),
                          # Prioritize 5 GHz over 2.4.
                          bands=sorted(attrs['bands'], reverse=True))
        for interface_name, attrs
        in get_client_interfaces().iteritems()
    ], key=lambda w: w.metric)

    for wifi in self.wifi:
      wifi.last_wifi_scan_time = -self._wifi_scan_period_s

  def is_interface_up(self, interface_name):
    """Explicitly check whether an interface is up.

    Only used on startup, and only if ifplugd file is missing.

    Args:
      interface_name:  The name of the interface to check.

    Returns:
      Whether the interface is up.
    """
    try:
      lines = subprocess.check_output(self.IP_LINK).splitlines()
    except subprocess.CalledProcessError as e:
      raise EnvironmentError('Failed to call "ip link": %r', e.message)

    for line in lines:
      if interface_name in line and 'LOWER_UP' in line:
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

      if not wifi.attached():
        logging.debug('Attempting to attach to wpa control interface for %s',
                      wifi.name)
        wifi.status.attached_to_wpa_supplicant = wifi.attach_wpa_control(
            self._wpa_control_interface)
      wifi.handle_wpa_events()

      if continue_wifi:
        logging.debug('Running AP on %s, nothing else to do.', wifi.name)
        continue

      # If this interface is connected to the user's WLAN, there is nothing else
      # to do.
      if self._connected_to_wlan(wifi):
        wifi.status.connected_to_wlan = True
        logging.debug('Connected to WLAN on %s, nothing else to do.', wifi.name)
        break

      # This interface is not connected to the WLAN, so scan for potential
      # routes to the ACS for provisioning.
      if ((not self.acs() or self.provisioning_failed(wifi)) and
          not getattr(wifi, 'last_successful_bss_info', None) and
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
          logging.info('Trying to join WLAN on %s.', wifi.name)
          wlan_configuration.start_client()
          if self._connected_to_wlan(wifi):
            logging.info('Joined WLAN on %s.', wifi.name)
            wifi.status.connected_to_wlan = True
            self._try_wlan_after[band] = 0
            break
          else:
            logging.error('Failed to connect to WLAN on %s.', wifi.name)
            wifi.status.connected_to_wlan = False
            self._try_wlan_after[band] = time.time() + self._wlan_retry_s
      else:
        # If we are aren't on the WLAN, can ping the ACS, and haven't gotten a
        # new WLAN configuration yet, there are two possibilities:
        #
        # 1) The configuration didn't change, and we should retry connecting.
        # 2) cwmpd isn't writing a configuration, possibly because the device
        #    isn't registered to any accounts.
        logging.debug('Unable to join WLAN on %s', wifi.name)
        wifi.status.connected_to_wlan = False
        provisioning_failed = self.provisioning_failed(wifi)
        if self.acs():
          logging.debug('Connected to ACS')
          if self._try_to_upload_logs:
            self._try_upload_logs()
            self._try_to_upload_logs = False

          if wifi.acs():
            wifi.last_successful_bss_info = getattr(wifi,
                                                    'last_attempted_bss_info',
                                                    None)
            if provisioning_failed:
              wifi.last_successful_bss_info = None

          now = time.time()
          if self._wlan_configuration:
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
            wait = wifi.complain_about_acs_at - self.provisioning_since(wifi)
            logging.info('Can ping ACS, but no WLAN configuration for %ds.',
                         wait)
            wifi.complain_about_acs_at += wait
        # If we didn't manage to join the WLAN, and we don't have an ACS
        # connection or the ACS session failed, we should try another open AP.
        if not self.acs() or provisioning_failed:
          now = time.time()
          if self._connected_to_open(wifi) and not provisioning_failed:
            logging.debug('Waiting for provisioning for %ds.',
                          now - self.provisioning_since(wifi))
          else:
            logging.debug('Not connected to ACS or provisioning failed')
            self._try_next_bssid(wifi)

    time.sleep(max(0, self._run_duration_s - (time.time() - start_time)))

  def acs(self):
    result = False
    for ifc in self.interfaces():
      acs = ifc.acs()
      ifc.status.can_reach_acs = acs
      result |= acs
    return result

  def internet(self):
    result = False
    for ifc in self.interfaces():
      internet = ifc.internet()
      ifc.status.can_reach_internet = internet
      result |= internet
    return result

  def _update_interfaces_and_routes(self):
    """Touch each interface via update_routes."""

    self.bridge.update_routes()
    for wifi in self.wifi:
      wifi.update_routes()
      # If wifi is connected to something that's not the WLAN, it must be a
      # provisioning attempt, and in particular that attempt must be via
      # last_attempted_bss_info.  If that is the same as the
      # last_successful_bss_info (i.e. the last attempt was successful) and we
      # aren't connected to the ACS after calling update_routes (which expires
      # the connection status cache), then this BSS is no longer successful.
      if (wifi.wpa_supplicant and
          not self._connected_to_wlan(wifi) and
          (getattr(wifi, 'last_successful_bss_info', None) ==
           getattr(wifi, 'last_attempted_bss_info', None)) and
          not wifi.acs()):
        wifi.last_successful_bss_info = None

    # Make sure these get called semi-regularly so that exported status is up-
    # to-date.
    self.acs()
    self.internet()

    # Update /etc/hosts (depends on routing table)
    self._update_tmp_hosts()

  def _update_tmp_hosts(self):
    """Update the contents of /tmp/hosts."""
    lowest_metric_interface = None
    for ifc in self.interfaces():
      route = ifc.current_routes().get('default', None)
      if route:
        metric = route.get('metric', 0)
        candidate = (metric, ifc)
        if (lowest_metric_interface is None or
            candidate < lowest_metric_interface):
          lowest_metric_interface = candidate

    ip_line = ''
    if lowest_metric_interface:
      ip = lowest_metric_interface[1].get_ip_address()
      ip_line = '%s %s\n' % (ip, HOSTNAME) if ip else ''
      logging.info('Lowest metric default route is on dev %r',
                   lowest_metric_interface[1].name)

    new_tmp_hosts = '%s127.0.0.1 localhost' % ip_line

    if not os.path.exists(TMP_HOSTS) or open(TMP_HOSTS).read() != new_tmp_hosts:
      tmp_hosts_tmp_filename = TMP_HOSTS + '.tmp'
      tmp_hosts_tmp = open(tmp_hosts_tmp_filename, 'w')
      tmp_hosts_tmp.write(new_tmp_hosts)
      os.rename(tmp_hosts_tmp_filename, TMP_HOSTS)

  def handle_event(self, path, filename, deleted):
    if deleted:
      self._handle_deleted_file(path, filename)
    else:
      self._process_file(path, filename)

  def _handle_deleted_file(self, path, filename):
    if path == self._config_dir:
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
    if not self._wlan_configuration:
      self.wifi_for_band(band).status.have_config = False

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
          logging.info('Ethernet %s', 'up' if self.bridge.ethernet else 'down')
        except ValueError:
          logging.error('Status file contents should be 0 or 1, not %s',
                        contents)
          return

    elif path == self._config_dir:
      if filename.startswith(self.COMMAND_FILE_PREFIX):
        match = re.match(self.COMMAND_FILE_REGEXP, filename)
        if match:
          band = match.group('band')
          wifi = self.wifi_for_band(band)
          if wifi:
            self._update_wlan_configuration(
                self.WLANConfiguration(band, wifi, contents,
                                       self._wpa_control_interface))
      elif filename.startswith(self.ACCESS_POINT_FILE_PREFIX):
        match = re.match(self.ACCESS_POINT_FILE_REGEXP, filename)
        if match:
          band = match.group('band')
          wifi = self.wifi_for_band(band)
          if wifi and band in self._wlan_configuration:
            self._wlan_configuration[band].access_point = True
          logging.info('AP enabled for %s GHz', band)

    elif path == self._tmp_dir:
      if filename.startswith(self.GATEWAY_FILE_PREFIX):
        interface_name = filename.split(self.GATEWAY_FILE_PREFIX)[-1]
        # If we get a new gateway file, we probably have a new subnet file, and
        # we need the subnet in order to add the gateway route.  So try to
        # process the subnet file before doing anything further with this file.
        self._process_file(path, self.SUBNET_FILE_PREFIX + interface_name)
        ifc = self.interface_by_name(interface_name)
        if ifc:
          ifc.set_gateway_ip(contents)
          logging.info('Received gateway %r for interface %s', contents,
                       ifc.name)

      if filename.startswith(self.SUBNET_FILE_PREFIX):
        interface_name = filename.split(self.SUBNET_FILE_PREFIX)[-1]
        ifc = self.interface_by_name(interface_name)
        if ifc:
          ifc.set_subnet(contents)
          logging.info('Received subnet %r for interface %s', contents,
                       ifc.name)

    elif path == self._moca_tmp_dir:
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
    for ifc in self.interfaces():
      if ifc.name == interface_name:
        return ifc

  def wifi_for_band(self, band):
    for wifi in self.wifi:
      if band in wifi.bands:
        return wifi

    logging.error('No wifi interface for %s GHz.  wlan interfaces:\n%s',
                  band, '\n'.join('%s: %r' %
                                  (w.name, w.bands) for w in self.wifi))

  def ifplugd_action(self, interface_name, up):
    subprocess.call(self.IFPLUGD_ACTION + [interface_name,
                                           'up' if up else 'down'])

  def _wifi_scan(self, wifi):
    """Perform a wifi scan and update wifi.cycler."""
    logging.info('Scanning on %s...', wifi.name)
    wifi.last_wifi_scan_time = time.time()
    subprocess.call(self.IFUP + [wifi.name])
    # /bin/wifi takes a --band option but then finds the right interface for it,
    # so it's okay to just pick the first band here.
    items = self._find_bssids(wifi.bands[0])
    logging.info('Done scanning on %s', wifi.name)
    if not hasattr(wifi, 'cycler'):
      wifi.cycler = cycler.AgingPriorityCycler(
          cycle_length_s=self._bssid_cycle_length_s)
    # Shuffle items to undefined determinism in scan results + dict
    # implementation unfairly biasing BSSID order.
    random.shuffle(items)
    wifi.cycler.update(items)

  def _find_bssids(self, band):
    """Wrapper used as a unit testing seam."""
    return iw.find_bssids(band, False)

  def _try_next_bssid(self, wifi):
    """Attempt to connect to the next BSSID in wifi's BSSID cycler.

    Args:
      wifi: The wifi interface to use.

    Returns:
      Whether connecting to the network succeeded.
    """
    if not hasattr(wifi, 'cycler'):
      return False

    last_successful_bss_info = getattr(wifi, 'last_successful_bss_info', None)
    bss_info = last_successful_bss_info or wifi.cycler.next()
    if bss_info is not None:
      logging.info('Attempting to connect to SSID %s (%s) for provisioning',
                   bss_info.ssid, bss_info.bssid)
      self.start_provisioning(wifi)
      connected = self._try_bssid(wifi, bss_info)
      if connected:
        wifi.status.connected_to_open = True
        now = time.time()
        wifi.complain_about_acs_at = now + 5
        logging.info('Attempting to provision via SSID %s', bss_info.ssid)
        self._try_to_upload_logs = True
      # If we can no longer connect to this, it's no longer successful.
      else:
        wifi.status.connected_to_open = False
        if bss_info == last_successful_bss_info:
          wifi.last_successful_bss_info = None
      return connected
    else:
      # TODO(rofrankel):  There are probably more cases in which this should be
      # true, e.g. if we keep trying the same few unsuccessful BSSIDs.
      # Relatedly, once we find ACS access on an open network we may want to
      # save that SSID/BSSID and that first in future.  If we do that then we
      # can declare that provisioning has failed much more aggressively.
      logging.info('Ran out of BSSIDs to try on %s', wifi.name)
      wifi.status.provisioning_failed = True

    return False

  def _try_bssid(self, wifi, bss_info):
    wifi.last_attempted_bss_info = bss_info
    return subprocess.call(self.WIFI_SETCLIENT +
                           ['--ssid', bss_info.ssid,
                            '--band', wifi.bands[0],
                            '--bssid', bss_info.bssid]) == 0

  def _connected_to_wlan(self, wifi):
    return (wifi.wpa_supplicant and
            any(config.client_up for band, config
                in self._wlan_configuration.iteritems()
                if band in wifi.bands))

  def _connected_to_open(self, wifi):
    result = wifi.connected_to_open()
    wifi.status.connected_to_open = result
    return result

  def _update_wlan_configuration(self, wlan_configuration):
    band = wlan_configuration.band
    current = self._wlan_configuration.get(band, None)
    if current is None or wlan_configuration.command != current.command:
      if current is not None:
        wlan_configuration.access_point = current.access_point
      else:
        ap_file = os.path.join(
            self._config_dir, self.ACCESS_POINT_FILE_PREFIX +
            (('%s.' % wlan_configuration.interface_suffix)
             if wlan_configuration.interface_suffix else '') + band)
        wlan_configuration.access_point = os.path.exists(ap_file)
      self._wlan_configuration[band] = wlan_configuration
      self.wifi_for_band(band).status.have_config = True
      logging.info('Updated WLAN configuration for %s GHz', band)
      self._update_access_point(wlan_configuration)

  def _update_access_point(self, wlan_configuration):
    if self.bridge.internet() and wlan_configuration.access_point:
      wlan_configuration.start_access_point()
    else:
      wlan_configuration.stop_access_point()
      wlan_configuration.start_client()

  def _stop_wifi(self, band, stopap, stopclient):
    """Stop running wifi processes.

    At least one of [stopap, stopclient] must be True.

    Args:
      band:  The band on which to stop wifi.
      stopap:  Whether to stop access points.
      stopclient:  Whether to stop wifi clients.

    Raises:
      ValueError:  If neither stopap nor stopclient is True.
    """
    if stopap and stopclient:
      command = 'stop'
    elif stopap:
      command = 'stopap'
    elif stopclient:
      command = 'stopclient'
    else:
      raise ValueError('Called _stop_wifi without specifying AP or client.')

    full_command = [command, '--band', band, '--persist']

    try:
      self._binwifi(*full_command)
    except subprocess.CalledProcessError as e:
      logging.error('wifi %s failed: "%s"', ' '.join(full_command), e.output)

  def _binwifi(self, *command):
    """Test seam for calls to /bin/wifi.

    Only used by _stop_wifi, and probably shouldn't be used by anything else.

    Args:
      *command:  A command for /bin/wifi

    Raises:
      subprocess.CalledProcessError:  If the command fails.  Deliberately not
      handled here to make future authors think twice before using this.
    """
    subprocess.check_output(self.BINWIFI + list(command),
                            stderr=subprocess.STDOUT)

  def _try_upload_logs(self):
    logging.info('Attempting to upload logs')
    if subprocess.call(self.UPLOAD_LOGS_AND_WAIT) != 0:
      logging.error('Failed to upload logs')

  def cwmp_wakeup(self):
    if subprocess.call(self.CWMP_WAKEUP) != 0:
      logging.error('cwmp wakeup failed')

  def start_provisioning(self, wifi):
    wifi.set_gateway_ip(None)
    wifi.set_subnet(None)
    wifi.provisioning_ratchet.reset()

  def provisioning_failed(self, wifi):
    try:
      wifi.provisioning_ratchet.check()
      if wifi.provisioning_ratchet.done_after:
        wifi.status.provisioning_completed = True
        logging.info('%s successfully provisioned', wifi.name)
      return False
    except ratchet.TimeoutException:
      wifi.status.provisioning_failed = True
      logging.info('%s failed to provision: %s', wifi.name,
                   wifi.provisioning_ratchet.current_step().name)
      return True

  def provisioning_since(self, wifi):
    return wifi.provisioning_ratchet.t0


def _wifi_show():
  try:
    return subprocess.check_output(['wifi', 'show'])
  except subprocess.CalledProcessError as e:
    logging.error('Failed to call "wifi show": %s', e)
    return ''


def _get_quantenna_interfaces():
  try:
    return subprocess.check_output(['get-quantenna-interfaces']).split()
  except subprocess.CalledProcessError:
    logging.fatal('Failed to call get-quantenna-interfaces')
    raise


def get_client_interfaces():
  """Find all client interfaces on the device.

  Returns:
    A dict mapping wireless client interfaces to their supported bands.
  """
  # TODO(mikemu): Use wifi_files instead of "wifi show".

  current_band = None
  result = collections.defaultdict(lambda: collections.defaultdict(set))
  for line in _wifi_show().splitlines():
    if line.startswith('Band:'):
      current_band = line.split()[1]
    elif line.startswith('Client Interface:'):
      result[line.split()[2]]['bands'].add(current_band)

  for quantenna_interface in _get_quantenna_interfaces():
    if quantenna_interface.startswith('wcli'):
      result[quantenna_interface]['bands'].add('5')
      result[quantenna_interface]['frenzy'] = True

  return result
