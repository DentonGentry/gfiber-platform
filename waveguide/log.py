# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# pylint:disable=invalid-name

"""Helper functions for logging."""

import errno
import hmac
import os
import struct
import sys
import helpers


LOGLEVEL = 0
ANONYMIZE = True
STATUS_DIR = None


def Log(s, *args):
  if args:
    print s % args
  else:
    print s
  sys.stdout.flush()


def Debug(s, *args):
  if LOGLEVEL >= 1:
    Log(s, *args)


def Debug2(s, *args):
  if LOGLEVEL >= 2:
    Log(s, *args)


SOFT = 'AEIOUY' 'V'
HARD = 'BCDFGHJKLMNPQRSTVWXYZ' 'AEIOU'


def Trigraph(num):
  """Given a value from 0..4095, encode it as a cons+vowel+cons sequence."""
  ns = len(SOFT)
  nh = len(HARD)
  assert nh * ns * nh >= 4096
  c3 = num % nh
  c2 = (num / nh) % ns
  c1 = num / nh / ns
  return HARD[c1] + SOFT[c2] + HARD[c3]


def WordFromBinary(s):
  """Encode a binary blob into a string of pronounceable syllables."""
  out = []
  while s:
    part = s[:3]
    s = s[3:]
    while len(part) < 4:
      part = '\0' + part
    bits = struct.unpack('!I', part)[0]
    out += [(bits >> 12) & 0xfff,
            (bits >> 0)  & 0xfff]
  return ''.join(Trigraph(i) for i in out)


# Note(apenwarr): There are a few ways to do this.  I elected to go with
# short human-usable strings (allowing for the small possibility of
# collisions) since the log messages will probably be "mostly" used by
# humans.
#
# An alternative would be to use "format preserving encryption" (basically
# a secure 1:1 mapping of unencrypted to anonymized, in the same number of
# bits) and then produce longer "words" with no possibility of collision.
# But with our current WordFromBinary() implementation, that would be
# 12 characters long, which is kind of inconvenient and we probably don't
# need that level of care.  Inside waveguide we use the real MAC addresses
# so collisions won't cause a real problem.
#
# TODO(apenwarr): consider not anonymizing the OUI.
#   That way we could see any behaviour differences between vendors.
#   Sadly, that might make it too easy to brute force a MAC address back out;
#   the remaining 3 bytes have too little entropy.
#
def AnonymizeMAC(consensus_key, macbin):
  """Anonymize a binary MAC address using the given key."""
  assert len(macbin) == 6
  if consensus_key and ANONYMIZE:
    return WordFromBinary(hmac.new(consensus_key, macbin).digest())[:6]
  else:
    return helpers.DecodeMAC(macbin)


def WriteEventFile(name):
  """Create a file in STATUS_DIR if it does not already exist.

  This is useful for reporting that an event has occurred.  We use O_EXCL
  to prevent any filesystem churn at all if the file still exists, so it's
  very fast.  A program watching for the event can unlink the file, then
  wait for it to be re-created as an indication that the event has
  occurred.

  Args:
    name: the name of the file to create.
  """
  fullname = os.path.join(STATUS_DIR, name)
  try:
    fd = os.open(fullname, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0666)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise
  else:
    os.close(fd)
