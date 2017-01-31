#!/usr/bin/python -S

"""Utils for interacting with iw and wl."""

import collections
import os
import re
import subprocess

import utils


INTERFACE_TYPE = collections.namedtuple(
    'InterfaceType', ('ap', 'client'))(ap='lan', client='cli')

RUNNABLE_IW = lambda: subprocess.call(('runnable', 'iw')) == 0
RUNNABLE_WL = lambda: subprocess.call(('runnable', 'wl')) == 0


class RequiresIwException(utils.Error):

  def __init__(self, function_name):
    super(RequiresIwException, self).__init__()
    self.function_name = function_name

  def __str__(self):
    return '%s requires iw' % (self.function_name)


def requires_iw(f):
  """Decorator for functions which require RUNNABLE_IW."""
  def inner(*args, **kwargs):
    if not RUNNABLE_IW():
      raise RequiresIwException(f.__name__)
    return f(*args, **kwargs)

  return inner


def _phy(**kwargs):
  return subprocess.check_output(('iw', 'phy'), **kwargs)


def _dev(**kwargs):
  return subprocess.check_output(('iw', 'dev'), **kwargs)


def _info(interface, **kwargs):
  return subprocess.check_output(('iw', 'dev', interface, 'info'), **kwargs)


def _link(interface, **kwargs):
  return subprocess.check_output(('iw', 'dev', interface, 'link'), **kwargs)


def _scan(interface, scan_args, **kwargs):
  return subprocess.check_output(
      ['iw', 'dev', interface, 'scan', '-u'] + scan_args, **kwargs)


_WIPHY_RE = re.compile(r'Wiphy (?P<phy>\S+)')
_FREQUENCY_AND_CHANNEL_RE = re.compile(r'\s*\* (?P<frequency>\d+) MHz'
                                       r' \[(?P<channel>\d+)\]')


def phy_parsed(**kwargs):
  """Parse the results of 'iw phy'.

  Args:
    **kwargs: Passed to the underlying subprocess call.

  Returns:
    A dict of the the form: {'phyX': {'frequency_and_channel': [(F, C), ...],
                                      'bands': set(band1, ...)}, ...}
  """
  result = {}
  phy = None

  for line in _phy(**kwargs).split('\n'):
    match = _WIPHY_RE.match(line)
    if match:
      phy = match.group('phy')
      result[phy] = {'frequency_and_channel': [], 'bands': set()}
    else:
      match = _FREQUENCY_AND_CHANNEL_RE.match(line)
      if match:
        frequency, channel = match.group('frequency'), match.group('channel')
        result[phy]['frequency_and_channel'].append((frequency, channel))
        if frequency.startswith('2'):
          result[phy]['bands'].add('2.4')
        elif frequency.startswith('5'):
          result[phy]['bands'].add('5')
        else:
          utils.log('Unrecognized frequency %s', frequency)

  return result


_PHY_RE = re.compile(r'(?P<phy>phy#\S+)')
_INTERFACE_RE = re.compile(r'\s*Interface (?P<interface>\S+)')


def dev_parsed(**kwargs):
  """Parse the results of 'iw dev'.

  Args:
    **kwargs: Passed to the underlying subprocess call.

  Returns:
    A dict of the the form: {'phyX': 'interfaces': ['interfaceN', ...]}
  """
  result = {}
  for line in _dev(**kwargs).split('\n'):
    match = _PHY_RE.match(line)
    if match:
      phy = match.group('phy').replace('#', '')
      result[phy] = {'interfaces': []}
    else:
      match = _INTERFACE_RE.match(line)
      if match:
        result[phy]['interfaces'].append(match.group('interface'))

  return result


_CHANNEL_WIDTH_RE = re.compile(
    r'\s+channel (?P<channel>\d+).*width: (?P<width>\d+)')


def info_parsed(interface, **kwargs):
  """Parse the results of 'iw <interface> info'.

  Args:
    interface: The interface.
    **kwargs: Passed to the underlying subprocess call.

  Returns:
    A dict of the the form: {'width': W, 'channel': C, ...}; the additional keys
    may include ['ifindex', 'wdev', 'addr', 'ssid', 'type', 'wiphy'].
  """
  result = {'width': None, 'channel': None}

  for line in _info(interface, **kwargs).split('\n'):
    match = _CHANNEL_WIDTH_RE.match(line)
    if match:
      result['width'] = match.group('width')
      result['channel'] = match.group('channel')
      break
    else:
      tokens = line.strip().split(' ', 1)
      if len(tokens) >= 2 and tokens[0] != 'Interface':
        result[tokens[0]] = tokens[1]

  return result


_WIDTH_RE = re.compile(r'[^\d](?P<width>\d+)MHz.*')
_FREQUENCY_RE = re.compile(r'\s*freq: (?P<frequency>\d+)')


def link_parsed(interface, **kwargs):
  """Parse the results of 'iw <interface> link'.

  Args:
    interface: The interface.
    **kwargs: Passed to the underlying subprocess call.

  Returns:
    A dict of the the form: {'width': W, 'frequency': F}
  """
  result = {'width': None, 'frequency': None}

  for line in _link(interface, **kwargs).split('\n'):
    if 'tx bitrate' in line:
      match = _WIDTH_RE.search(line)
      if match:
        result['width'] = match.group('width')

    if 'freq: ' in line:
      match = _FREQUENCY_RE.match(line)
      if match:
        result['frequency'] = match.group('frequency')

  return result


def find_phy(band, channel):
  """Find the name of the phy that supports a specific band and channel.

  Args:
    band: The band for which you want the phy.
    channel: The channel for which you want the phy.

  Returns:
    The name of the phy, e.g. 'phy0'.
  """
  for phy, data in phy_parsed().iteritems():
    for this_frequency, this_channel in data['frequency_and_channel']:
      if (channel == this_channel or
          (channel == 'auto' and band[0] == this_frequency[0])):
        return phy

  return None


def find_interface_from_phy(phy, interface_type, interface_suffix):
  """Finds the name of an interface on a given phy.

  Args:
    phy: The name of a phy, e.g. 'phy0'.
    interface_type: An INTERFACE_TYPE value.
    interface_suffix: An optional interface suffix.

  Returns:
    The name of the interface if found, otherwise None.
  """
  if interface_type not in (INTERFACE_TYPE.ap, INTERFACE_TYPE.client):
    utils.log('Invalid interface type %s.', interface_type)
    return None

  pattern = re.compile(r'w{interface_type}[0-9]{interface_suffix}\Z'.format(
      interface_type=re.escape(interface_type),
      interface_suffix=re.escape(interface_suffix)))
  for interface in dev_parsed()[phy]['interfaces']:
    if pattern.match(interface):
      return interface


def find_all_interfaces_from_phy(phy, interface_type=None):
  """Finds the names of all interfaces on a given phy.

  Args:
    phy: The name of a phy, e.g. 'phy0'.
    interface_type: An INTERFACE_TYPE value (optional).

  Returns:
    A list of all interfaces found.
  """
  interfaces = []
  interface_types = INTERFACE_TYPE
  if interface_type:
    interface_types = [interface_type]
  for interface_type in interface_types:
    pattern = re.compile(r'w%s[0-9]\w*\Z' % re.escape(interface_type))
    interfaces.extend(interface for interface
                      in dev_parsed()[phy]['interfaces']
                      if pattern.match(interface))
  return set(interfaces)


def find_interface_from_band(band, interface_type, interface_suffix):
  """Finds the name of an interface on a given band.

  Args:
    band: The band for which you want the interface.
    interface_type: An INTERFACE_TYPE value.
    interface_suffix: An optional interface suffix.

  Returns:
    The name of the interface if found, otherwise None.
  """
  phy = find_phy(band, 'auto')
  if phy is None:
    return None

  return find_interface_from_phy(phy, interface_type, interface_suffix)


def find_all_interfaces_from_band(band, interface_type=None):
  """Finds the names of all interfaces on a given band.

  Args:
    band: The band for which you want the interface(s).
    interface_type: An INTERFACE_TYPE value (optional).

  Returns:
    A list of all interfaces found.
  """
  phy = find_phy(band, 'auto')
  if phy is None:
    return []

  return find_all_interfaces_from_phy(phy, interface_type)


def find_interfaces_from_band_and_suffix(band, suffix, interface_type=None):
  """Finds the names of interfaces on a given band.

  Args:
    band: The band for which you want the interface(s).
    suffix: The interface suffix.  'ALL' is a special value that matches all
            suffixes.
    interface_type: An INTERFACE_TYPE value (optional).

  Returns:
    A list of all interfaces found.
  """
  interfaces = set()
  if suffix == 'ALL':
    interfaces = find_all_interfaces_from_band(band, interface_type)
  else:
    interface_types = interface_type or (INTERFACE_TYPE.ap,
                                         INTERFACE_TYPE.client)
    for ifc_type in interface_types:
      interface = find_interface_from_band(band, ifc_type, suffix)
      if interface:
        interfaces.add(interface)

  return interfaces


def find_width_and_channel(interface):
  """Finds the width and channel being used by a given interface.

  Args:
    interface: The interface to check.

  Returns:
    A tuple containing the width and channel being used by the interface, either
    of which may be None.
  """
  # This doesn't work on TV boxes.
  with open(os.devnull, 'w') as devnull:
    info_result = info_parsed(interface, stderr=devnull)
    result = (info_result['width'], info_result['channel'])
    if None not in result:
      return result

  # This works on TV boxes.
  with open(os.devnull, 'w') as devnull:
    link_result = link_parsed(interface, stderr=devnull)
  width, frequency = link_result['width'], link_result['frequency']

  # iw only prints channel width when it is 40 or 80; if it's not printed then
  # we can assume it's 20.
  if width is None:
    width = '20'

  channel = None
  if frequency is not None:
    for data in phy_parsed().itervalues():
      for this_frequency, this_channel in data['frequency_and_channel']:
        if frequency == this_frequency:
          channel = this_channel
          break

  return width, channel


def does_interface_exist(interface):
  return utils.subprocess_quiet(('iw', interface, 'info'), no_stdout=True) == 0


def create_client_interface(interface, phy, suffix):
  """Creates a client interface.

  Args:
    interface: The name of the interface to create.
    phy: The phy on which to create the interface.
    suffix: The suffix of the AP interface on the same phy.

  Returns:
    Whether interface creation succeeded.
  """
  utils.log('Creating client interface %s.', interface)
  try:
    utils.subprocess_quiet(
        ('iw', 'phy', phy, 'interface', 'add', interface, 'type', 'station'),
        no_stdout=True)

    ap_mac_address = utils.get_mac_address_for_interface(
        find_interface_from_phy(phy, INTERFACE_TYPE.ap, suffix))
    mac_address = utils.increment_mac_address(ap_mac_address)
    subprocess.check_call(
        ('ip', 'link', 'set', interface, 'address', mac_address))
  except subprocess.CalledProcessError as e:
    utils.log('Creating client interface failed: %s', e)


def station_dump(interface):
  """Dumps station stats into a string.

  Args:
    interface: Which interface to output info about.
  Returns:
    String containing station information.
  """
  return utils.subprocess_output_or_none(
      ['iw', 'dev', interface, 'station', 'dump'])


def phy_bands(which_phy):
  """Returns the bands supported by the given phy.

  If a phy P supports more than one band, and another phy Q supports only one of
  those bands, P is not said to support that band.

  Args:
    which_phy: The phy for which to get bands.

  Returns:
    The bands supported by the given phy.
  """

  band_phys = {}
  for phy, info in phy_parsed().iteritems():
    bands = info['bands']
    for band in bands:
      band_phys.setdefault(band, phy)
    if len(bands) == 1:
      band_phys[list(bands)[0]] = phy

  result = set()
  for band, phy in band_phys.iteritems():
    if which_phy == phy:
      result.add(band)

  return result


def scan(interface, scan_args):
  """Return 'iw scan' output for printing."""
  return _scan(interface, scan_args)


def set_4address_mode(interface, on):
  try:
    setting = 'on' if on else 'off'
    subprocess.check_output(['iw', 'dev', interface, 'set', '4addr', setting])
  except subprocess.CalledProcessError as e:
    raise utils.BinWifiException('Failed to set 4addr mode %s: %s', setting, e)
