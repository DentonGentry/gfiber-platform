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

#ifndef SPEEDTEST_TRANSFER_RUNNER_H
#define SPEEDTEST_TRANSFER_RUNNER_H

#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "task.h"
#include "transfer_task.h"

namespace speedtest {

struct Interval {
  long bytes = 0;
  long running_time = 0;
  double short_megabits = 0.0;
  double long_megabits = 0.0;
};

// Run a variable length transfer test using two moving averages.
// The test runs between min_runtime and max_runtime and otherwise
// ends when the speed is "stable" meaning the two moving averages
// are relatively close to one another.
class TransferRunner : public Task {
 public:
  struct Options : public Task::Options {
    TransferTask *task = nullptr;
    int min_runtime = 0;
    int max_runtime = 0;
    int interval_millis = 0;
    int progress_millis = 0;
    int min_intervals = 0;
    int max_intervals = 0;
    double max_variance = 0.0;
    bool exponential_moving_average = false;
    std::function<void(Interval)> progress_fn;
  };

  explicit TransferRunner(const Options &options);

  double GetSpeedInMegabits() const;
  Interval GetLastInterval() const;

 protected:
  void RunInternal() override;
  void StopInternal() override;

 private:
  const Interval &AddInterval();
  double GetSimpleAverage(int num_intervals);
  double GetShortEma(int num_intervals);
  double GetLongEma(int num_intervals);

  Options options_;

  mutable std::mutex mutex_;
  std::vector<Interval> intervals_;
  std::vector<std::thread> threads_;
  double speed_;

  // disallowed
  TransferRunner(const TransferRunner &) = delete;
  void operator=(const TransferRunner &) = delete;
};

}  // namespace

#endif //SPEEDTEST_TRANSFER_RUNNER_H
