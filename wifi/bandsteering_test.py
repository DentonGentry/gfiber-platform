#!/usr/bin/python -S

"""Tests for bandsteering.py."""

import os
import shutil
import tempfile

import bandsteering
import experiment_testutils
from wvtest import wvtest


def bandsteering_test(f):
  """Decorator for bandsteering tests.

  Creates a temporary bandsteering directory, and removes it after the test.

  Args:
    f: The function to decorate.

  Returns:
    The decorated function.
  """
  # pylint: disable=protected-access,missing-docstring
  def inner():
    bandsteering._BANDSTEERING_DIR = tempfile.mkdtemp()

    try:
      f()
    finally:
      shutil.rmtree(bandsteering._BANDSTEERING_DIR)

  inner.func_name = f.func_name
  return inner


@wvtest.wvtest
@bandsteering_test
# pylint: disable=unused-argument
def hostapd_options_no_bandsteering_test():
  """Test bandsteering.hostapd_options when not bandsteering."""
  wvtest.WVPASSEQ([], bandsteering.hostapd_options('2.4', 'my_ssid'))
  wvtest.WVPASSEQ([], bandsteering.hostapd_options('5', 'my_ssid'))


@wvtest.wvtest
@bandsteering_test
def hostapd_options_bandsteering_test():
  """Test bandsteering.hostapd_options when normally bandsteering."""
  unused_raii = experiment_testutils.MakeExperimentDirs()
  experiment_testutils.enable('WifiBandsteering')
  bandsteering_dir = bandsteering._BANDSTEERING_DIR

  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '2.4_30abcc9ec8'),
                   '-S', os.path.join(bandsteering_dir, '5_30abcc9ec8')],
                  bandsteering.hostapd_options('2.4', 'my_ssid'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '5_30abcc9ec8')],
                  bandsteering.hostapd_options('5', 'my_ssid'))


@wvtest.wvtest
@bandsteering_test
def hostapd_options_reverse_bandsteering_test():
  """Test bandsteering.hostapd_options when reverse bandsteering."""
  unused_raii = experiment_testutils.MakeExperimentDirs()
  experiment_testutils.enable('WifiReverseBandsteering')
  bandsteering_dir = bandsteering._BANDSTEERING_DIR

  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '2.4_30abcc9ec8')],
                  bandsteering.hostapd_options('2.4', 'my_ssid'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '5_30abcc9ec8'),
                   '-S', os.path.join(bandsteering_dir, '2.4_30abcc9ec8')],
                  bandsteering.hostapd_options('5', 'my_ssid'))


@wvtest.wvtest
@bandsteering_test
def hostapd_options_preexisting_dir_test():
  """Test normal bandsteering when there is a preexisting directory."""
  unused_raii = experiment_testutils.MakeExperimentDirs()
  experiment_testutils.enable('WifiBandsteering')
  bandsteering_dir = bandsteering._BANDSTEERING_DIR

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
def hostapd_options_logging_test():
  """Test bandsteering.hostapd_options when when logging only."""
  unused_raii = experiment_testutils.MakeExperimentDirs()
  experiment_testutils.enable('WifiHostapdLogging')
  bandsteering_dir = bandsteering._BANDSTEERING_DIR

  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '2.4_30abcc9ec8')],
                  bandsteering.hostapd_options('2.4', 'my_ssid'))
  wvtest.WVPASSEQ(['-L', os.path.join(bandsteering_dir, '5_30abcc9ec8')],
                  bandsteering.hostapd_options('5', 'my_ssid'))


if __name__ == '__main__':
  wvtest.wvtest_main()
