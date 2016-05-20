#!/usr/bin/python

"""Models wired and wireless interfaces."""

import json
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

    # Until this is set True, the routing table will not be touched.
    self._initialized = False

  def _connection_check(self, check_acs):
    """Check this interface's connection status.

    Args:
      check_acs:  If true, check for ACS access rather than internet access.

    Returns:
      Whether the connection is working.
    """
    # Until initialized, we want to act as if the interface is down.
    if not self._initialized:
      logging.debug('%s not initialized; not running connection_check%s',
                    self.name, ' (ACS)' if check_acs else '')
      return None

    if not self.links:
      logging.debug('Connection check for %s failed due to no links', self.name)
      return False

    logging.debug('Gateway IP for %s is %s', self.name, self._gateway_ip)
    if self._gateway_ip is None:
      logging.debug('Connection check for %s failed due to no gateway IP',
                    self.name)
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
      added_temporary_route = True

    cmd = [self.CONNECTION_CHECK, '-I', self.name]
    if check_acs:
      cmd.append('-a')

    with open(os.devnull, 'w') as devnull:
      result = subprocess.call(cmd, stdout=devnull, stderr=devnull) == 0
      logging.debug('Connection check%s for %s %s',
                    ' (ACS)' if check_acs else '',
                    self.name,
                    'passed' if result else 'failed')

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
    if not self._initialized:
      logging.debug('Not initialized, not running %s %s',
                    ' '.join(self.IP_ROUTE), ' '.join(args))
      return ''

    return self._really_ip_route(*args)

  def _really_ip_route(self, *args):
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
    maybe_had_acs = self._has_acs
    maybe_had_internet = self._has_internet

    if expire_cache:
      self.expire_connection_status_cache()

    has_acs = self.acs()
    has_internet = self.internet()

    # This is a little confusing:  We want to try adding a route if we _may_
    # have gone from no access to some access, and we want to try deleting the
    # route if we _may_ have lost *all* access. So the first condition checks
    # for truthiness but the elif checks for explicit Falsity (i.e. excluding
    # the None/unknown case).
    had_access = maybe_had_acs or maybe_had_internet
    # pylint: disable=g-explicit-bool-comparison
    maybe_had_access = maybe_had_acs != False or maybe_had_internet != False
    has_access = has_acs or has_internet
    if not had_access and has_access:
      self.add_route()
    elif maybe_had_access and not has_access:
      self.delete_route()

  def initialize(self):
    """Tell the interface it has its initial state.

    Until this is called, the interface won't run connection checks or touch the
    routing table.
    """
    self._initialized = True
    self.update_routes()


class Bridge(Interface):
  """Represents the wired bridge."""

  def __init__(self, *args, **kwargs):
    self._acs_autoprovisioning_filepath = kwargs.pop(
        'acs_autoprovisioning_filepath')
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

  def add_route(self):
    """We only want ACS autoprovisioning when we're using a wired route."""
    super(Bridge, self).add_route()
    open(self._acs_autoprovisioning_filepath, 'w')

  def delete_route(self):
    """We only want ACS autoprovisioning when we're using a wired route."""
    if os.path.exists(self._acs_autoprovisioning_filepath):
      os.unlink(self._acs_autoprovisioning_filepath)
    super(Bridge, self).delete_route()


class Wifi(Interface):
  """Represents a wireless interface."""

  WPA_EVENT_RE = re.compile(r'<\d+>CTRL-EVENT-(?P<event>[A-Z\-]+).*')
  # pylint: disable=invalid-name
  WPACtrl = wpactrl.WPACtrl

  def __init__(self, *args, **kwargs):
    self.bands = kwargs.pop('bands', [])
    super(Wifi, self).__init__(*args, **kwargs)
    self._wpa_control = None
    self.initial_ssid = None

  @property
  def wpa_supplicant(self):
    return 'wpa_supplicant' in self.links

  @wpa_supplicant.setter
  def wpa_supplicant(self, is_up):
    self._set_link_status('wpa_supplicant', is_up)

  def attached(self):
    return self._wpa_control and self._wpa_control.attached

  def attach_wpa_control(self, path):
    """Attach to the wpa_supplicant control interface.

    Args:
      path:  The path containing the wpa_supplicant control interface socket.

    Returns:
      Whether attaching was successful.
    """
    if self.attached():
      return True

    socket = os.path.join(path, self.name)
    try:
      self._wpa_control = self.get_wpa_control(socket)
      self._wpa_control.attach()
    except wpactrl.error as e:
      logging.error('Error attaching to wpa_supplicant: %s', e)
      return False

    for line in self._wpa_control.request('STATUS').splitlines():
      if '=' not in line:
        continue
      key, value = line.split('=', 1)
      if key == 'wpa_state':
        self.wpa_supplicant = value == 'COMPLETED'
      elif key == 'ssid' and not self._initialized:
        self.initial_ssid = value

    return True

  def get_wpa_control(self, socket):
    return self.WPACtrl(socket)

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

  def initialize(self):
    """Unset self.initial_ssid, which is only relevant during initialization."""

    self.initial_ssid = None
    super(Wifi, self).initialize()


class FrenzyWPACtrl(object):
  """A fake WPACtrl for Frenzy devices.

  Implements the same functions used on the normal WPACtrl, using a combination
  of the QCSAPI and wifi_files.  Keeps state in order to generate fake events by
  diffing saved state with current system state.
  """

  WIFIINFO_PATH = '/tmp/wifi/wifiinfo'

  def __init__(self, socket):
    self._interface = os.path.split(socket)[-1]

    # State from QCSAPI and wifi_files.
    self._client_mode = False
    self._ssid = None
    self._status = None

    self._events = []

  def _qcsapi(self, *command):
    try:
      return subprocess.check_output(['qcsapi'] + list(command)).strip()
    except subprocess.CalledProcessError:
      return None

  def _wifiinfo_filename(self):
    return os.path.join(self.WIFIINFO_PATH, self._interface)

  def _get_wifiinfo(self):
    try:
      return json.load(open(self._wifiinfo_filename()))
    except IOError:
      return None

  def _get_ssid(self):
    wifiinfo = self._get_wifiinfo()
    if wifiinfo:
      return wifiinfo.get('SSID')

  def _check_client_mode(self):
    return self._qcsapi('get_mode', 'wifi0') == 'Station'

  def attach(self):
    self._update()

  def attached(self):
    return self._client_mode

  def detach(self):
    raise wpactrl.error('Real WPACtrl always raises this when detaching.')

  def pending(self):
    self._update()
    return bool(self._events)

  def _update(self):
    """Generate and cache events, update state."""
    client_mode = self._check_client_mode()
    ssid = self._get_ssid()
    status = self._qcsapi('get_status', 'wifi0')

    # If we have an SSID and are in client mode, and at least one of those is
    # new, then we have just connected.
    if client_mode and ssid and (not self._client_mode or ssid != self._ssid):
      self._events.append('<2>CTRL-EVENT-CONNECTED')

    # If we are in client mode but lost SSID, we disconnected.
    if client_mode and self._ssid and not ssid:
      self._events.append('<2>CTRL-EVENT-DISCONNECTED')

    # If there is an auth/assoc failure, then status (above) is 'Error'.  We
    # really want the converse of this implication (i.e. that 'Error' implies an
    # auth/assoc failure), but due to limited documentation this will have to
    # do.  It should be good enough:  if something else causes get_status to
    # return 'Error', we are probably not connected, and we don't do anything
    # special with auth/assoc failures specifically.
    if client_mode and status == 'Error' and self._status != 'Error':
      self._events.append('<2>CTRL-EVENT-AUTH-REJECT')

    # If we left client mode, wpa_supplicant has terminated.
    if self._client_mode and not client_mode:
      self._events.append('<2>CTRL-EVENT-TERMINATING')

    self._client_mode = client_mode
    self._ssid = ssid
    self._status = status

  def recv(self):
    return self._events.pop(0)

  def request(self, request_type):
    if request_type == 'STATUS' and self._client_mode and self._ssid:
      return 'wpa_state=COMPLETED\nssid=%s\n' % self._ssid

    return ''


class FrenzyWifi(Wifi):
  """Represents a Frenzy wireless interface."""

  # pylint: disable=invalid-name
  WPACtrl = FrenzyWPACtrl
