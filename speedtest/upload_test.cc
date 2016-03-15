/*
 * Copyright 2015 Google Inc. All rights reserved.
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

#include "upload_test.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include "generic_test.h"
#include "utils.h"

namespace speedtest {

UploadTest::UploadTest(const Options &options)
    : TransferTest(options),
      options_(options) {
  assert(options_.payload);
  assert(options_.payload->size() > 0);
}

void UploadTest::RunInternal() {
  ResetCounters();
  threads_.clear();
  if (options_.verbose) {
    std::cout << "Uploading " << options_.num_transfers
              << " threads with " << options_.payload->size() << " bytes\n";
  }
  for (int i = 0; i < options_.num_transfers; ++i) {
    threads_.emplace_back([=]{
      RunUpload(i);
    });
  }
}

void UploadTest::StopInternal() {
  std::for_each(threads_.begin(), threads_.end(), [](std::thread &t) {
    t.join();
  });
}

void UploadTest::RunUpload(int id) {
  GenericTest::RequestPtr upload = options_.request_factory(id);
  while (GetStatus() == TestStatus::RUNNING) {
    long uploaded = 0;
    upload->set_param("i", to_string(id));
    upload->set_param("time", to_string(SystemTimeMicros()));
    upload->set_progress_fn([&](curl_off_t,
                                  curl_off_t,
                                  curl_off_t,
                                  curl_off_t ulnow) -> bool {
      if (ulnow > uploaded) {
        TransferBytes(ulnow - uploaded);
        uploaded = ulnow;
      }
      return GetStatus() != TestStatus::RUNNING;
    });

    // disable the Expect header as the server isn't expecting it (perhaps
    // it should?). If the server isn't then libcurl waits for 1 second
    // before sending the data anyway. So sending this header eliminated
    // the 1 second delay.
    upload->set_header("Expect", "");

    StartRequest();
    upload->Post(options_.payload->c_str(), options_.payload->size());
    EndRequest();
    upload->Reset();
  }
}

}  // namespace speedtest
