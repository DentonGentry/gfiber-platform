# Copyright 2015 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# pylint: disable=undefined-variable


"""Python bindings for working with Linux MTD."""

import collections
import _py_mtd

MtdEccStats = collections.namedtuple(
    'MtdEccStats', 'corrected failed badblocks bbtblocks')

def eccstats(mtddev):
  """Return Linux MTD stats for an MTD device.

  Arguments:
    mtddev: a path to an mtd device, like /dev/mtd1

  Returns:
    a namedtuple with the following fields:
      corrected: errors corrected via ECC
      failed: uncorrectable ECC errors
      badblocks: blocks marked bad
      bbtblocks: blocks reserved for bad block tables
  """
  (corrected, failed, badblocks, bbtblocks) = _py_mtd.eccstats(mtddev)
  return MtdEccStats(corrected=corrected, failed=failed, badblocks=badblocks,
                     bbtblocks=bbtblocks)
