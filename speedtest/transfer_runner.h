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
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "status.h"
#include "utils.h"

namespace speedtest {

struct Bucket {
  long total_bytes = 0;
  long start_time = 0;
  double short_megabits = 0.0;
  double long_megabits = 0.0;
};

struct TransferOptions {
  bool verbose = false;
  long min_runtime_millis = 0;
  long max_runtime_millis = 0;
  long interval_millis = 0;
  long progress_millis = 0;
  int min_intervals = 0;
  int max_intervals = 0;
  double max_variance = 0.0;
  bool exponential_moving_average = false;
  std::function<void(const Bucket)> progress_fn;
};

struct TransferResult {
  long start_time;
  long end_time;
  Status status;
  std::vector<Bucket> buckets;
  double speed_mbps;
  long total_bytes;
};

double GetShortEma(std::vector<Bucket> *buckets, int num_buckets);
double GetLongEma(std::vector<Bucket> *buckets, int num_intervals);
double GetSimpleAverage(std::vector<Bucket> *buckets, int num_intervals);

// Run a variable length transfer test using two moving averages.
// The test runs between min_runtime and max_runtime and otherwise
// ends when the speed is "stable" meaning the two moving averages
// are relatively close to one another.
template <typename F>
TransferResult
RunTransfer(F &&fn, std::atomic_bool *cancel, TransferOptions options) {
  TransferResult result;
  result.start_time = SystemTimeMicros();

  // sentinel value of all zeroes
  result.buckets.emplace_back();

  // If progress updates are created add a thread to send updates
  std::thread progress;
  std::atomic_bool local_cancel(false);
  if (options.progress_fn && options.progress_millis > 0) {
    if (options.verbose) {
      std::cout << "Progress updates every "
                << options.progress_millis << " ms\n";
    }
    progress = std::thread([&] {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(options.progress_millis));
      while (!local_cancel) {
        options.progress_fn(result.buckets.back());
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options.progress_millis));
      }
      options.progress_fn(result.buckets.back());
    });
  } else if (options.verbose) {
    std::cout << "No progress updates\n";
  }

  // Updating thread
  if (options.verbose) {
    std::cout << "Transfer runner updates every "
              << options.interval_millis << " ms\n";
  }
  long min_runtime_micros = options.min_runtime_millis * 1000;
  long max_runtime_micros = options.max_runtime_millis * 1000;
  std::mutex mutex;
  std::thread updater([&] {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(options.interval_millis));
    while (!local_cancel) {
      if (*cancel) {
        local_cancel = true;
        break;
      }

      Bucket last_bucket;
      long running_time = SystemTimeMicros() - result.start_time;
      {
        std::lock_guard <std::mutex> lock(mutex);
        result.buckets.emplace_back();
        Bucket &bucket = result.buckets.back();
        bucket.start_time = running_time;
        bucket.total_bytes = fn.get().bytes_transferred();
        if (options.exponential_moving_average) {
          bucket.short_megabits = GetShortEma(&result.buckets,
                                              options.min_intervals);
          bucket.long_megabits = GetLongEma(&result.buckets,
                                            options.max_intervals);
        } else {
          bucket.short_megabits = GetSimpleAverage(&result.buckets,
                                                   options.min_intervals);
          bucket.long_megabits = GetSimpleAverage(&result.buckets,
                                                  options.max_intervals);
        }
        result.speed_mbps = bucket.long_megabits;
        last_bucket = result.buckets.back();
      }

      if (running_time > max_runtime_micros) {
        local_cancel = true;
        break;
      }
      if (running_time > min_runtime_micros &&
          last_bucket.short_megabits > 0 &&
          last_bucket.long_megabits > 0) {
        double speed_variance = variance(last_bucket.short_megabits,
                                         last_bucket.long_megabits);
        if (speed_variance <= options.max_variance) {
          local_cancel = true;
          break;
        }
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(options.interval_millis));
    }
  });

  // transfer task
  std::thread task([&]{
    fn(&local_cancel);
  });

  task.join();
  updater.join();
  if (progress.joinable()) {
    progress.join();
  }

  if (*cancel) {
    result.status = Status(StatusCode::ABORTED, "transfer runner aborted");
  } else {
    result.status = Status::OK;
  }
  result.end_time = SystemTimeMicros();
  return result;
}

}  // namespace

#endif // SPEEDTEST_TRANSFER_RUNNER_H
