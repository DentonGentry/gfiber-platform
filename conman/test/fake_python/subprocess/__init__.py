#!/usr/bin/python

"""subprocess replacement that implements specific programs in Python."""

import importlib
import logging
import os
import types

logger = logging.getLogger('subprocess')
logger.setLevel(logging.DEBUG)


CALL_HISTORY = []

# Values are only for when the module name does not match the command name.
_COMMAND_NAMES = {
    'connection_check': None,
    'cwmp': None,
    'get_quantenna_interfaces': 'get-quantenna-interfaces',
    'ifdown': None,
    'ifplugd_action': '/etc/ifplugd/ifplugd.action',
    'ifup': None,
    'ip': None,
    'register_experiment': None,
    'run_dhclient': 'run-dhclient',
    'qcsapi': None,
    'upload_logs_and_wait': 'upload-logs-and-wait',
    'wifi': None,
    'wpa_cli': None,
    'hotplug': None,
}
_COMMANDS = {v or k: importlib.import_module('.' + k, __name__)
             for k, v in _COMMAND_NAMES.iteritems()}

STDOUT = 1
STDERR = 2


class CalledProcessError(Exception):

  def __init__(self, returncode, cmd, output):
    super(CalledProcessError, self).__init__()
    self.returncode = returncode
    self.cmd = cmd
    self.output = output

  def __repr__(self):
    return ('CalledProcessError: '
            'Command "%r" returned non-zero exit status %d: %s'
            % (self.cmd, self.returncode, self.output))


def _call(command, **kwargs):
  """Fake subprocess call."""
  CALL_HISTORY.append((command, kwargs))

  if type(command) not in (tuple, list):
    raise Exception('Fake subprocess.call only supports list/tuple commands, '
                    'got: %s', command)

  ignored_kwargs = ('stdout', 'stderr')
  for ignored_kwarg in ignored_kwargs:
    kwargs.pop(ignored_kwarg, None)
  extra_env = kwargs.pop('env', {})
  if kwargs:
    raise Exception('Fake subprocess.call does not support these kwargs: %s'
                    % kwargs.keys())

  logger.debug('%r%s', command, (', env %r' % extra_env) if extra_env else '')

  command, args = command[0], command[1:]

  if command not in _COMMANDS:
    raise Exception('Fake subprocess.call does not support %r, supports %r' %
                    (command, _COMMANDS.keys()))

  impl = _COMMANDS[command]
  if isinstance(impl, types.ModuleType):
    impl = impl.call

  forwarded_kwargs = {}
  if extra_env:
    forwarded_kwargs['env'] = extra_env
  return impl(*args, **forwarded_kwargs)


def call(command, **kwargs):
  rc, _ = _call(command, **kwargs)
  return rc


def check_call(command, **kwargs):
  rc, output = _call(command, **kwargs)
  if rc:
    raise CalledProcessError(rc, command, output)
  return True


def check_output(command, **kwargs):
  rc, output = _call(command, **kwargs)
  if rc != 0:
    raise CalledProcessError(rc, command, output)
  return output


def mock(command, *args, **kwargs):
  _COMMANDS[command].mock(*args, **kwargs)


def reset():
  """Reset all state."""
  global CALL_HISTORY
  CALL_HISTORY = []
  for command in _COMMANDS.itervalues():
    if isinstance(command, types.ModuleType):
      reload(command)


def set_conman_paths(tmp_path=None, config_path=None, cwmp_path=None,
                     interface_path=None):
  for command in ('run-dhclient', '/etc/ifplugd/ifplugd.action'):
    _COMMANDS[command].CONMAN_PATH = tmp_path

  for command in ('cwmp',):
    _COMMANDS[command].CONMAN_CONFIG_PATH = config_path

  for command in ('cwmp',):
    _COMMANDS[command].CWMP_PATH = cwmp_path

  for command in ('hotplug',):
    _COMMANDS[command].INTERFACE_PATH = interface_path

  # Make sure <tmp_path>/interfaces exists.
  tmp_interfaces_path = os.path.join(tmp_path, 'interfaces')
  if not os.path.exists(tmp_interfaces_path):
    os.mkdir(tmp_interfaces_path)


# Some tiny fake implementations don't need their own file.


def echo(*s):
  return 0, ' '.join(s)


def env(extra_env, *command, **kwargs):
  final_env = kwargs.get('env', {})
  k, v = extra_env.split('=')
  final_env[k] = v
  kwargs['env'] = final_env
  return _call(command, **kwargs)


def timeout(unused_t, *command, **kwargs):
  """Just a transparent pass-through."""
  return _call(command, **kwargs)


_COMMANDS.update({'echo': echo, 'env': env, 'timeout': timeout,})
