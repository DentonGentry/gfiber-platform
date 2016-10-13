#!/usr/bin/python

"""Fake WPACtrl implementation."""

import os

import subprocess
import subprocess.wifi


CONNECTED_EVENT = '<2>CTRL-EVENT-CONNECTED'
DISCONNECTED_EVENT = '<2>CTRL-EVENT-DISCONNECTED'
TERMINATING_EVENT = '<2>CTRL-EVENT-TERMINATING'


# pylint: disable=invalid-name
class error(Exception):
  pass


class WPACtrl(object):
  """Fake wpactrl.WPACtrl."""

  # pylint: disable=unused-argument
  def __init__(self, wpa_socket):
    self._socket = wpa_socket
    self.interface_name = os.path.split(self._socket)[-1]
    self.attached = False
    self.connected = False
    self.request_status_fails = False
    self._clear_events()

  def pending(self):
    return bool(subprocess.wifi.INTERFACE_EVENTS[self.interface_name])

  def recv(self):
    return subprocess.wifi.INTERFACE_EVENTS[self.interface_name].pop(0)

  def attach(self):
    if not os.path.exists(self._socket):
      raise error('wpactrl_attach failed')
    self.attached = True

  def detach(self):
    self.attached = False
    self.connected = False
    self.check_socket_exists('wpactrl_detach failed')
    self._clear_events()

  def request(self, request_type):
    if request_type == 'STATUS':
      if self.request_status_fails:
        raise error('test error')
      try:
        return subprocess.check_output(['wpa_cli', '-i', self.interface_name,
                                        'status'])
      except subprocess.CalledProcessError as e:
        raise error(e.output)
    else:
      raise ValueError('Invalid request_type %s' % request_type)

  @property
  def ctrl_iface_path(self):
    return os.path.split(self._socket)[0]

  # Below methods are not part of WPACtrl.

  def check_socket_exists(self, msg='Socket does not exist'):
    if not os.path.exists(self._socket):
      raise error(msg)

  def _clear_events(self):
    subprocess.wifi.INTERFACE_EVENTS[self.interface_name] = []
