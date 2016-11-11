#!/usr/bin/python -S

"""Unit test utils for experiment.py."""

import logging
import os
import shutil
import tempfile

import experiment

logger = logging.getLogger(__name__)


def enable(name):
  """Enable an experiment.  For unit tests only."""
  open(os.path.join(experiment.EXPERIMENTS_TMP_DIR, name + '.available'), 'w')
  open(os.path.join(experiment.EXPERIMENTS_DIR, name + '.active'), 'w')
  logger.debug('Enabled %s for unit tests', name)


def disable(name):
  """Enable an experiment.  For unit tests only."""
  filename = os.path.join(experiment.EXPERIMENTS_DIR, name + '.active')
  if os.path.exists(filename):
    os.unlink(filename)
  logger.debug('Disabled %s for unit tests', name)


class MakeExperimentDirs(object):
  """RAII class for tests which involve experiments.

  Creates temporary experiment directories, and removes them
  upon deletion.
  """

  def __init__(self):
    # pylint: disable=protected-access,missing-docstring
    experiment.EXPERIMENTS_DIR = tempfile.mkdtemp()
    experiment.EXPERIMENTS_TMP_DIR = tempfile.mkdtemp()

  def __del__(self):
    shutil.rmtree(experiment.EXPERIMENTS_DIR)
    shutil.rmtree(experiment.EXPERIMENTS_TMP_DIR)
