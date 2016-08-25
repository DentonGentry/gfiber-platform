#!/usr/bin/python

"""Models wired and wireless interfaces."""

import logging
import os
import re
import subprocess

# This has to be called before another module calls it with a higher log level.
# pylint: disable=g-import-not-at-top
logging.basicConfig(level=logging.DEBUG)

import experiment
import wpactrl

METRIC_5GHZ = 20
METRIC_24GHZ_5GHZ = 21
METRIC_24GHZ = 22

RFC2385_MULTICAST_ROUTE = '239.0.0.0/8'

experiment.register('WifiSimulateWireless')
CWMP_PATH = '/tmp/cwmp'
MAX_ACS_FAILURE_S = 60


class Interface(object):
  """Represents an interface.

  Base class for more specific interface types.
  """

  CONNECTION_CHECK = 'connection_check'
  IP_ROUTE = ['ip', 'route']
  IP_ADDR_SHOW = ['ip', 'addr', 'show', 'dev']

  def __init__(self, name, base_metric):
    self.name = name

    # Currently connected links for this interface, e.g. ethernet.
    self.links = set()

    # Whether [ACS, internet] access is currently available via this interface.
    self._has_acs = None
    self._has_internet = None

    self._subnet = None
    self._gateway_ip = None
    self.base_metric = base_metric
    self.metric_offset = 0

    # Until this is set True, the routing table will not be touched.
    self._initialized = False

  @property
  def metric(self):
    return str(int(self.base_metric) + self.metric_offset)

  def _connection_check(self, check_acs):
    """Check this interface's connection status.

    Args:
      check_acs:  If true, check for ACS access rather than internet access.

    Returns:
      Whether the connection is working.
    """
    # Until initialized, we want to act as if the interface is down.
    if not self._initialized:
      logging.info('%s not initialized; not running connection_check%s',
                   self.name, ' (ACS)' if check_acs else '')
      return None

    if not self.links:
      logging.info('Connection check for %s failed due to no links', self.name)
      return False

    logging.debug('Gateway IP for %s is %s', self.name, self._gateway_ip)
    if self._gateway_ip is None:
      logging.info('Connection check%s for %s failed due to no gateway IP',
                   ' (ACS)' if check_acs else '', self.name)
      return False

    self.add_routes()

    cmd = [self.CONNECTION_CHECK, '-I', self.name]
    if check_acs:
      cmd.append('-a')

    with open(os.devnull, 'w') as devnull:
      result = subprocess.call(cmd, stdout=devnull, stderr=devnull) == 0
      logging.info('Connection check%s for %s %s',
                   ' (ACS)' if check_acs else '',
                   self.name,
                   'passed' if result else 'failed')

    return result

  def gateway(self):
    return self._gateway_ip

  def acs(self):
    if self._has_acs is None:
      self._has_acs = self._connection_check(check_acs=True)

    return self._has_acs

  def internet(self):
    if self._has_internet is None:
      self._has_internet = self._connection_check(check_acs=False)

    return self._has_internet

  def add_routes(self):
    """Update default routes for this interface.

    Remove any stale routes and add any missing desired routes.
    """
    if self.metric is None:
      logging.info('Cannot add route for %s without a metric.', self.name)
      return

    # If the current routes are the same, there is nothing to do.  If either
    # exists but is different, delete it before adding an updated one.
    current = self.current_routes()

    to_add = []

    subnet = current.get('subnet', {})
    if self._subnet:
      if ((subnet.get('route', None), subnet.get('metric', None)) !=
          (self._subnet, str(self.metric))):
        logging.debug('Adding subnet route for dev %s', self.name)
        to_add.append(('subnet', ('add', self._subnet, 'dev', self.name,
                                  'metric', str(self.metric))))
        subnet = self._subnet
    else:
      subnet = None
      self.delete_route('default', 'subnet')

    default = current.get('default', {})
    if self._gateway_ip:
      if (subnet and
          (default.get('via', None), default.get('metric', None)) !=
          (self._gateway_ip, str(self.metric))):
        logging.debug('Adding default route for dev %s', self.name)
        to_add.append(('default',
                       ('add', 'default', 'via', self._gateway_ip,
                        'dev', self.name, 'metric', str(self.metric))))
    else:
      self.delete_route('default')

    # RFC2365 multicast route.
    if current.get('multicast', {}).get('metric', None) != str(self.metric):
      logging.debug('Adding multicast route for dev %s', self.name)
      to_add.append(('multicast', ('add', RFC2385_MULTICAST_ROUTE,
                                   'dev', self.name,
                                   'metric', str(self.metric))))

    for route_type, _ in to_add[::-1]:
      self.delete_route(route_type)

    for _, cmd in to_add:
      self._ip_route(*cmd)

  def delete_route(self, *args):
    """Delete default and/or subnet routes for this interface.

    Args:
      *args:  Which routes to delete.  Must be at least one of 'default',
          'subnet', 'multicast'.

    Raises:
      ValueError:  If neither default nor subnet is True.
    """
    args = set(args)
    args &= set(('default', 'subnet', 'multicast'))
    if not args:
      raise ValueError(
          'Must specify at least one of default, subnet, multicast to delete.')

    # Use a sorted list to ensure that default comes before subnet.
    for route_type in sorted(list(args)):
      while route_type in self.current_routes():
        logging.debug('Deleting %s route for dev %s', route_type, self.name)
        self._ip_route('del', self.current_routes()[route_type]['route'],
                       'dev', self.name)

  def current_routes(self):
    """Read the current routes for this interface.

    Returns:
      A dict mapping 'default' and/or 'subnet' to a dict containing the gateway
      [and metric] of the route.  Only contains keys for routes that are
      present.
    """
    result = {}
    for line in self._ip_route().splitlines():
      if 'dev %s' % self.name in line:
        if line.startswith('default'):
          route_type = 'default'
        elif re.search(r'/\d{1,2}$', line.split()[0]):
          route_type = 'subnet'
        else:
          continue
        route = {}
        key = 'route'
        for token in line.split():
          if token in ['via', 'metric']:
            key = token
          elif key:
            if key == 'route' and token == RFC2385_MULTICAST_ROUTE:
              route_type = 'multicast'
            route[key] = token
            key = None
        if route:
          result[route_type] = route

    return result

  def _ip_route(self, *args):
    if not self._initialized:
      logging.info('Not initialized, not running %s %s',
                   ' '.join(self.IP_ROUTE), ' '.join(args))
      return ''

    try:
      logging.debug('%s calling ip route %s', self.name, ' '.join(args))
      return self._really_ip_route(*args)
    except subprocess.CalledProcessError as e:
      logging.error('Failed to call "ip route" with args %r: %s', args,
                    e.message)
      return ''

  def _really_ip_route(self, *args):
    return subprocess.check_output(self.IP_ROUTE + list(args))

  def _ip_addr_show(self):
    try:
      return subprocess.check_output(self.IP_ADDR_SHOW + [self.name])
    except subprocess.CalledProcessError as e:
      logging.error('Could not get IP address for %s: %s', self.name, e.message)
      return None

  def get_ip_address(self):
    match = re.search(r'^\s*inet (?P<IP>\d+\.\d+\.\d+\.\d+)',
                      self._ip_addr_show(), re.MULTILINE)
    return match and match.group('IP') or None

  def set_gateway_ip(self, gateway_ip):
    logging.info('New gateway IP %s for %s', gateway_ip, self.name)
    self._gateway_ip = gateway_ip
    self.update_routes(expire_cache=True)

  def set_subnet(self, subnet):
    logging.info('New subnet %s for %s', subnet, self.name)
    self._subnet = subnet
    self.update_routes(expire_cache=True)

  def _set_link_status(self, link, is_up):
    """Set whether a link is up or not."""
    if is_up == (link in self.links):
      return

    had_links = bool(self.links)

    if is_up:
      logging.info('%s gained link %s', self.name, link)
      self.links.add(link)
    else:
      logging.info('%s lost link %s', self.name, link)
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

    If the interface has ACS or internet access, prioritize its routes.  If it
    doesn't but has a link, deprioritize the routes.  If it has no links, delete
    the routes.

    Args:
      expire_cache:  If true, force a recheck of connection status before
      deciding how to prioritize routes.
    """
    logging.debug('Updating routes for %s', self.name)
    if expire_cache:
      self.expire_connection_status_cache()

    if self.acs() or self.internet():
      self.prioritize_routes()
    else:
      # If we still have a link, just deprioritize the routes, in case we're
      # wrong about the connection check.  If there's no actual link, then
      # really delete the routes.
      if self.links:
        self.deprioritize_routes()
      else:
        self.delete_route('default', 'subnet', 'multicast')

  def prioritize_routes(self):
    """When connection check succeeds, route priority (metric) should be normal.

    This is the inverse of deprioritize_routes.
    """
    if not self._initialized:
      return
    logging.info('%s routes have normal priority', self.name)
    self.metric_offset = 0
    self.add_routes()

  def deprioritize_routes(self):
    """When connection check fails, deprioritize routes by increasing metric.

    This is conservative alternative to deleting routes, in case we are mistaken
    about route not providing a useful connection.
    """
    if not self._initialized:
      return
    logging.info('%s routes have low priority', self.name)
    self.metric_offset = 50
    self.add_routes()

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

  def prioritize_routes(self):
    """We only want ACS autoprovisioning when we're using a wired route."""
    super(Bridge, self).prioritize_routes()
    open(self._acs_autoprovisioning_filepath, 'w')

  def deprioritize_routes(self, *args, **kwargs):
    """We only want ACS autoprovisioning when we're using a wired route."""
    if os.path.exists(self._acs_autoprovisioning_filepath):
      os.unlink(self._acs_autoprovisioning_filepath)
    super(Bridge, self).deprioritize_routes(*args, **kwargs)

  def delete_route(self, *args, **kwargs):
    """We only want ACS autoprovisioning when we're using a wired route."""
    if os.path.exists(self._acs_autoprovisioning_filepath):
      os.unlink(self._acs_autoprovisioning_filepath)
    super(Bridge, self).delete_route(*args, **kwargs)

  def _connection_check(self, check_acs):
    """Support for WifiSimulateWireless."""
    failure_s = self._acs_session_failure_s()
    if (experiment.enabled('WifiSimulateWireless')
        and failure_s < MAX_ACS_FAILURE_S):
      logging.info('WifiSimulateWireless: failing bridge connection check%s '
                   '(no ACS contact for %d seconds, max %d seconds)',
                   ' (ACS)' if check_acs else '', failure_s, MAX_ACS_FAILURE_S)
      return False

    return super(Bridge, self)._connection_check(check_acs)

  def _acs_session_failure_s(self):
    """How long have we been failing to connect to the ACS?

    Returns:
      The number of seconds between the last attempted ACS session and the last
      successful ACS session.
    """
    contact = os.path.join(CWMP_PATH, 'acscontact')
    connected = os.path.join(CWMP_PATH, 'acsconnected')

    if not os.path.exists(contact) or not os.path.exists(connected):
      return 0

    return os.stat(contact).st_mtime - os.stat(connected).st_mtime


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
    logging.debug('%s socket is %s', self.name, socket)
    try:
      self._wpa_control = self.get_wpa_control(socket)
      self._wpa_control.attach()
      logging.debug('%s successfully attached', self.name)
    except wpactrl.error as e:
      logging.error('Error attaching to wpa_supplicant: %s', e)
      return False

    status = self.wpa_status()
    logging.debug('%s status after attaching is %s', self.name, status)
    self.wpa_supplicant = status.get('wpa_state') == 'COMPLETED'
    if not self._initialized:
      self.initial_ssid = status.get('ssid')

    return True

  def wpa_status(self):
    """Parse the STATUS response from the wpa_supplicant control interface.

    Returns:
      A dict containing the parsed results, where key and value are separated by
      '=' on each line.
    """
    status = {}

    if self._wpa_control and self._wpa_control.attached:
      logging.debug('%s ctrl_iface_path %s',
                    self, self._wpa_control.ctrl_iface_path)
      lines = []
      try:
        lines = self._wpa_control.request('STATUS').splitlines()
      except wpactrl.error as e:
        logging.error('wpa_control STATUS request failed %s args %s',
                      e.message, e.args)
      for line in lines:
        if '=' not in line:
          continue
        k, v = line.strip().split('=', 1)
        status[k] = v

    logging.debug('%s wpa status is %s', self.name, status)
    return status

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
                       'SSID-TEMP-DISABLED', 'AUTH-REJECT'):
          self.wpa_supplicant = False
          if event == 'TERMINATING':
            self.detach_wpa_control()
            break

        self.update_routes()

  def initialize(self):
    """Unset self.initial_ssid, which is only relevant during initialization."""

    self.initial_ssid = None
    super(Wifi, self).initialize()

  def connected_to_open(self):
    return (self.wpa_status().get('wpa_state', None) == 'COMPLETED' and
            self.wpa_status().get('key_mgmt', None) == 'NONE')


class FrenzyWPACtrl(object):
  """A WPACtrl for Frenzy devices.

  Implements the same functions used on the normal WPACtrl, using a combination
  of the QCSAPI and wifi_files.  Keeps state in order to generate events by
  diffing saved state with current system state.
  """

  WIFIINFO_PATH = '/tmp/wifi/wifiinfo'

  def __init__(self, socket):
    self.ctrl_iface_path, self._interface = os.path.split(socket)

    # State from QCSAPI and wifi_files.
    self._client_mode = False
    self._ssid = None
    self._status = None
    self._security = None

    self._events = []

  def _qcsapi(self, *command):
    try:
      return subprocess.check_output(['qcsapi'] + list(command)).strip()
    except subprocess.CalledProcessError as e:
      logging.error('QCSAPI call failed: %s: %s', e, e.output)
      raise

  def attach(self):
    self._update()

  @property
  def attached(self):
    return self._client_mode

  def detach(self):
    raise wpactrl.error('Real WPACtrl always raises this when detaching.')

  def pending(self):
    self._update()
    return bool(self._events)

  def _update(self):
    """Generate and cache events, update state."""
    try:
      client_mode = self._qcsapi('get_mode', 'wifi0') == 'Station'
      ssid = self._qcsapi('get_ssid', 'wifi0')
      status = self._qcsapi('get_status', 'wifi0')
      security = (self._qcsapi('ssid_get_authentication_mode', 'wifi0', ssid)
                  if ssid else None)
    except subprocess.CalledProcessError:
      # If QCSAPI failed, skip update.
      return

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
      self._events.append('<2>CTRL-EVENT-SSID-TEMP-DISABLED')

    # If we left client mode, wpa_supplicant has terminated.
    if self._client_mode and not client_mode:
      self._events.append('<2>CTRL-EVENT-TERMINATING')

    self._client_mode = client_mode
    self._ssid = ssid
    self._status = status
    self._security = security

  def recv(self):
    return self._events.pop(0)

  def request(self, request_type):
    """Partial implementation of WPACtrl.request."""

    if request_type != 'STATUS':
      return ''

    self._update()

    if not self._client_mode or not self._ssid:
      return ''

    return ('wpa_state=COMPLETED\nssid=%s\nkey_mgmt=%s' %
            (self._ssid, self._security or 'NONE'))


class FrenzyWifi(Wifi):
  """Represents a Frenzy wireless interface."""

  # pylint: disable=invalid-name
  WPACtrl = FrenzyWPACtrl
