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

#ifndef SPEEDTEST_UPLOAD_TASK_H
#define SPEEDTEST_UPLOAD_TASK_H

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "transfer_task.h"

namespace speedtest {

class UploadTask : public TransferTask {
 public:
  struct Options : TransferTask::Options {
    std::shared_ptr<std::string> payload;
  };

  explicit UploadTask(const Options &options);

 protected:
  void RunInternal() override;
  void StopInternal() override;

 private:
  void RunUpload(int id);

  Options options_;
  std::vector<std::thread> threads_;

  // disallowed
  UploadTask(const UploadTask &) = delete;
  void operator=(const UploadTask &) = delete;
};

}  // namespace speedtest

#endif  // SPEEDTEST_UPLOAD_TASK_H
