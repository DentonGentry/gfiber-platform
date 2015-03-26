#!/usr/bin/python -S

"""Functions related to bandsteering."""

import errno
import glob
import hashlib
import os

import experiment
import utils


_BANDSTEERING_DIR = '/tmp/wifi/steering'


def hostapd_options(band, ssid):
  """Returns hostapd options for bandsteering.

  Respects the experiments WifiBandsteering and WifiReverseBandsteering, in that
  order.

  Uses (and renames if necessary) a preexisting bandsteering directory for this
  band, if one exists.  Otherwise, creates that directory.

  Args:
    band: The band on which hostapd is being started.
    ssid: The SSID of the AP.

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
  elif experiment.enabled('WifiHostapdLogging'):
    target = ''
  else:
    return ()

  band_dir = _bandsteering_dir(band, ssid)
  target_dir = _bandsteering_dir(target, ssid)

  # Make sure band_dir exist, since we want hostapd to write to it.  If there's
  # a pre-existing one for the same band, use that; otherwise, create it.
  subdirs = (os.path.normpath(path)
             for path in glob.glob(os.path.join(_BANDSTEERING_DIR, '*/.')))

  for subdir in subdirs:
    if os.path.basename(subdir).startswith(band):
      try:
        os.rename(subdir, band_dir)
      except OSError:
        raise utils.BinWifiException("Couldn't update bandsteering directory")
      break
  else:
    try:
      os.makedirs(band_dir)
    except OSError as e:
      if e.errno != errno.EEXIST:
        raise utils.BinWifiException(
            "Couldn't create bandsteering directory %s", band_dir)

  result = ('-L', band_dir)
  if target and band != target:
    result += ('-S', target_dir)

  return result


def _bandsteering_dir(band, ssid):
  dir_suffix = hashlib.md5(ssid).hexdigest()[:10]
  return os.path.join(_BANDSTEERING_DIR, '%s_%s' % (band, dir_suffix))
