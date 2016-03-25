#!/usr/bin/python

"""Runs a ConnectionManager."""

import logging
import os
import sys

import connection_manager

TMP_DIR = '/tmp/conman'

if __name__ == '__main__':
  loglevel = logging.INFO
  if '--debug' in sys.argv:
    loglevel = logging.DEBUG
  logging.basicConfig(level=loglevel)
  logging.debug('Debug logging enabled.')

  sys.stdout = os.fdopen(1, 'w', 1)  # force line buffering even if redirected
  sys.stderr = os.fdopen(2, 'w', 1)  # force line buffering even if redirected

  if not os.path.exists(TMP_DIR):
    os.makedirs(TMP_DIR)

  c = connection_manager.ConnectionManager(tmp_dir=TMP_DIR)
  c.run()
