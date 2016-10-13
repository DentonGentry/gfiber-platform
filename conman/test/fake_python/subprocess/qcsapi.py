#!/usr/bin/python -S

"""Fake QCSAPI implementation."""


STATE = {}


def call(*args):
  if args not in STATE:
    return 1, 'No mocked value for args %r' % (args,)

  return 0, STATE[args]


def mock(*args, **kwargs):
  import logging
  if 'value' not in kwargs:
    raise ValueError('Must specify value for mock qcsapi call %r' % args)
  value = kwargs['value']
  logging.debug  ('qcsapi %r mocked: %r', args, value)
  if value is None and args in STATE:
    del STATE[args]
  else:
    STATE[args] = value
