#!/usr/bin/python
# Copyright 2015 Google Inc. All Rights Reserved.
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

import os

import waveguide
from wvtest import wvtest


class FakeOptDict(object):
  """A fake options.OptDict containing default values."""

  def __init__(self):
    self.status_dir = '/tmp/waveguide'


@wvtest.wvtest
def IwTimeoutTest():
  old_timeout = waveguide.IW_TIMEOUT_SECS
  waveguide.IW_TIMEOUT_SECS = 1
  old_path = os.environ['PATH']
  os.environ['PATH'] = 'fake:' + os.environ['PATH']
  waveguide.RunProc(lambda e, so, se: wvtest.WVPASSEQ(e, -9),
                    ['iw', 'sleepn', str(waveguide.IW_TIMEOUT_SECS + 1)])
  os.environ['PATH'] = old_path
  waveguide.IW_TIMEOUT_SECS = old_timeout


@wvtest.wvtest
def ParseDevListTest():
  waveguide.opt = FakeOptDict()

  old_path = os.environ['PATH']
  os.environ['PATH'] = 'fake:' + os.environ['PATH']
  managers = []
  waveguide.CreateManagers(managers, False, False, None)

  got_manager_summary = set((m.phyname, m.vdevname, m.primary)
                            for m in managers)
  want_manager_summary = set((
      ('phy1', 'wlan1', True),
      ('phy1', 'wlan1_portal', False),
      ('phy0', 'wlan0', True),
      ('phy0', 'wlan0_portal', False)))

  wvtest.WVPASSEQ(got_manager_summary, want_manager_summary)

  os.environ['PATH'] = old_path


if __name__ == '__main__':
  wvtest.wvtest_main()
