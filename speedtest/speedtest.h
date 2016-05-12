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

#ifndef SPEEDTEST_SPEEDTEST_H
#define SPEEDTEST_SPEEDTEST_H

#include <atomic>
#include <string>
#include "config.h"
#include "init.h"
#include "options.h"
#include "ping.h"
#include "region.h"
#include "request.h"
#include "status.h"
#include "transfer_runner.h"
#include "url.h"
#include "utils.h"

namespace speedtest {

class Speedtest {
 public:
  struct Result {
    long start_time;
    long end_time;
    Status status;
    Init::Result init_result;

    bool download_run;
    TransferResult download_result;

    bool upload_run;
    TransferResult upload_result;

    bool ping_run;
    Ping::Result ping_result;
  };

  explicit Speedtest(const Options &options);

  Result operator()(std::atomic_bool *cancel);

 private:
  TransferResult RunDownloadTest(std::atomic_bool *cancel);
  TransferResult RunUploadTest(std::atomic_bool *cancel);
  Ping::Result RunPingTest(std::atomic_bool *cancel);

  int GetNumDownloads() const;
  long GetDownloadSizeBytes() const;
  int GetNumUploads() const;
  long GetUploadSizeBytes() const;
  long GetPingTimeoutMillis() const;
  long GetPingRunTimeMillis() const;
  long GetMinTransferRunTimeMillis() const;
  long GetMaxTransferRunTimeMillis() const;
  int GetMinTransferIntervals() const;
  int GetMaxTransferIntervals() const;
  double GetMaxTransferVariance() const;
  long GetIntervalMillis() const;

  http::Request::Ptr MakeRequest(const http::Url &url) const;
  http::Request::Ptr MakeBaseRequest(int id, const std::string &path) const;
  http::Request::Ptr MakeTransferRequest(int id, const std::string &path) const;

  Options options_;
  Config config_;
  Region selected_region_;

  DISALLOW_COPY_AND_ASSIGN(Speedtest);
};

}  // namespace speedtest

#endif  // SPEEDTEST_SPEEDTEST_H
