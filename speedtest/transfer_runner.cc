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

#include "transfer_runner.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include "transfer_task.h"
#include "utils.h"

namespace speedtest {
namespace {

const int kDefaultIntervalMillis = 200;

}  // namespace

TransferRunner::TransferRunner(const Options &options)
    : Task(options),
      options_(options) {
  if (options_.interval_millis <= 0) {
    options_.interval_millis = kDefaultIntervalMillis;
  }
}

void TransferRunner::RunInternal() {
  threads_.clear();
  intervals_.clear();

  // sentinel value of all zeroes
  intervals_.emplace_back();

  // If progress updates are created add a thread to send updates
  if (options_.progress_fn && options_.progress_millis > 0) {
    if (options_.verbose) {
      std::cout << "Progress updates every "
                << options_.progress_millis << " ms\n";
    }
    threads_.emplace_back([&] {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(options_.progress_millis));
      while (GetStatus() == TaskStatus::RUNNING) {
        Interval progress = GetLastInterval();
        options_.progress_fn(progress);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options_.progress_millis));
      }
      Interval progress = GetLastInterval();
      options_.progress_fn(progress);
    });
  } else if (options_.verbose) {
    std::cout << "No progress updates\n";
  }

  // Updating thread
  if (options_.verbose) {
    std::cout << "Transfer runner updates every "
              << options_.interval_millis << " ms\n";
  }
  threads_.emplace_back([&] {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(options_.interval_millis));
    while (GetStatus() == TaskStatus::RUNNING) {
      const Interval &interval = AddInterval();
      if (interval.running_time > options_.max_runtime * 1000) {
        Stop();
        return;
      }
      if (interval.running_time >= options_.min_runtime * 1000 &&
          interval.long_megabits > 0 &&
          interval.short_megabits > 0) {
        double speed_variance = variance(interval.short_megabits,
                                         interval.long_megabits);
        if (speed_variance <= options_.max_variance) {
          Stop();
          return;
        }
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(options_.interval_millis));
    }
  });

  options_.task->Run();
}

void TransferRunner::StopInternal() {
  options_.task->Stop();
  options_.task->WaitForEnd();
  std::for_each(threads_.begin(), threads_.end(), [](std::thread &t) {
    t.join();
  });
  threads_.clear();
}

const Interval &TransferRunner::AddInterval() {
  std::lock_guard <std::mutex> lock(mutex_);
  intervals_.emplace_back();
  Interval &interval = intervals_[intervals_.size() - 1];
  interval.running_time = options_.task->GetRunningTimeMicros();
  interval.bytes = options_.task->bytes_transferred();
  if (options_.exponential_moving_average) {
    interval.short_megabits = GetShortEma(options_.min_intervals);
    interval.long_megabits = GetLongEma(options_.max_intervals);
  } else {
    interval.short_megabits = GetSimpleAverage(options_.min_intervals);
    interval.long_megabits = GetSimpleAverage(options_.max_intervals);
  }
  speed_ = interval.long_megabits;
  return intervals_.back();
}

Interval TransferRunner::GetLastInterval() const {
  std::lock_guard <std::mutex> lock(mutex_);
  return intervals_.back();
}

double TransferRunner::GetSpeedInMegabits() const {
  std::lock_guard <std::mutex> lock(mutex_);
  return speed_;
}

double TransferRunner::GetShortEma(int num_intervals) {
  if (intervals_.empty() || num_intervals <= 0) {
    return 0.0;
  }
  Interval last_interval = GetLastInterval();
  double percent = 2.0d / (num_intervals + 1);
  return GetSimpleAverage(1) * percent +
      last_interval.short_megabits * (1 - percent);
}

double TransferRunner::GetLongEma(int num_intervals) {
  if (intervals_.empty() || num_intervals <= 0) {
    return 0.0;
  }
  Interval last_interval = GetLastInterval();
  double percent = 2.0d / (num_intervals + 1);
  return GetSimpleAverage(1) * percent +
      last_interval.long_megabits * (1 - percent);
}

double TransferRunner::GetSimpleAverage(int num_intervals) {
  if (intervals_.empty() || num_intervals <= 0) {
    return 0.0;
  }
  int end_index = intervals_.size() - 1;
  int start_index = std::max(0, end_index - num_intervals);
  const Interval &end = intervals_[end_index];
  const Interval &start = intervals_[start_index];
  return ToMegabits(end.bytes - start.bytes,
                    end.running_time - start.running_time);
}

}  // namespace
