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

#ifndef SPEEDTEST_UTILS_H
#define SPEEDTEST_UTILS_H

#include <string>

namespace speedtest {

// Return relative time in microseconds
// This isn't convertible to an absolute date and time
long SystemTimeMicros();

// Return a string representation of n
std::string to_string(long n);

// return an integer value from the string str.
int stoi(const std::string& str);

}  // namespace speedtst

#endif  // SPEEDTEST_UTILS_H
