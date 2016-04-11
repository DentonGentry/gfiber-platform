#!/usr/bin/python
# Copyright 2016 Google Inc. All Rights Reserved.
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

"""Tests for craftui."""

__author__ = 'edjames@google.com (Ed James)'

import traceback
import craftui

if __name__ == '__main__':
  try:
    craftui.main()
  # pylint: disable=broad-except
  except Exception as e:
    traceback.print_exc()
    # exit cleanly to close the socket so next listen doesn't fail with in-use
    exit(1)
