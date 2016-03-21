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

#ifndef SPEEDTEST_PING_TASK_H
#define SPEEDTEST_PING_TASK_H

#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "http_task.h"
#include "request.h"
#include "url.h"

namespace speedtest {

struct PingStats {
  long total_micros = 0;
  int pings_received = 0;
  long min_micros = std::numeric_limits<long>::max();
  http::Url url;
};

class PingTask : public HttpTask {
 public:
  struct Options : HttpTask::Options {
    int timeout = 0;
    int num_pings = 0;
  };

  explicit PingTask(const Options &options);

  bool IsSucceeded() const;

  PingStats GetFastest() const;

 protected:
  void RunInternal() override;
  void StopInternal() override;

 private:
  void RunPing(size_t index);

  void ResetCounters();

  Options options_;
  std::vector<PingStats> stats_;
  std::vector<std::thread> threads_;
  std::atomic_bool success_;

  mutable std::mutex mutex_;
  PingStats fastest_;

  // disallowed
  PingTask(const PingTask &) = delete;
  void operator=(const PingTask &) = delete;
};

}  // namespace speedtest

#endif  // SPEEDTEST_PING_TASK_H
