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
import os
import sys


LOGLEVEL = 0
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
