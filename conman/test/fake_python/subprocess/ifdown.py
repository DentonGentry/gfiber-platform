#!/usr/bin/python

"""Fake ifdown implementation."""

import ifup


def call(interface):
  ifup.INTERFACE_STATE[interface] = False
  return 0, ''
