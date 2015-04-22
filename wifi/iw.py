#!/usr/bin/python -S

"""Utils for interacting with iw and wl."""

import collections
import os
import re
import subprocess

import utils


INTERFACE_TYPE = collections.namedtuple(
    'InterfaceType', ['ap', 'client'])(ap='lan', client='cli')

RUNNABLE_IW = lambda: subprocess.call(['runnable', 'iw']) == 0
RUNNABLE_WL = lambda: subprocess.call(['runnable', 'wl']) == 0


class RequiresIwException(Exception):

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
  return subprocess.check_output(['iw', 'phy'], **kwargs)


def _dev(**kwargs):
  return subprocess.check_output(['iw', 'dev'], **kwargs)


def _info(interface, **kwargs):
  return subprocess.check_output(['iw', interface, 'info'], **kwargs)


def _link(interface, **kwargs):
  return subprocess.check_output(['iw', interface, 'link'], **kwargs)


def phy_parsed(**kwargs):
  """Parse the results of 'iw phy'.

  Args:
    **kwargs:  Passed to the underlying subprocess call.

  Returns:
    A dict of the the form: {'phyX': 'frequency_and_channel': [(F, C), ...]}
  """
  result = {}
  phy = None

  for tokens in (line.split() for line in _phy(**kwargs).split(b'\n')):
    if tokens and tokens[0] == 'Wiphy':
      phy = tokens[1]
      result[phy] = {
          'frequency_and_channel': [],
      }
    elif len(tokens) >= 4 and tokens[2] == 'MHz':
      result[phy]['frequency_and_channel'].append(
          (tokens[1], tokens[3].strip('[]')))

  return result


def dev_parsed(**kwargs):
  """Parse the results of 'iw dev'.

  Args:
    **kwargs:  Passed to the underlying subprocess call.

  Returns:
    A dict of the the form: {'phyX': 'interfaces': ['interfaceN', ...]}
  """
  result = {}
  for tokens in (line.split() for line in _dev(**kwargs).split(b'\n')):
    if tokens and tokens[0].startswith('phy#'):
      phy = ''.join(tokens[0].split('#'))
      result[phy] = {
          'interfaces': []
      }
    elif len(tokens) >= 2 and tokens[0] == 'Interface':
      result[phy]['interfaces'].append(tokens[1])

  return result


_CHANNEL_WIDTH_RE = re.compile(
    r'\s+channel (?P<channel>\d+).*width: (?P<width>\d+).*')


def info_parsed(interface, **kwargs):
  """Parse the results of 'iw <interface> info'.

  Args:
    interface:  The interface
    **kwargs:  Passed to the underlying subprocess call.

  Returns:
    A dict of the the form: {'width': W, 'channel': C, ...}; the additional keys
    may include ['ifindex', 'wdev', 'addr', 'ssid', 'type', 'wiphy'].
  """
  result = {'width': None, 'channel': None}

  for line in _info(interface, **kwargs).split(b'\n'):
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


_WIDTH_RE = re.compile(r'.*[^\d](?P<width>\d+)MHz.*')
_FREQUENCY_RE = re.compile(r'.*freq: (?P<frequency>\d+)')


def link_parsed(interface, **kwargs):
  """Parse the results of 'iw <interface> link'.

  Args:
    interface:  The interface
    **kwargs:  Passed to the underlying subprocess call.

  Returns:
    A dict of the the form: {'width': W, 'frequency': F}
  """
  result = {'width': None, 'frequency': None}

  for line in _link(interface, **kwargs).split(b'\n'):
    if 'tx bitrate' in line:
      match = _WIDTH_RE.match(line)
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
  if interface_type not in [INTERFACE_TYPE.ap, INTERFACE_TYPE.client]:
    utils.log('Invalid interface type %s', interface_type)
    return None

  pattern = r'^w{ifc_type}[0-9]{ifc_suffix}$'.format(
      ifc_type=interface_type, ifc_suffix=interface_suffix)
  for interface in dev_parsed()[phy]['interfaces']:
    if re.match(pattern, interface):
      return interface


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


def find_width_and_channel(interface):
  """Finds the width and channel being used by a given interface.

  Args:
    interface: The interface to check.

  Returns:
    A tuple containing the width and channel being used by the interface, either
    of which may be None.
  """
  # This doesn't work on TV boxes.
  with open(os.devnull, 'wb') as devnull:
    info_result = info_parsed(interface, stderr=devnull)
    result = (info_result['width'], info_result['channel'])
    if None not in result:
      return result

  # This works on TV boxes.
  with open(os.devnull, 'wb') as devnull:
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
  return utils.subprocess_quiet(['iw', interface, 'info'], no_stdout=True) == 0


def create_client_interface(interface, phy, suffix):
  """Creates a client interface.

  Args:
    interface: The name of the interface to create.
    phy: The phy on which to create the interface.
    suffix: The suffix of the AP interface on the same phy.

  Returns:
    Whether interface creation succeeded.
  """
  utils.log('Creating client interface %s', interface)
  try:
    utils.subprocess_quiet(
        ['iw', 'phy', phy, 'interface', 'add', interface, 'type', 'station'],
        no_stdout=True)

    ap_mac_address = utils.get_mac_address_for_interface(
        find_interface_from_phy(phy, INTERFACE_TYPE.ap, suffix))
    mac_address = utils.increment_mac_address(ap_mac_address)
    subprocess.check_call(
        ['ip', 'link', 'set', interface, 'address', mac_address])
  except subprocess.CalledProcessError as e:
    utils.log('Creating client interface failed: %s', e)
