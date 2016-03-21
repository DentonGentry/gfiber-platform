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

#ifndef SPEEDTEST_TASK_H
#define SPEEDTEST_TASK_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace speedtest {

enum class TaskStatus {
  NOT_STARTED,
  RUNNING,
  STOPPING,
  STOPPED
};

const char *AsString(TaskStatus status);

class Task {
 public:
  struct Options {
    bool verbose = false;
  };

  explicit Task(const Options &options);
  virtual ~Task();

  void Run();
  void Stop();

  TaskStatus GetStatus() const;
  long GetStartTime() const;
  long GetEndTime() const;
  long GetRunningTimeMicros() const;
  void WaitForEnd();

 protected:
  virtual void RunInternal() = 0;
  virtual void StopInternal() {}

 private:
  // Only call with mutex_
  void UpdateStatusLocked(TaskStatus status);

  void WaitFor(TaskStatus status);

  mutable std::mutex mutex_;
  std::thread runner_;
  std::thread stopper_;
  std::condition_variable status_cond_;
  TaskStatus status_;
  long start_time_;
  long end_time_;

  // disallowed
  Task(const Task &) = delete;
  void operator=(const Task &) = delete;
};

}  // namespace speedtest

#endif  //SPEEDTEST_TASK_H
