#!/usr/bin/python -S

"""Tests for bandsteering.py."""

import os
import shutil
import tempfile

import bandsteering
import experiment
from wvtest import wvtest


def bandsteering_test(f):
  """Decorator for bandsteering tests.

  Creates temporary experiment and bandsteering directories, and removes them
  after the test.  The test function should take two arguments, experiments_dir
  and bandsteering_dir.

  Args:
    f: The function to decorate.

  Returns:
    The decorated function.
  """
  # pylint: disable=protected-access,missing-docstring
  def inner():
    experiments_dir = tempfile.mkdtemp()
    experiment._EXPERIMENTS_TMP_DIR = experiments_dir
    experiment._EXPERIMENTS_DIR = experiments_dir
    bandsteering_dir = tempfile.mkdtemp()
    bandsteering._BANDSTEERING_DIR = bandsteering_dir

    f(experiments_dir, bandsteering_dir)

    shutil.rmtree(experiments_dir)
    shutil.rmtree(bandsteering_dir)

  return inner


@wvtest.wvtest
@bandsteering_test
# pylint: disable=unused-argument
def hostapd_options_no_bandsteering_test(experiments_dir, bandsteering_dir):
  """Test bandsteering.hostapd_options when not bandsteering."""
  wvtest.WVPASSEQ([], bandsteering.hostapd_options('2.4', 'my_ssid'))
  wvtest.WVPASSEQ([], bandsteering.hostapd_options('5', 'my_ssid'))


@wvtest.wvtest
@bandsteering_test
def hostapd_options_bandsteering_test(experiments_dir, bandsteering_dir):
  """Test bandsteering.hostapd_options when normally bandsteering."""
  open(os.path.join(experiments_dir, 'WifiBandsteering.available'), 'a').close()
  open(os.path.join(experiments_dir, 'WifiBandsteering.active'), 'a').close()

  wvtest.WVPASS(experiment.enabled('WifiBandsteering'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '2.4_30abcc9ec8'),
                   '-S', os.path.join(bandsteering_dir, '5_30abcc9ec8')],
                  bandsteering.hostapd_options('2.4', 'my_ssid'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '5_30abcc9ec8')],
                  bandsteering.hostapd_options('5', 'my_ssid'))


@wvtest.wvtest
@bandsteering_test
def hostapd_options_reverse_bandsteering_test(experiments_dir,
                                              bandsteering_dir):
  """Test bandsteering.hostapd_options when reverse bandsteering."""
  open(os.path.join(experiments_dir,
                    'WifiReverseBandsteering.available'), 'a').close()
  open(os.path.join(experiments_dir,
                    'WifiReverseBandsteering.active'), 'a').close()

  wvtest.WVPASS(experiment.enabled('WifiReverseBandsteering'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '2.4_30abcc9ec8')],
                  bandsteering.hostapd_options('2.4', 'my_ssid'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '5_30abcc9ec8'),
                   '-S', os.path.join(bandsteering_dir, '2.4_30abcc9ec8')],
                  bandsteering.hostapd_options('5', 'my_ssid'))


@wvtest.wvtest
@bandsteering_test
def hostapd_options_preexisting_dir_test(experiments_dir, bandsteering_dir):
  """Test normal bandsteering when there is a preexisting directory."""
  open(os.path.join(experiments_dir, 'WifiBandsteering.available'), 'a').close()
  open(os.path.join(experiments_dir, 'WifiBandsteering.active'), 'a').close()
  wvtest.WVPASS(experiment.enabled('WifiBandsteering'))

  # Create a preexisting 2.4 GHz bandsteering directory with a file in it.
  os.makedirs(os.path.join(bandsteering_dir, '2.4_xxxxxxxxxx'))
  filename = 'foo'
  open(os.path.join(bandsteering_dir, '2.4_xxxxxxxxxx', filename), 'a').close()

  # Get the options for 2.4 GHz; this should move the old directory.
  bandsteering.hostapd_options('2.4', 'my_ssid')

  # If the old directory was moved correctly, we should see our file in the new
  # one, and the old directory should be gone.
  wvtest.WVPASS(os.path.isfile(
      os.path.join(bandsteering_dir, '2.4_30abcc9ec8', filename)))
  wvtest.WVFAIL(os.path.exists(
      os.path.join(bandsteering_dir, '2.4_xxxxxxxxxx')))


@wvtest.wvtest
@bandsteering_test
def hostapd_options_logging_test(experiments_dir, bandsteering_dir):
  """Test bandsteering.hostapd_options when when logging only."""
  open(os.path.join(experiments_dir,
                    'WifiHostapdLogging.available'), 'a').close()
  open(os.path.join(experiments_dir, 'WifiHostapdLogging.active'), 'a').close()

  wvtest.WVPASS(experiment.enabled('WifiHostapdLogging'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '2.4_30abcc9ec8')],
                  bandsteering.hostapd_options('2.4', 'my_ssid'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '5_30abcc9ec8')],
                  bandsteering.hostapd_options('5', 'my_ssid'))


if __name__ == '__main__':
  wvtest.wvtest_main()
