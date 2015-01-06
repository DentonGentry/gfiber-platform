#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.
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

"""Simple helper functions that don't belong elsewhere."""

import errno
import grp
import os
import os.path
import pwd
import socket
import sys


# Unit tests can override these
CHOWN = os.chown
GETGID = grp.getgrnam
GETUID = pwd.getpwnam


def Unlink(filename):
  """Like os.unlink, but doesn't raise exception if file was missing already.

  After all, you want the file gone.  Its gone.  Stop complaining.

  Args:
    filename: the filename to delete
  Raises:
    OSError: if os.unlink() fails with other than ENOENT.
  """
  try:
    os.unlink(filename)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise


def _SetOwner(filename, owner, group):
  """Set the user and group for filename.

  Args:
    filename: the path to the file to change ownership of
    owner: the string name of the owning user, like 'daemon'
    group: the string name of the owning group, like 'wheel'

    None or the empty string means not to change the ownership.
  """
  uid = gid = -1
  if owner:
    uid = GETUID(owner).pw_uid
  if group:
    gid = GETGID(group).gr_gid
  CHOWN(filename, uid, gid)


class AtomicFile(object):
  """Like a normal file object, but atomically replaces file on close().

  Example:
      with AtomicFile('filename') as f:
        f.write('hello world')
        f.write('more stuff')

  The above program creates filename.tmp, writes content to it, then
  closes it and renames it to filename, thus overwriting any existing file
  named 'filename' atomically.
  """

  def __init__(self, filename, owner=None, group=None):
    self.filename = filename
    self.file = None
    self.owner = owner
    self.group = group

  def __enter__(self):
    filename = self.filename + '.tmp'
    self.file = open(filename, 'w')
    _SetOwner(filename, self.owner, self.group)
    return self.file

  def __exit__(self, unused_type, unused_value, unused_traceback):
    if self.file:
      self.file.close()
      os.rename(self.filename + '.tmp', self.filename)


def WriteFileAtomic(filename, data, owner=None, group=None):
  """A shortcut for calling AtomicFile with a static string as content."""
  with AtomicFile(filename, owner=owner, group=group) as f:
    f.write(data)


def EncodeMAC(mac):
  s = mac.split(':')
  assert len(s) == 6
  return ''.join([chr(int(i, 16)) for i in s])


def DecodeMAC(macbin):
  """Turn the given binary MAC address into a printable string."""
  assert len(macbin) == 6
  return ':'.join(['%02x' % ord(i) for i in macbin])


def EncodeIP(ip):
  return socket.inet_aton(ip)


def DecodeIP(ipbin):
  return socket.inet_ntoa(ipbin)


_experiment_warned = set()
_experiment_enabled = set()


def Experiment(name):
  if not os.path.exists(os.path.join('/tmp/experiments',
                                     name + '.available')):
    if name not in _experiment_warned:
      _experiment_warned.add(name)
      sys.stderr.write('Warning: experiment %r not registered\n' % name)
  else:
    enabled = os.path.exists(os.path.join('/config/experiments',
                                          name + '.active'))
    if enabled and name not in _experiment_enabled:
      _experiment_enabled.add(name)
      sys.stderr.write('Notice: using experiment %r\n' % name)
    elif not enabled and name in _experiment_enabled:
      _experiment_enabled.remove(name)
      sys.stderr.write('Notice: stopping experiment %r\n' % name)
    return enabled
