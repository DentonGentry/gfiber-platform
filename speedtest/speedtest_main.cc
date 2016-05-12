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

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include "curl_env.h"
#include "options.h"
#include "request.h"
#include "speedtest.h"
#include "utils.h"

namespace {

const char *kFileSerial = "/etc/serial";
const char *kFileVersion = "/etc/version";

std::string LoadFile(const std::string &file_name) {
  std::ifstream in(file_name);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

std::string GetDefaultUserAgent() {
  std::string serial = LoadFile(kFileSerial);
  std::string version = LoadFile(kFileVersion);
  speedtest::Trim(&serial);
  speedtest::Trim(&version);
  std::string user_agent_ = "CPE";
  if (!version.empty()) {
    user_agent_ += "/" + version;
    if (!serial.empty()) {
      user_agent_ += "/" + serial;
    }
  }
  return user_agent_;
}

}

int main(int argc, char *argv[]) {
  speedtest::Options options;
  if (!speedtest::ParseOptions(argc, argv, &options) || options.usage) {
    speedtest::PrintUsage(argv[0]);
    std::exit(1);
  }
  if (options.user_agent.empty()) {
    options.user_agent = GetDefaultUserAgent();
  }
  if (options.verbose) {
    speedtest::PrintOptions(options);
  }
  http::CurlEnv::Options curl_options;
  curl_options.disable_dns_cache = options.disable_dns_cache;
  curl_options.max_connections = options.max_connections;
  std::shared_ptr<http::CurlEnv> curl_env =
      http::CurlEnv::NewCurlEnv(curl_options);
  options.request_factory = [&](const http::Url &url) -> http::Request::Ptr {
    return curl_env->NewRequest(url);
  };
  speedtest::Speedtest speed(options);
  std::atomic_bool cancel(false);
  speed(&cancel);
  return 0;
}
