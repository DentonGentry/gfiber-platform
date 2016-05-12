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

#include "ping.h"

#include <curl/curl.h>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>
#include "errors.h"
#include "url.h"

namespace speedtest {

Ping::Ping(const Options &options)
    : options_(options),
      start_time_(0),
      end_time_(0),
      pings_received_(0),
      min_ping_micros_(std::numeric_limits<long>::max()) {
}

Ping::Result Ping::operator()(std::atomic_bool *cancel) {
  start_time_ = SystemTimeMicros();

  if (!cancel) {
    end_time_ = SystemTimeMicros();
    return GetResult(Status(StatusCode::FAILED_PRECONDITION, "cancel is null"));
  }

  if (!options_.request_factory) {
    end_time_ = SystemTimeMicros();
    return GetResult(Status(StatusCode::INVALID_ARGUMENT,
                            "request factory not set"));
  }

  if (options_.region.urls.empty()) {
    end_time_ = SystemTimeMicros();
    return GetResult(Status(StatusCode::INVALID_ARGUMENT, "region URLs empty"));
  }

  std::vector<std::thread> threads;
  min_ping_micros_ = std::numeric_limits<long>::max();
  pings_received_ = 0;
  int num_pings = options_.num_concurrent_pings > 0
                  ? options_.num_concurrent_pings
                  : options_.region.urls.size();
  for (int index = 0; index < num_pings; ++index) {
    threads.emplace_back([&]{
      size_t url_index = index % options_.region.urls.size();
      http::Url url(options_.region.urls[url_index]);
      url.set_path("/ping");
      http::Request::Ptr ping = options_.request_factory(url);
      while (!*cancel) {
        ping->add_param("i", to_string(index + 1));
        ping->add_param("time", to_string(SystemTimeMicros()));
        ping->UpdateUrl();
        if (options_.timeout_millis > 0) {
          ping->set_timeout_millis(options_.timeout_millis);
        }
        long req_start = SystemTimeMicros();
        CURLcode curl_code = ping->Get();
        if (curl_code == CURLE_OK) {
          long req_end = SystemTimeMicros();
          long ping_time = req_end - req_start;
          pings_received_++;
          std::lock_guard<std::mutex> lock(mutex_);
          min_ping_micros_ = std::min(min_ping_micros_, ping_time);
        } else if (options_.verbose) {
          std::cout << "Ping " << ping->url().url() << " failed: "
                    << http::ErrorString(curl_code) << "\n";
        }
        ping->Reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    });
  }

  for (std::thread &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  end_time_ = SystemTimeMicros();
  return GetResult(Status::OK);
}

long Ping::min_ping_micros() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return min_ping_micros_;
}

Ping::Result Ping::GetResult(Status status) const {
  Ping::Result result;
  result.start_time = start_time_;
  result.end_time = end_time_;
  result.status = status;
  result.region = options_.region;
  result.min_ping_micros = min_ping_micros();
  result.received = pings_received_;
  return result;
}

}  // namespace speedtest
