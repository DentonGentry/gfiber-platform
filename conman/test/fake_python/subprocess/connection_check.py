#!/usr/bin/python

"""Fake connection_check implementation."""

RESULTS = {}


def mock(interface, result):
  RESULTS[interface] = result


def call(*args):
  interface = args[args.index('-I') + 1]
  result = RESULTS.get(interface, 'fail')

  if result == 'restricted' and '-a' in args:
    result = 'succeed'

  return (0 if result == 'succeed' else 1), ''
