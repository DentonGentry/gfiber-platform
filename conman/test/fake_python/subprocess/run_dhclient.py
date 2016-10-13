#!/usr/bin/python

"""Fake run-dhclient implementation."""

import os


CONMAN_PATH = None
_FAILURE = {}


def mock(interface, failure=False):
  _FAILURE[interface] = failure


def call(interface):
  if CONMAN_PATH is None:
    raise ValueError('Need to set subprocess.ifplugd_action.CONMAN_PATH')

  if not _FAILURE.get(interface, False):
    _write_subnet_file(interface)
    _write_gateway_file(interface)


def _write_gateway_file(interface):
  gateway_file = os.path.join(CONMAN_PATH, 'gateway.' + interface)
  with open(gateway_file, 'w') as f:
    # This value doesn't matter to conman, so it's fine to hard code it here.
    f.write('192.168.1.1')


def _write_subnet_file(interface):
  subnet_file = os.path.join(CONMAN_PATH, 'subnet.' + interface)
  with open(subnet_file, 'w') as f:
    # This value doesn't matter to conman, so it's fine to hard code it here.
    f.write('192.168.1.0/24')
