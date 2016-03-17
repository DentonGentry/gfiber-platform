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

#ifndef SPEEDTEST_SPEEDTEST_H
#define SPEEDTEST_SPEEDTEST_H

#include <atomic>
#include <memory>
#include <string>

#include "curl_env.h"
#include "options.h"
#include "url.h"
#include "request.h"

namespace speedtest {

class Speedtest {
 public:
  explicit Speedtest(const Options &options);
  virtual ~Speedtest();

  void Run();
  void RunDownloadTest();
  void RunUploadTest();
  bool RunPingTest();

 private:
  void RunDownload(int id);
  void RunUpload(int id);
  void RunPing(size_t host_index);

  std::unique_ptr<http::Request> MakeRequest(int id, const std::string &path);

  std::shared_ptr<http::CurlEnv> env_;
  Options options_;
  http::Url url_;
  std::atomic_bool end_ping_;
  std::atomic_bool end_download_;
  std::atomic_bool end_upload_;
  std::atomic_long bytes_downloaded_;
  std::atomic_long bytes_uploaded_;
  std::vector<long> min_ping_micros_;
  const char *send_data_;

  // disable
  Speedtest(const Speedtest &) = delete;
  void operator=(const Speedtest &) = delete;
};

}  // namespace speedtest

#endif  // SPEEDTEST_SPEEDTEST_H
