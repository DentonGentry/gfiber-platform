#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.
#
"""A tool for a burnin Flash test."""

__author__ = 'dgentry@google.com (Denton Gentry)'

import ctypes
import fcntl
import os
import os.path
import subprocess
import sys
import time
import options
import py_mtd


try:
  import monotime  # pylint: disable-msg=unused-import,g-import-not-at-top
except ImportError:
  pass
try:
  gettime = time.monotonic
except AttributeError:
  gettime = time.time



optspec = """
burnin-flash [options...] /path/to/write/to
--
m,mtd=  /dev/mtd# to check for errors.
p,print-interval=   Seconds between printouts [10]
r,runtime=   Number of seconds to run the test [60]
"""


def main():
  o = options.Options(optspec)
  (opt, flags, extra) = o.parse(sys.argv[1:])  #pylint: disable-msg=W0612
  if not opt.mtd:
    o.fatal('an mtd argument is required')
  if len(extra) != 1:
    o.fatal('exactly one path expected')

  filename = extra[0]
  if os.path.isdir(filename):
    filename = os.path.join(filename, 'burnin.tmp')

  f = open(filename, 'w+')

  # we still have the file open; this will ensure it gets deleted upon
  # process exit.
  os.unlink(filename)

  start_time = now = last_print_time = gettime()
  nbytes = 0
  start_ecc = py_mtd.eccstats(opt.mtd)

  while now - start_time < opt.runtime:
    try:
      # strongly random data so as to not compress well.
      dsiz = 1048576
      randdata = subprocess.check_output(['randomdata', '0', str(dsiz)])

      # write random data to flash
      f.seek(0, os.SEEK_SET)
      f.write(randdata)
      f.flush()
      os.fsync(f.fileno())

      # read back, to notice ECC errors.
      open('/proc/sys/vm/drop_caches', 'w+').write('3')
      f.seek(0, os.SEEK_SET)
      readback = f.read(dsiz)
      if readback != randdata:
        sys.stderr.write('Read back from %s != written\n' % filename)
        return 2
      nbytes += dsiz
    except (OSError, IOError) as e:
      sys.stderr.write('%s write: %s\n' % (filename, e))
      return 2

    now = gettime()
    if opt.print_interval and now - last_print_time > opt.print_interval:
      print ('%-15s %7.2fM in %5.2fs = %6.2fM/s'
             % (filename + ':',
                nbytes / 1024. / 1024.,
                now - last_print_time,
                nbytes / 1024. / 1024. / (now - last_print_time)))
      sys.stdout.flush()
      last_print_time = now
      nbytes = 0

    ecc = py_mtd.eccstats(opt.mtd)
    # Ignore ecc.corrected, they are uncommon but normal.
    print('%s: Corrected ECC error = %d' % (opt.mtd, ecc.corrected))
    if ecc.failed != start_ecc.failed:
      print('%s: Uncorrectable ECC error during test, %d != %d' %
            (opt.mtd, ecc.failed, start_ecc.failed))
      return 2
    if ecc.badblocks != start_ecc.badblocks:
      print('%s: bad blocks developed during test, %d != %d' %
            (opt.mtd, ecc.badblocks, start_ecc.badblocks))
      return 2
    if ecc.bbtblocks != start_ecc.bbtblocks:
      print('%s: bad block table error during test, %d != %d' %
            (opt.mtd, ecc.bbtblocks, start_ecc.bbtblocks))
      return 2

  return 0


if __name__ == '__main__':
  rc = main()
  sys.exit(rc)
