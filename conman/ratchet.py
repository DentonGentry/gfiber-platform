#!/usr/bin/python -S

"""Utility for ensuring a series of events occur or time out."""

import logging
import os
import time

try:
  import monotime  # pylint: disable=unused-import,g-import-not-at-top
except ImportError:
  pass
try:
  _gettime = time.monotonic
except AttributeError:
  _gettime = time.time

# This has to be called before another module calls it with a higher log level.
# pylint: disable=g-import-not-at-top
logging.basicConfig(level=logging.DEBUG)


class TimeoutException(Exception):
  pass


class Condition(object):
  """Wrapper for a function that may time out."""

  def __init__(self, name, evaluate, timeout, logger=None, callback=None):
    self.name = name
    if evaluate:
      self.evaluate = evaluate
    self.timeout = timeout
    self.logger = logger or logging
    self.callback = callback
    self.reset()

  def reset(self, t0=None, start_at=None):
    """Reset the Condition to an initial state.

    Takes two different timestamp values to account for uncertainty in when a
    previous condition may have been met.

    Args:
      t0:  The timestamp after which to evaluate the condition.
      start_at:  The timestamp from which to compute the timeout.
    """
    self.t0 = t0 or _gettime()
    self.start_at = start_at or _gettime()
    self.done_after = None
    self.done_by = None
    self.timed_out = False
    self.not_done_before = self.t0

  def check(self):
    """Check whether the condition has completed or timed out."""
    if self.timed_out:
      raise TimeoutException()

    if self.done_after:
      return True

    if self.evaluate():
      self.mark_done()
      return True

    now = _gettime()
    if now > self.start_at + self.timeout:
      self.timed_out = True
      self.logger.info('%s timed out after %.2f seconds',
                       self.name, now - self.t0)
      raise TimeoutException()

    self.not_done_before = _gettime()
    return False

  def mark_done(self):
    # In general, we don't know when a condition finished, but we know it was
    # *after* whenever it was most recently not done.
    self.done_after = self.not_done_before
    self.done_by = _gettime()
    self.logger.info('%s completed after %.2f seconds',
                     self.name, self.done_by - self.t0)

    if self.callback:
      self.callback()


class FileExistsCondition(Condition):
  """A condition that checks for the existence of a file."""

  def __init__(self, name, filename, timeout):
    self._filename = filename
    super(FileExistsCondition, self).__init__(name, None, timeout)

  def evaluate(self):
    return os.path.exists(self._filename)

  def mtime(self):
    if os.path.exists(self._filename):
      return os.stat(self._filename).st_mtime

    return None

  def mark_done(self):
    super(FileExistsCondition, self).mark_done()
    # We have to check this because the file could have been deleted while this
    # was being called.  But this condition should almost always be true.
    mtime = self.mtime()
    if mtime:
      self.done_after = self.done_by = mtime


class FileTouchedCondition(FileExistsCondition):
  """A condition that checks that a file was touched after a certain time."""

  def reset(self, t0=None, start_at=None):
    mtime = self.mtime
    if t0 and mtime and mtime < t0:
      self.initial_mtime = self.mtime()
    else:
      self.initial_mtime = None
    super(FileTouchedCondition, self).reset(t0, start_at)

  def evaluate(self):
    if not super(FileTouchedCondition, self).evaluate():
      return False

    if self.initial_mtime:
      return self.mtime() > self.initial_mtime

    return self.mtime() >= self.t0


class Ratchet(Condition):
  """A condition that comprises a series of subconditions."""

  def __init__(self, name, steps, status):
    self.name = name
    self.steps = steps
    for step in self.steps:
      step.logger = logging.getLogger(self.name)
    self._status = status
    super(Ratchet, self).__init__(name, None, 0)

  def reset(self):
    self._current_step = 0
    self.active = False
    for step in self.steps:
      step.reset()
      self._set_step_status(step, False)
    super(Ratchet, self).reset()

  def start(self):
    self.reset()
    self.active = True
    self._set_current_step_status(True)

  def stop(self):
    self.active = False

  # Override check rather than evaluate because we don't want the Ratchet to
  # time out unless one of its steps does.
  def check(self):
    if not self.active:
      return

    if not self.done_after:
      while self.current_step().check():
        if not self.advance():
          self.mark_done()
          break

    return self.done_after

  def current_step(self):
    return self.steps[self._current_step]

  def on_final_step(self):
    return self._current_step == len(self.steps) - 1

  def advance(self):
    if self.on_final_step():
      return False
    else:
      prev_step = self.current_step()
      self._current_step += 1
      self.current_step().start_at = prev_step.done_by
      self._set_current_step_status(True)
      return True

  def mark_done(self):
    super(Ratchet, self).mark_done()
    self.done_after = self.steps[-1].done_after

  def _set_step_status(self, step, value):
    setattr(self._status, step.name, value)

  def _set_current_step_status(self, value):
    self._set_step_status(self.current_step(), value)
