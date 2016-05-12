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

#include "download.h"

#include <string>
#include <vector>
#include <thread>

namespace speedtest {

Download::Download(const Options &options)
    : options_(options),
      start_time_(0),
      end_time_(0),
      bytes_transferred_(0) {
}

Download::Result Download::operator()(std::atomic_bool *cancel) {
  start_time_ = SystemTimeMicros();
  bytes_transferred_ = 0;

  if (!cancel) {
    end_time_ = SystemTimeMicros();
    return GetResult(Status(StatusCode::FAILED_PRECONDITION, "cancel is null"));
  }

  std::vector<std::thread> threads;
  for (int i = 0; i < options_.num_transfers; ++i) {
    threads.emplace_back([=]{
      http::Request::Ptr download = options_.request_factory(i);
      while (!*cancel) {
        long downloaded = 0;
        download->set_param("i", to_string(i));
        download->set_param("size", to_string(options_.download_bytes));
        download->set_param("time", to_string(SystemTimeMicros()));
        download->set_progress_fn([&](curl_off_t,
                                      curl_off_t dlnow,
                                      curl_off_t,
                                      curl_off_t) -> bool {
          if (dlnow > downloaded) {
            bytes_transferred_ += dlnow - downloaded;
            downloaded = dlnow;
          }
          return *cancel;
        });
        download->Get();
        download->Reset();
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

Download::Result Download::GetResult(Status status) const {
  Download::Result result;
  result.start_time = start_time_;
  result.end_time = end_time_;
  result.status = status;
  result.bytes_transferred = bytes_transferred_;
  return result;
}

}  // namespace
