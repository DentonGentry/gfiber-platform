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

#ifndef SPEEDTEST_TIMED_RUNNER_H
#define SPEEDTEST_TIMED_RUNNER_H

#include <future>
#include <thread>
#include <type_traits>
#include "utils.h"

namespace speedtest {

template <typename F>
typename std::result_of<F(std::atomic_bool *)>::type
RunTimed(F &&fn, std::atomic_bool *cancel, long timeout_millis) {
  std::atomic_bool local_cancel(false);
  long start_time = SystemTimeMicros();
  long end_time = start_time + timeout_millis * 1000;
  auto fut = ReallyAsync(std::forward<F>(fn), &local_cancel);
  std::thread timer([&]{
    while (!*cancel && SystemTimeMicros() <= end_time) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    local_cancel = true;
  });
  timer.join();
  fut.wait();
  return fut.get();
}

}  // namespace

#endif // SPEEDTEST_TIMED_RUNNER_H
