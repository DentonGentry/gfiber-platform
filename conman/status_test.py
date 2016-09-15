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


def has_file(s, filename):
  return file_in(s._export_path, filename)


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
  export_path_s = tempfile.mkdtemp()
  export_path_t = tempfile.mkdtemp()
  export_path_st = tempfile.mkdtemp()

  try:
    s = status.Status(export_path_s)
    t = status.Status(export_path_t)
    st = status.CompositeStatus(export_path_st, [s, t])

    # Sanity check that there are no contradictions.
    for p, (want_true, want_false) in status.IMPLICATIONS.iteritems():
      setattr(s, p.lower(), True)
      wvtest.WVPASS(has_file(s, p))
      for wt in want_true:
        wvtest.WVPASS(has_file(s, wt))
      for wf in want_false:
        wvtest.WVFAIL(has_file(s, wf))

    def check_exported(check_s, check_t, filename):
      wvtest.WVPASSEQ(check_s, has_file(s, filename))
      wvtest.WVPASSEQ(check_t, has_file(t, filename))
      wvtest.WVPASSEQ(check_s or check_t, has_file(st, filename))

    s.trying_wlan = True
    t.trying_wlan = False
    check_exported(True, False, status.P.TRYING_WLAN)
    check_exported(False, False, status.P.CONNECTED_TO_WLAN)

    s.connected_to_open = True
    check_exported(True, False, status.P.CONNECTED_TO_OPEN)
    check_exported(False, False, status.P.CONNECTED_TO_WLAN)

    s.connected_to_wlan = True
    t.trying_wlan = True
    check_exported(True, False, status.P.CONNECTED_TO_WLAN)
    check_exported(True, False, status.P.HAVE_WORKING_CONFIG)
    check_exported(False, False, status.P.CONNECTED_TO_OPEN)
    check_exported(False, True, status.P.TRYING_WLAN)
    check_exported(False, False, status.P.TRYING_OPEN)

    s.can_reach_acs = True
    s.can_reach_internet = True
    check_exported(True, False, status.P.CAN_REACH_ACS)
    check_exported(True, False, status.P.COULD_REACH_ACS)
    check_exported(True, False, status.P.CAN_REACH_INTERNET)
    check_exported(False, False, status.P.PROVISIONING_FAILED)

    # These should not have changed
    check_exported(True, False, status.P.CONNECTED_TO_WLAN)
    check_exported(True, False, status.P.HAVE_WORKING_CONFIG)
    check_exported(False, False, status.P.CONNECTED_TO_OPEN)
    check_exported(False, True, status.P.TRYING_WLAN)
    check_exported(False, False, status.P.TRYING_OPEN)

    # Test provisioning statuses.
    s.waiting_for_dhcp = True
    check_exported(False, True, status.P.TRYING_WLAN)
    check_exported(False, False, status.P.TRYING_OPEN)
    check_exported(False, False, status.P.CONNECTED_TO_WLAN)
    check_exported(True, False, status.P.CONNECTED_TO_OPEN)
    check_exported(True, False, status.P.WAITING_FOR_PROVISIONING)
    check_exported(True, False, status.P.WAITING_FOR_DHCP)
    s.waiting_for_cwmp_wakeup = True
    check_exported(False, False, status.P.WAITING_FOR_DHCP)
    check_exported(True, False, status.P.WAITING_FOR_CWMP_WAKEUP)
    s.waiting_for_acs_session = True
    check_exported(False, False, status.P.WAITING_FOR_DHCP)
    check_exported(False, False, status.P.WAITING_FOR_CWMP_WAKEUP)
    check_exported(True, False, status.P.WAITING_FOR_ACS_SESSION)
    s.provisioning_completed = True
    check_exported(False, False, status.P.WAITING_FOR_PROVISIONING)
    check_exported(False, False, status.P.WAITING_FOR_DHCP)
    check_exported(False, False, status.P.WAITING_FOR_CWMP_WAKEUP)
    check_exported(False, False, status.P.WAITING_FOR_CWMP_WAKEUP)

  finally:
    shutil.rmtree(export_path_s)
    shutil.rmtree(export_path_t)
    shutil.rmtree(export_path_st)


if __name__ == '__main__':
  wvtest.wvtest_main()
