#!/usr/bin/python

"""Tests for connection_manager.py."""

import logging
import os
import shutil
import subprocess  # Fake subprocess module in test/fake_python.
import tempfile
import time
import traceback

import connection_manager
import experiment_testutils
import interface_test
import iw
import status
import test_common
from wvtest import wvtest

logger = logging.getLogger(__name__)


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


WIFI_SHOW_OUTPUT_MARVELL8897 = (
    subprocess.wifi.MockInterface(phynum='0', bands=['2.4', '5'],
                                  driver='cfg80211'),
)
WIFI_SHOW_OUTPUT_ATH9K_ATH10K = (
    subprocess.wifi.MockInterface(phynum='0', bands=['2.4'], driver='cfg80211'),
    subprocess.wifi.MockInterface(phynum='1', bands=['5'], driver='cfg80211'),
)
# See b/27328894.
WIFI_SHOW_OUTPUT_MARVELL8897_NO_5GHZ = (
    subprocess.wifi.MockInterface(phynum='0', bands=['2.4'], driver='cfg80211'),
)
WIFI_SHOW_OUTPUT_ATH9K_FRENZY = (
    subprocess.wifi.MockInterface(phynum='0', bands=['2.4'], driver='cfg80211'),
    subprocess.wifi.MockInterface(phynum='1', bands=['5'], driver='frenzy'),
)
WIFI_SHOW_OUTPUT_FRENZY = (
    subprocess.wifi.MockInterface(phynum='0', bands=['5'], driver='frenzy'),
)

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


@test_common.wvtest
def get_client_interfaces_test():
  """Test get_client_interfaces."""
  subprocess.reset()

  subprocess.mock('wifi', 'interfaces', *WIFI_SHOW_OUTPUT_MARVELL8897)
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(),
                  {'wcli0': {'bands': set(['2.4', '5'])}})

  subprocess.mock('wifi', 'interfaces', *WIFI_SHOW_OUTPUT_ATH9K_ATH10K)
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(), {
      'wcli0': {'bands': set(['2.4'])},
      'wcli1': {'bands': set(['5'])}
  })

  # Test Quantenna devices.

  # 2.4 GHz cfg80211 radio + 5 GHz Frenzy (e.g. Optimus Prime).
  subprocess.mock('wifi', 'interfaces', *WIFI_SHOW_OUTPUT_ATH9K_FRENZY)
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(), {
      'wcli0': {'bands': set(['2.4'])},
      'wcli1': {'frenzy': True, 'bands': set(['5'])}
  })

  # Only Frenzy (e.g. Lockdown).
  subprocess.mock('wifi', 'interfaces', *WIFI_SHOW_OUTPUT_FRENZY)
  wvtest.WVPASSEQ(connection_manager.get_client_interfaces(),
                  {'wcli0': {'frenzy': True, 'bands': set(['5'])}})


@test_common.wvtest
def WLANConfigurationParseTest():  # pylint: disable=invalid-name
  """Test WLANConfiguration parsing."""
  subprocess.reset()

  cmd = '\n'.join([
      'WIFI_PSK=abcdWIFI_PSK=qwer', 'wifi', 'set', '-P', '-b', '5',
      '--bridge=br0', '-s', 'my ssid=1', '--interface-suffix', '_suffix',
  ])
  config = connection_manager.WLANConfiguration(
      '5', interface_test.Wifi('wcli0', 20), cmd, 10)

  wvtest.WVPASSEQ('my ssid=1', config.ssid)
  wvtest.WVPASSEQ('abcdWIFI_PSK=qwer', config.passphrase)
  wvtest.WVPASSEQ('_suffix', config.interface_suffix)


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

  def __init__(self, *args, **kwargs):
    self._binwifi_commands = []

    for interface_name in kwargs.pop('__test_interfaces_already_up', ['eth0']):
      subprocess.call(['ifup', interface_name])
      if interface_name.startswith('w'):
        phynum = interface_name[-1]
        for band, interface in subprocess.wifi.INTERFACE_FOR_BAND.iteritems():
          if interface.phynum == phynum:
            break
        else:
          raise ValueError('Could not find matching interface for '
                           '__test_interfaces_already_up')
        ssid = 'my ssid'
        psk = 'passphrase'
        # If band is undefined then a ValueError will be raised above.  pylint
        # isn't smart enough to figure that out.
        # pylint: disable=undefined-loop-variable
        subprocess.mock('cwmp', band, ssid=ssid, psk=psk, write_now=True)
        subprocess.mock('wifi', 'remote_ap', band=band, ssid=ssid, psk=psk,
                        bssid='00:00:00:00:00:00')

    super(ConnectionManager, self).__init__(*args, **kwargs)

  # Just looking for last_wifi_scan_time to change doesn't work because the
  # tests run too fast.
  def _wifi_scan(self, wifi):
    super(ConnectionManager, self)._wifi_scan(wifi)
    wifi.wifi_scan_counter += 1

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

  # # Test methods

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
    logger.debug('running until scan on band %r', band)
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


def check_tmp_hosts(expected_contents):
  wvtest.WVPASSEQ(open(connection_manager.TMP_HOSTS).read(), expected_contents)


def connection_manager_test(radio_config, wlan_configs=None, **cm_kwargs):
  """Returns a decorator that does ConnectionManager test boilerplate."""
  if wlan_configs is None:
    wlan_configs = {}

  def inner(f):
    """The actual decorator."""
    def actual_test():
      """The actual test function."""
      subprocess.reset()

      run_duration_s = .01
      interface_update_period = 5
      wifi_scan_period = 15
      wifi_scan_period_s = run_duration_s * wifi_scan_period
      associate_wait_s = 0
      dhcp_wait_s = .5
      acs_cc_wait_s = 0
      acs_start_wait_s = 0
      acs_finish_wait_s = 0.25

      subprocess.mock('wifi', 'interfaces', *radio_config)

      try:
        # No initial state.
        connection_manager.TMP_HOSTS = tempfile.mktemp()
        tmp_dir = tempfile.mkdtemp()
        config_dir = tempfile.mkdtemp()
        interfaces_dir = os.path.join(tmp_dir, 'interfaces')
        if not os.path.exists(interfaces_dir):
          os.mkdir(interfaces_dir)
        moca_tmp_dir = tempfile.mkdtemp()
        wpa_control_interface = tempfile.mkdtemp()
        subprocess.mock('wifi', 'wpa_path', wpa_control_interface)
        connection_manager.CWMP_PATH = tempfile.mkdtemp()
        subprocess.set_conman_paths(tmp_dir, config_dir,
                                    connection_manager.CWMP_PATH)

        for band, access_point in wlan_configs.iteritems():
          subprocess.mock('cwmp', band, ssid='initial ssid', psk='initial psk',
                          access_point=access_point, write_now=True)

        # Test that missing directories are created by ConnectionManager.
        shutil.rmtree(tmp_dir)

        kwargs = dict(tmp_dir=tmp_dir,
                      config_dir=config_dir,
                      moca_tmp_dir=moca_tmp_dir,
                      run_duration_s=run_duration_s,
                      interface_update_period=interface_update_period,
                      wlan_retry_s=0,
                      wifi_scan_period_s=wifi_scan_period_s,
                      associate_wait_s=associate_wait_s,
                      dhcp_wait_s=dhcp_wait_s,
                      acs_connection_check_wait_s=acs_cc_wait_s,
                      acs_start_wait_s=acs_start_wait_s,
                      acs_finish_wait_s=acs_finish_wait_s,
                      bssid_cycle_length_s=1)
        kwargs.update(cm_kwargs)
        c = ConnectionManager(**kwargs)

        f(c)
      except Exception:
        logger.error('Uncaught exception!')
        traceback.print_exc()
        raise
      finally:
        if os.path.exists(connection_manager.TMP_HOSTS):
          os.unlink(connection_manager.TMP_HOSTS)
        shutil.rmtree(tmp_dir)
        shutil.rmtree(config_dir)
        shutil.rmtree(moca_tmp_dir)
        shutil.rmtree(wpa_control_interface)
        shutil.rmtree(connection_manager.CWMP_PATH)

    actual_test.func_name = f.func_name
    return actual_test

  return inner


def _enable_basic_scan_results(band):
  for bssid, ssid, ccr in (('00:11:22:33:44:55', 's1', 'fail'),
                           ('66:77:88:99:aa:bb', 's1', 'fail'),
                           ('01:23:45:67:89:ab', 's2', 'succeed')):
    subprocess.mock('wifi', 'remote_ap', bssid=bssid, ssid=ssid,
                    band=band, security=None, connection_check_result=ccr)


def _disable_basic_scan_results(band):
  for bssid in (('00:11:22:33:44:55'), ('66:77:88:99:aa:bb'),
                ('01:23:45:67:89:ab')):
    subprocess.mock('wifi', 'remote_ap_remove', bssid=bssid, band=band)


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

  # Each Wifi's provisioning ratchet has beeen created, but not started.
  wvtest.WVFAIL(c.has_status_files([status.P.TRYING_OPEN]))

  # Initially, there is ethernet access (via explicit check of ethernet status,
  # rather than the interface status file).
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.has_status_files([status.P.CAN_REACH_ACS,
                                    status.P.CAN_REACH_INTERNET]))
  hostname = connection_manager.HOSTNAME

  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_routes())
  wvtest.WVPASS(os.path.exists(acs_autoprov_filepath))
  for wifi in c.wifi:
    wvtest.WVFAIL(wifi.current_routes_normal_testonly())
  wvtest.WVFAIL(c.has_status_files([status.P.CONNECTED_TO_WLAN,
                                    status.P.HAVE_CONFIG]))

  # Take down ethernet, no access.
  c.set_ethernet(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVFAIL(c.bridge.current_routes_normal_testonly())
  wvtest.WVFAIL(os.path.exists(acs_autoprov_filepath))
  wvtest.WVFAIL(c.has_status_files([status.P.CAN_REACH_ACS,
                                    status.P.CAN_REACH_INTERNET]))

  # Bring up moca, access.
  c.set_moca(True)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_routes())

  # Bring up ethernet, access via both moca and ethernet.
  c.set_ethernet(True)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_routes())

  # Bring down moca, still have access via ethernet.
  c.set_moca(False)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASS(c.bridge.current_routes())

  # The bridge interfaces are up, but they can't reach anything.
  c.bridge.set_connection_check_result('fail')
  c.run_until_interface_update()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVFAIL(c.bridge.current_routes_normal_testonly())

  # Now c connects to a restricted network.
  c.bridge.set_connection_check_result('restricted')
  c.run_until_interface_update()
  wvtest.WVPASS(c.acs())
  wvtest.WVFAIL(c.internet())
  wvtest.WVPASS(c.bridge.current_routes())

  # Now the wired connection goes away.
  c.set_ethernet(False)
  c.set_moca(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  # We have no links, so we should have no routes (not even low priority ones),
  # and /tmp/hosts should only contain a line for localhost.
  wvtest.WVFAIL(c.bridge.current_routes())
  check_tmp_hosts('127.0.0.1 localhost')

  # Now there are some scan results.
  _enable_basic_scan_results(band)

  # Create a WLAN configuration which should eventually be connected to.
  ssid = 'wlan'
  psk = 'password'
  subprocess.mock('wifi', 'remote_ap',
                  bssid='11:22:33:44:55:66',
                  ssid=ssid, psk=psk, band=band, security='WPA2')
  subprocess.mock('cwmp', band, ssid=ssid, psk=psk, access_point=False)

  wvtest.WVFAIL(subprocess.upload_logs_and_wait.uploaded_logs())
  # Wait for a scan, then until s2 is tried.
  c.run_until_scan(band)
  subprocess.call(['ip', 'addr', 'add', '192.168.1.100',
                   'dev', c.wifi_for_band(band).name])
  for _ in range(len(c.wifi_for_band(band).cycler)):
    c.run_once()
    wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_OPEN]))
    last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
    if last_bss_info.ssid == 's2':
      break

  wvtest.WVPASSEQ(last_bss_info.ssid, 's2')
  wvtest.WVPASSEQ(last_bss_info.bssid, '01:23:45:67:89:ab')

  # Wait for the connection to be processed.
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).current_routes())
  wvtest.WVPASS(subprocess.upload_logs_and_wait.uploaded_logs())
  c.run_until_interface_update()
  check_tmp_hosts('192.168.1.100 %s\n127.0.0.1 localhost' % hostname)

  c.run_once()
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).current_routes())
  wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_WLAN]))

  # Kill wpa_supplicant.  conman should restart it.
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c._connected_to_wlan(c.wifi_for_band(band)))
  subprocess.mock('wifi', 'kill_wpa_supplicant', band)
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVFAIL(c._connected_to_wlan(c.wifi_for_band(band)))
  # Make sure we stay connected to s2, rather than disconnecting and
  # reconnecting.
  c.run_once()
  wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_WLAN]))
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c._connected_to_wlan(c.wifi_for_band(band)))

  # Now, update the WLAN configuration and make sure we reprovision.

  # The AP restarts with a new configuration, kicking us off.  We should
  # reprovision.
  ssid = 'wlan2'
  psk = 'password2'
  subprocess.mock('cwmp', band, ssid=ssid, psk=psk)
  # Overwrites previous one due to same BSSID.
  subprocess.mock('wifi', 'remote_ap',
                  bssid='11:22:33:44:55:66',
                  ssid=ssid, psk=psk, band=band, security='WPA2')
  subprocess.mock('wifi', 'disconnected_event', band)
  c.run_once()
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVPASS(c._connected_to_open(c.wifi_for_band(band)))
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_attempted_bss_info.ssid, 's2')

  # Run once for cwmp wakeup to get called, then once more for the new config to
  # be received.
  c.run_once()
  c.run_once()
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).wpa_status()['ssid'] == ssid)
  wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_WLAN]))
  wvtest.WVPASS(c.wifi_for_band(band).current_routes())

  # Now, remove the WLAN configuration and make sure we are disconnected.  Then
  # disable the previously used ACS connection via s2, re-enable scan results,
  # add the user's WLAN to the scan results, and scan again.  This time, the
  # first SSID tried should be 's3', which is now present in the scan results
  # (with its SSID hidden, but included via vendor IE).
  subprocess.mock('cwmp', band, delete_config=True, write_now=True)
  del c.wifi_for_band(band).cycler
  _enable_basic_scan_results(band)
  # Remove s2.
  subprocess.mock('wifi', 'remote_ap_remove',
                  bssid='01:23:45:67:89:ab', band=band)
  # Create s3.
  subprocess.mock('wifi', 'remote_ap', bssid='ff:ee:dd:cc:bb:aa', ssid='s3',
                  band=band, security=None, hidden=True,
                  vendor_ies=(('f4:f5:e8', '01'), ('f4:f5:e8', '03 73 33')))
  #### Now, recreate the same WLAN configuration, which should be connected to.
  subprocess.mock('cwmp', band, ssid=ssid, psk=psk)

  c.run_until_interface_update_and_scan(band)
  c.run_until_interface_update()
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_attempted_bss_info.ssid, 's3')
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_attempted_bss_info.bssid,
                  'ff:ee:dd:cc:bb:aa')
  wvtest.WVPASS(subprocess.upload_logs_and_wait.uploaded_logs())

  c.run_once()
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).current_routes())
  wvtest.WVPASS(c.has_status_files([status.P.CONNECTED_TO_WLAN]))

  # Now enable the AP.  Since we have no wired connection, this should have no
  # effect.
  subprocess.mock('cwmp', band, access_point=True, write_now=True)
  c.run_once()
  wvtest.WVPASS(c.client_up(band))
  wvtest.WVPASS(c.wifi_for_band(band).current_routes())
  wvtest.WVFAIL(c.bridge.current_routes_normal_testonly())
  c.run_until_interface_update()
  check_tmp_hosts('192.168.1.100 %s\n127.0.0.1 localhost' % hostname)

  # Now bring up the bridge.  We should remove the wifi connection and start
  # an AP.
  c.set_ethernet(True)
  c.bridge.set_connection_check_result('succeed')
  subprocess.call(['ip', 'addr', 'add', '192.168.1.101', 'dev', c.bridge.name])
  c.run_until_interface_update()
  wvtest.WVPASS(c.access_point_up(band))
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVFAIL(c.wifi_for_band(band).current_routes())
  wvtest.WVPASS(c.bridge.current_routes())
  check_tmp_hosts('192.168.1.101 %s\n127.0.0.1 localhost' % hostname)

  # Now move (rather than delete) the configuration file.  The AP should go
  # away, and we should not be able to join the WLAN.  Routes should not be
  # affected.
  filename = subprocess.cwmp.wlan_config_filename(band)
  other_filename = filename + '.bak'
  os.rename(filename, other_filename)
  c.run_once()
  wvtest.WVFAIL(c.access_point_up(band))
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVFAIL(c.wifi_for_band(band).current_routes_normal_testonly())
  wvtest.WVPASS(c.bridge.current_routes())
  wvtest.WVFAIL(c.has_status_files([status.P.HAVE_CONFIG]))

  # Now move it back, and the AP should come back.
  os.rename(other_filename, filename)
  c.run_once()
  wvtest.WVPASS(c.access_point_up(band))
  wvtest.WVFAIL(c.client_up(band))
  wvtest.WVFAIL(c.wifi_for_band(band).current_routes_normal_testonly())
  wvtest.WVPASS(c.bridge.current_routes())

  # Now delete the config and bring down the bridge and make sure we reprovision
  # via the last working BSS.
  subprocess.mock('cwmp', band, delete_config=True, write_now=True)
  c.bridge.set_connection_check_result('fail')
  scan_count_for_band = c.wifi_for_band(band).wifi_scan_counter
  c.run_until_interface_update()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.internet())
  # We still have a link and might be wrong about the connection_check, so
  # /tmp/hosts should still contain a line for this hostname.
  check_tmp_hosts('192.168.1.101 %s\n127.0.0.1 localhost' % hostname)
  # s3 is not what the cycler would suggest trying next.
  wvtest.WVPASSNE('s3', c.wifi_for_band(band).cycler.peek())
  subprocess.mock('cwmp', band, ssid=ssid, psk=psk)
  # Run only once, so that only one BSS can be tried.  It should be the s3 one,
  # since that worked previously.
  c.run_once()
  wvtest.WVPASS(c.acs())
  # Make sure we didn't scan on `band`.
  wvtest.WVPASSEQ(scan_count_for_band, c.wifi_for_band(band).wifi_scan_counter)
  c.run_once()
  wvtest.WVPASS(subprocess.upload_logs_and_wait.uploaded_logs())

  # Now wait for the WLAN config to be created, connect to the WLAN, and make
  # sure that s3 is unset as last_successful_bss_info, since it is no longer
  # available.
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  # Remove s3.
  subprocess.mock('wifi', 'remote_ap_remove',
                  bssid='ff:ee:dd:cc:bb:aa', band=band)
  # Bring s2 back.
  subprocess.mock('wifi', 'remote_ap', bssid='01:23:45:67:89:ab', ssid='s2',
                  band=band, security=None)

  subprocess.mock('cwmp', band, delete_config=True, write_now=True)
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
  subprocess.mock('cwmp', band, ssid=ssid, psk=psk, write_now=True)
  # Connect
  c.run_once()
  # Process DHCP results
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  subprocess.mock('cwmp', band, delete_config=True, write_now=True)
  c.run_once()
  wvtest.WVFAIL(c.wifi_for_band(band).acs())

  # Give it time to try all BSSIDs.  This means sleeping long enough that
  # everything in the cycler is active, then doing n+1 loops (the n+1st loop is
  # when we decide that the SSID in the nth loop was successful).
  time.sleep(c._bssid_cycle_length_s)
  subprocess.mock('cwmp', band, ssid=ssid, psk=psk)
  for _ in range(len(c.wifi_for_band(band).cycler) + 1):
    c.run_once()
  s2_bss = iw.BssInfo(bssid='01:23:45:67:89:ab', ssid='s2', band=band)
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_successful_bss_info, s2_bss)
  c.run_once()
  wvtest.WVPASS(subprocess.upload_logs_and_wait.uploaded_logs())

  # Make s2's connection check fail.
  subprocess.mock('wifi', 'remote_ap', bssid='01:23:45:67:89:ab', ssid='s2',
                  band=band, security=None, connection_check_result='fail')

  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_successful_bss_info, s2_bss)
  # Disconnect.
  ssid = 'wlan3'
  psk = 'password3'
  subprocess.mock('wifi', 'remote_ap',
                  bssid='11:22:33:44:55:66',
                  ssid=ssid, psk=psk, band=band, security='WPA2')
  subprocess.mock('wifi', 'disconnected_event', band)
  # Run once so that c will reconnect to s2.
  c.run_once()
  wvtest.WVPASS(c.wifi_for_band(band).connected_to_open())
  wvtest.WVPASSEQ(c.wifi_for_band(band).wpa_status().get('ssid'), 's2')
  # Now run until it sees the lack of ACS access.
  c.run_until_interface_update()
  wvtest.WVPASSEQ(c.wifi_for_band(band).last_successful_bss_info, None)

  # Test that we wait dhcp_wait_s seconds for a DHCP lease before trying the
  # next BSSID.  The scan results contain an s3 AP with vendor IEs that fails to
  # send a DHCP lease.  This ensures that s3 will be tried before any other AP,
  # which lets us force a timeout and proceed to the next AP.  Having a stale
  # WLAN configuration shouldn't interrupt provisioning.
  del c.wifi_for_band(band).cycler
  subprocess.mock('wifi', 'remote_ap', bssid='ff:ee:dd:cc:bb:aa', ssid='s3',
                  band=band, security=None, hidden=True,
                  vendor_ies=(('f4:f5:e8', '01'), ('f4:f5:e8', '03 73 33')))
  subprocess.mock('run-dhclient', c.wifi_for_band(band).name, failure=True)

  # First iteration: check that we try s3.
  c.run_until_scan(band)
  last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
  wvtest.WVPASSEQ(last_bss_info.ssid, 's3')
  wvtest.WVPASSEQ(last_bss_info.bssid, 'ff:ee:dd:cc:bb:aa')
  # Attempt to interrupt provisioning, make sure it doesn't work.
  c._wlan_configuration[band].try_after = 0
  # Second iteration: check that we try s3 again since there's no gateway yet.
  c.run_once()
  last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
  wvtest.WVPASSEQ(last_bss_info.ssid, 's3')
  wvtest.WVPASSEQ(last_bss_info.bssid, 'ff:ee:dd:cc:bb:aa')
  wvtest.WVPASS(c.has_status_files([status.P.WAITING_FOR_DHCP,
                                    status.P.WAITING_FOR_PROVISIONING]))
  # Third iteration: sleep for dhcp_wait_s and check that we try another AP.
  time.sleep(c._dhcp_wait_s)
  c.run_once()
  last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
  wvtest.WVPASSNE(last_bss_info.ssid, 's3')
  wvtest.WVPASSNE(last_bss_info.bssid, 'ff:ee:dd:cc:bb:aa')

  # We can delete the stale WLAN config now, to simplify subsequent tests.
  # c.delete_wlan_config(band)
  subprocess.mock('cwmp', band, delete_config=True, write_now=True)

  # Now repeat the above, but for an ACS session that takes a while.  We don't
  # necessarily want to leave if it fails (so we don't want the third check),
  # but we do want to make sure we don't leave while we're still waiting for it.
  #
  # Unlike DHCP, which we can always simulate working immediately above, it is
  # wrong to simulate ACS sessions working for connections without ACS access.
  # This means we can either always wait for the ACS session timeout in every
  # test above, making the tests much slower, or we can set that timeout very
  # low and then be a little gross here and change it.  The latter is
  # unfortunately the lesser evil, because slow tests are bad.
  del c.wifi_for_band(band).cycler
  subprocess.mock('run-dhclient', c.wifi_for_band(band).name, failure=False)
  subprocess.mock('cwmp', band, acs_session_fails=True)

  # First iteration: check that we try s3.
  c.run_until_scan(band)
  last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
  wvtest.WVPASSEQ(last_bss_info.ssid, 's3')
  wvtest.WVPASSEQ(last_bss_info.bssid, 'ff:ee:dd:cc:bb:aa')

  # This is the gross part.
  # pylint: disable=protected-access
  c.wifi_for_band(band).provisioning_ratchet.steps[3].timeout = 0.5

  # Second iteration: check that we don't leave while waiting.
  c.run_once()
  last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
  wvtest.WVPASSEQ(last_bss_info.ssid, 's3')
  wvtest.WVPASSEQ(last_bss_info.bssid, 'ff:ee:dd:cc:bb:aa')
  wvtest.WVPASS(c.has_status_files([status.P.WAITING_FOR_ACS_SESSION,
                                    status.P.WAITING_FOR_PROVISIONING]))
  time.sleep(0.5)
  c.run_once()
  wvtest.WVPASS(c.has_status_files([status.P.PROVISIONING_FAILED]))
  c.wifi_for_band(band).provisioning_ratchet.steps[3].timeout = 0

  # Finally, test successful provisioning.
  del c.wifi_for_band(band).cycler
  subprocess.mock('cwmp', band, acs_session_fails=False)
  # First iteration: check that we try s3.
  c.run_until_scan(band)
  last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
  wvtest.WVPASSEQ(last_bss_info.ssid, 's3')
  wvtest.WVPASSEQ(last_bss_info.bssid, 'ff:ee:dd:cc:bb:aa')

  c.run_once()
  last_bss_info = c.wifi_for_band(band).last_attempted_bss_info
  wvtest.WVPASSEQ(last_bss_info.ssid, 's3')
  wvtest.WVPASSEQ(last_bss_info.bssid, 'ff:ee:dd:cc:bb:aa')
  wvtest.WVFAIL(c.has_status_files([status.P.WAITING_FOR_ACS_SESSION,
                                    status.P.WAITING_FOR_PROVISIONING]))
  wvtest.WVPASS(c.has_status_files([status.P.PROVISIONING_COMPLETED]))


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897)
def connection_manager_test_generic_marvell8897_2g(c):
  connection_manager_test_generic(c, '2.4')


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897)
def connection_manager_test_generic_marvell8897_5g(c):
  connection_manager_test_generic(c, '5')


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_ATH10K)
def connection_manager_test_generic_ath9k_ath10k_2g(c):
  connection_manager_test_generic(c, '2.4')


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_ATH10K)
def connection_manager_test_generic_ath9k_ath10k_5g(c):
  connection_manager_test_generic(c, '5')


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_FRENZY)
def connection_manager_test_generic_ath9k_frenzy_2g(c):
  connection_manager_test_generic(c, '2.4')


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_FRENZY)
def connection_manager_test_generic_ath9k_frenzy_5g(c):
  connection_manager_test_generic(c, '5')


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_FRENZY)
def connection_manager_test_generic_frenzy_5g(c):
  connection_manager_test_generic(c, '5')


def connection_manager_test_dual_band_two_radios(c):
  """Test ConnectionManager for devices with two radios.

  This test should be kept roughly parallel to the one-radio test.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
  """
  ssid = 'my ssid'
  psk = 'passphrase'

  wvtest.WVPASSEQ(len(c._binwifi_commands), 2)

  for band in ['2.4', '5']:
    wvtest.WVPASS(('stop', '--band', band, '--persist') in c._binwifi_commands)

    subprocess.mock('wifi', 'remote_ap',
                    bssid='11:22:33:44:55:66',
                    ssid=ssid, psk=psk, band=band, security='WPA2')

  # Bring up ethernet, access.
  c.set_ethernet(True)
  c.run_once()
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  # Bring up both access points.
  for band in ('2.4', '5'):
    subprocess.mock('cwmp', band, ssid=ssid, psk=psk, access_point=True,
                    write_now=True)
  c.run_once()
  wvtest.WVPASS(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVPASS(c.bridge.current_routes())
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVFAIL(c.client_up('5'))
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # Disable the 2.4 GHz AP, make sure the 5 GHz AP stays up.  2.4 GHz should
  # join the WLAN.
  subprocess.mock('cwmp', '2.4', access_point=False, write_now=True)
  c.run_until_interface_update()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVPASS(c.client_up('2.4'))
  wvtest.WVPASS(c.bridge.current_routes())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_routes())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # Delete the 2.4 GHz WLAN configuration; it should leave the WLAN but nothing
  # else should change.
  subprocess.mock('cwmp', '2.4', delete_config=True, write_now=True)
  c.run_until_interface_update()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVPASS(c.bridge.current_routes())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # Disable the wired connection and remove the WLAN configurations.  Both
  # radios should scan.  Wait for 5 GHz to scan, then enable scan results for
  # 2.4. This should lead to ACS access.
  subprocess.mock('cwmp', '5', delete_config=True, write_now=True)
  c.set_ethernet(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.bridge.current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # The 5 GHz scan has no results.
  c.run_until_scan('5')
  c.run_once()
  c.run_until_interface_update()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.bridge.current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # The next 2.4 GHz scan will have results.
  _enable_basic_scan_results('2.4')
  c.run_until_scan('2.4')
  # Now run for enough cycles that s2 will have been tried.
  for _ in range(len(c.wifi_for_band('2.4').cycler)):
    c.run_once()
  c.run_until_interface_update()
  wvtest.WVPASS(c.acs())
  wvtest.WVFAIL(c.bridge.current_routes_normal_testonly())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_routes())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())
  c.run_once()
  wvtest.WVPASS(subprocess.upload_logs_and_wait.uploaded_logs())


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_ATH10K)
def connection_manager_test_dual_band_two_radios_ath9k_ath10k(c):
  connection_manager_test_dual_band_two_radios(c)


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_FRENZY)
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
  subprocess.mock('cwmp', '2.4', ssid=ssid, psk=psk, access_point=True,
                  write_now=True)
  subprocess.mock('cwmp', '5', ssid=ssid, psk=psk, access_point=True,
                  write_now=True)
  c.run_once()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVPASS(c.bridge.current_routes())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # Disable the 2.4 GHz AP; nothing should change.  The 2.4 GHz client should
  # not be up because the same radio is being used to run a 5 GHz AP.
  subprocess.mock('cwmp', '2.4', access_point=False, write_now=True)
  c.run_until_interface_update()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVPASS(c.bridge.current_routes())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # Delete the 2.4 GHz WLAN configuration; nothing should change.
  subprocess.mock('cwmp', '2.4', delete_config=True, write_now=True)
  c.run_once()
  wvtest.WVFAIL(c.access_point_up('2.4'))
  wvtest.WVPASS(c.access_point_up('5'))
  wvtest.WVFAIL(c.client_up('2.4'))
  wvtest.WVPASS(c.bridge.current_routes())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # Disable the wired connection and remove the WLAN configurations.  There
  # should be a single scan that leads to ACS access.  (It doesn't matter which
  # band we specify in run_until_scan, since both bands point to the same
  # interface.)
  subprocess.mock('cwmp', '5', delete_config=True, write_now=True)
  c.set_ethernet(False)
  c.run_once()
  wvtest.WVFAIL(c.acs())
  wvtest.WVFAIL(c.bridge.current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('2.4').current_routes_normal_testonly())
  wvtest.WVFAIL(c.wifi_for_band('5').current_routes_normal_testonly())

  # The scan will have results that will lead to ACS access.
  _enable_basic_scan_results('2.4')
  c.run_until_scan('5')
  for _ in range(len(c.wifi_for_band('2.4').cycler)):
    c.run_once()
  c.run_until_interface_update()
  wvtest.WVPASS(c.acs())
  wvtest.WVFAIL(c.bridge.current_routes_normal_testonly())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_routes())
  wvtest.WVPASS(c.wifi_for_band('5').current_routes())
  c.run_once()
  wvtest.WVPASS(subprocess.upload_logs_and_wait.uploaded_logs())


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897)
def connection_manager_test_dual_band_one_radio_marvell8897(c):
  connection_manager_test_dual_band_one_radio(c)


@test_common.wvtest
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
  ssid = 'my ssid'
  psk = 'my psk'
  subprocess.mock('wifi', 'remote_ap', band='2.4', ssid=ssid, psk=psk,
                  bssid='00:00:00:00:00:00', security='WPA2',
                  connection_check_result='succeed')

  wvtest.WVPASSEQ(c.wifi_for_band('5'), None)

  c.set_ethernet(True)
  wvtest.WVPASS(c.acs())
  wvtest.WVPASS(c.internet())

  # Make sure this doesn't crash.
  subprocess.mock('cwmp', '5', ssid=ssid, psk=psk, write_now=True)
  c.run_once()
  subprocess.mock('cwmp', '5', access_point=True, write_now=True)
  c.run_once()
  subprocess.mock('cwmp', '5', access_point=False, write_now=True)
  c.run_once()
  subprocess.mock('cwmp', '5', delete_config=True, write_now=True)
  c.run_once()

  # Make sure 2.4 still works.
  subprocess.mock('cwmp', '2.4', ssid=ssid, psk=psk, write_now=True)
  # Connect
  c.run_once()
  # Process DHCP results
  c.run_once()
  wvtest.WVPASS(c.wifi_for_band('2.4').acs())
  wvtest.WVPASS(c.wifi_for_band('2.4').internet())
  wvtest.WVPASS(c.wifi_for_band('2.4').current_routes())


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897,
                         __test_interfaces_already_up=['eth0', 'wcli0'])
def connection_manager_test_wifi_already_up(c):
  """Test ConnectionManager when wifi is already up.

  Args:
    c:  The ConnectionManager set up by @connection_manager_test.
  """
  wvtest.WVPASS(c._connected_to_wlan(c.wifi_for_band('2.4')))
  wvtest.WVPASS(c.wifi_for_band('2.4').current_routes())


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897, wlan_configs={'5': True})
def connection_manager_one_radio_marvell8897_existing_config_5g_ap(c):
  wvtest.WVPASSEQ(len(c._binwifi_commands), 1)
  wvtest.WVPASSEQ(('stopclient', '--band', '5', '--persist'),
                  c._binwifi_commands[0])


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897,
                         wlan_configs={'5': False})
def connection_manager_one_radio_marvell8897_existing_config_5g_no_ap(c):
  wvtest.WVPASSEQ(len(c._binwifi_commands), 1)
  wvtest.WVPASSEQ(('stopap', '--band', '5', '--persist'),
                  c._binwifi_commands[0])


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_ATH9K_ATH10K,
                         wlan_configs={'5': True})
def connection_manager_two_radios_ath9k_ath10k_existing_config_5g_ap(c):
  wvtest.WVPASSEQ(len(c._binwifi_commands), 2)
  wvtest.WVPASS(('stop', '--band', '2.4', '--persist') in c._binwifi_commands)
  wvtest.WVPASS(('stopclient', '--band', '5', '--persist')
                in c._binwifi_commands)


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897)
def connection_manager_conman_no_2g_wlan(c):
  unused_raii = experiment_testutils.MakeExperimentDirs()

  # First, establish that we connect on 2.4 without the experiment, to make sure
  # this test doesn't spuriously pass.
  ssid = 'my ssid'
  psk = 'my psk'
  subprocess.mock('wifi', 'remote_ap', ssid=ssid, psk=psk, band='2.4',
                  bssid='00:00:00:00:00:00')
  subprocess.mock('cwmp', '2.4', ssid=ssid, psk=psk, write_now=True)
  c.run_once()
  wvtest.WVPASS(c.client_up('2.4'))

  # Now, force a disconnect by deleting the config.
  subprocess.mock('cwmp', '2.4', delete_config=True, write_now=True)
  c.run_once()
  wvtest.WVFAIL(c.client_up('2.4'))

  # Now enable the experiment, recreate the config, and make sure we don't
  # connect.
  experiment_testutils.enable('WifiNo2GClient')
  subprocess.mock('cwmp', '2.4', ssid=ssid, psk=psk, write_now=True)
  c.run_once()
  wvtest.WVFAIL(c.client_up('2.4'))


@test_common.wvtest
@connection_manager_test(WIFI_SHOW_OUTPUT_MARVELL8897,
                         wlan_configs={'5': False}, wlan_retry_s=30,
                         __test_interfaces_already_up=[])
def test_regression_b29364958(c):
  def count_setclient_calls():
    return len([1 for cmd, _ in subprocess.CALL_HISTORY
                if 'wifi' in cmd and 'setclient' in cmd])

  wvtest.WVPASSEQ(1, count_setclient_calls())
  for _ in range(10):
    c.run_once()
  wvtest.WVPASSEQ(1, count_setclient_calls())


if __name__ == '__main__':
  wvtest.wvtest_main()
