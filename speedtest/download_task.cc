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

#include "download_task.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <thread>
#include "utils.h"

namespace speedtest {

DownloadTask::DownloadTask(const Options &options)
    : TransferTask(options_),
      options_(options) {
  assert(options_.num_transfers > 0);
  assert(options_.download_size > 0);
}

void DownloadTask::RunInternal() {
  ResetCounters();
  threads_.clear();
  if (options_.verbose) {
    std::cout << "Downloading " << options_.num_transfers
              << " threads with " << options_.download_size << " bytes\n";
  }
  for (int i = 0; i < options_.num_transfers; ++i) {
    threads_.emplace_back([=]{
      RunDownload(i);
    });
  }
}

void DownloadTask::StopInternal() {
  std::for_each(threads_.begin(), threads_.end(), [](std::thread &t) {
    t.join();
  });
}

void DownloadTask::RunDownload(int id) {
  http::Request::Ptr download = options_.request_factory(id);
  while (GetStatus() == TaskStatus::RUNNING) {
    long downloaded = 0;
    download->set_param("i", to_string(id));
    download->set_param("size", to_string(options_.download_size));
    download->set_param("time", to_string(SystemTimeMicros()));
    download->set_progress_fn([&](curl_off_t,
                                  curl_off_t dlnow,
                                  curl_off_t,
                                  curl_off_t) -> bool {
      if (dlnow > downloaded) {
        TransferBytes(dlnow - downloaded);
        downloaded = dlnow;
      }
      return GetStatus() != TaskStatus::RUNNING;
    });
    StartRequest();
    download->Get();
    EndRequest();
    download->Reset();
  }
}

}  // namespace speedtest
