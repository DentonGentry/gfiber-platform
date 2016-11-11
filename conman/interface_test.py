#!/usr/bin/python

"""Tests for connection_manager.py."""

import logging
import os
import shutil
import subprocess
import tempfile
import time

# This has to be called before another module calls it with a higher log level.
# pylint: disable=g-import-not-at-top
logging.basicConfig(level=logging.DEBUG)

import experiment_testutils
import interface
from wvtest import wvtest


# pylint: disable=line-too-long
_IP_ADDR_SHOW_TPL = """4: {name}: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP qlen 1000
    inet {ip}/21 brd 100.100.55.255 scope global {name}
       valid_lft forever preferred_lft forever
"""


class FakeInterfaceMixin(object):
  """Replace Interface methods which interact with the system."""

  def __init__(self, *args, **kwargs):
    super(FakeInterfaceMixin, self).__init__(*args, **kwargs)
    self.set_connection_check_result('succeed')
    subprocess.ip.register_testonly(self.name)

  def set_connection_check_result(self, result):
    if result in ['succeed', 'fail', 'restricted']:
      subprocess.mock(self.CONNECTION_CHECK, self.name, result)
    else:
      raise ValueError('Invalid fake connection_check value.')

  def current_routes_normal_testonly(self):
    result = self.current_routes()
    return {k: v for k, v in result.iteritems() if int(v.get('metric', 0)) < 50}


class Bridge(FakeInterfaceMixin, interface.Bridge):
  pass


class Wifi(FakeInterfaceMixin, interface.Wifi):
  """Fake Wifi for testing."""
  pass


class FrenzyWifi(FakeInterfaceMixin, interface.FrenzyWifi):
  pass


@wvtest.wvtest
def bridge_test():
  """Test Interface and Bridge."""
  tmp_dir = tempfile.mkdtemp()

  try:
    autoprov_filepath = os.path.join(tmp_dir, 'autoprov')
    b = Bridge('br0', '10', acs_autoprovisioning_filepath=autoprov_filepath)
    b.set_connection_check_result('succeed')

    wvtest.WVFAIL(b.acs())
    wvtest.WVFAIL(b.internet())
    wvtest.WVFAIL(b.current_routes())
    wvtest.WVFAIL(b.current_routes_normal_testonly())
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))

    b.add_moca_station(0)
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))
    b.set_subnet('192.168.1.0/24')
    b.set_gateway_ip('192.168.1.1')
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))
    # Everything should fail because the interface is not initialized.
    wvtest.WVFAIL(b.acs())
    wvtest.WVFAIL(b.internet())
    wvtest.WVFAIL(b.current_routes_normal_testonly())
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))
    b.initialize()
    wvtest.WVPASS(b.acs())
    wvtest.WVPASS(b.internet())
    current_routes = b.current_routes()
    wvtest.WVPASSEQ(len(current_routes), 3)
    wvtest.WVPASS('default' in current_routes)
    wvtest.WVPASS('subnet' in current_routes)
    wvtest.WVPASS('multicast' in current_routes)
    wvtest.WVPASS(os.path.exists(autoprov_filepath))

    b.add_moca_station(1)
    wvtest.WVPASS(b.acs())
    wvtest.WVPASS(b.internet())
    wvtest.WVPASSEQ(len(b.current_routes()), 3)
    wvtest.WVPASS(os.path.exists(autoprov_filepath))

    b.remove_moca_station(0)
    b.remove_moca_station(1)
    wvtest.WVFAIL(b.acs())
    wvtest.WVFAIL(b.internet())
    # We have no links, so should have no routes.
    wvtest.WVFAIL(b.current_routes())
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))

    b.add_moca_station(2)
    wvtest.WVPASS(b.acs())
    wvtest.WVPASS(b.internet())
    wvtest.WVPASSEQ(len(b.current_routes()), 3)
    wvtest.WVPASS(os.path.exists(autoprov_filepath))

    b.set_connection_check_result('fail')
    b.update_routes()
    wvtest.WVFAIL(b.acs())
    wvtest.WVFAIL(b.internet())
    # We have links but the connection check failed, so we should only have a
    # low priority route, i.e. metric at least 50.
    wvtest.WVPASSEQ(len(b.current_routes()), 3)
    wvtest.WVFAIL(b.current_routes_normal_testonly())
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))

    b.set_connection_check_result('restricted')
    b.update_routes()
    wvtest.WVPASS(b.acs())
    wvtest.WVFAIL(b.internet())
    wvtest.WVPASSEQ(len(b.current_routes()), 3)
    wvtest.WVPASS(os.path.exists(autoprov_filepath))

    wvtest.WVFAIL(b.get_ip_address())
    subprocess.call(['ip', 'addr', 'add', '192.168.1.100', 'dev', b.name])
    wvtest.WVPASSEQ(b.get_ip_address(), '192.168.1.100')

    # Get a new gateway/subnet (e.g. due to joining a new network).
    # Not on the subnet; adding IP should fail.
    b.set_gateway_ip('192.168.2.1')
    wvtest.WVFAIL('default' in b.current_routes())
    wvtest.WVPASS('subnet' in b.current_routes())
    # Without a default route, the connection check should fail.
    wvtest.WVFAIL(b.acs())

    # Now we get the subnet and should add updated subnet and gateway routes.
    b.set_subnet('192.168.2.0/24')
    wvtest.WVPASSEQ(b.current_routes()['default']['via'], '192.168.2.1')
    wvtest.WVPASSLE(int(b.current_routes()['default']['metric']), 50)
    wvtest.WVPASSEQ(b.current_routes()['subnet']['route'], '192.168.2.0/24')
    wvtest.WVPASSLE(int(b.current_routes()['subnet']['metric']), 50)
    wvtest.WVPASS(b.acs())

    # If we have no subnet, make sure that both subnet and default routes are
    # removed.
    b.set_subnet(None)
    wvtest.WVFAIL('subnet' in b.current_routes())
    wvtest.WVFAIL('default' in b.current_routes())

    # Now repeat the new-network test, but with a faulty connection.  Make sure
    # the metrics are set appropriately.
    b.set_connection_check_result('fail')
    b.set_subnet('192.168.3.0/24')
    b.set_gateway_ip('192.168.3.1')
    wvtest.WVPASSGE(int(b.current_routes()['default']['metric']), 50)
    wvtest.WVPASSGE(int(b.current_routes()['subnet']['metric']), 50)

    # Now test deleting only the gateway IP.
    b.set_gateway_ip(None)
    wvtest.WVPASS('subnet' in b.current_routes())
    wvtest.WVFAIL('default' in b.current_routes())

  finally:
    shutil.rmtree(tmp_dir)


def generic_wifi_test(w, wpa_path):
  # Not currently connected.
  subprocess.wifi.WPA_PATH = wpa_path
  wvtest.WVFAIL(w.wpa_supplicant)

  # wpa_supplicant connects.
  ssid = 'my=ssid'
  psk = 'passphrase'
  subprocess.mock('wifi', 'remote_ap', ssid=ssid, psk=psk, band='5',
                  bssid='00:00:00:00:00:00', connection_check_result='succeed')
  subprocess.check_call(['wifi', 'setclient', '--ssid', ssid, '--band', '5'],
                        env={'WIFI_CLIENT_PSK': psk})
  wvtest.WVPASS(w.wpa_supplicant)
  w.set_gateway_ip('192.168.1.1')

  # wpa_supplicant disconnects.
  subprocess.mock('wifi', 'disconnected_event', '5')
  wvtest.WVFAIL(w.wpa_supplicant)

  # The wpa_supplicant process disconnects and terminates.
  subprocess.check_call(['wifi', 'stopclient', '--band', '5'])
  wvtest.WVFAIL(w.wpa_supplicant)


@wvtest.wvtest
def wifi_test():
  """Test Wifi."""
  w = Wifi('wcli0', '21')
  w.initialize()

  try:
    wpa_path = tempfile.mkdtemp()
    conman_path = tempfile.mkdtemp()
    subprocess.set_conman_paths(conman_path, None)
    subprocess.mock('wifi', 'interfaces',
                    subprocess.wifi.MockInterface(phynum='0', bands=['5'],
                                                  driver='cfg80211'))
    generic_wifi_test(w, wpa_path)

  finally:
    shutil.rmtree(wpa_path)
    shutil.rmtree(conman_path)


@wvtest.wvtest
def frenzy_wifi_test():
  """Test FrenzyWifi."""
  w = FrenzyWifi('wlan0', '20')
  w.initialize()

  try:
    wpa_path = tempfile.mkdtemp()
    conman_path = tempfile.mkdtemp()
    subprocess.set_conman_paths(conman_path, None)
    subprocess.mock('wifi', 'interfaces',
                    subprocess.wifi.MockInterface(phynum='0', bands=['5'],
                                                  driver='frenzy'))
    generic_wifi_test(w, wpa_path)

  finally:
    shutil.rmtree(wpa_path)
    shutil.rmtree(conman_path)


@wvtest.wvtest
def simulate_wireless_test():
  """Test the WifiSimulateWireless experiment."""
  unused_raii = experiment_testutils.MakeExperimentDirs()

  tmp_dir = tempfile.mkdtemp()
  interface.CWMP_PATH = tempfile.mkdtemp()
  interface.MAX_ACS_FAILURE_S = 1

  contact = os.path.join(interface.CWMP_PATH, 'acscontact')
  connected = os.path.join(interface.CWMP_PATH, 'acsconnected')

  try:
    autoprov_filepath = os.path.join(tmp_dir, 'autoprov')
    b = Bridge('br0', '10', acs_autoprovisioning_filepath=autoprov_filepath)
    b.add_moca_station(0)
    b.set_gateway_ip('192.168.1.1')
    b.set_subnet('192.168.1.0/24')
    b.set_connection_check_result('succeed')
    b.initialize()

    # Initially, the connection check passes.
    wvtest.WVPASS(b.internet())

    # Enable the experiment.
    experiment_testutils.enable('WifiSimulateWireless')
    # Calling update_routes overwrites the connection status cache, which we
    # need in order to see the effects we are looking for immediately
    # (ConnectionManager calls this every few seconds).
    b.update_routes()
    wvtest.WVFAIL(b.internet())

    # Create an ACS connection attempt.
    open(contact, 'w')
    b.update_routes()
    wvtest.WVFAIL(b.internet())

    # Record success.
    open(connected, 'w')
    b.update_routes()
    wvtest.WVFAIL(b.internet())

    # Disable the experiment and the connection check should pass again.
    experiment_testutils.disable('WifiSimulateWireless')
    b.update_routes()
    wvtest.WVPASS(b.internet())

    # Reenable the experiment and the connection check should fail again.
    experiment_testutils.enable('WifiSimulateWireless')
    b.update_routes()
    wvtest.WVFAIL(b.internet())

    # Wait until we've failed for long enough for the experiment to "expire",
    # then log another attempt without success.  Make sure the connection check
    # passes.
    time.sleep(interface.MAX_ACS_FAILURE_S)
    open(contact, 'w')
    b.update_routes()
    wvtest.WVPASS(b.internet())

  finally:
    shutil.rmtree(tmp_dir)
    shutil.rmtree(interface.CWMP_PATH)


if __name__ == '__main__':
  wvtest.wvtest_main()
