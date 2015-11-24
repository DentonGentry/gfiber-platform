#!/usr/bin/python

"""Models wired and wireless interfaces."""

import logging
import os
import re
import subprocess

import wpactrl

METRIC_5GHZ = 20
METRIC_24GHZ_5GHZ = 21
METRIC_24GHZ = 22
METRIC_TEMPORARY_CONNECTION_CHECK = 99


class Interface(object):
  """Represents an interface.

  Base class for more specific interface types.
  """

  CONNECTION_CHECK = 'connection_check'
  IP_ROUTE = ['ip', 'route']

  def __init__(self, name, metric):
    self.name = name

    # Currently connected links for this interface, e.g. ethernet.
    self.links = set()

    # Whether [ACS, internet] access is currently available via this interface.
    self._has_acs = None
    self._has_internet = None

    # The gateway IP for this interface.
    self._gateway_ip = None
    self.metric = metric

  def _connection_check(self, check_acs):
    """Check this interface's connection status.

    Args:
      check_acs:  If true, check for ACS access rather than internet access.

    Returns:
      Whether the connection is working.
    """
    if not self.links:
      return False

    logging.debug('gateway ip for %s is %s', self.name, self._gateway_ip)
    if self._gateway_ip is None:
      return False

    # Temporarily add a route to make sure the connection check can be run.
    # Give it a high metric so that it won't interfere with normal default
    # routes.
    added_temporary_route = False
    if not self.current_route():
      logging.debug('Adding temporary connection check route for dev %s',
                    self.name)
      self._ip_route('add', 'default',
                     'via', self._gateway_ip,
                     'dev', self.name,
                     'metric', str(METRIC_TEMPORARY_CONNECTION_CHECK))

    cmd = [self.CONNECTION_CHECK, '-I', self.name]
    if check_acs:
      cmd.append('-a')

    with open(os.devnull, 'w') as devnull:
      result = subprocess.call(cmd, stdout=devnull, stderr=devnull) == 0

    # Delete the temporary route.
    if added_temporary_route:
      logging.debug('Deleting temporary connection check route for dev %s',
                    self.name)
      self._ip_route('del', 'default',
                     'dev', self.name,
                     'metric', str(METRIC_TEMPORARY_CONNECTION_CHECK))

    return result

  def acs(self):
    if self._has_acs is None:
      self._has_acs = self._connection_check(check_acs=True)

    return self._has_acs

  def internet(self):
    if self._has_internet is None:
      self._has_internet = self._connection_check(check_acs=False)

    return self._has_internet

  def add_route(self):
    """Adds a default route for this interface.

    First, checks whether an equivalent route already exists, and if so,
    returns.
    """
    if self.metric is None:
      logging.info('Cannot add route for %s without a metric.', self.name)
      return

    if self._gateway_ip is None:
      logging.info('Cannot add route for %s without a gateway IP.', self.name)
      return

    # If the current default route is the same, there is nothing to do.  If it
    # exists but is different, delete it before adding an updated one.
    current = self.current_route()
    if current:
      if (current.get('via', None) == self._gateway_ip and
          current.get('metric', None) == str(self.metric)):
        return
      else:
        self.delete_route()

    logging.debug('Adding default route for dev %s', self.name)
    self._ip_route('add', 'default',
                   'via', self._gateway_ip,
                   'dev', self.name,
                   'metric', str(self.metric))

  def delete_route(self):
    while self.current_route():
      logging.debug('Deleting default route for dev %s', self.name)
      self._ip_route('del', 'default',
                     'dev', self.name)

  def current_route(self):
    """Read the current default route for this interface.

    Returns:
      A dict containing the gateway [and metric] of the route, or an empty dict
      if there is currently no default route for this interface.
    """
    result = {}
    for line in self._ip_route().splitlines():
      if line.startswith('default') and 'dev %s' % self.name in line:
        key = None
        for token in line.split():
          if token in ['via', 'metric']:
            key = token
          elif key:
            result[key] = token
            key = None

    return result

  def _ip_route(self, *args):
    try:
      logging.debug('%s calling ip route %s', self.name, ' '.join(args))
      return subprocess.check_output(self.IP_ROUTE + list(args))
    except subprocess.CalledProcessError as e:
      logging.error('Failed to call "ip route" with args %r: %s', args,
                    e.message)
      return ''

  def set_gateway_ip(self, gateway_ip):
    logging.debug('New gateway IP %s for %s', gateway_ip, self.name)
    self._gateway_ip = gateway_ip
    self.update_routes()

  def _set_link_status(self, link, is_up):
    """Set whether a link is up or not."""
    if is_up == (link in self.links):
      return

    had_links = bool(self.links)

    if is_up:
      logging.debug('%s gained link %s', self.name, link)
      self.links.add(link)
    else:
      logging.debug('%s lost link %s', self.name, link)
      self.links.remove(link)

    # If a link goes away, we may have lost access to something but not gained
    # it, and vice versa.
    if is_up != self._has_acs:
      self._has_acs = None

    if is_up != self._has_internet:
      self._has_internet = None

    if had_links != bool(self.links):
      self.update_routes(expire_cache=False)

  def expire_connection_status_cache(self):
    logging.debug('Expiring connection status cache for %s', self.name)
    self._has_internet = self._has_acs = None

  def update_routes(self, expire_cache=True):
    """Update this interface's routes.

    If the interface has gained ACS or internet access, add a route.  If it had
    either and now has neither, delete the route.

    Args:
      expire_cache:  If true, force a recheck of connection status before
      deciding whether to add or remove routes.
    """
    logging.debug('Updating routes for %s', self.name)
    # We care about the distinction between None (unknown) and False (known
    # inaccessible) here.
    # pylint: disable=g-explicit-bool-comparison
    maybe_had_acs = self._has_acs != False
    maybe_had_internet = self._has_internet != False

    if expire_cache:
      self.expire_connection_status_cache()

    has_acs = self.acs()
    has_internet = self.internet()
    if ((not maybe_had_acs and has_acs) or
        (not maybe_had_internet and has_internet)):
      self.add_route()
    elif ((maybe_had_acs or maybe_had_internet) and not
          (has_acs or has_internet)):
      self.delete_route()


class Bridge(Interface):
  """Represents the wired bridge."""

  def __init__(self, *args, **kwargs):
    super(Bridge, self).__init__(*args, **kwargs)
    self._moca_stations = set()

  @property
  def moca(self):
    return bool(self._moca_stations)

  @moca.setter
  def moca(self, is_up):
    self._set_link_status('moca', is_up)

  @property
  def ethernet(self):
    return 'ethernet' in self.links

  @ethernet.setter
  def ethernet(self, is_up):
    self._set_link_status('ethernet', is_up)

  def add_moca_station(self, node_id):
    if node_id not in self._moca_stations:
      self._moca_stations.add(node_id)
      self.moca = True

  def remove_moca_station(self, node_id):
    if node_id in self._moca_stations:
      self._moca_stations.remove(node_id)
      self.moca = bool(self._moca_stations)


class Wifi(Interface):
  """Represents the wireless interface."""

  WPA_EVENT_RE = re.compile(r'<\d+>CTRL-EVENT-(?P<event>[A-Z\-]+).*')

  def __init__(self, *args, **kwargs):
    self.bands = kwargs.pop('bands', [])
    super(Wifi, self).__init__(*args, **kwargs)
    self._wpa_control = None

  @property
  def wpa_supplicant(self):
    return 'wpa_supplicant' in self.links

  @wpa_supplicant.setter
  def wpa_supplicant(self, is_up):
    self._set_link_status('wpa_supplicant', is_up)

  def attached(self):
    return self._wpa_control and self._wpa_control.attached

  def attach_wpa_control(self, path):
    if self.attached():
      return

    socket = os.path.join(path, self.name)
    if os.path.exists(socket):
      try:
        self._wpa_control = self.get_wpa_control(socket)
        self._wpa_control.attach()
      except wpactrl.error as e:
        logging.error('Error attaching to wpa_supplicant: %s', e)
        return

      self.wpa_supplicant = ('wpa_state=COMPLETED' in
                             self._wpa_control.request('STATUS'))

  def get_wpa_control(self, socket):
    return wpactrl.WPACtrl(socket)

  def detach_wpa_control(self):
    if self.attached():
      try:
        self._wpa_control.detach()
      except wpactrl.error:
        logging.error('Failed to detach from wpa_supplicant interface. This '
                      'may mean something else killed wpa_supplicant.')
        self._wpa_control = None

      self.wpa_supplicant = False

  def handle_wpa_events(self):
    if not self.attached():
      self.wpa_supplicant = False
      return

    while self._wpa_control.pending():
      match = self.WPA_EVENT_RE.match(self._wpa_control.recv())
      if match:
        event = match.group('event')
        if event == 'CONNECTED':
          self.wpa_supplicant = True
        elif event in ('DISCONNECTED', 'TERMINATING', 'ASSOC-REJECT',
                       'AUTH-REJECT'):
          self.wpa_supplicant = False
          if event == 'TERMINATING':
            self.detach_wpa_control()
            break

        self.update_routes()
