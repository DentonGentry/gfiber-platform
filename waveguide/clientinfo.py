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
      return ';'.join(taxonomy.identify_wifi_device(signature, mac))
  except IOError:
    return None
