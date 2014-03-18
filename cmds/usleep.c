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

/* There is no checking of the value passed in. -1 will cause you wait for a
 * while. it will not bring you back but enough long to make you forget. */
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc == 2) {
    usleep(atoi(argv[1]));
    return (0);
  }
  exit(1);
}
