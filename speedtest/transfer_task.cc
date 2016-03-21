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

#include "transfer_task.h"

#include <cassert>
#include <thread>
#include <vector>

namespace speedtest {

TransferTask::TransferTask(const Options &options)
    : HttpTask(options),
      bytes_transferred_(0),
      requests_started_(0),
      requests_ended_(0) {
  assert(options.num_transfers > 0);
}

void TransferTask::ResetCounters() {
  bytes_transferred_ = 0;
  requests_started_ = 0;
  requests_ended_ = 0;
}

void TransferTask::StartRequest() {
  requests_started_++;
}

void TransferTask::EndRequest() {
  requests_ended_++;
}

void TransferTask::TransferBytes(long bytes) {
  bytes_transferred_ += bytes;
}

}  // namespace speedtest
