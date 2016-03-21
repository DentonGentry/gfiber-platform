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
#include "timed_runner.h"
#include "transfer_runner.h"
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
    user_agent_ = "CPE";
    if (!version.empty()) {
      user_agent_ += "/" + version;
      if (!serial.empty()) {
        user_agent_ += "/" + serial;
      }
    }
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

  PingTask::Options options;
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
  options.request_factory = [&](int id) -> http::Request::Ptr{
    return MakeRequest(hosts[id]);
  };
  PingTask find_nearest(options);
  if (options_.verbose) {
    std::cout << "Starting to find nearest server\n";
  }
  RunTimed(&find_nearest, 1500);
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
  http::Request::Ptr request = MakeRequest(config_url);
  request->set_url(config_url);
  std::string json;
  request->Get([&](void *data, size_t size){
    json.assign(static_cast<const char *>(data), size);
  });
  return json;
}

void Speedtest::RunPingTest() {
  PingTask::Options options;
  options.verbose = options_.verbose;
  options.timeout = PingTimeout();
  options.num_pings = 1;
  http::Url ping_url(*server_url_);
  ping_url.set_path("/ping");
  options.request_factory = [&](int id) -> http::Request::Ptr{
    return MakeRequest(ping_url);
  };
  std::unique_ptr<PingTask> ping(new PingTask(options));
  RunTimed(ping.get(), PingRunTime());
  ping->WaitForEnd();
  PingStats fastest = ping->GetFastest();
  if (ping->IsSucceeded()) {
    long micros = fastest.min_micros;
    std::cout << "Ping time: " << round(micros / 1000.0d, 3) << " ms\n";
  } else {
    std::cout << "Failed to get ping response from "
              << config_.location_name << " (" << fastest.url << ")\n";
  }
}

void Speedtest::RunDownloadTest() {
  if (options_.verbose) {
    std::cout << "Starting download test to " << config_.location_name
              << " (" << server_url_->url() << ")\n";
  }
  DownloadTask::Options download_options;
  download_options.verbose = options_.verbose;
  download_options.num_transfers = NumDownloads();
  download_options.download_size = DownloadSize();
  download_options.request_factory = [this](int id) -> http::Request::Ptr{
    return MakeTransferRequest(id, "/download");
  };
  std::unique_ptr<DownloadTask> download(new DownloadTask(download_options));
  TransferRunner::Options runner_options;
  runner_options.verbose = options_.verbose;
  runner_options.task = download.get();
  runner_options.min_runtime = MinTransferRuntime();
  runner_options.max_runtime = MaxTransferRuntime();
  runner_options.min_intervals = MinTransferIntervals();
  runner_options.max_intervals = MaxTransferIntervals();
  runner_options.max_variance = MaxTransferVariance();
  runner_options.interval_millis = IntervalMillis();
  if (options_.progress_millis > 0) {
    runner_options.progress_millis = options_.progress_millis;
    runner_options.progress_fn = [](Interval interval) {
      double speed_variance = variance(interval.short_megabits,
                                       interval.long_megabits);
      std::cout << "[+" << round(interval.running_time / 1000.0, 0) << " ms] "
                << "Download speed: " << round(interval.short_megabits, 2)
                << " - " << round(interval.long_megabits, 2)
                << " Mbps (" << interval.bytes << " bytes, variance "
                << round(speed_variance, 4) << ")\n";
    };
  }
  TransferRunner runner(runner_options);
  runner.Run();
  runner.WaitForEnd();
  if (options_.verbose) {
    long running_time = download->GetRunningTimeMicros();
    std::cout << "Downloaded " << download->bytes_transferred()
              << " bytes in " << round(running_time / 1000.0, 0) << " ms\n";
  }
  std::cout << "Download speed: "
            << round(runner.GetSpeedInMegabits(), 2) << " Mbps\n";
}

void Speedtest::RunUploadTest() {
  if (options_.verbose) {
    std::cout << "Starting upload test to " << config_.location_name
              << " (" << server_url_->url() << ")\n";
  }
  UploadTask::Options upload_options;
  upload_options.verbose = options_.verbose;
  upload_options.num_transfers = NumUploads();
  upload_options.payload = MakeRandomData(UploadSize());
  upload_options.request_factory = [this](int id) -> http::Request::Ptr{
    return MakeTransferRequest(id, "/upload");
  };

  std::unique_ptr<UploadTask> upload(new UploadTask(upload_options));
  TransferRunner::Options runner_options;
  runner_options.verbose = options_.verbose;
  runner_options.task = upload.get();
  runner_options.min_runtime = MinTransferRuntime();
  runner_options.max_runtime = MaxTransferRuntime();
  runner_options.min_intervals = MinTransferIntervals();
  runner_options.max_intervals = MaxTransferIntervals();
  runner_options.max_variance = MaxTransferVariance();
  runner_options.interval_millis = IntervalMillis();
  if (options_.progress_millis > 0) {
    runner_options.progress_millis = options_.progress_millis;
    runner_options.progress_fn = [](Interval interval) {
      double speed_variance = variance(interval.short_megabits,
                                       interval.long_megabits);
      std::cout << "[+" << round(interval.running_time / 1000.0, 0) << " ms] "
                << "Upload speed: " << round(interval.short_megabits, 2)
                << " - " << round(interval.long_megabits, 2)
                << " Mbps (" << interval.bytes << " bytes, variance "
                << round(speed_variance, 4) << ")\n";
    };
  }
  TransferRunner runner(runner_options);
  runner.Run();
  runner.WaitForEnd();
  if (options_.verbose) {
    long running_time = upload->GetRunningTimeMicros();
    std::cout << "Uploaded " << upload->bytes_transferred()
              << " bytes in " << round(running_time / 1000.0, 0) << " ms\n";
  }
  std::cout << "Upload speed: "
            << round(runner.GetSpeedInMegabits(), 2) << " Mbps\n";
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

int Speedtest::MinTransferRuntime() const {
  return options_.min_transfer_runtime
         ? options_.min_transfer_runtime
         : config_.min_transfer_runtime;
}

int Speedtest::MaxTransferRuntime() const {
  return options_.max_transfer_runtime
         ? options_.max_transfer_runtime
         : config_.max_transfer_runtime;
}

int Speedtest::MinTransferIntervals() const {
  return options_.min_transfer_intervals
         ? options_.min_transfer_intervals
         : config_.min_transfer_intervals;
}

int Speedtest::MaxTransferIntervals() const {
  return options_.max_transfer_intervals
         ? options_.max_transfer_intervals
         : config_.max_transfer_intervals;
}

double Speedtest::MaxTransferVariance() const {
  return options_.max_transfer_variance
         ? options_.max_transfer_variance
         : config_.max_transfer_variance;
}

int Speedtest::IntervalMillis() const {
  return options_.interval_millis
         ? options_.interval_millis
         : config_.interval_millis;
}

http::Request::Ptr Speedtest::MakeRequest(const http::Url &url) {
  http::Request::Ptr request = env_->NewRequest(url);
  if (!user_agent_.empty()) {
    request->set_user_agent(user_agent_);
  }
  return std::move(request);
}

http::Request::Ptr Speedtest::MakeBaseRequest(
    int id, const std::string &path) {
  http::Url url(*server_url_);
  url.set_path(path);
  return MakeRequest(url);
}

http::Request::Ptr Speedtest::MakeTransferRequest(
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
