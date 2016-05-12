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

#ifndef SPEEDTEST_UPLOAD_H
#define SPEEDTEST_UPLOAD_H

#include <atomic>
#include <functional>
#include <memory>
#include "request.h"
#include "status.h"
#include "utils.h"

namespace speedtest {

class Upload {
 public:
  struct Options {
    bool verbose;
    std::function<http::Request::Ptr(int)> request_factory;
    int num_transfers;
    std::shared_ptr<std::string> payload;
  };

  struct Result {
    long start_time;
    long end_time;
    Status status;
    long bytes_transferred;
  };

  explicit Upload(const Options &options);

  Result operator()(std::atomic_bool *cancel);

  long start_time() const { return start_time_; }
  long end_time() const { return end_time_; }
  long bytes_transferred() const { return bytes_transferred_; }

 private:
  Result GetResult(Status status) const;

  Options options_;
  std::atomic_long start_time_;
  std::atomic_long end_time_;
  std::atomic_long bytes_transferred_;

  DISALLOW_COPY_AND_ASSIGN(Upload);
};

}  // namespace speedtest

#endif // SPEEDTEST_UPLOAD_H
