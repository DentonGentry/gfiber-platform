#!/usr/bin/python -S

"""Python implementation of the experiment framework."""

import logging
import os
import subprocess

logger = logging.getLogger(__name__)

EXPERIMENTS_TMP_DIR = '/tmp/experiments'
EXPERIMENTS_DIR = '/config/experiments'

_experiment_warned = set()
_experiment_enabled = set()


def register(name):
  try:
    rv = subprocess.call(['register_experiment', name])
  except OSError as e:
    logger.info('register_experiment: %s', e)
  else:
    if rv:
      logger.error('Failed to register experiment %s.', name)


def enabled(name):
  """Check whether an experiment is enabled.

  Copy/pasted from waveguide/helpers.py.

  Args:
    name: The name of the experiment to check.

  Returns:
    Whether the experiment is enabled.
  """
  if not os.path.exists(os.path.join(EXPERIMENTS_TMP_DIR,
                                     name + '.available')):
    if name not in _experiment_warned:
      _experiment_warned.add(name)
      logger.warning('Warning: experiment %r not registered.', name)
  else:
    is_enabled = os.path.exists(os.path.join(EXPERIMENTS_DIR,
                                             name + '.active'))
    if is_enabled and name not in _experiment_enabled:
      _experiment_enabled.add(name)
      logger.info('Notice: using experiment %r.', name)
    elif not is_enabled and name in _experiment_enabled:
      _experiment_enabled.remove(name)
      logger.info('Notice: stopping experiment %r.', name)
    return is_enabled
