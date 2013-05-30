#!/usr/bin/python
# Copyright 2013 Google Inc. All Rights Reserved.

"""Utility to recursively change folder/file ownership and access mode.

Recurses through a given list of folders and changes all files and
folders to a given user/group ownership and access mode.
"""

__author__ = 'ckuiper@google.com (Chris Kuiper)'


import grp
import os
import pwd
import sys
import options


DEFAULT_MODE_DIR = 0775   # octal
DEFAULT_MODE_FILE = 0664  # octal


optspec = """
chg_mod_own.py [options...] {<folders>}
--
uid=        Username for ownership [video]
gid=        Groupname for ownership [video]
mode_dir=   Access mode for directories [0775]
mode_file=  Access mode for files [0664]
v,verbose   Print information
"""


def PrintErr(err_string):
  """Print error message to stderr."""
  sys.stdout.flush()
  sys.stderr.write(err_string + '\n')


def GetInt(parameter, default_val, base=10):
  """Convert parameter into an integer using the provided base and default."""
  try:
    return int(str(parameter), base)
  except ValueError:
    return default_val


def Fixit(basedir, uid, gid, mode_dir, mode_file, verbose):
  """Change access mode and ownership for everything in a given directory.

  Args:
    basedir: directory to work in
    uid: User-Id (string, e.g. "video")
    gid: Group-Id (string, e.g. "video")
    mode_dir: directory acccess mode (numerical, e.g. "0775")
    mode_file: files access mode (numerical, e.g. "0664")
    verbose: Print verbose information
  Returns:
    A tuple of statistics:
    folders_changed: Number of folders successfully changed
    folders_failed: Number of folders failed to be changed
    files_changed: Number of files successfully changed
    files_failed: Number of files failed to be changed
    slinks_skipped: Number of symbolic links ignored
  """
  stats_folders_changed = 0
  stats_folders_failed = 0
  stats_files_changed = 0
  stats_files_failed = 0
  stats_slinks_skipped = 0

  os.chdir(basedir)
  os.chown('.', uid, gid)
  os.chmod('.', mode_dir)
  stats_folders_changed += 1
  realdir = os.getcwd()
  if realdir != '/': realdir += '/'

  for path, folders, files in os.walk(realdir):
    # Change the folders first
    for name in folders:
      try:
        d = os.path.join(path, name)
        if os.path.islink(d):
          if verbose:
            print '%r is folder-link, skip.' % d
          stats_slinks_skipped += 1
          continue
        os.chdir(d)
        curdir = os.getcwd()
        # The following check should be redundant, since os.walk does not
        # follow symbolic links and we just checked that this is not one
        # either. Still worthwhile to do anyway...
        if not curdir.startswith(realdir):
          PrintErr('%r is not inside %r - skip.' % (curdir, realdir))
          continue
        if verbose:
          print '%r (%.3o %s.%s)' % (d, mode_dir, pwd.getpwuid(uid).pw_name,
                                     grp.getgrgid(gid).gr_name)
        os.chown('.', uid, gid)
        os.chmod('.', mode_dir)
        stats_folders_changed += 1
      except (OSError, IOError), e:
        PrintErr('%r: %s' % (d, e))
        stats_folders_failed += 1

    # Now change the files
    for name in files:
      try:
        f = os.path.join(path, name)
        if os.path.islink(f):
          if verbose:
            print '%r is file-link, skip.' % f
          stats_slinks_skipped += 1
          continue
        os.chdir(path)
        curdir = os.getcwd()
        if curdir != '/': curdir += '/'
        # As above, the following check should be redundant, since
        # os.walk does not follow symbolic links.
        if not curdir.startswith(realdir):
          PrintErr('%r is not inside %r - skip.' % (curdir, realdir))
          continue
        if verbose:
          print '%r (%.3o %s.%s)' % (f, mode_file, pwd.getpwuid(uid).pw_name,
                                     grp.getgrgid(gid).gr_name)
        fd = os.open(name, os.O_NOFOLLOW | os.O_NONBLOCK |
                     os.O_NOCTTY | os.O_NOATIME)
        try:
          os.fchown(fd, uid, gid)
          os.fchmod(fd, mode_file)
          stats_files_changed += 1
        finally:
          os.close(fd)
      except (OSError, IOError), e:
        PrintErr('%r: %s' % (f, e))
        stats_files_failed += 1

  return (stats_folders_changed, stats_folders_failed,
          stats_files_changed, stats_files_failed, stats_slinks_skipped)


def main():
  o = options.Options(optspec)
  opt, _, extra = o.parse(sys.argv[1:])

  mode_dir = GetInt(opt.mode_dir, DEFAULT_MODE_DIR, 8)
  mode_file = GetInt(opt.mode_file, DEFAULT_MODE_FILE, 8)
  try:
    uid = pwd.getpwnam(opt.uid).pw_uid
  except KeyError:
    PrintErr('Error: UID %r does not exist!' % opt.uid)
    return 1
  try:
    gid = grp.getgrnam(opt.gid).gr_gid
  except KeyError:
    PrintErr('Error: GID %r does not exist!' % opt.gid)
    return 1

  print 'Setting ownership to %r.%r' % (opt.uid, opt.gid)
  print 'Set directory access mode to %.3o' % mode_dir
  print 'Set file access mode to %.3o' % mode_file
  print 'Folders to work on: %r' % extra

  startdir = os.getcwd()
  for folder in extra:
    try:
      print 'Working on %r' % folder
      d_chng, d_fail, f_chng, f_fail, slinks = Fixit(folder, uid, gid,
                                                     mode_dir, mode_file,
                                                     opt.verbose)
      print 'Completed %r. Statistics:' % folder
      print 'Folders changed/failed : %d/%d' % (d_chng, d_fail)
      print 'Files changed/failed   : %d/%d' % (f_chng, f_fail)
      print 'Symlinks skipped       : %d' % slinks
    except (OSError, IOError), e:
      PrintErr('%r: %s' % (folder, e))
    # make sure we are back in our original starting folder
    os.chdir(startdir)

  # return non-zero if we had failures
  if d_fail or f_fail:
    return 2

  return 0

if __name__ == '__main__':
  sys.exit(main())
