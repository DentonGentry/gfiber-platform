#!/usr/bin/python

"""Tests for connection_manager.py."""

import logging
import os
import shutil
import tempfile

import connection_manager
import interface_test
import iw
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

WIFI_SHOW_OUTPUT_ONE_RADIO = """Band: 2.4
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

WIFI_SHOW_OUTPUT_TWO_RADIOS = """Band: 2.4
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
WIFI_SHOW_OUTPUT_ONE_RADIO_NO_5GHZ = """Band: 2.4
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

IW_SCAN_OUTPUT = """BSS 00:11:22:33:44:55(on wcli0)
  SSID: s1
  Vendor specific: OUI f4:f5:e8, data: 01
BSS 66:77:88:99:aa:bb(on wcli0)
  SSID: s1
  Vendor specific: OUI f4:f5:e8, data: 01
BSS 01:23:45:67:89:ab(on wcli0)
  SSID: s2
"""


@wvtest.wvtest
def get_client_interfaces_test():
  """Test get_client_interfaces."""
  # pylint: disable=protected-access
  original_wifi_show = connection_manager._wifi_show
  connection_manager._wifi_show = lambda: WIFI_SHOW_OUTPUT_ONE_RADIO
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(),
                  {'wcli0': set(['2.4', '5'])})
  connection_manager._wifi_show = lambda: WIFI_SHOW_OUTPUT_TWO_RADIOS
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(),
                  {'wcli0': set(['2.4']), 'wcli1': set(['5'])})
  connection_manager._wifi_show = original_wifi_show


class WLANConfiguration(connection_manager.WLANConfiguration):
  """WLANConfiguration subclass for testing."""

  WIFI_STOPAP = ['echo', 'stopap']
  WIFI_SETCLIENT = ['echo', 'setclient']
  WIFI_STOPCLIENT = ['echo', 'stopclient']

  def start_client(self):
    if not self.client_up:
      self.wifi.set_connection_check_result('succeed')

      if self.wifi.attached():
        self.wifi.add_connected_event()
      else:
        open(self._socket(), 'w')

      # Normally, wpa_supplicant would bring up wcli*, which would trigger
      # ifplugd, which would run ifplugd.action, which would do two things:
      #
      # 1)  Write an interface status file.
      # 2)  Call run-dhclient, which would call dhclient-script, which would
      #     write a gateway file.
      #
      # Fake both of these things instead.
      self.write_interface_status_file('1')
      self.write_gateway_file()

    super(WLANConfiguration, self).start_client()

  def stop_client(self):
    if self.client_up:
      self.wifi.add_terminating_event()
      os.unlink(self._socket())
      self.wifi.set_connection_check_result('fail')

    # See comments in start_client.
    self.write_interface_status_file('0')

    super(WLANConfiguration, self).stop_client()

  def _socket(self):
    return os.path.join(self._wpa_control_interface, self.wifi.name)

  def write_gateway_file(self):
    gateway_file = os.path.join(self.status_dir,
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
    # Whether wpa_supplicant is connected to a network.
    self._initially_connected = True
    self.wifi_scan_counter = 0


class ConnectionManager(connection_manager.ConnectionManager):
  """ConnectionManager subclass for testing."""

  # pylint: disable=invalid-name
  Bridge = interface_test.Bridge
  Wifi = Wifi
  WLANConfiguration = WLANConfiguration

  WIFI_SETCLIENT = ['echo', 'setclient']
  IFUP = ['echo', 'ifup']
  IFPLUGD_ACTION = ['echo', 'ifplugd.action']
  # This simulates the output of 'ip link' when eth0 is up.
  IP_LINK = ['echo', 'eth0 LOWER_UP']

  def __init__(self, *args, **kwargs):
    super(ConnectionManager, self).__init__(*args, **kwargs)
    self.scan_has_results = False

  def _update_access_point(self, wlan_configuration):
    client_was_up = wlan_configuration.client_up
    super(ConnectionManager, self)._update_access_point(wlan_configuration)
    if wlan_configuration.access_point_up:
      if client_was_up:
        wifi = self.wifi_for_band(wlan_configuration.band)
        wifi.add_terminating_event()

  def _try_next_bssid(self, wifi):
    if hasattr(wifi, 'cycler'):
      bss_info = wifi.cycler.peek()
      if bss_info:
        self.last_provisioning_attempt = bss_info

    super(ConnectionManager, self)._try_next_bssid(wifi)

    socket = os.path.join(self._wpa_control_interface, wifi.name)

    if bss_info and bss_info.ssid == 's1':
      if wifi.attached():
        wifi.add_connected_event()
      else:
        open(socket, 'w')
      wifi.set_connection_check_result('fail')
      self.write_interface_status_file(wifi.name, '1')
      return True

    if bss_info and bss_info.ssid == 's2':
      if wifi.attached():
        wifi.add_connected_event()
      else:
        open(socket, 'w')
      wifi.set_connection_check_result('restricted')
      self.ifplugd_action(wifi.name, True)
      return True

    return False

  def _wifi_stopclient(self, band):
    super(ConnectionManager, self)._wifi_stopclient(band)
    self.wifi_for_band(band).add_terminating_event()

  # pylint: disable=unused-argument,protected-access
  def _find_bssids(self, wcli):
    # Only the 5 GHz scan finds anything.
    if wcli == 'wcli0' and self.scan_has_results:
      iw._scan = lambda interface: IW_SCAN_OUTPUT
    else:
      iw._scan = lambda interface: ''
    return super(ConnectionManager, self)._find_bssids(wcli)

  def _update_wlan_configuration(self, wlan_configuration):
    wlan_configuration.command.insert(0, 'echo')
    wlan_configuration._wpa_control_interface = self._wpa_control_interface
    wlan_configuration.status_dir = self._status_dir
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

  def wlan_config_filename(self, band):
    return os.path.join(self._config_dir, 'command.%s' % band)

  def access_point_filename(self, band):
    return os.path.join(self._config_dir, 'access_point.%s' % band)

  def delete_wlan_config(self, band):
    os.unlink(self.wlan_config_filename(band))

  def write_wlan_config(self, band, ssid, psk, atomic=False):
    final_filename = self.wlan_config_filename(band)
    filename = final_filename + ('.tmp' if atomic else '')
    with open(filename, 'w') as f:
      f.write('\n'.join(['env', 'WIFI_PSK=%s' % psk,
                         'wifi', 'set', '-b', band, '--ssid', ssid]))
    if atomic:
      os.rename(filename, final_filename)

  def enable_access_point(self, band):
    open(self.access_point_filename(band), 'w')

  def disable_access_point(self, band):
    ap_filename = self.access_point_filename(band)
    if os.path.isfile(ap_filename):
      os.unlink(ap_filename)

  def write_gateway_file(self, interface_name):
    gateway_file = os.path.join(self._status_dir,
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
    moca_node1_file = os.path.join(self._moca_status_dir,
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


def connection_manager_test(radio_config):
  """Returns a decorator that does ConnectionManager test boilerplate."""
  def inner(f):
    """The actual decorator."""
    def actual_test():
      """The actual test function."""
      run_duration_s = .01
      interface_update_period = 5
      wifi_scan_period = 5
      wifi_scan_period_s = run_duration_s * wifi_scan_period

      # pylint: disable=protected-access
      original_wifi_show = connection_manager._wifi_show
      connection_manager._wifi_show = lambda: radio_config

      try:
        # No initial state.
        status_dir = tempfile.mkdtemp()
        config_dir = tempfile.mkdtemp()
        os.mkdir(os.path.join(status_dir, 'interfaces'))
        moca_status_dir = tempfile.mkdtemp()
        wpa_control_interface = tempfile.mkdtemp()

        # Test that missing directories are created by ConnectionManager.
        shutil.rmtree(status_dir)

        c = ConnectionManager(status_dir=status_dir,
                              config_dir=config_dir,
                              moca_status_dir=moca_status_dir,
                              wpa_control_interface=wpa_control_interface,
                              run_duration_s=run_duration_s,
                              interface_update_period=interface_update_period,
                              wifi_scan_period_s=wifi_scan_period_s)

        c.test_interface_update_period = interface_update_period
        c.test_wifi_scan_period = wifi_scan_period

        f(c)

      finally:
        shutil.rmtree(status_dir)
        shutil.rmtree(config_dir)
        shutil.rmtree(moca_status_dir)
        shutil.rmtree(wpa_control_interface)
        # pylint: disable=protected-access
        connection_manager._wifi_show = original_wifi_show

    actual_test.func_name = f.func_name
    return actual_test

  return inner


def connection_manager_test_radio_independent(c):
  """Test ConnectionManager for things independent of radio configuration.

  To verify that these things are both independent, this function is called
  twice below, once with each radio configuration.  Those wrappers have the
  relevant test decorators.

  Args:
    c:  A ConnectionManager set up by @connection_manager_test.
  """
  # This test only checks that this file gets created and deleted once each.
  # ConnectionManager cares that the file is created *where* expected, but it is
  # Bridge's responsbility to make sure its creation and deletion are generally
  # correct; more thorough tests are in bridge_test in interface_test.py.
  acs_autoprov_filepath = os.path.join(c._status_dir, 'acs_autoprovisioning')

  # Initially, there is ethernet access (via explicit check of ethernet status,
  # rather than the interface status file).
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_route())
  wvtest.WVPASS(os.path.exists(acs_autoprov_filepath))
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.wifi_for_band('5').current_route())

  # Take down ethernet, no access.
  c.set_ethernet(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVFAIL(c.bridge.current_route())
  wvtest.WVFAIL(os.path.exists(acs_autoprov_filepath))

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
  c.scan_has_results = True
  # Wait for a scan, plus 3 cycles, so that s2 will have been tried.
  c.run_until_scan('2.4')
  for _ in range(3):
    c.run_once()
  wvtest.WVPASSEQ(c.last_provisioning_attempt.ssid, 's2')
  wvtest.WVPASSEQ(c.last_provisioning_attempt.bssid, '01:23:45:67:89:ab')
  # Wait for the connection to be processed.
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route())

  # Now, create a WLAN configuration which should be connected to.  Also, test
  # that atomic writes/renames work.
  ssid = 'wlan'
  psk = 'password'
  c.write_wlan_config('2.4', ssid, psk, atomic=True)
  c.disable_access_point('2.4')
  c.run_once()
  wvtest.WVPASS(c.client_up('2.4'))
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route())

  # Now enable the AP.  Since we have no wired connection, this should have no
  # effect.
  c.enable_access_point('2.4')
  c.run_once()
  wvtest.WVPASS(c.client_up('2.4'))
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route())
  wvtest.WVFAIL(c.bridge.current_route())

  # Now bring up the bridge.  We should remove the wifi connection and start
  # an AP.
  c.set_ethernet(True)
  c.bridge.set_connection_check_result('succeed')
  c.run_until_interface_update()
  wvtest.WVPASS(c.access_point_up('2.4'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVPASS(c.bridge.current_route())

  # Now move (rather than delete) the configuration file.  The AP should go
  # away, and we should not be able to join the WLAN.  Routes should not be
  # affected.
  filename = c.wlan_config_filename('2.4')
  other_filename = filename + '.bak'
  os.rename(filename, other_filename)
  c.run_once()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVPASS(c.bridge.current_route())

  # Now move it back, and the AP should come back.
  os.rename(other_filename, filename)
  c.run_once()
  wvtest.WVPASS(c.access_point_up('2.4'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_route())
  wvtest.WVPASS(c.bridge.current_route())


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ONE_RADIO)
def connection_manager_test_radio_independent_one_radio(c):
  connection_manager_test_radio_independent(c)


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_TWO_RADIOS)
def connection_manager_test_radio_independent_two_radios(c):
  connection_manager_test_radio_independent(c)


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_TWO_RADIOS)
def connection_manager_test_two_radios(c):
  """Test ConnectionManager for devices with two radios.

  This test should be kept roughly parallel to the one-radio test.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
  """
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
  c.scan_has_results = True
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
@connection_manager_test(WIFI_SHOW_OUTPUT_ONE_RADIO)
def connection_manager_test_one_radio(c):
  """Test ConnectionManager for devices with one radio.

  This test should be kept roughly parallel to the two-radio test.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
  """
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

  # The wcli0 scan will have results that will lead to ACS access.
  c.scan_has_results = True
  c.run_until_scan('5')
  for _ in range(3):
    c.run_once()
  c.run_until_interface_update()
  wvtest.WVPASS(c.acs())
  wvtest.WVFAIL(c.bridge.current_route())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_route())
  wvtest.WVPASS(c.wifi_for_band('5').current_route())


@wvtest.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ONE_RADIO_NO_5GHZ)
def connection_manager_test_one_radio_no_5ghz(c):
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


if __name__ == '__main__':
  wvtest.wvtest_main()
