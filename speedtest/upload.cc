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

#include "upload.h"

#include <string>
#include <thread>
#include <vector>

namespace speedtest {

Upload::Upload(const Options &options)
    : options_(options),
      start_time_(0),
      end_time_(0),
      bytes_transferred_(0) {
}

Upload::Result Upload::operator()(std::atomic_bool *cancel) {
  start_time_ = SystemTimeMicros();
  bytes_transferred_ = 0;

  if (!cancel) {
    end_time_ = SystemTimeMicros();
    return GetResult(Status(StatusCode::FAILED_PRECONDITION, "cancel is null"));
  }

  std::vector<std::thread> threads;
  for (int i = 0; i < options_.num_transfers; ++i) {
    threads.emplace_back([=]{
      http::Request::Ptr upload = options_.request_factory(i);
      while (!*cancel) {
        long uploaded = 0;
        upload->set_param("i", to_string(i));
        upload->set_param("time", to_string(SystemTimeMicros()));
        upload->set_progress_fn([&](curl_off_t,
                                    curl_off_t,
                                    curl_off_t,
                                    curl_off_t ulnow) -> bool {
          if (ulnow > uploaded) {
            bytes_transferred_ += ulnow - uploaded;
            uploaded = ulnow;
          }
          return *cancel;
        });

        // disable the Expect header as the server isn't expecting it (perhaps
        // it should?). If the server isn't then libcurl waits for 1 second
        // before sending the data anyway. So sending this header eliminated
        // the 1 second delay.
        upload->set_header("Expect", "");

        upload->Post(options_.payload->c_str(), options_.payload->size());
        upload->Reset();
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

Upload::Result Upload::GetResult(Status status) const {
  Upload::Result result;
  result.start_time = start_time_;
  result.end_time = end_time_;
  result.status = status;
  result.bytes_transferred = bytes_transferred_;
  return result;
}

}  // namespace
