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

#ifndef SPEEDTEST_HTTP_TASK_H
#define SPEEDTEST_HTTP_TASK_H

#include "task.h"

#include "request.h"

namespace speedtest {

class HttpTask : public Task {
 public:
  struct Options : Task::Options {
    bool verbose = false;
    std::function<http::Request::Ptr(int)> request_factory;
  };

  explicit HttpTask(const Options &options);

 private:
  // disallowed
  HttpTask(const Task &) = delete;
  void operator=(const HttpTask &) = delete;
};

}  // namespace speedtest

#endif  // SPEEDTEST_HTTP_TASK_H
