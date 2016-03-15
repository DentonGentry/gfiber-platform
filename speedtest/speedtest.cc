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

#include "speedtest.h"

#include <chrono>
#include <cstring>
#include <limits>
#include <random>
#include <thread>
#include <iomanip>
#include <fstream>
#include <streambuf>

#include "errors.h"
#include "runner.h"
#include "utils.h"

namespace speedtest {
namespace {

std::shared_ptr<std::string> MakeRandomData(size_t size) {
  std::random_device rd;
  std::default_random_engine random_engine(rd());
  std::uniform_int_distribution<char> uniform_dist(1, 255);
  auto random_data = std::make_shared<std::string>();
  random_data->resize(size);
  for (size_t i = 0; i < size; ++i) {
    (*random_data)[i] = uniform_dist(random_engine);
  }
  return std::move(random_data);
}

const char *kFileSerial = "/etc/serial";
const char *kFileVersion = "/etc/version";

std::string LoadFile(const std::string &file_name) {
  std::ifstream in(file_name);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

}  // namespace

Speedtest::Speedtest(const Options &options)
    : options_(options) {
  http::CurlEnv::Options curl_options;
  curl_options.disable_dns_cache = options_.disable_dns_cache;
  curl_options.max_connections = options_.max_connections;
  env_ = http::CurlEnv::NewCurlEnv(curl_options);
}

Speedtest::~Speedtest() {
}

void Speedtest::Run() {
  InitUserAgent();
  LoadServerList();
  if (servers_.empty()) {
    std::cerr << "No servers found in global server list\n";
    std::exit(1);
  }
  FindNearestServer();
  if (!server_url_) {
    std::cout << "No servers responded. Exiting\n";
    return;
  }
  std::string json = LoadConfig(*server_url_);
  if (!ParseConfig(json, &config_)) {
    std::cout << "Could not parse config\n";
    return;
  }
  if (options_.verbose) {
    std::cout << "Server config:\n";
    PrintConfig(config_);
  }
  std::cout << "Location: " << config_.location_name << "\n";
  std::cout << "URL: " << server_url_->url() << "\n";
  RunDownloadTest();
  RunUploadTest();
  RunPingTest();
}

void Speedtest::InitUserAgent() {
  if (options_.user_agent.empty()) {
    std::string serial = LoadFile(kFileSerial);
    std::string version = LoadFile(kFileVersion);
    Trim(&serial);
    Trim(&version);
    user_agent_ = std::string("CPE/") +
                  (version.empty() ? "unknown version" : version) + "/" +
                  (serial.empty() ? "unknown serial" : serial);
  } else {
    user_agent_ = options_.user_agent;
    return;
  }
  if (options_.verbose) {
    std::cout << "Setting user agent to " << user_agent_ << "\n";
  }
}

void Speedtest::LoadServerList() {
  servers_.clear();
  if (!options_.global) {
    if (options_.verbose) {
      std::cout << "Explicit server list:\n";
      for (const auto &url : options_.hosts) {
        std::cout << "  " << url.url() << "\n";
      }
    }
    servers_ = options_.hosts;
    return;
  }

  std::string json = LoadConfig(options_.global_host);
  if (json.empty()) {
    std::cerr << "Failed to load config JSON\n";
    std::exit(1);
  }
  if (options_.verbose) {
    std::cout << "Loaded config JSON: " << json << "\n";
  }
  if (!ParseServers(json, &servers_)) {
    std::cerr << "Failed to parse server list: " << json << "\n";
    std::exit(1);
  }
  if (options_.verbose) {
    std::cout << "Loaded servers:\n";
    for (const auto &url : servers_) {
      std::cout << "  " << url.url() << "\n";
    }
  }
}

void Speedtest::FindNearestServer() {
  server_url_.reset();
  if (servers_.size() == 1) {
    server_url_.reset(new http::Url(servers_[0]));
    if (options_.verbose) {
      std::cout << "Only 1 server so using " << server_url_->url() << "\n";
    }
    return;
  }

  PingTest::Options options;
  options.verbose = options_.verbose;
  options.timeout = PingTimeout();
  std::vector<http::Url> hosts;
  for (const auto &server : servers_) {
    http::Url url(server);
    url.set_path("/ping");
    hosts.emplace_back(url);
  }
  options.num_pings = hosts.size();
  if (options_.verbose) {
    std::cout << "There are " << hosts.size() << " ping URLs:\n";
    for (const auto &host : hosts) {
      std::cout << "  " << host.url() << "\n";
    }
  }
  options.request_factory = [&](int id) -> GenericTest::RequestPtr{
    return MakeRequest(hosts[id]);
  };
  PingTest find_nearest(options);
  if (options_.verbose) {
    std::cout << "Starting to find nearest server\n";
  }
  TimedRun(&find_nearest, 1500);
  find_nearest.WaitForEnd();
  if (find_nearest.IsSucceeded()) {
    PingStats fastest = find_nearest.GetFastest();
    server_url_.reset(new http::Url(fastest.url));
    server_url_->clear_path();
    if (options_.verbose) {
      double ping_millis = fastest.min_micros / 1000.0d;
      std::cout << "Found nearest server: " << fastest.url.url()
                   << " (" << round(ping_millis, 2) << " ms)\n";
    }
  }
}

std::string Speedtest::LoadConfig(const http::Url &url) {
  http::Url config_url(url);
  config_url.set_path("/config");
  if (options_.verbose) {
    std::cout << "Loading config from " << config_url.url() << "\n";
  }
  GenericTest::RequestPtr request = MakeRequest(config_url);
  request->set_url(config_url);
  std::string json;
  request->Get([&](void *data, size_t size){
    json.assign(static_cast<const char *>(data), size);
  });
  return json;
}

void Speedtest::RunPingTest() {
  PingTest::Options options;
  options.verbose = options_.verbose;
  options.timeout = PingTimeout();
  options.num_pings = 1;
  http::Url ping_url(*server_url_);
  ping_url.set_path("/ping");
  options.request_factory = [&](int id) -> GenericTest::RequestPtr{
    return MakeRequest(ping_url);
  };
  ping_test_.reset(new PingTest(options));
  TimedRun(ping_test_.get(), PingRunTime());
  ping_test_->WaitForEnd();
  PingStats fastest = ping_test_->GetFastest();
  if (ping_test_->IsSucceeded()) {
    long micros = fastest.min_micros;
    std::cout << "Ping time: " << round(micros / 1000.0d, 3) << " ms\n";
  } else {
    std::cout << "Failed to get ping response from "
              << config_.location_name << " (" << fastest.url << ")\n";
  }
}

void Speedtest::RunDownloadTest() {
  if (options_.verbose) {
    std::cout << "Starting download test at " << config_.location_name
              << " (" << server_url_->url() << ")\n";
  }
  DownloadTest::Options options;
  options.verbose = options_.verbose;
  options.num_transfers = NumDownloads();
  options.download_size = DownloadSize();
  options.request_factory = [this](int id) -> GenericTest::RequestPtr{
    return MakeTransferRequest(id, "/download");
  };
  download_test_.reset(new DownloadTest(options));
  TimedRun(download_test_.get(), 5000);
  download_test_->WaitForEnd();
  long bytes = download_test_->bytes_transferred();
  long micros = download_test_->GetRunningTime();
  if (options_.verbose) {
    std::cout << "Downloaded " << bytes << " bytes in "
              << round(micros / 1000.0d, 2) << " ms\n";
  }
  std::cout << "Download speed: "
            << round(ToMegabits(bytes, micros), 3) << " Mbps\n";
}

void Speedtest::RunUploadTest() {
  if (options_.verbose) {
    std::cout << "Starting upload test at " << config_.location_name
              << " (" << server_url_->url() << ")\n";
  }
  UploadTest::Options options;
  options.verbose = options_.verbose;
  options.num_transfers = NumUploads();
  options.payload = MakeRandomData(UploadSize());
  options.request_factory = [this](int id) -> GenericTest::RequestPtr{
    return MakeTransferRequest(id, "/upload");
  };
  upload_test_.reset(new UploadTest(options));
  TimedRun(upload_test_.get(), 5000);
  upload_test_->WaitForEnd();
  long bytes = upload_test_->bytes_transferred();
  long micros = upload_test_->GetRunningTime();
  if (options_.verbose) {
    std::cout << "Uploaded " << bytes << " bytes in "
              << round(micros / 1000.0d, 2) << " ms\n";
  }
  std::cout << "Upload speed: "
            << round(ToMegabits(bytes, micros), 3) << " Mbps\n";
}

int Speedtest::NumDownloads() const {
  return options_.num_downloads
         ? options_.num_downloads
         : config_.num_downloads;
}

int Speedtest::DownloadSize() const {
  return options_.download_size
         ? options_.download_size
         : config_.download_size;
}

int Speedtest::NumUploads() const {
  return options_.num_uploads
         ? options_.num_uploads
         : config_.num_uploads;
}

int Speedtest::UploadSize() const {
  return options_.upload_size
         ? options_.upload_size
         : config_.upload_size;
}

int Speedtest::PingRunTime() const {
  return options_.ping_runtime
         ? options_.ping_runtime
         : config_.ping_runtime;
}

int Speedtest::PingTimeout() const {
  return options_.ping_timeout
         ? options_.ping_timeout
         : config_.ping_timeout;
}

GenericTest::RequestPtr Speedtest::MakeRequest(const http::Url &url) {
  GenericTest::RequestPtr request = env_->NewRequest(url);
  if (!user_agent_.empty()) {
    request->set_user_agent(user_agent_);
  }
  return std::move(request);
}

GenericTest::RequestPtr Speedtest::MakeBaseRequest(
    int id, const std::string &path) {
  http::Url url(*server_url_);
  url.set_path(path);
  return MakeRequest(url);
}

GenericTest::RequestPtr Speedtest::MakeTransferRequest(
    int id, const std::string &path) {
  http::Url url(*server_url_);
  int port_start = config_.transfer_port_start;
  int port_end = config_.transfer_port_end;
  int num_ports = port_end - port_start + 1;
  if (num_ports > 0) {
    url.set_port(port_start + (id % num_ports));
  }
  url.set_path(path);
  return MakeRequest(url);
}

}  // namespace speedtest
