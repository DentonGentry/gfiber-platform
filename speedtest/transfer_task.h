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

#ifndef SPEEDTEST_TRANSFER_TEST_H
#define SPEEDTEST_TRANSFER_TEST_H

#include <atomic>
#include "http_task.h"

namespace speedtest {

class TransferTask : public HttpTask {
 public:
  struct Options : HttpTask::Options {
    int num_transfers = 0;
  };

  explicit TransferTask(const Options &options);

  long bytes_transferred() const { return bytes_transferred_; }
  long requests_started() const { return requests_started_; }
  long requests_ended() const { return requests_ended_; }

 protected:
  void ResetCounters();
  void StartRequest();
  void EndRequest();
  void TransferBytes(long bytes);

 private:
  std::atomic_long bytes_transferred_;
  std::atomic_int requests_started_;
  std::atomic_int requests_ended_;

  // disallowed
  TransferTask(const TransferTask &) = delete;
  void operator=(const TransferTask &) = delete;
};

}  // namespace speedtest

#endif  // SPEEDTEST_TRANSFER_TEST_H
