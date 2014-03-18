/*
 * Copyright 2012-2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Shared library to force stdout to be unbuffered.
 * This is useful when working with binaries we cannot
 * otherwise modify; LD_PRELOAD can force this
 * library to be loaded and execute its constructor.
 */

#include <stdio.h>

__attribute__((constructor))
void stdoutnobuffer()
{
  if (setvbuf(stdout, NULL, _IOLBF, 0) == 0) {
    printf("stdout set to line buffering.\n");
  } else {
    fprintf(stderr, "Unable to make stdout line buffered.\n");
  }
}
