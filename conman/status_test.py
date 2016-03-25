#!/usr/bin/python

"""Tests for connection_manager.py."""

import logging
import os
import shutil
import tempfile

import status
from wvtest import wvtest

logging.basicConfig(level=logging.DEBUG)


def file_in(path, filename):
  return os.path.exists(os.path.join(path, filename))


@wvtest.wvtest
def test_proposition():
  export_path = tempfile.mkdtemp()

  try:
    rain = status.Proposition('rain', export_path)
    wet = status.Proposition('wet', export_path)
    dry = status.Proposition('dry', export_path)

    rain.implies(wet)
    wet.implies_not(dry)

    # Test basics.
    rain.set(True)
    wvtest.WVPASS(file_in(export_path, 'rain'))
    wvtest.WVPASS(file_in(export_path, 'wet'))
    wvtest.WVFAIL(file_in(export_path, 'dry'))

    # It may be wet even if it is not raining, but even in that case it is still
    # not dry.
    rain.set(False)
    wvtest.WVFAIL(file_in(export_path, 'rain'))
    wvtest.WVPASS(file_in(export_path, 'wet'))
    wvtest.WVFAIL(file_in(export_path, 'dry'))

    # Test contrapositives.
    dry.set(True)
    wvtest.WVFAIL(file_in(export_path, 'rain'))
    wvtest.WVFAIL(file_in(export_path, 'wet'))
    wvtest.WVPASS(file_in(export_path, 'dry'))

    # Make sure cycles are okay.
    tautology = status.Proposition('tautology', export_path)
    tautology.implies(tautology)
    tautology.set(True)
    wvtest.WVPASS(file_in(export_path, 'tautology'))

    zig = status.Proposition('zig', export_path)
    zag = status.Proposition('zag', export_path)
    zig.implies(zag)
    zag.implies(zig)
    zig.set(True)
    wvtest.WVPASS(file_in(export_path, 'zig'))
    wvtest.WVPASS(file_in(export_path, 'zag'))
    zag.set(False)
    wvtest.WVFAIL(file_in(export_path, 'zig'))
    wvtest.WVFAIL(file_in(export_path, 'zag'))

  finally:
    shutil.rmtree(export_path)


@wvtest.wvtest
def test_status():
  export_path = tempfile.mkdtemp()

  try:
    s = status.Status(export_path)

    # Sanity check that there are no contradictions.
    for p, (want_true, want_false) in status.IMPLICATIONS.iteritems():
      setattr(s, p.lower(), True)
      wvtest.WVPASS(file_in(export_path, p))
      for wt in want_true:
        wvtest.WVPASS(file_in(export_path, wt))
      for wf in want_false:
        wvtest.WVFAIL(file_in(export_path, wf))

    s.trying_wlan = True
    wvtest.WVPASS(file_in(export_path, status.P.TRYING_WLAN))
    wvtest.WVFAIL(file_in(export_path, status.P.CONNECTED_TO_WLAN))

    s.connected_to_open = True
    wvtest.WVPASS(file_in(export_path, status.P.CONNECTED_TO_OPEN))
    wvtest.WVFAIL(file_in(export_path, status.P.CONNECTED_TO_WLAN))

    s.connected_to_wlan = True
    wvtest.WVPASS(file_in(export_path, status.P.CONNECTED_TO_WLAN))
    wvtest.WVPASS(file_in(export_path, status.P.HAVE_WORKING_CONFIG))
    wvtest.WVFAIL(file_in(export_path, status.P.CONNECTED_TO_OPEN))
    wvtest.WVFAIL(file_in(export_path, status.P.TRYING_WLAN))
    wvtest.WVFAIL(file_in(export_path, status.P.TRYING_OPEN))

    s.can_reach_acs = True
    s.can_reach_internet = True
    wvtest.WVPASS(file_in(export_path, status.P.CAN_REACH_ACS))
    wvtest.WVPASS(file_in(export_path, status.P.COULD_REACH_ACS))
    wvtest.WVPASS(file_in(export_path, status.P.CAN_REACH_INTERNET))
    wvtest.WVFAIL(file_in(export_path, status.P.PROVISIONING_FAILED))

    # These should not have changed
    wvtest.WVPASS(file_in(export_path, status.P.CONNECTED_TO_WLAN))
    wvtest.WVPASS(file_in(export_path, status.P.HAVE_WORKING_CONFIG))
    wvtest.WVFAIL(file_in(export_path, status.P.CONNECTED_TO_OPEN))
    wvtest.WVFAIL(file_in(export_path, status.P.TRYING_WLAN))
    wvtest.WVFAIL(file_in(export_path, status.P.TRYING_OPEN))

  finally:
    shutil.rmtree(export_path)


if __name__ == '__main__':
  wvtest.wvtest_main()
