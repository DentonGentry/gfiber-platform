/*
 * Copyright 2016 Google Inc. All rights reserved.
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

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <stdio.h>
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

std::string to_string(long n) {
  std::ostringstream s;
  s << n;
  return s.str();
}

std::string round(double d, int digits) {
  char buf[20];
  sprintf(buf, "%.*f", digits, d);
  return buf;
}

double variance(double d1, double d2) {
  if (d2 == 0) {
    return 0.0;
  }
  double smaller = std::min(d1, d2);
  double larger = std::max(d1, d2);
  return 1.0 - smaller / larger;
}

double ToMegabits(long bytes, long micros) {
  return (8.0d * bytes) / micros;
}

std::string ToMillis(long micros) {
  double millis = micros / 1000.0d;
  if (millis < 1) {
    return round(millis, 3);
  } else if (millis < 10) {
    return round(millis, 2);
  } else if (millis < 1000) {
    return round(millis, 1);
  }
  return round(millis, 0);
}

bool ParseInt(const std::string &str, int *result) {
  if (!result) {
    return false;
  }
  std::istringstream n(str);
  return !(n >> *result).fail();
}

// Trim from start in place
// Caller retains ownership
void LeftTrim(std::string *s) {
  s->erase(s->begin(),
           std::find_if(s->begin(),
                        s->end(),
                        std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// Trim from end in place
// Caller retains ownership
void RightTrim(std::string *s) {
  s->erase(std::find_if(s->rbegin(),
                        s->rend(),
                        std::not1(std::ptr_fun<int, int>(std::isspace))).base(),
           s->end());
}

// Trim from both ends in place
// Caller retains ownership
void Trim(std::string *s) {
  LeftTrim(s);
  RightTrim(s);
}

std::shared_ptr<std::string> MakeRandomData(size_t size) {
  std::random_device rd;
  std::default_random_engine random_engine(rd());
  std::uniform_int_distribution<char> uniform_dist(1, 255);
  auto random_data = std::make_shared<std::string>();
  random_data->resize(size);
  for (size_t i = 0; i < size; ++i) {
    (*random_data)[i] = uniform_dist(random_engine);
  }
  return std::move(random_data);
}

}  // namespace speedtest
