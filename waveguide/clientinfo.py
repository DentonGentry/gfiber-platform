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
#
# pylint:disable=invalid-name

"""Client identification support."""

import os.path
import taxonomy


# unit tests can override these.
FINGERPRINTS_DIR = '/tmp/wifi/fingerprints'


def taxonomize(mac):
  """Try to identify the type of client device."""
  mac = mac.lower().strip()
  try:
    with open(os.path.join(FINGERPRINTS_DIR, mac)) as f:
      signature = f.read()
      (genus, species, perf) = taxonomy.identify_wifi_device(signature, mac)

      # Preserve older output format of chipset;model;performance. We no
      # longer track chipsets, but we output the leading ';' separator to
      # maintain compatibility with the format.
      #
      # For example, in the old code:
      # unknown: SHA:c1...7b;Unknown;802.11n n:2,w:40
      # known:   BCM4329;iPad (1st/2nd gen);802.11n n:1,w:20
      #
      # In the current code, in the unknown case:
      # genus = 'SHA:c1...7b', species = 'Unknown', perf = '802.11n n:2,w:40'
      # SHA:c1...7b;Unknown;802.11n n:2,w:40
      #
      # In the current code, known, with species information:
      # genus = 'iPad', species = '(1st/2nd gen)', perf = '802.11n n:1,w:20'
      # ;iPad (1st/2nd gen);802.11n n:1,w:20
      #
      # In the current code, known, no specific species:
      # genus = 'Samsung Galaxy S6', species = '', perf = '802.11ac n:2,w:80'
      # ;Samsung Galaxy S6;802.11ac n:2,w:80
      # We don't want an extra space at the end of the model, so we need to be
      # careful about a join of the empty species.
      # ;Samsung Galaxy S6 ;802.11ac n:2,w:80

      if genus.startswith('SHA:'):
        return genus + ';' + species + ';' + perf
      elif species:
        return ';' + genus + ' ' + species + ';' + perf
      else:
        return ';' + genus + ';' + perf
  except IOError:
    return None
