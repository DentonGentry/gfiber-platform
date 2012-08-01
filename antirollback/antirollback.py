#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.

"""Antirollback clock user space support.

This daemon serves several purposes:
  1. Maintain a file containing the minimum time, and periodically
     update its value.
  2. At startup, write the minimum time to /proc/ar_clock.
     The kernel will not allow the time to be set substantially
     earlier than this value (there is a small amount of wiggle
     room).
"""

__author__ = 'dgentry@google.com (Denton Gentry)'

import os
import pwd
import sys
import tempfile
import time
import options


optspec = """
antirollback [options...]
--
i,interval=   seconds between updates [28800]
p,persist=    path to persistent file [/config/ar_clock]
u,user=       setuid to this user to run
"""


# Unit tests can override these.
BIRTHDAY = 1342940400.0  # 7/22/2012
PROC_AR = '/proc/ar_clock'
PROC_UPTIME = '/proc/uptime'
SLEEP = time.sleep
TIMENOW = time.time


def GetPersistTime(ar_filename):
  """Return time stored in ar_filename, or 0.0 if it does not exist."""
  try:
    with open(ar_filename, 'r') as f:
      return float(f.read())
  except (IOError, ValueError):
    return 0.0


def GetMonotime():
  """Return a monotonically increasing count of seconds."""
  return float(open(PROC_UPTIME).read().split()[0])


def GetAntirollbackTime(ar_filename):
  """Return the appropriate antirollback time to use at startup."""
  now = max(TIMENOW(), GetPersistTime(ar_filename), BIRTHDAY)
  return now


def StoreAntirollback(now, ar_filename, kern_f):
  """Write time to /proc/ar_clock and the persistent file."""
  print 'antirollback time now ' + str(now)
  sys.stdout.flush()
  kern_f.write(str(now))
  kern_f.flush()
  tmpdir=os.path.dirname(ar_filename)
  with tempfile.NamedTemporaryFile(mode='w', dir=tmpdir, delete=False) as f:
    f.write(str(now) + '\n')
    f.flush()
    os.fsync(f.fileno())
    os.rename(f.name, ar_filename)


def LoopIterate(uptime, now, sleeptime, ar_filename, kern_f):
  SLEEP(sleeptime)
  new_uptime = GetMonotime()
  now += (new_uptime - uptime)
  uptime = new_uptime
  now = max(now, TIMENOW())
  StoreAntirollback(now=now, ar_filename=ar_filename, kern_f=kern_f)
  return (uptime, now)


def main():
  o = options.Options(optspec)
  (opt, _, _) = o.parse(sys.argv[1:])

  kern_f = open(PROC_AR, 'w')

  # Drop privileges
  if opt.user:
    pd = pwd.getpwnam(opt.user)
    os.setuid(pd.pw_uid)

  uptime = GetMonotime()
  now = GetAntirollbackTime(opt.persist)

  StoreAntirollback(now=now, ar_filename=opt.persist, kern_f=kern_f)

  while True:
    (uptime, now) = LoopIterate(uptime=uptime, now=now,
                                sleeptime=opt.interval,
                                ar_filename=opt.persist,
                                kern_f=kern_f)


if __name__ == '__main__':
  main()
