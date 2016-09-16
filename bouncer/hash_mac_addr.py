#!/usr/bin/python

"""hash_mac_addr: hash MAC addresses for privacy."""

import hashlib
import re
import sys


def hash_mac_addr(maybe_mac_addr):
  if re.match('([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$', maybe_mac_addr):
    mac_addr = maybe_mac_addr.lower()
  else:
    raise ValueError('%r not a MAC address', maybe_mac_addr)

  return mac_addr, hashlib.sha1(mac_addr).hexdigest()

if __name__ == '__main__':
  print 'SHA1(%s): %s' % hash_mac_addr(sys.argv[1])
