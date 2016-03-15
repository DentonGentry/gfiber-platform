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

#ifndef SPEEDTEST_GENERIC_TEST_H
#define SPEEDTEST_GENERIC_TEST_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include "request.h"

namespace speedtest {

enum class TestStatus {
  NOT_STARTED,
  RUNNING,
  STOPPING,
  STOPPED
};

const char *AsString(TestStatus status);

class GenericTest {
 public:
  using RequestPtr = std::unique_ptr<http::Request>;

  struct Options {
    bool verbose = false;
    std::function<RequestPtr(int)> request_factory;
  };

  explicit GenericTest(const Options &options);

  void Run();
  void Stop();

  TestStatus GetStatus() const;
  long GetRunningTime() const;
  void WaitForEnd();

 protected:
  virtual void RunInternal() = 0;
  virtual void StopInternal() {}

 private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  TestStatus status_;
  bool running_ = false;
  long start_time_;
  long end_time_;

  // disallowed
  GenericTest(const GenericTest &) = delete;

  void operator=(const GenericTest &) = delete;
};

}  // namespace speedtest

#endif  //SPEEDTEST_GENERIC_TEST_H
