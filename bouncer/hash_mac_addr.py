#!/usr/bin/python

"""hash_mac_addr: hash MAC addresses for privacy."""

import hashlib
import re
import sys

import options

optspec = """
hash_mac_addr -a ##:##:##:##:##:##
--
a,addr= MAC address to hash
"""


def normalize_mac_addr(maybe_mac_addr):
  if re.match('([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$', maybe_mac_addr):
    return maybe_mac_addr.lower()
  else:
    raise ValueError('%r not a MAC address' % maybe_mac_addr)


def hash_mac_addr(maybe_mac_addr):
  mac_addr = normalize_mac_addr(maybe_mac_addr)
  return hashlib.sha1(mac_addr).hexdigest()


if __name__ == '__main__':
  o = options.Options(optspec)
  opt, unused_flags, unused_extra = o.parse(sys.argv[1:])

  if not opt.addr:
    o.usage()

  try:
    hashed_mac_addr = hash_mac_addr(str(opt.addr))
    print hashed_mac_addr
  except ValueError as e:
    print >>sys.stderr, 'error:', e.message
    sys.exit(1)
