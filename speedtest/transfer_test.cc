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

#include "transfer_test.h"

#include <cassert>

namespace speedtest {

TransferTest::TransferTest(const Options &options)
    : GenericTest(options),
      bytes_transferred_(0),
      requests_started_(0),
      requests_ended_(0) {
  assert(options.num_transfers > 0);
}

void TransferTest::ResetCounters() {
  bytes_transferred_ = 0;
  requests_started_ = 0;
  requests_ended_ = 0;
}

void TransferTest::StartRequest() {
  requests_started_++;
}

void TransferTest::EndRequest() {
  requests_ended_++;
}

void TransferTest::TransferBytes(long bytes) {
  bytes_transferred_ += bytes;
}

}  // namespace speedtest
