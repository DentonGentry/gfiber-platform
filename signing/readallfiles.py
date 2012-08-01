#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.

"""Read all files in a directory (exclude symlink and mount dir)."""

__author__ = 'kedong@google.com (Ke Dong)'

import os
import stat
import sys
import options

optspec = """
readallfiles [options...] <dirnames>
--
q,quiet     Suppress unnecessary output.
"""

PROCMOUNTS = '/proc/mounts'
BUFSIZE = 256 * 1024

# Verbosity of output
quiet = False


def VerbosePrint(string):
  if not quiet:
    print string


def ReadFile(path, st):
  """Read the file content."""
  VerbosePrint(path)
  sz_read = 0
  file_read = 0
  # to avoid failure on symbolic links, adding a check just before opening
  # Normally, we expect the race condition should only happen on mount.
  cst = os.lstat(path)
  if stat.S_ISREG(cst.st_mode):
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOATIME |
                 os.O_NOFOLLOW)
    try:
      cst = os.fstat(fd)
      if stat.S_ISREG(cst.st_mode) and cst.st_dev == st.st_dev:
        buf = os.read(fd, BUFSIZE)
        while buf:
          sz_read += len(buf)
          buf = os.read(fd, BUFSIZE)
        file_read += 1
    finally:
      os.close(fd)
  return file_read, sz_read


def ReadAllFiles(path):
  """Read all files in the specified directory."""
  count = 0
  st = os.stat(path)
  for dirpath, dirnames, filenames in os.walk(path):
    skipdirs = []
    for dirname in dirnames:
      fullpath = os.path.join(dirpath, dirname)
      cst = os.lstat(fullpath)
      if cst.st_dev != st.st_dev:
        skipdirs.append(dirname)
    for skipdir in skipdirs:
      dirnames.remove(skipdir)
    for filename in filenames:
      fullpath = os.path.join(dirpath, filename)
      c, _ = ReadFile(fullpath, st)
      count += c
  return count


def main():
  global quiet  #gpylint: disable-msg=W0603
  o = options.Options(optspec)
  (opt, _, paths) = o.parse(sys.argv[1:])
  if paths:
    quiet = opt.quiet
    filecount = 0
    for path in paths:
      filecount += ReadAllFiles(path)
    print 'Finished scanning ' + str(filecount) + ' files.'
  else:
    o.fatal('at least one directory')


if __name__ == '__main__':
  sys.exit(main())
