#!/usr/bin/python

"""Configures generic stuff for unit tests."""


import collections
import logging

from wvtest import wvtest as real_wvtest


logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)


class LoggerCounter(logging.Handler):
  """Counts the number of messages from each logger."""

  def __init__(self):
    super(LoggerCounter, self).__init__()
    self.counts = collections.defaultdict(int)

  def handle(self, record):
    self.counts[record.name] += 1


def num_root_messages(logger_counter):
  logging.getLogger(__name__).debug('logger counts: %s', logger_counter.counts)
  return logger_counter.counts['root']


def wvtest(f):
  @real_wvtest.wvtest
  def inner(*args, **kwargs):
    logger_counter = LoggerCounter()
    logging.getLogger().addHandler(logger_counter)
    f(*args, **kwargs)
    real_wvtest.WVPASSEQ(num_root_messages(logger_counter), 0)
  inner.func_name = f.func_name
  return inner
