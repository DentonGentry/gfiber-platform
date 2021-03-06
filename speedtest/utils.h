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

#ifndef SPEEDTEST_UTILS_H
#define SPEEDTEST_UTILS_H

#include <future>
#include <memory>
#include <string>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  TypeName& operator=(const TypeName&) = delete

namespace speedtest {

template <typename F, typename... Ts>
inline std::future<typename std::result_of<F(Ts...)>::type>
ReallyAsync(F&& f, Ts&&... params) {
  return std::async(std::launch::async, std::forward<F>(f),
                    std::forward<Ts>(params)...);
}

// Return relative time in microseconds
// This isn't convertible to an absolute date and time
long SystemTimeMicros();

// Return a string representation of n
std::string to_string(long n);

// Round a double to a minimum number of significant digits
std::string round(double d, int digits);

// Return 1 - (shorter / larger)
double variance(double d1, double d2);

// Convert bytes and time in micros to speed in megabits
double ToMegabits(long bytes, long micros);

// Convert to milliseconds, round to at least 3 significant figures.
std::string ToMillis(long micros);

// Parse an int.
// If successful, write result to result and return true.
// If result is null or the int can't be parsed, return false.
bool ParseInt(const std::string &str, int *result);

// Trim from start in place
// Caller retains ownership
void LeftTrim(std::string *s);

// Trim from end in place
// Caller retains ownership
void RightTrim(std::string *s);

// Trim from both ends in place
// Caller retains ownership
void Trim(std::string *s);

std::shared_ptr<std::string> MakeRandomData(size_t size);

}  // namespace speedtst

#endif  // SPEEDTEST_UTILS_H
