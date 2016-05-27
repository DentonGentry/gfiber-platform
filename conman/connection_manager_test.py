#!/usr/bin/python

"""Tests for connection_manager.py."""

import logging
import os
import shutil
import tempfile

import connection_manager
import interface_test
import iw
import status
from wvtest import wvtest

logging.basicConfig(level=logging.DEBUG)

FAKE_MOCA_NODE1_FILE = """{
  "NodeId": 1,
  "RxNBAS": 25
}
"""

FAKE_MOCA_NODE1_FILE_DISCONNECTED = """{
  "NodeId": 1,
  "RxNBAS": 0
}
"""

WIFI_SHOW_OUTPUT_MARVELL8897 = """Band: 2.4
RegDomain: US
Interface: wlan0  # 2.4 GHz ap
Channel: 149
BSSID: f4:f5:e8:81:1b:a0
AutoChannel: True
AutoType: NONDFS
Station List for band: 2.4

Client Interface: wcli0  # 2.4 GHz client
Client BSSID: f4:f5:e8:81:1b:a1

Band: 5
RegDomain: US
Interface: wlan0  # 5 GHz ap
Channel: 149
BSSID: f4:f5:e8:81:1b:a0
AutoChannel: True
AutoType: NONDFS
Station List for band: 5

Client Interface: wcli0  # 5 GHz client
Client BSSID: f4:f5:e8:81:1b:a1
"""

WIFI_SHOW_OUTPUT_ATH9K_ATH10K = """Band: 2.4
RegDomain: US
Interface: wlan0  # 2.4 GHz ap
Channel: 149
BSSID: f4:f5:e8:81:1b:a0
AutoChannel: True
AutoType: NONDFS
Station List for band: 2.4

Client Interface: wcli0  # 2.4 GHz client
Client BSSID: f4:f5:e8:81:1b:a1

Band: 5
RegDomain: US
Interface: wlan1  # 5 GHz ap
Channel: 149
BSSID: f4:f5:e8:81:1b:a0
AutoChannel: True
AutoType: NONDFS
Station List for band: 5

Client Interface: wcli1  # 5 GHz client
Client BSSID: f4:f5:e8:81:1b:a1
"""

# See b/27328894.
WIFI_SHOW_OUTPUT_MARVELL8897_NO_5GHZ = """Band: 2.4
RegDomain: 00
Interface: wlan0  # 2.4 GHz ap
BSSID: 00:50:43:02:fe:01
AutoChannel: False
Station List for band: 2.4

Client Interface: wcli0  # 2.4 GHz client
Client BSSID: 00:50:43:02:fe:02

Band: 5
RegDomain: 00
"""

WIFI_SHOW_OUTPUT_ATH9K_FRENZY = """Band: 2.4
RegDomain: US
Interface: wlan0  # 2.4 GHz ap
Channel: 149
BSSID: f4:f5:e8:81:1b:a0
AutoChannel: True
AutoType: NONDFS
Station List for band: 2.4

Client Interface: wcli0  # 2.4 GHz client
Client BSSID: f4:f5:e8:81:1b:a1

Band: 5
RegDomain: 00
Interface: wlan0  # 5 GHz ap
AutoChannel: False
Station List for band: 5

Client Interface: wlan1  # 5 GHz client
"""

WIFI_SHOW_OUTPUT_FRENZY = """Band: 2.4
RegDomain: 00
Band: 5
RegDomain: 00
Interface: wlan0  # 5 GHz ap
AutoChannel: False
Station List for band: 5

Client Interface: wlan0  # 5 GHz client
"""

IW_SCAN_DEFAULT_OUTPUT = """BSS 00:11:22:33:44:55(on wcli0)
  SSID: s1
BSS 66:77:88:99:aa:bb(on wcli0)
  SSID: s1
BSS 01:23:45:67:89:ab(on wcli0)
  SSID: s2
"""

IW_SCAN_HIDDEN_OUTPUT = """BSS ff:ee:dd:cc:bb:aa(on wcli0)
  Vendor specific: OUI f4:f5:e8, data: 01
  Vendor specific: OUI f4:f5:e8, data: 03 73 33
"""


@wvtest.wvtest
def get_client_interfaces_test():
  """Test get_client_interfaces."""
  # pylint: disable=protected-access
  original_wifi_show = connection_manager._wifi_show
  original_get_quantenna_interface = connection_manager._get_quantenna_interface
  connection_manager._get_quantenna_interface = lambda: ''
  connection_manager._wifi_show = lambda: WIFI_SHOW_OUTPUT_MARVELL8897
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(),
                  {'wcli0': {'bands': set(['2.4', '5'])}})
  connection_manager._wifi_show = lambda: WIFI_SHOW_OUTPUT_ATH9K_ATH10K
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(), {
      'wcli0': {'bands': set(['2.4'])},
      'wcli1': {'bands': set(['5'])}
  })

  # Test Quantenna devices.

  # 2.4 GHz cfg80211 radio + 5 GHz Frenzy (Optimus Prime).
  connection_manager._wifi_show = lambda: WIFI_SHOW_OUTPUT_ATH9K_FRENZY
  connection_manager._get_quantenna_interface = lambda: 'wlan1'
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(), {
      'wcli0': {'bands': set(['2.4'])},
      'wlan1': {'frenzy': True, 'bands': set(['5'])}
  })

  # Only Frenzy (e.g. Lockdown).
  connection_manager._wifi_show = lambda: WIFI_SHOW_OUTPUT_FRENZY
  connection_manager._get_quantenna_interface = lambda: 'wlan0'
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(),
                  {'wlan0': {'frenzy': True, 'bands': set(['5'])}})

  connection_manager._wifi_show = original_wifi_show
  connection_manager._get_quantenna_interface = original_get_quantenna_interface


class WLANConfiguration(connection_manager.WLANConfiguration):
  """WLANConfiguration subclass for testing."""

  WIFI_STOPAP = ['echo', 'stopap']
  WIFI_SETCLIENT = ['echo', 'setclient']
  WIFI_STOPCLIENT = ['echo', 'stopclient']

  def start_client(self):
    client_was_up = self.client_up
    was_attached = self.wifi.attached()
    # Do this before calling the super method so that the attach call at the end
    # succeeds.
    if not client_was_up and not was_attached:
      self.wifi._initial_ssid_testonly = self.ssid
      self.wifi.start_wpa_supplicant_testonly(self._wpa_control_interface)

    super(WLANConfiguration, self).start_client()

    if not client_was_up:
      self.wifi.set_connection_check_result('succeed')

      if was_attached:
        self.wifi._wpa_control.ssid_testonly = self.ssid
        self.wifi.add_connected_event()

      # Normally, wpa_supplicant would bring up the client interface, which
      # would trigger ifplugd, which would run ifplugd.action, which would do
      # two things:
      #
      # 1)  Write an interface status file.
      # 2)  Call run-dhclient, which would call dhclient-script, which would
      #     write a gateway file.
      #
      # Fake both of these things instead.
      self.write_interface_status_file('1')
      self.write_gateway_file()

  def stop_client(self):
    client_was_up = self.client_up

    super(WLANConfiguration, self).stop_client()

    if client_was_up:
      self.wifi.add_terminating_event()
      self.wifi.set_connection_check_result('fail')

    # See comments in start_client.
    self.write_interface_status_file('0')

  def write_gateway_file(self):
    gateway_file = os.path.join(self.tmp_dir,
                                self.gateway_file_prefix + self.wifi.name)
    with open(gateway_file, 'w') as f:
      # This value doesn't matter to conman, so it's fine to hard code it here.
      f.write('192.168.1.1')

  def write_interface_status_file(self, value):
    status_file = os.path.join(self.interface_status_dir, self.wifi.name)
    with open(status_file, 'w') as f:
      # This value doesn't matter to conman, so it's fine to hard code it here.
      f.write(value)


class Wifi(interface_test.Wifi):

  def __init__(self, *args, **kwargs):
    super(Wifi, self).__init__(*args, **kwargs)
    self.wifi_scan_counter = 0


class FrenzyWifi(interface_test.FrenzyWifi):

  def __init__(self, *args, **kwargs):
    super(FrenzyWifi, self).__init__(*args, **kwargs)
    self.wifi_scan_counter = 0


class ConnectionManager(connection_manager.ConnectionManager):
  """ConnectionManager subclass for testing."""

  # pylint: disable=invalid-name
  Bridge = interface_test.Bridge
  Wifi = Wifi
  FrenzyWifi = FrenzyWifi
  WLANConfiguration = WLANConfiguration

  WIFI_SETCLIENT = ['echo', 'setclient']
  IFUP = ['echo', 'ifup']
  IFPLUGD_ACTION = ['echo', 'ifplugd.action']
  BINWIFI = ['echo', 'wifi']

  def __init__(self, *args, **kwargs):
    self._binwifi_commands = []

    self.interfaces_already_up = kwargs.pop('__test_interfaces_already_up',
                                            ['eth0'])

    self.wifi_interfaces_already_up = [ifc for ifc in self.interfaces_already_up
                                       if ifc.startswith('w')]
    for wifi in self.wifi_interfaces_already_up:
      # wcli1 is always 5 GHz.  wcli0 always *includes* 2.4.  wlan* client
      # interfaces are Frenzy interfaces and therefore 5 GHz-only.
      band = '5' if wifi in ('wlan0', 'wlan1', 'wcli1') else '2.4'
      # This will happen in the super function, but in order for
      # write_wlan_config to work we have to do it now.  This has to happen
      # before the super function so that the files exist before the inotify
      # registration.
      self._config_dir = kwargs['config_dir']
      self.write_wlan_config(band, 'my ssid', 'passphrase')

      # Also create the wpa_supplicant socket to which to attach.
      open(os.path.join(kwargs['wpa_control_interface'], wifi), 'w')

    super(ConnectionManager, self).__init__(*args, **kwargs)

    self.interface_with_scan_results = None
    self.scan_results_include_hidden = False
    # Should we be able to connect to open network s2?
    self.can_connect_to_s2 = True
    self.can_connect_to_s3 = True
    # Will s2 fail rather than providing ACS access?
    self.s2_fail = False

  def create_wifi_interfaces(self):
    super(ConnectionManager, self).create_wifi_interfaces()
    for wifi in self.wifi_interfaces_already_up:
      # pylint: disable=protected-access
      self.interface_by_name(wifi)._initial_ssid_testonly = 'my ssid'

  @property
  def IP_LINK(self):
    return ['echo'] + ['%s LOWER_UP' % ifc
                       for ifc in self.interfaces_already_up]

  def _update_access_point(self, wlan_configuration):
    client_was_up = wlan_configuration.client_up
    super(ConnectionManager, self)._update_access_point(wlan_configuration)
    if wlan_configuration.access_point_up:
      if client_was_up:
        wifi = self.wifi_for_band(wlan_configuration.band)
        wifi.add_terminating_event()

  def _try_bssid(self, wifi, bss_info):
    self.last_provisioning_attempt = bss_info

    super(ConnectionManager, self)._try_bssid(wifi, bss_info)

    def connect(connection_check_result):
      # pylint: disable=protected-access
      if wifi.attached():
        wifi._wpa_control._ssid_testonly = bss_info.ssid
        wifi.add_connected_event()
      else:
        wifi._initial_ssid_testonly = bss_info.ssid
        wifi.start_wpa_supplicant_testonly(self._wpa_control_interface)
      wifi.set_connection_check_result(connection_check_result)
      self.ifplugd_action(wifi.name, True)

    if bss_info and bss_info.ssid == 's1':
      connect('fail')
      return True

    if bss_info and bss_info.ssid == 's2' and self.can_connect_to_s2:
      connect('fail' if self.s2_fail else 'succeed')
      return True

    if bss_info and bss_info.ssid == 's3' and self.can_connect_to_s3:
      connect('restricted')
      return True

    return False

  # pylint: disable=unused-argument,protected-access
  def _find_bssids(self, band):
    scan_output = ''
    if (self.interface_with_scan_results and
        band in self.interface_by_name(self.interface_with_scan_results).bands):
      scan_output = IW_SCAN_DEFAULT_OUTPUT
      if self.scan_results_include_hidden:
        scan_output += IW_SCAN_HIDDEN_OUTPUT
    iw._scan = lambda interface: scan_output
    return super(ConnectionManager, self)._find_bssids(band)

  def _update_wlan_configuration(self, wlan_configuration):
    wlan_configuration.command.insert(0, 'echo')
    wlan_configuration._wpa_control_interface = self._wpa_control_interface
    wlan_configuration.tmp_dir = self._tmp_dir
    wlan_configuration.interface_status_dir = self._interface_status_dir
    wlan_configuration.gateway_file_prefix = self.GATEWAY_FILE_PREFIX

    super(ConnectionManager, self)._update_wlan_configuration(
        wlan_configuration)

  # Just looking for last_wifi_scan_time to change doesn't work because the
  # tests run too fast.
  def _wifi_scan(self, wifi):
    super(ConnectionManager, self)._wifi_scan(wifi)
    wifi.wifi_scan_counter += 1

  def ifplugd_action(self, interface_name, up):
    # Typically, when moca comes up, conman calls ifplugd.action, which writes
    # this file.  Also, when conman starts, it calls ifplugd.action for eth0.
    self.write_interface_status_file(interface_name, '1' if up else '0')

    # ifplugd calls run-dhclient, which results in a gateway file if the link is
    # up (and working).
    if up:
      self.write_gateway_file('br0' if interface_name in ('eth0', 'moca0')
                              else interface_name)

  def _binwifi(self, *command):
    super(ConnectionManager, self)._binwifi(*command)
    self._binwifi_commands.append(command)

  # Non-overrides

  def access_point_up(self, band):
    if band not in self._wlan_configuration:
      return False

    return self._wlan_configuration[band].access_point_up

  def client_up(self, band):
    if band not in self._wlan_configuration:
      return False

    return self._wlan_configuration[band].client_up

  # Test methods

  def delete_wlan_config(self, band):
    delete_wlan_config(self._config_dir, band)

  def write_wlan_config(self, *args, **kwargs):
    write_wlan_config(self._config_dir, *args, **kwargs)

  def enable_access_point(self, band):
    enable_access_point(self._config_dir, band)

  def disable_access_point(self, band):
    disable_access_point(self._config_dir, band)

  def write_gateway_file(self, interface_name):
    gateway_file = os.path.join(self._tmp_dir,
                                self.GATEWAY_FILE_PREFIX + interface_name)
    with open(gateway_file, 'w') as f:
      # This value doesn't matter to conman, so it's fine to hard code it here.
      f.write('192.168.1.1')

  def write_interface_status_file(self, interface_name, value):
    status_file = os.path.join(self._interface_status_dir, interface_name)
    with open(status_file, 'w') as f:
      # This value doesn't matter to conman, so it's fine to hard code it here.
      f.write(value)

  def set_ethernet(self, up):
    self.ifplugd_action('eth0', up)

  def set_moca(self, up):
    moca_node1_file = os.path.join(self._moca_tmp_dir,
                                   self.MOCA_NODE_FILE_PREFIX + '1')
    with open(moca_node1_file, 'w') as f:
      f.write(FAKE_MOCA_NODE1_FILE if up else
              FAKE_MOCA_NODE1_FILE_DISCONNECTED)

  def run_until_interface_update(self):
    while self._interface_update_counter == 0:
      self.run_once()
    while self._interface_update_counter != 0:
      self.run_once()

  def run_until_scan(self, band):
    wifi = self.wifi_for_band(band)
    wifi_scan_counter = wifi.wifi_scan_counter
    while wifi_scan_counter == wifi.wifi_scan_counter:
      self.run_once()

  def run_until_interface_update_and_scan(self, band):
    wifi = self.wifi_for_band(band)
    wifi_scan_counter = wifi.wifi_scan_counter
    self.run_until_interface_update()
    while wifi_scan_counter == wifi.wifi_scan_counter:
      self.run_once()

  def has_status_files(self, files):
    return not set(files) - set(os.listdir(self._status_dir))


def wlan_config_filename(path, band):
  return os.path.join(path, 'command.%s' % band)


def access_point_filename(path, band):
  return os.path.join(path, 'access_point.%s' % band)


def write_wlan_config(path, band, ssid, psk, atomic=False):
  final_filename = wlan_config_filename(path, band)
  filename = final_filename + ('.tmp' if atomic else '')
  with open(filename, 'w') as f:
    f.write('\n'.join(['env', 'WIFI_PSK=%s' % psk,
                       'wifi', 'set', '-b', band, '--ssid', ssid]))
  if atomic:
    os.rename(filename, final_filename)


def delete_wlan_config(path, band):
  os.unlink(wlan_config_filename(path, band))


def enable_access_point(path, band):
  open(access_point_filename(path, band), 'w')


def disable_access_point(path, band):
  ap_filename = access_point_filename(path, band)
  if os.path.isfile(ap_filename):
    os.unlink(ap_filename)


def connection_manager_test(radio_config, wlan_configs=None,
                            quantenna_interface='', **cm_kwargs):
  """Returns a decorator that does ConnectionManager test boilerplate."""
  if wlan_configs is None:
    wlan_configs = {}

  def inner(f):
    """The actual decorator."""
    def actual_test():
      """The actual test function."""
      run_duration_s = .01
      interface_update_period = 5
      wifi_scan_period = 15
      wifi_scan_period_s = run_duration_s * wifi_scan_period

      # pylint: disable=protected-access
      original_wifi_show = connection_manager._wifi_show
      connection_manager._wifi_show = lambda: radio_config

      original_gqi = connection_manager._get_quantenna_interface
      connection_manager._get_quantenna_interface = lambda: quantenna_interface

      try:
        # No initial state.
        tmp_dir = tempfile.mkdtemp()
        config_dir = tempfile.mkdtemp()
        os.mkdir(os.path.join(tmp_dir, 'interfaces'))
        moca_tmp_dir = tempfile.mkdtemp()
        wpa_control_interface = tempfile.mkdtemp()
        FrenzyWifi.WPACtrl.WIFIINFO_PATH = tempfile.mkdtemp()

        for band, access_point in wlan_configs.iteritems():
          write_wlan_config(config_dir, band, 'initial ssid', 'initial psk')
          if access_point:
            open(os.path.join(config_dir, 'access_point.%s' % band), 'w')

        # Test that missing directories are created by ConnectionManager.
        shutil.rmtree(tmp_dir)

        c = ConnectionManager(tmp_dir=tmp_dir,
                              config_dir=config_dir,
                              moca_tmp_dir=moca_tmp_dir,
                              wpa_control_interface=wpa_control_interface,
                              run_duration_s=run_duration_s,
                              interface_update_period=interface_update_period,
                              wifi_scan_period_s=wifi_scan_period_s,
                              **cm_kwargs)

        c.test_interface_update_period = interface_update_period
        c.test_wifi_scan_period = wifi_scan_period

        f(c)
      finally:
        shutil.rmtree(tmp_dir)
        shutil.rmtree(config_dir)
        shutil.rmtree(moca_tmp_dir)
        shutil.rmtree(wpa_control_interface)
        shutil.rmtree(FrenzyWifi.WPACtrl.WIFIINFO_PATH)
        # pylint: disable=protected-access
        connection_manager._wifi_show = original_wifi_show
        connection_manager._get_quantenna_interface = original_gqi

    actual_test.func_name = f.func_name
    return actual_test

  return inner


def connection_manager_test_generic(c, band):
  """Test ConnectionManager for things independent of radio configuration.

  To verify that these things are both independent, this function is called once
  below with each radio configuration.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
    band:  The band to test.
  """
  # This test only checks that this file gets created and deleted once each.
  # ConnectionManager cares that the file is created *where* expected, but it is
  # Bridge's responsbility to make sure its creation and deletion are generally
  # correct; more thorough tests are in bridge_test in interface_test.py.
  acs_autoprov_filepath = os.path.join(c._tmp_dir, 'acs_autoprovisioning')

  # Initially, there is ethernet access (via explicit check of ethernet status,
  # rather than the interface status file).
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.has_status_files([status.P.CAN_REACH_ACS,
                                    status.P.CAN_REACH_INTERNET]))

  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVPASS(os.path.exists(acs_autoprov_filepath))
  for wifi in c.wifi:
    wvtest.WVFAIL(wifi.current_route())
  wvtest.WVFAIL(c.has_status_files([status.P.CONNECTED_TO_WLAN,
                                    status.P.HAVE_CONFIG]))

  # Take down ethernet, no access.
  c.set_ethernet(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVFAIL(c.bridge.current_route())
  wvtest.WVFAIL(os.path.exists(acs_autoprov_filepath))
  wvtest.WVFAIL(c.has_status_files([status.P.CAN_REACH_ACS,
                                    status.P.CAN_REACH_INTERNET]))

  # Bring up moca, access.
  c.set_moca(True)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_route())

  # Bring up ethernet, access via both moca and ethernet.
  c.set_ethernet(True)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_route())

  # Bring down moca, still have access via ethernet.
  c.set_moca(False)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_route())

  # The bridge interfaces are up, but they can't reach anything.
  c.bridge.set_connection_check_result('fail')
  c.run_until_interface_update()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVFAIL(c.bridge.current_route())

  # Now c connects to a restricted network.
  c.bridge.set_connection_check_result('restricted')
  c.run_until_interface_update()
  wvtest.WVPASS(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVPASS(c.bridge.current_route())

  # Now the wired connection goes away.
  c.set_ethernet(False)
  c.set_moca(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVFAIL(c.bridge.current_route())

  # Now there are some scan results.
  c.interface_with_scan_results = c.wifi_for_band(band).name
  # Wait for a scan, plus 3 cycles, so that s2 will have been tried.
  c.run_until_scan(band)
  for _ in range(3):
    c.run_once()
    wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_OPEN]))

  last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
  wvtest.WVPASSEQ(last_bss_info.ssid, 's2')
  wvtest.WVPASSEQ(last_bss_info.bssid, '01:23:45:67:89:ab')

  # Wait for the connection to be processed.
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).current_route())
  # Disable scan results again.
  c.interface_with_scan_results = None

  # Now, create a WLAN configuration which should be connected to.
  ssid = 'wlan'
  psk = 'password'
  c.write_wlan_config(band, ssid, psk)
  c.disable_access_point(band)
  c.run_once()
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).current_route())
  wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_WLAN]))

  # Kill wpa_supplicant.  conman should restart it.
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c._connected_to_wlan(c.wifi_for_band(band)))
  c.wifi_for_band(band).kill_wpa_supplicant_testonly(c._wpa_control_interface)
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVFAIL(c._connected_to_wlan(c.wifi_for_band(band)))
  c.run_once()
  wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_WLAN]))
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c._connected_to_wlan(c.wifi_for_band(band)))

  # Now, remove the WLAN configuration and make sure we are disconnected.  Then
  # disable the previously used ACS connection via s2, re-enable scan results,
  # add the user's WLAN to the scan results, and scan again.  This time, the
  # first SSID tried should be 's3', which is now present in the scan results
  # (with its SSID hidden, but included via vendor IE).
  c.delete_wlan_config(band)
  c.can_connect_to_s2 = False
  c.interface_with_scan_results = c.wifi_for_band(band).name
  c.scan_results_include_hidden = True
  c.run_until_interface_update_and_scan(band)
  wvtest.WVFAIL(c.has_status_files([status.P.CONNECTED_TO_WLAN]))
  c.run_until_interface_update()
  wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_OPEN]))
  wvtest.WVPASSEQ(c.last_provisioning_attempt.ssid, 's3')
  wvtest.WVPASSEQ(c.last_provisioning_attempt.bssid, 'ff:ee:dd:cc:bb:aa')

  # Now, recreate the same WLAN configuration, which should be connected to.
  # Also, test that atomic writes/renames work.
  ssid = 'wlan'
  psk = 'password'
  c.write_wlan_config(band, ssid, psk, atomic=True)
  c.disable_access_point(band)
  c.run_once()
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).current_route())
  wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_WLAN]))

  # Now enable the AP.  Since we have no wired connection, this should have no
  # effect.
  c.enable_access_point(band)
  c.run_once()
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).current_route())
  wvtest.WVFAIL(c.bridge.current_route())

  # Now bring up the bridge.  We should remove the wifi connection and start
  # an AP.
  c.set_ethernet(True)
  c.bridge.set_connection_check_result('succeed')
  c.run_until_interface_update()
  wvtest.WVPASS(c.access_point_up(band))
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVFAIL(c.wifi_for_band(band).current_route())
  wvtest.WVPASS(c.bridge.current_route())

  # Now move (rather than delete) the configuration file.  The AP should go
  # away, and we should not be able to join the WLAN.  Routes should not be
  # affected.
  filename = wlan_config_filename(c._config_dir, band)
  other_filename = filename + '.bak'
  os.rename(filename, other_filename)
  c.run_once()
  wvtest.WVFAIL(c.access_point_up(band))
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVFAIL(c.wifi_for_band(band).current_route())
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVFAIL(c.has_status_files([status.P.HAVE_CONFIG]))

  # Now move it back, and the AP should come back.
  os.rename(other_filename, filename)
  c.run_once()
  wvtest.WVPASS(c.access_point_up(band))
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVFAIL(c.wifi_for_band(band).current_route())
  wvtest.WVPASS(c.bridge.current_route())

  # Now delete the config and bring down the bridge and make sure we reprovision
  # via the last working BSS.
  c.delete_wlan_config(band)
  c.bridge.set_connection_check_result('fail')
  scan_count_for_band = c.wifi_for_band(band).wifi_scan_counter
  c.run_until_interface_update()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  # s3 is not what the cycler would suggest trying next.
  wvtest.WVPASSNE('s3', c.wifi_for_band(band).cycler.peek())
  # Run only once, so that only one BSS can be tried.  It should be the s3 one,
  # since that worked previously.
  c.run_once()
  wvtest.WVPASS(c.acs())
  # Make sure we didn't scan on `band`.
  wvtest.WVPASSEQ(scan_count_for_band, c.wifi_for_band(band).wifi_scan_counter)

  # Now re-create the WLAN config, connect to the WLAN, and make sure that s3 is
  # unset as last_successful_bss_info, since it is no longer available.
  c.write_wlan_config(band, ssid, psk)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  c.can_connect_to_s3 = False
  c.scan_results_include_hidden = False
  c.delete_wlan_config(band)
  c.run_once()
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_successful_bss_info, None)

  # Now do the same, except this time s2 is connected to but doesn't provide ACS
  # access.  This requires first re-establishing s2 as successful, so there are
  # four steps:
  #
  # 1) Connect to WLAN.
  # 2) Disconnect, reprovision via s2 (establishing it as successful).
  # 3) Reconnect to WLAN so that we can trigger re-provisioning by
  #    disconnecting.
  # 4) Connect to s2 but get no ACS access; see that last_successful_bss_info is
  #    unset.
  c.write_wlan_config(band, ssid, psk)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  c.delete_wlan_config(band)
  c.run_once()
  wvtest.WVFAIL(c.wifi_for_band(band).acs())

  c.can_connect_to_s2 = True
  # Give it time to try all BSSIDs.
  for _ in range(3):
    c.run_once()
  s2_bss = iw.BssInfo('01:23:45:67:89:ab', 's2')
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_successful_bss_info, s2_bss)

  c.s2_fail = True
  c.write_wlan_config(band, ssid, psk)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  wvtest.WVPASSEQ(c.wifi_for_band(band).last_successful_bss_info, s2_bss)
  c.delete_wlan_config(band)
  # Run once so that c will reconnect to s2.
  c.run_once()
  # Now run until it sees the lack of ACS access.
  c.run_until_interface_update()
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_successful_bss_info, None)


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897)
def connection_manager_test_generic_marvell8897_2g(c):
  connection_manager_test_generic(c, '2.4')


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897)
def connection_manager_test_generic_marvell8897_5g(c):
  connection_manager_test_generic(c, '5')


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_ATH10K)
def connection_manager_test_generic_ath9k_ath10k_2g(c):
  connection_manager_test_generic(c, '2.4')


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_ATH10K)
def connection_manager_test_generic_ath9k_ath10k_5g(c):
  connection_manager_test_generic(c, '5')


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_FRENZY,
                         quantenna_interface='wlan1')
def connection_manager_test_generic_ath9k_frenzy_2g(c):
  connection_manager_test_generic(c, '2.4')


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_FRENZY,
                         quantenna_interface='wlan1')
def connection_manager_test_generic_ath9k_frenzy_5g(c):
  connection_manager_test_generic(c, '5')


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_FRENZY,
                         quantenna_interface='wlan0')
def connection_manager_test_generic_frenzy_5g(c):
  connection_manager_test_generic(c, '5')


def connection_manager_test_dual_band_two_radios(c):
  """Test ConnectionManager for devices with two radios.

  This test should be kept roughly parallel to the one-radio test.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
  """
  wvtest.WVPASSEQ(len(c._binwifi_commands), 2)
  for band in ['2.4', '5']:
    wvtest.WVPASS(('stop', '--band', band, '--persist') in c._binwifi_commands)

  # Bring up ethernet, access.
  c.set_ethernet(True)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  ssid = 'my ssid'
  psk = 'passphrase'

  # Bring up both access points.
  c.write_wlan_config('2.4', ssid, psk)
  c.enable_access_point('2.4')
  c.write_wlan_config('5', ssid, psk)
  c.enable_access_point('5')
  c.run_once()
  wvtest.WVPASS(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVFAIL(c.client_up('5'))
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # Disable the 2.4 GHz AP, make sure the 5 GHz AP stays up.  2.4 GHz should
  # join the WLAN.
  c.disable_access_point('2.4')
  c.run_until_interface_update()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVPASS(c.client_up('2.4'))
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # Delete the 2.4 GHz WLAN configuration; it should leave the WLAN but nothing
  # else should change.
  c.delete_wlan_config('2.4')
  c.run_until_interface_update()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # Disable the wired connection and remove the WLAN configurations.  Both
  # radios should scan.  Wait for 5 GHz to scan, then enable scan results for
  # 2.4. This should lead to ACS access.
  c.delete_wlan_config('5')
  c.set_ethernet(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.bridge.current_route())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # The 5 GHz scan has no results.
  c.run_until_scan('5')
  c.run_once()
  c.run_until_interface_update()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.bridge.current_route())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # The next 2.4 GHz scan will have results.
  c.interface_with_scan_results = c.wifi_for_band('2.4').name
  c.run_until_scan('2.4')
  # Now run 3 cycles, so that s2 will have been tried.
  for _ in range(3):
    c.run_once()
  c.run_until_interface_update()
  wvtest.WVPASS(c.acs())
  wvtest.WVFAIL(c.bridge.current_route())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_ATH10K)
def connection_manager_test_dual_band_two_radios_ath9k_ath10k(c):
  connection_manager_test_dual_band_two_radios(c)


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_FRENZY,
                         quantenna_interface='wlan1')
def connection_manager_test_dual_band_two_radios_ath9k_frenzy(c):
  connection_manager_test_dual_band_two_radios(c)


def connection_manager_test_dual_band_one_radio(c):
  """Test ConnectionManager for devices with one dual-band radio.

  This test should be kept roughly parallel to
  connection_manager_test_dual_band_two_radios.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
  """
  wvtest.WVPASSEQ(len(c._binwifi_commands), 1)
  wvtest.WVPASSEQ(('stop', '--band', '5', '--persist'), c._binwifi_commands[0])

  # Bring up ethernet, access.
  c.set_ethernet(True)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  ssid = 'my ssid'
  psk = 'passphrase'

  # Enable both access points.  Only 5 should be up.
  c.write_wlan_config('2.4', ssid, psk)
  c.enable_access_point('2.4')
  c.write_wlan_config('5', ssid, psk)
  c.enable_access_point('5')
  c.run_once()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # Disable the 2.4 GHz AP; nothing should change.  The 2.4 GHz client should
  # not be up because the same radio is being used to run a 5 GHz AP.
  c.disable_access_point('2.4')
  c.run_until_interface_update()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # Delete the 2.4 GHz WLAN configuration; nothing should change.
  c.delete_wlan_config('2.4')
  c.run_once()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # Disable the wired connection and remove the WLAN configurations.  There
  # should be a single scan that leads to ACS access.  (It doesn't matter which
  # band we specify in run_until_scan, since both bands point to the same
  # interface.)
  c.delete_wlan_config('5')
  c.set_ethernet(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.bridge.current_route())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # The 2.4 GHz scan will have results that will lead to ACS access.
  c.interface_with_scan_results = c.wifi_for_band('2.4').name
  c.run_until_scan('5')
  for _ in range(3):
    c.run_once()
  c.run_until_interface_update()
  wvtest.WVPASS(c.acs())
  wvtest.WVFAIL(c.bridge.current_route())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route())
  wvtest.WVPASS(c.wifi_for_band('5').current_route())


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897)
def connection_manager_test_dual_band_one_radio_marvell8897(c):
  connection_manager_test_dual_band_one_radio(c)


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897_NO_5GHZ)
def connection_manager_test_marvell8897_no_5ghz(c):
  """Test ConnectionManager for the case documented in b/27328894.

  conman should be able to handle the lack of 5 GHz without actually
  crashing.  Wired connections should not be affected.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
  """
  # Make sure we've correctly set up the test; that there is no 5 GHz wifi
  # interface.
  wvtest.WVPASSEQ(c.wifi_for_band('5'), None)

  c.set_ethernet(True)
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  # Make sure this doesn't crash.
  c.write_wlan_config('5', 'my ssid', 'my psk')
  c.run_once()
  c.enable_access_point('5')
  c.run_once()
  c.disable_access_point('5')
  c.run_once()
  c.delete_wlan_config('5')
  c.run_once()

  # Make sure 2.4 still works.
  c.write_wlan_config('2.4', 'my ssid', 'my psk')
  c.run_once()
  wvtest.WVPASS(c.wifi_for_band('2.4').acs())
  wvtest.WVPASS(c.wifi_for_band('2.4').internet())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route())


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897,
                         __test_interfaces_already_up=['eth0', 'wcli0'])
def connection_manager_test_wifi_already_up(c):
  """Test ConnectionManager when wifi is already up.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
  """
  wvtest.WVPASS(c._connected_to_wlan(c.wifi_for_band('2.4')))
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route)


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897, wlan_configs={'5': True})
def connection_manager_one_radio_marvell8897_existing_config_5g_ap(c):
  wvtest.WVPASSEQ(len(c._binwifi_commands), 1)
  wvtest.WVPASSEQ(('stopclient', '--band', '5', '--persist'),
                  c._binwifi_commands[0])


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897,
                         wlan_configs={'5': False})
def connection_manager_one_radio_marvell8897_existing_config_5g_no_ap(c):
  wvtest.WVPASSEQ(len(c._binwifi_commands), 1)
  wvtest.WVPASSEQ(('stopap', '--band', '5', '--persist'),
                  c._binwifi_commands[0])


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_ATH10K,
                         wlan_configs={'5': True})
def connection_manager_two_radios_ath9k_ath10k_existing_config_5g_ap(c):
  wvtest.WVPASSEQ(len(c._binwifi_commands), 2)
  wvtest.WVPASS(('stop', '--band', '2.4', '--persist') in c._binwifi_commands)
  wvtest.WVPASS(('stopclient', '--band', '5', '--persist')
                in c._binwifi_commands)


if __name__ == '__main__':
  wvtest.wvtest_main()
