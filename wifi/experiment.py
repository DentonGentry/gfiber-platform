#!/usr/bin/python -S

"""Python implementation of the experiment framework."""

import os

import utils

_EXPERIMENTS_DIR = '/config/experiments'

_experiment_warned = set()
_experiment_enabled = set()


# TODO(rofrankel): Move registration to /etc/init.d/S50wifi.
def register(name):
  def touch(fn):
    open(fn, 'a').close()

  def filename(extension):
    return os.path.join(_EXPERIMENTS_DIR, name + '.' + extension)

  touch(filename('available'))

  if os.path.exists(filename('requested')):
    os.rename(filename('requested'), filename('active'))
  elif os.path.exists(filename('unrequested')):
    os.remove(filename('active'))
    os.remove(filename('requested'))


def enabled(name):
  """Check whether an experiment is enabled.

  Copy/pasted from waveguide/helpers.py.

  Args:
    name: The name of the experiment to check.

  Returns:
    Whether the experiment is enabled.
  """
  if not os.path.exists(os.path.join(_EXPERIMENTS_DIR, name + '.available')):
    if name not in _experiment_warned:
      _experiment_warned.add(name)
      utils.log('Warning: experiment %r not registered\n', name)
  else:
    is_enabled = os.path.exists(os.path.join(_EXPERIMENTS_DIR,
                                             name + '.active'))
    if is_enabled and name not in _experiment_enabled:
      _experiment_enabled.add(name)
      utils.log('Notice: using experiment %r\n', name)
    elif not is_enabled and name in _experiment_enabled:
      _experiment_enabled.remove(name)
      utils.log('Notice: stopping experiment %r\n', name)
    return is_enabled
