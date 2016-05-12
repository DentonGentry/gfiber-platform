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

#ifndef SPEEDTEST_FIND_NEAREST_H
#define SPEEDTEST_FIND_NEAREST_H

#include <vector>
#include "ping.h"
#include "region.h"
#include "request.h"
#include "status.h"
#include "utils.h"

namespace speedtest {

class FindNearest {
 public:
  struct Options {
    bool verbose;
    http::Request::Factory request_factory;
    std::vector<Region> regions;
    long ping_timeout_millis;
  };

  struct Result {
    long start_time;
    long end_time;
    std::vector<Ping::Result> ping_results;
    Status status;
    Region selected_region;
    long min_ping_micros;
  };

  explicit FindNearest(const Options &options);

  Result operator()(std::atomic_bool *cancel);

  long start_time() const { return start_time_; }
  long end_time() const { return end_time_; }

 private:
  Options options_;
  std::atomic_long start_time_;
  std::atomic_long end_time_;

  DISALLOW_COPY_AND_ASSIGN(FindNearest);
};

}  // namespace speedtest

#endif // SPEEDTEST_FIND_NEAREST_H
