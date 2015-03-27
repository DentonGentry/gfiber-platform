#!/usr/bin/python -S

"""Functions related to bandsteering."""

import errno
import os

import experiment
import utils


_BANDSTEERING_DIR = '/tmp/wifi/steering'


def hostapd_options(band):
  """Returns hostapd options for bandsteering.

  Respects the experiments WifiBandsteering and WifiReverseBandsteering, in that
  order.

  Args:
    band: The band on which hostapd is being started.

  Returns:
    A list containing options to be passed to hostapd.

  Raises:
    BinWifiException: If the directory for storing bandsteering timestamps
    cannot be created.
  """

  if experiment.enabled('WifiBandsteering'):
    target = '5'
  elif experiment.enabled('WifiReverseBandsteering'):
    target = '2.4'
  else:
    return []

  for band_dir in ['2.4', '5']:
    path = os.path.join(_BANDSTEERING_DIR, band_dir)
    try:
      os.makedirs(path)
    except OSError as e:
      if e.errno != errno.EEXIST:
        raise utils.BinWifiException(
            'Couldn\'t create bandsteering directory %s', path)

  result = ['-L', os.path.join(_BANDSTEERING_DIR, band)]
  if band != target:
    result += ['-S', os.path.join(_BANDSTEERING_DIR, target)]

  return result
