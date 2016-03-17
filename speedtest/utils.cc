/*
 * Copyright 2015 Google Inc. All rights reserved.
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

#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>

namespace speedtest {

long SystemTimeMicros() {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    std::cerr << "Error reading system time\n";
    std::exit(1);
  }
  return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

std::string to_string(long n)
{
  std::ostringstream s;
  s << n;
  return s.str();
}

int stoi(const std::string& str)
{
  int rc;
  std::istringstream n(str);

  if (!(n >> rc)) {
    std::cerr << "Not a number: " << str;
    std::exit(1);
  }

  return rc;
}

}  // namespace speedtest
