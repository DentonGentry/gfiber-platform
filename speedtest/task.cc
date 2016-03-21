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

#include "task.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <thread>
#include "utils.h"

namespace speedtest {

const char *AsString(TaskStatus status) {
  switch (status) {
    case TaskStatus::NOT_STARTED: return "NOT_STARTED";
    case TaskStatus::RUNNING: return "RUNNING";
    case TaskStatus::STOPPING: return "STOPPING";
    case TaskStatus::STOPPED: return "STOPPED";
  }
  std::exit(1);
}

Task::Task(const Options &options)
    : status_(TaskStatus::NOT_STARTED) {
  assert(options.request_factory);
}

Task::~Task() {
  Stop();
  if (runner_.joinable()) {
    runner_.join();
  }
  if (stopper_.joinable()) {
    stopper_.join();
  }
}

void Task::Run() {
  runner_ = std::thread([=]{
    {
      std::lock_guard <std::mutex> lock(mutex_);
      if (status_ != TaskStatus::NOT_STARTED &&
          status_ != TaskStatus::STOPPED) {
        return;
      }
      UpdateStatusLocked(TaskStatus::RUNNING);
      start_time_ = SystemTimeMicros();
    }
    RunInternal();
  });
  stopper_ = std::thread([=]{
    WaitFor(TaskStatus::STOPPING);
    StopInternal();
    std::lock_guard <std::mutex> lock(mutex_);
    UpdateStatusLocked(TaskStatus::STOPPED);
    end_time_ = SystemTimeMicros();
  });
}

void Task::Stop() {
  std::lock_guard <std::mutex> lock(mutex_);
  if (status_ != TaskStatus::RUNNING) {
    return;
  }
  UpdateStatusLocked(TaskStatus::STOPPING);
}

TaskStatus Task::GetStatus() const {
  std::lock_guard <std::mutex> lock(mutex_);
  return status_;
}

long Task::GetStartTime() const {
  std::lock_guard <std::mutex> lock(mutex_);
  return start_time_;
}

long Task::GetEndTime() const {
  std::lock_guard <std::mutex> lock(mutex_);
  return end_time_;
}

long Task::GetRunningTimeMicros() const {
  std::lock_guard <std::mutex> lock(mutex_);
  switch (status_) {
    case TaskStatus::NOT_STARTED:
      break;
    case TaskStatus::RUNNING:
    case TaskStatus::STOPPING:
      return SystemTimeMicros() - start_time_;
    case TaskStatus::STOPPED:
      return end_time_ - start_time_;
  }
  return 0;
}

void Task::WaitForEnd() {
  WaitFor(TaskStatus::STOPPED);
}

void Task::UpdateStatusLocked(TaskStatus status) {
  status_ = status;
  status_cond_.notify_all();
}

void Task::WaitFor(TaskStatus status) {
  std::unique_lock<std::mutex> lock(mutex_);
  status_cond_.wait(lock, [=]{
    return status_ == status;
  });
}

}  // namespace speedtest
