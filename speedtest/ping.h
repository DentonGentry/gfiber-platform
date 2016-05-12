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

#ifndef SPEEDTEST_PING_H
#define SPEEDTEST_PING_H

#include <atomic>
#include <mutex>
#include <string>
#include "region.h"
#include "request.h"
#include "status.h"
#include "utils.h"

namespace speedtest {

class Ping {
 public:
  struct Options {
    bool verbose;
    http::Request::Factory request_factory;
    long timeout_millis;
    long num_concurrent_pings;
    Region region;
  };

  struct Result {
    long start_time;
    long end_time;
    Status status;
    Region region;
    long min_ping_micros;
    int received;
  };

  explicit Ping(const Options &options);

  Result operator()(std::atomic_bool *cancel);

  long start_time() const { return start_time_; }
  long end_time() const { return end_time_; }
  long min_ping_micros() const;

 private:
  Result GetResult(Status status) const;

  Options options_;
  std::atomic_long start_time_;
  std::atomic_long end_time_;
  std::atomic_int pings_received_;

  mutable std::mutex mutex_;
  long min_ping_micros_;

  DISALLOW_COPY_AND_ASSIGN(Ping);
};

}  // namespace speedtest

#endif // SPEEDTEST_PING_H
