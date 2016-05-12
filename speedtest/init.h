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

#ifndef SPEEDTEST_INIT_H
#define SPEEDTEST_INIT_H

#include <atomic>
#include <vector>
#include "config.h"
#include "find_nearest.h"
#include "region.h"
#include "request.h"
#include "status.h"
#include "url.h"
#include "utils.h"

namespace speedtest {

class Init {
 public:
  struct Options {
    bool verbose;
    http::Request::Factory request_factory;
    bool global;
    http::Url global_url;
    std::vector<http::Url> regional_urls;
    long ping_timeout_millis;
  };

  struct Result {
    long start_time;
    long end_time;
    Status status;
    RegionResult region_result;
    FindNearest::Result find_nearest_result;
    Region selected_region;
    ConfigResult config_result;
  };

  explicit Init(const Options &options);

  Result operator()(std::atomic_bool *cancel);

 private:
  Options options_;

  DISALLOW_COPY_AND_ASSIGN(Init);
};

}  // namespace speedtest

#endif // SPEEDTEST_INIT_H
