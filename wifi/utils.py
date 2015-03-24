#!/usr/bin/python -S

"""Small util functions for /bin/wifi scripts."""

from __future__ import print_function

import collections
import os
import subprocess
import sys
import time


_CONFIG_DIR = '/config/wifi'
FILENAME_KIND = collections.namedtuple(
    'FilenameKind', ['options', 'config', 'pid', 'alive'])(
        options='opts', config='conf', pid='pid', alive='alive')


class BinWifiException(Exception):

  def __init__(self, message, *args):
    super(BinWifiException, self).__init__(message)
    self.args = args

  def __str__(self):
    return '/bin/wifi failed: %s' % (self.message % self.args)


def log(msg, *args, **kwargs):
  print(msg % args, file=sys.stderr, **kwargs)


def atomic_write(filename, data):
  """Performs an atomic file write of data to filename.

  This is done by writing data to filename.new and then renaming filename.new to
  filename.

  Args:
    filename: The filename to to write to.
    data: The data to write.

  Raises:
    BinWifiException:  If the write fails.
  """
  tmp_filename = filename + '.new'
  try:
    with open(tmp_filename, 'w') as tmp:
      tmp.write(data)
    os.rename(tmp_filename, filename)
  except (OSError, IOError) as e:
    raise BinWifiException('Writing %s failed: %s', filename, e)


def subprocess_quiet(args, no_stderr=True, no_stdout=False):
  """Run a subprocess command with no stderr, and optionally no stdout."""
  with open(os.devnull, 'wb') as devnull:
    kwargs = {}
    if no_stderr:
      kwargs['stderr'] = devnull
    if no_stdout:
      kwargs['stdout'] = devnull
    return subprocess.call(args, **kwargs)


def subprocess_output_or_none(args):
  try:
    return subprocess.check_output(args)
  except subprocess.CalledProcessError:
    return None


def subprocess_lines(args, no_stderr=False):
  """Yields each line in the stdout of a subprocess call."""
  with open(os.devnull, 'wb') as devnull:
    kwargs = {}
    if no_stderr:
      kwargs['stderr'] = devnull
    for line in subprocess.check_output(args, **kwargs).split(b'\n'):
      yield line


def subprocess_line_tokens(args, no_stderr=False):
  return (line.split() for line in subprocess_lines(args, no_stderr))


def babysit(command, name, retry_timeout, pid_filename):
  """Run a command wrapped with babysit and startpid, and piped to logos.

  Args:
    command: The command to run, e.g. ['ls', '-l'].
    name: The name to pass to logos.
    retry_timeout: The babysit retry_timeout, in seconds.
    pid_filename: The filename to use for the startpid pid file.

  Returns:
    The name of the interface if found, otherwise None.
  """
  args = ['babysit', str(retry_timeout), 'startpid', pid_filename] + command
  process = subprocess.Popen(args,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  # Sleep for two seconds to give startpid time to create the pid filename.
  time.sleep(2)
  subprocess.Popen(['logos', name], stdin=process.stdout)


def get_mac_address_for_interface(interface):
  with open('/sys/class/net/%s/address' % interface) as mac_address_file:
    return mac_address_file.read().strip()


def increment_mac_address(mac_address):
  numeric_mac_address = int(''.join(mac_address.split(':')), 16) + 1
  return ':'.join(octet.encode('hex')
                  for octet in ('%x' % numeric_mac_address).decode('hex'))


def get_filename(program, kind, disambiguator, tmp=False):
  """Gets the filename for storing a specific kind of state.

  Args:
    program: E.g. 'hostapd' or 'wpa_supplicant'.
    kind: A FILENAME_KIND value.
    disambiguator: E.g. an interface or a band.
    tmp: True if you want the /tmp filename rather than the _CONFIG_DIR one.

  Returns:
    The requested filename.
  """
  return os.path.join('/tmp' if tmp else _CONFIG_DIR,
                      '%s.%s.%s' % (program, kind, disambiguator))


def check_pid(pid_filename):
  """Checks whether a program with a given pid is running.

  Args:
    pid_filename: The location of a file containing the pid to check.

  Returns:
    Whether the program is running.

  Raises:
    BinWifiException: If the pidfile cannot be opened.
  """
  try:
    with open(pid_filename, 'r') as pid_file:
      pid = pid_file.read().strip()
      return subprocess_quiet(['kill', '-0', pid]) == 0
  except IOError as e:
    raise BinWifiException("Couldn't open specified pidfile %s: %s",
                           pid_filename, e)


def kill_pid(program, pid_filename):
  """Kill a program which was run with startpid.

  Args:
    program: The program to stop.
    pid_filename: The location of the startpid pid file.

  Returns:
    Whether stopping the program succeeded.
  """
  try:
    subprocess.check_call(['pkillwait', '-f', program])
    subprocess.check_call(['killpid', pid_filename])
    try:
      os.remove(pid_filename)
    except OSError:
      pass
  except subprocess.CalledProcessError as e:
    log('Error stopping process: %s', e)
    return False

  return True


def read_or_empty(filename):
  try:
    with open(filename) as f:
      return f.read().strip()
  except IOError:
    return ''


def validate_set_wifi_options(band, width, autotype, protocols, encryption):
  """Validates options to set_wifi.

  Args:
    band: The specified band, as a string; '2.4' or '5'.
    width: The specified channel width.
    autotype: The specified autotype.
    protocols: The specified 802.11 levels, as a collection of strings.
    encryption: The specified encryption type.

  Raises:
    BinWifiException: if anything is not valid.
  """
  if band not in ('2.4', '5'):
    raise BinWifiException('You must specify band with -b2.4 or -b5')

  if (band, width) == ('2.4', '80'):
    raise BinWifiException(
        '80 MHz not valid in 2.4 GHz: type=%s band=%s width=%s',
        autotype, band, width)

  if (band, autotype) == ('2.4', 'DFS'):
    raise BinWifiException('DFS not valid in 2.4 GHz: type=%s band=%s width=%s',
                           autotype, band, width)

  if (band, autotype) == ('5', 'OVERLAP'):
    raise BinWifiException(
        'OVERLAP not allowed in 5 GHz: type=%s band=%s width=%s',
        autotype, band, width)

  if not protocols:
    raise BinWifiException('Must specify some 802.11 protocols')

  for protocol in protocols:
    if protocol not in ('a', 'b', 'ab', 'g', 'n', 'ac'):
      raise BinWifiException('Unknown 802.11 protocol: %s', protocol)

  if width not in ('20', '40', '80'):
    raise BinWifiException('Invalid channel width %s', width)
  elif width == '40' and 'n' not in protocols:
    raise BinWifiException('-p n is needed for 40 MHz channels')
  elif width == '80' and 'ac' not in protocols:
    raise BinWifiException('-p ac is needed for 40 MHz channels')

  if encryption == 'WEP' or '_PSK_' in encryption:
    if 'WIFI_PSK' not in os.environ:
      raise BinWifiException(
          'encryption enabled; use WIFI_PSK=whatever wifi set ...')
