#!/usr/bin/python -S

"""Fake get-quantenna-interfaces implementation."""

_INTERFACES = []


def call(*unused_args, **unused_kwargs):
  return 0, '\n'.join(_INTERFACES)


def mock(interfaces):
  global _INTERFACES
  _INTERFACES = list(interfaces)
