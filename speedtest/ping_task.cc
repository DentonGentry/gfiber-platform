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

#include "ping_task.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include "utils.h"

namespace speedtest {

PingTask::PingTask(const Options &options)
    : HttpTask(options),
      options_(options) {
  assert(options_.num_pings > 0);
}

void PingTask::RunInternal() {
  ResetCounters();
  success_ = false;
  threads_.clear();
  for (int i = 0; i < options_.num_pings; ++i) {
    threads_.emplace_back([=]() {
      RunPing(i);
    });
  }
}

void PingTask::StopInternal() {
  std::for_each(threads_.begin(), threads_.end(), [](std::thread &t) {
    t.join();
  });
  threads_.clear();
  if (options_.verbose) {
    std::cout << "Pinged " << options_.num_pings << " "
              << (options_.num_pings == 1 ? "host" : "hosts") << ":\n";
  }
  const PingStats *min_stats = nullptr;
  for (const auto &stat : stats_) {
    if (options_.verbose) {
      std::cout << "  " << stat.url.url() << ": ";
      if (stat.pings_received == 0) {
        std::cout << "no packets received";
      } else {
        double mean_micros = ((double) stat.total_micros) / stat.pings_received;
        std::cout << "min " << round(stat.min_micros / 1000.0d, 2) << " ms"
                  << " from " << stat.pings_received << " pings"
                  << " (mean " << round(mean_micros / 1000.0d, 2) << " ms)";
      }
      std::cout << "\n";
    }
    if (stat.pings_received > 0) {
      if (!min_stats || stat.min_micros < min_stats->min_micros) {
        min_stats = &stat;
      }
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!min_stats) {
    // no servers respondeded
    success_ = false;
  } else {
    fastest_ = *min_stats;
    success_ = true;
  }
}

void PingTask::RunPing(size_t index) {
  http::Request::Ptr ping = options_.request_factory(index);
  stats_[index].url = ping->url();
  while (GetStatus() == TaskStatus::RUNNING) {
    long req_start = SystemTimeMicros();
    if (ping->Get() == CURLE_OK) {
      long req_end = SystemTimeMicros();
      long ping_time = req_end - req_start;
      stats_[index].total_micros += ping_time;
      stats_[index].pings_received++;
      stats_[index].min_micros = std::min(stats_[index].min_micros, ping_time);
    }
    ping->Reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

bool PingTask::IsSucceeded() const {
  return success_;
}

PingStats PingTask::GetFastest() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return fastest_;
}

void PingTask::ResetCounters() {
  stats_.clear();
  stats_.resize(options_.num_pings);
}

}  // namespace speedtest
