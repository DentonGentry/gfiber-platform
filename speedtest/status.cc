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

#include "status.h"

#include <sstream>
#include <type_traits>
#include "utils.h"

namespace speedtest {

std::string ErrorString(StatusCode status_code) {
  switch (status_code) {
    case StatusCode::OK: return "OK";
    case StatusCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case StatusCode::ABORTED: return "ABORTED";
    case StatusCode::INTERNAL: return "INTERNAL";
    case StatusCode::FAILED_PRECONDITION: return "FAILED_PRECONDITION";
    case StatusCode::UNAVAILABLE: return "UNAVAILABLE";
    case StatusCode::UNKNOWN: return "UNKNOWN";
  }
  return std::string("Unknown status code ") + to_string(
      static_cast<std::underlying_type<StatusCode>::type>(status_code));
}

const Status Status::OK;

Status::Status(): code_(StatusCode::OK) {
}

Status::Status(StatusCode code): code_(code) {
}

Status::Status(StatusCode code, std::string message)
    : code_(code), message_(message) {
}

bool Status::ok() const {
  return code_ == StatusCode::OK;
}

StatusCode Status::code() const {
  return code_;
}

const std::string & Status::message() const {
  return message_;
}

std::string Status::ToString() const {
  std::stringstream ss;
  ss << ErrorString(code_) << ": " << message_;
  return ss.str();
}

bool Status::operator==(const Status &status) const {
  return code_ == status.code_ && message_ == status.message_;
}

bool Status::operator!=(const Status &status) const {
  return code_ != status.code_ || message_ != status.message_;
}

}  // namespace speedtest
