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

#include "generic_test.h"

#include <cassert>
#include "utils.h"

namespace speedtest {

const char *AsString(TestStatus status) {
  switch (status) {
    case TestStatus::NOT_STARTED: return "NOT_STARTED";
    case TestStatus::RUNNING: return "RUNNING";
    case TestStatus::STOPPING: return "STOPPING";
    case TestStatus::STOPPED: return "STOPPED";
  }
  std::exit(1);
}

GenericTest::GenericTest(const Options &options)
    : status_(TestStatus::NOT_STARTED) {
  assert(options.request_factory);
}

void GenericTest::Run() {
  {
    std::lock_guard <std::mutex> lock(mutex_);
    if (status_ != TestStatus::NOT_STARTED &&
        status_ != TestStatus::STOPPED) {
      return;
    }
    status_ = TestStatus::RUNNING;
    start_time_ = SystemTimeMicros();
  }
  RunInternal();
}

void GenericTest::Stop() {
  {
    std::lock_guard <std::mutex> lock(mutex_);
    if (status_ != TestStatus::RUNNING) {
      return;
    }
    status_ = TestStatus::STOPPING;
  }
  StopInternal();
  std::lock_guard <std::mutex> lock(mutex_);
  status_ = TestStatus::STOPPED;
  end_time_ = SystemTimeMicros();
}

TestStatus GenericTest::GetStatus() const {
  std::lock_guard <std::mutex> lock(mutex_);
  return status_;
}

long GenericTest::GetRunningTime() const {
  std::lock_guard <std::mutex> lock(mutex_);
  switch (status_) {
    case TestStatus::NOT_STARTED:
      break;
    case TestStatus::RUNNING:
    case TestStatus::STOPPING:
      return SystemTimeMicros() - start_time_;
    case TestStatus::STOPPED:
      return end_time_ - start_time_;
  }
  return 0;
}

void GenericTest::WaitForEnd() {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_.wait(lock, [this]{
    return status_ == TestStatus::STOPPED;
  });
}

}  // namespace speedtest
