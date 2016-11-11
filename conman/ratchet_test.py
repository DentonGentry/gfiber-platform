#!/usr/bin/python

"""Tests for ratchet.py."""

import os
import shutil
import tempfile
import time

import ratchet
import status
import test_common
from wvtest import wvtest


@test_common.wvtest
def condition_test():
  """Test basic Condition functionality."""
  x = y = 0
  callback_sink = []
  cx = ratchet.Condition('cx', lambda: x != 0, 0)
  cy = ratchet.Condition('cx', lambda: y != 0, 0.1,
                         callback=lambda: callback_sink.append([0]))
  wvtest.WVEXCEPT(ratchet.TimeoutException, cx.check)
  wvtest.WVFAIL(cy.check())
  time.sleep(0.1)
  wvtest.WVEXCEPT(ratchet.TimeoutException, cy.check)

  x = 1
  wvtest.WVEXCEPT(ratchet.TimeoutException, cx.check)
  cx.reset()
  wvtest.WVPASS(cx.check())

  y = 1
  cy.reset()
  wvtest.WVPASS(cy.check())
  wvtest.WVPASSEQ(len(callback_sink), 1)
  # Callback shouldn't fire again.
  wvtest.WVPASS(cy.check())
  wvtest.WVPASSEQ(len(callback_sink), 1)
  cy.reset()
  wvtest.WVPASS(cy.check())
  wvtest.WVPASSEQ(len(callback_sink), 2)


@test_common.wvtest
def file_condition_test():
  """Test File*Condition functionality."""
  _, filename = tempfile.mkstemp()
  c_exists = ratchet.FileExistsCondition('c exists', filename, 0.1)
  c_touched = ratchet.FileTouchedCondition('c touched', filename, 0.1)
  wvtest.WVPASS(c_exists.check())
  wvtest.WVFAIL(c_touched.check())
  # File mtime resolution isn't fine enough to see the difference between this
  # write and the previous one, so sleep for a short time before writing to
  # ensure a different mtime.
  time.sleep(0.01)
  open(filename, 'w')
  wvtest.WVPASS(c_touched.check())

  # Test that pre-existing files don't count.
  c_touched.reset()
  wvtest.WVFAIL(c_touched.check())
  time.sleep(0.1)
  wvtest.WVEXCEPT(ratchet.TimeoutException, c_touched.check)

  # Test that deleting files doesn't count.
  c_touched.reset()
  wvtest.WVFAIL(c_touched.check())
  os.unlink(filename)
  wvtest.WVFAIL(c_touched.check())


@test_common.wvtest
def ratchet_test():
  """Test Ratchet functionality."""

  class P(object):
    X = 'X'
    Y = 'Y'
    Z = 'Z'
  status.P = P
  status.IMPLICATIONS = {}

  status_export_path = tempfile.mkdtemp()
  try:
    x = y = z = 0
    r = ratchet.Ratchet('test ratchet', [
        ratchet.Condition('x', lambda: x, 0.1),
        ratchet.Condition('y', lambda: y, 0.1),
        ratchet.Condition('z', lambda: z, 0.1),
    ], status.Status('test ratchet', status_export_path))
    x = y = 1

    # Test that timeouts are not just summed, but start whenever the previous
    # step completed.
    wvtest.WVPASSEQ(r._current_step, 0)  # pylint: disable=protected-access
    wvtest.WVFAIL(os.path.isfile(os.path.join(status_export_path, 'X')))
    wvtest.WVFAIL(os.path.isfile(os.path.join(status_export_path, 'Y')))
    wvtest.WVFAIL(os.path.isfile(os.path.join(status_export_path, 'Z')))
    r.start()
    wvtest.WVPASS(os.path.isfile(os.path.join(status_export_path, 'X')))
    wvtest.WVFAIL(os.path.isfile(os.path.join(status_export_path, 'Y')))
    wvtest.WVFAIL(os.path.isfile(os.path.join(status_export_path, 'Z')))
    time.sleep(0.05)
    wvtest.WVFAIL(r.check())
    wvtest.WVPASSEQ(r._current_step, 2)  # pylint: disable=protected-access
    wvtest.WVPASS(os.path.isfile(os.path.join(status_export_path, 'X')))
    wvtest.WVPASS(os.path.isfile(os.path.join(status_export_path, 'Y')))
    wvtest.WVPASS(os.path.isfile(os.path.join(status_export_path, 'Z')))
    wvtest.WVFAIL(r.check())
    wvtest.WVPASSEQ(r._current_step, 2)  # pylint: disable=protected-access
    time.sleep(0.1)
    wvtest.WVEXCEPT(ratchet.TimeoutException, r.check)

    x = y = z = 1
    r.start()
    wvtest.WVPASS(r.check())
  finally:
    shutil.rmtree(status_export_path)


if __name__ == '__main__':
  wvtest.wvtest_main()
