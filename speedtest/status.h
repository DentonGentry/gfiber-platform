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

#ifndef SPEEDTEST_STATUS_H
#define SPEEDTEST_STATUS_H

#include <string>

namespace speedtest {

enum class StatusCode {
  OK = 0,
  INVALID_ARGUMENT = 1,
  ABORTED = 2,
  INTERNAL = 3,
  FAILED_PRECONDITION = 4,
  UNAVAILABLE = 5,
  UNKNOWN = 6
};

std::string ErrorString(StatusCode status_code);

class Status {
 public:
  Status();
  explicit Status(StatusCode code);
  Status(StatusCode code, std::string message);

  bool ok() const;
  StatusCode code() const;
  const std::string &message() const;
  std::string ToString() const;

  bool operator==(const Status &status) const;
  bool operator!=(const Status &status) const;

  static const Status OK;

 private:
  StatusCode code_;
  std::string message_;
};

}  // namespace speedtest

#endif  // SPEEDTEST_STATUS_H
