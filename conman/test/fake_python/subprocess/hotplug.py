"""Fake /sbin/hotplug implemenation."""

import logging
import os

logger = logging.getLogger('subprocess.hotplug')


INTERFACE_PATH = None


def call(unused_command, env):
  if env['SUBSYSTEM'] == 'net' and env['ACTION'] == 'add':
    interface = env['INTERFACE']
    logger.debug('Simulating creation of %s', interface)
    open(os.path.join(INTERFACE_PATH, interface), 'w')
