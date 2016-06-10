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

# One version of gpylint wants to see clientinfo first and taxonomy last.
# Another wants to see the reverse. Cannot satisfy both, so tell both of them
# to shove the error so far up their stdin that it should never trouble us
# again.
# pylint:disable=g-bad-import-order
import taxonomy
import clientinfo
from wvtest import wvtest


@wvtest.wvtest
def TaxonomyTest():
  taxonomy.dhcp.DHCP_LEASES_FILE = 'fake/dhcp.leases'
  clientinfo.FINGERPRINTS_DIR = 'fake/taxonomy'
  wvtest.WVPASS('Nexus 6' in clientinfo.taxonomize('00:00:01:00:00:01'))
  wvtest.WVPASS('Nexus 6' in clientinfo.taxonomize('00:00:01:00:00:01\n'))
  v = 'Moto G or Moto X'
  wvtest.WVPASS(v in clientinfo.taxonomize('9c:d9:17:00:00:02'))
  wvtest.WVPASSEQ(clientinfo.taxonomize('00:00:22:00:00:22'), None)


if __name__ == '__main__':
  wvtest.wvtest_main()
