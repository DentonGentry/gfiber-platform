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

#include "find_nearest.h"

#include <iostream>
#include <limits>
#include <mutex>
#include <thread>

namespace speedtest {
namespace {

const long kDefaultPingTimeoutMillis = 500;

}

FindNearest::FindNearest(const Options &options)
    : options_(options),
      start_time_(0),
      end_time_(0) {
}

FindNearest::Result FindNearest::operator()(std::atomic_bool *cancel) {
  FindNearest::Result result;
  result.start_time = SystemTimeMicros();

  if (!cancel) {
    result.status = Status(StatusCode::FAILED_PRECONDITION, "cancel is null");
    result.end_time = SystemTimeMicros();
    return result;
  }

  if (options_.regions.size() == 1) {
    result.selected_region = options_.regions.front();
    result.status = Status::OK;
    result.end_time = SystemTimeMicros();
    return result;
  }

  std::vector<std::thread> threads;
  std::mutex mutex;
  for (const Region &region : options_.regions) {
    threads.emplace_back([&]{
      Ping::Options ping_options;
      ping_options.verbose = options_.verbose;
      ping_options.request_factory = options_.request_factory;
      ping_options.timeout_millis = options_.ping_timeout_millis > 0
                                    ? options_.ping_timeout_millis
                                    : kDefaultPingTimeoutMillis;
      ping_options.num_concurrent_pings = 0;
      ping_options.region = region;
      Ping ping(ping_options);
      Ping::Result ping_result = ping(cancel);
      std::lock_guard<std::mutex> lock(mutex);
      result.ping_results.push_back(ping_result);
    });
  }

  for (std::thread &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  const Ping::Result *fastest = nullptr;
  for (const Ping::Result &ping_result : result.ping_results) {
    if (ping_result.received > 0) {
      if (!fastest) {
        fastest = &ping_result;
      } else if (ping_result.min_ping_micros < fastest->min_ping_micros) {
        fastest = &ping_result;
      }
    }
  }

  if (!fastest) {
    result.status = Status(StatusCode::UNAVAILABLE,
                           "All pings failed for find nearest");
  } else {
    result.selected_region = fastest->region;
    result.min_ping_micros = fastest->min_ping_micros;
    result.status = Status::OK;
  }
  result.end_time = SystemTimeMicros();
  return result;
}

}  // namespace speedtest
