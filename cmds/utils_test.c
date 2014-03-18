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
#include <stdio.h>
#include <string.h>
#include "utils.h"

#define ARRAYSIZE(x) (int)(sizeof(x)/sizeof(x[0]))


int main() {
  int i;
  char *tests[] = {
    "a", "abc", "_a", "abc_", "_", "____",
  };
  char *expect[] = {
    "a", "abc", "a", "abc", "", "",
  };
  int fail = 0;

  for (i = 0; i < ARRAYSIZE(tests); ++i) {
    tests[i] = strdup(tests[i]);
    strip_underscores(tests[i]);
    if (strcmp(tests[i], expect[i])) {
      printf("Failed: expected: %s got: %s\n", tests[i], expect[i]);
      fail = 1;
    }
    printf("stripped version: '%s'\n", tests[i]);
  }

  return fail;  // an optimist would return success.
}
