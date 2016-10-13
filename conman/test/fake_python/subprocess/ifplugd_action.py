#!/usr/bin/python

"""Fake ifplugd.action implementation."""

import os

import run_dhclient

CONMAN_PATH = None


def call(interface, state):
  if CONMAN_PATH is None:
    raise ValueError('Need to set subprocess.ifplugd_action.CONMAN_PATH')

  if state not in ('up', 'down'):
    raise ValueError('state should be "up" or "down"')

  status_file = os.path.join(CONMAN_PATH, 'interfaces', interface)
  with open(status_file, 'w') as f:
    # This value doesn't matter to conman, so it's fine to hard code it here.
    f.write('1' if state == 'up' else '0')

  # ifplugd.action calls run-dhclient.
  run_dhclient.call('br0' if interface in ('eth0', 'moca0') else interface)

  return 0, ''
