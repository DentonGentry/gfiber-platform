#!/usr/bin/python

"""Fake ifup implementation."""

INTERFACE_STATE = {}


def call(interface):
  INTERFACE_STATE[interface] = True
  return 0, ''
