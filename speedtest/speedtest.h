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

#include "config.h"
#include "curl_env.h"
#include "download_test.h"
#include "generic_test.h"
#include "options.h"
#include "ping_test.h"
#include "upload_test.h"
#include "url.h"
#include "request.h"

namespace speedtest {

class Speedtest {
 public:
  explicit Speedtest(const Options &options);
  virtual ~Speedtest();

  void Run();

 private:
  void InitUserAgent();
  void LoadServerList();
  void FindNearestServer();
  std::string LoadConfig(const http::Url &url);
  void RunPingTest();
  void RunDownloadTest();
  void RunUploadTest();

  int NumDownloads() const;
  int DownloadSize() const;
  int NumUploads() const;
  int UploadSize() const;
  int PingTimeout() const;
  int PingRunTime() const;

  GenericTest::RequestPtr MakeRequest(const http::Url &url);
  GenericTest::RequestPtr MakeBaseRequest(int id, const std::string &path);
  GenericTest::RequestPtr MakeTransferRequest(int id, const std::string &path);

  std::shared_ptr <http::CurlEnv> env_;
  Options options_;
  Config config_;
  std::string user_agent_;
  std::vector<http::Url> servers_;
  std::unique_ptr<http::Url> server_url_;
  std::unique_ptr<std::string> send_data_;
  std::unique_ptr<PingTest> ping_test_;
  std::unique_ptr<DownloadTest> download_test_;
  std::unique_ptr<UploadTest> upload_test_;

  // disable
  Speedtest(const Speedtest &) = delete;
  void operator=(const Speedtest &) = delete;
};

}  // namespace speedtest

#endif  // SPEEDTEST_SPEEDTEST_H
