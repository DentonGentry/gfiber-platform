#!/usr/bin/python

"""Tests for connection_manager.py."""

import logging
import os
import shutil
import tempfile

import wpactrl

import interface
from wvtest import wvtest

logging.basicConfig(level=logging.DEBUG)


class FakeInterfaceMixin(object):
  """Replace Interface methods which interact with the system."""

  def __init__(self, *args, **kwargs):
    super(FakeInterfaceMixin, self).__init__(*args, **kwargs)
    self.set_connection_check_result('succeed')
    self.routing_table = {}

  def set_connection_check_result(self, result):
    if result in ['succeed', 'fail', 'restricted']:
      # pylint: disable=invalid-name
      self.CONNECTION_CHECK = './test/' + result
    else:
      raise ValueError('Invalid fake connection_check script.')

  def _really_ip_route(self, *args):
    if not args:
      return '\n'.join(self.routing_table.values() +
                       ['1.2.3.4/24 dev %s proto kernel scope link' % self.name,
                        'default via 1.2.3.4 dev fake0',
                        'random junk'])

    metric = None
    if 'metric' in args:
      metric = args[args.index('metric') + 1]
    key = (self.name, metric)
    if args[0] == 'add' and key not in self.routing_table:
      logging.debug('Adding route for %r', key)
      self.routing_table[key] = ' '.join(args[1:])
    elif args[0] == 'del':
      if key in self.routing_table:
        logging.debug('Deleting route for %r', key)
        del self.routing_table[key]
      elif key[1] is None:
        # pylint: disable=g-builtin-op
        for k in self.routing_table.keys():
          if k[0] == key[0]:
            logging.debug('Deleting route for %r (generalized from %s)', k, key)
            del self.routing_table[k]
            break


class Bridge(FakeInterfaceMixin, interface.Bridge):
  pass


class FakeWPACtrl(object):
  """Fake wpactrl.WPACtrl."""

  # pylint: disable=unused-argument
  def __init__(self, socket):
    self._socket = socket
    self.events = []
    self.attached = False
    self.connected = False

  def pending(self):
    return bool(self.events)

  def recv(self):
    return self.events.pop(0)

  def attach(self):
    if not os.path.exists(self._socket):
      raise wpactrl.error('wpactrl_attach failed')
    self.attached = True

  def detach(self):
    if not os.path.exists(self._socket):
      raise wpactrl.error('wpactrl_detach failed')
    self.attached = False

  def request(self, request_type):
    if request_type == 'STATUS':
      return 'foo\nwpa_state=COMPLETED\nbar' if self.connected else 'foo'
    else:
      raise ValueError('Invalid request_type %s' % request_type)

  # Below methods are not part of WPACtrl.

  def add_event(self, event):
    self.events.append(event)


class Wifi(FakeInterfaceMixin, interface.Wifi):
  """Fake Wifi for testing."""

  CONNECTED_EVENT = '<2>CTRL-EVENT-CONNECTED'
  DISCONNECTED_EVENT = '<2>CTRL-EVENT-DISCONNECTED'
  TERMINATING_EVENT = '<2>CTRL-EVENT-TERMINATING'

  def __init__(self, *args, **kwargs):
    super(Wifi, self).__init__(*args, **kwargs)
    self._initially_connected = False

  def attach_wpa_control(self, *args, **kwargs):
    if self._initially_connected and self._wpa_control:
      self._wpa_control.connected = True
    super(Wifi, self).attach_wpa_control(*args, **kwargs)

  def get_wpa_control(self, socket):
    result = FakeWPACtrl(socket)
    result.connected = self._initially_connected
    return result

  def add_connected_event(self):
    if self.attached():
      self._wpa_control.add_event(self.CONNECTED_EVENT)

  def add_disconnected_event(self):
    if self.attached():
      self._wpa_control.add_event(self.DISCONNECTED_EVENT)

  def add_terminating_event(self):
    if self.attached():
      self._wpa_control.add_event(self.TERMINATING_EVENT)


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
    wvtest.WVFAIL(b.current_route())
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))

    b.add_moca_station(0)
    b.set_gateway_ip('192.168.1.1')
    # Everything should fail because the interface is not initialized.
    wvtest.WVFAIL(b.acs())
    wvtest.WVFAIL(b.internet())
    wvtest.WVFAIL(b.current_route())
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))
    b.initialize()
    wvtest.WVPASS(b.acs())
    wvtest.WVPASS(b.internet())
    wvtest.WVPASS(b.current_route())
    wvtest.WVPASS(os.path.exists(autoprov_filepath))

    b.add_moca_station(1)
    wvtest.WVPASS(b.acs())
    wvtest.WVPASS(b.internet())
    wvtest.WVPASS(b.current_route())
    wvtest.WVPASS(os.path.exists(autoprov_filepath))

    b.remove_moca_station(0)
    b.remove_moca_station(1)
    wvtest.WVFAIL(b.acs())
    wvtest.WVFAIL(b.internet())
    wvtest.WVFAIL(b.current_route())
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))

    b.add_moca_station(2)
    wvtest.WVPASS(b.acs())
    wvtest.WVPASS(b.internet())
    wvtest.WVPASS(b.current_route())
    wvtest.WVPASS(os.path.exists(autoprov_filepath))

    b.set_connection_check_result('fail')
    b.update_routes()
    wvtest.WVFAIL(b.acs())
    wvtest.WVFAIL(b.internet())
    wvtest.WVFAIL(b.current_route())
    wvtest.WVFAIL(os.path.exists(autoprov_filepath))

    b.set_connection_check_result('restricted')
    b.update_routes()
    wvtest.WVPASS(b.acs())
    wvtest.WVFAIL(b.internet())
    wvtest.WVPASS(b.current_route())
    wvtest.WVPASS(os.path.exists(autoprov_filepath))

  finally:
    shutil.rmtree(tmp_dir)


@wvtest.wvtest
def wifi_test():
  """Test Wifi."""
  w = Wifi('wcli0', '21')
  w.set_connection_check_result('succeed')
  w.initialize()

  try:
    wpa_path = tempfile.mkdtemp()
    socket = os.path.join(wpa_path, w.name)
    open(socket, 'w')

    # Not currently connected.
    w.attach_wpa_control(wpa_path)
    wvtest.WVFAIL(w.wpa_supplicant)

    # pylint: disable=protected-access
    wpa_control = w._wpa_control

    # wpa_supplicant connects.
    wpa_control.add_event(Wifi.CONNECTED_EVENT)
    wvtest.WVFAIL(w.wpa_supplicant)
    w.handle_wpa_events()
    wvtest.WVPASS(w.wpa_supplicant)
    w.set_gateway_ip('192.168.1.1')

    # wpa_supplicant disconnects.
    wpa_control.add_event(Wifi.DISCONNECTED_EVENT)
    w.handle_wpa_events()
    wvtest.WVFAIL(w.wpa_supplicant)

    # Now, start over so we can test what happens when wpa_supplicant is already
    # connected when we attach.
    w.detach_wpa_control()
    # pylint: disable=protected-access
    w._initially_connected = True
    w.attach_wpa_control(wpa_path)
    wpa_control = w._wpa_control

    # wpa_supplicant was already connected when we attached.
    wvtest.WVPASS(w.wpa_supplicant)

    # The wpa_supplicant process disconnects and terminates.
    wpa_control.add_event(Wifi.DISCONNECTED_EVENT)
    wpa_control.add_event(Wifi.TERMINATING_EVENT)
    os.unlink(socket)
    w.handle_wpa_events()
    wvtest.WVFAIL(w.wpa_supplicant)

  finally:
    shutil.rmtree(wpa_path)


if __name__ == '__main__':
  wvtest.wvtest_main()
