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

#include "download.h"
#include "timed_runner.h"
#include "upload.h"

namespace speedtest {

Speedtest::Speedtest(const Options &options): options_(options) {
}

Speedtest::Result Speedtest::operator()(std::atomic_bool *cancel) {
  Speedtest::Result result;
  result.start_time = SystemTimeMicros();
  result.download_run = false;
  result.upload_run = false;
  result.ping_run = false;

  if (*cancel) {
    result.status = Status(StatusCode::ABORTED, "Speedtest aborted");
    result.end_time = SystemTimeMicros();
    return result;
  }

  Init::Options init_options;
  init_options.verbose = options_.verbose;
  init_options.request_factory = options_.request_factory;
  init_options.global = options_.global;
  init_options.global_url = options_.global_url;
  init_options.ping_timeout_millis = options_.ping_timeout_millis;
  init_options.regional_urls = options_.regional_urls;
  Init init(init_options);
  result.init_result = init(cancel);
  if (!result.init_result.status.ok()) {
    result.status = result.init_result.status;
    result.end_time = SystemTimeMicros();
    return result;
  }
  selected_region_ = result.init_result.selected_region;
  if (options_.verbose) {
    std::cout << "Setting selected region to "
              << DescribeRegion(selected_region_) << "\n";
  }

  if (*cancel) {
    result.status = Status(StatusCode::ABORTED, "Speedtest aborted");
    result.end_time = SystemTimeMicros();
    return result;
  }

  config_ = result.init_result.config_result.config;

  std::cout << "ID: " << result.init_result.selected_region.id << "\n";
  std::cout << "Location: " << result.init_result.selected_region.name << "\n";

  if (options_.skip_download) {
    std::cout << "Skipping download test\n";
  } else {
    result.download_result = RunDownloadTest(cancel);
    if (!result.download_result.status.ok()) {
      result.status = result.download_result.status;
      result.end_time = SystemTimeMicros();
      return result;
    }
    result.download_run = true;
    std::cout << "Download speed: "
              << round(result.download_result.speed_mbps, 2)
              << " Mbps\n";
  }

  if (*cancel) {
    result.status = Status(StatusCode::ABORTED, "Speedtest aborted");
    result.end_time = SystemTimeMicros();
    return result;
  }

  if (options_.skip_upload) {
    std::cout << "Skipping upload test\n";
  } else {
    result.upload_result = RunUploadTest(cancel);
    if (!result.upload_result.status.ok()) {
      result.status = result.upload_result.status;
      result.end_time = SystemTimeMicros();
      return result;
    }
    result.upload_run = true;
    std::cout << "Upload speed: "
              << round(result.upload_result.speed_mbps, 2)
              << " Mbps\n";
  }

  if (*cancel) {
    result.status = Status(StatusCode::ABORTED, "Speedtest aborted");
    result.end_time = SystemTimeMicros();
    return result;
  }

  if (options_.skip_ping) {
    std::cout << "Skipping ping test\n";
  } else {
    result.ping_result = RunPingTest(cancel);
    if (!result.ping_result.status.ok()) {
      result.status = result.ping_result.status;
      result.end_time = SystemTimeMicros();
      return result;
    }
    result.ping_run = true;
    std::cout << "Ping time: "
              << ToMillis(result.ping_result.min_ping_micros)
              << " ms\n";
  }
  
  result.status = Status::OK;
  result.end_time = SystemTimeMicros();
  return result;
}

TransferResult Speedtest::RunDownloadTest(std::atomic_bool *cancel) {
  if (options_.verbose) {
    std::cout << "Starting download test to "
              << DescribeRegion(selected_region_) << ")\n";
  }
  Download::Options download_options;
  download_options.verbose = options_.verbose;
  download_options.num_transfers = GetNumDownloads();
  download_options.download_bytes = GetDownloadSizeBytes();
  download_options.request_factory = [this](int id) -> http::Request::Ptr{
    return MakeTransferRequest(id, "/download");
  };
  Download download(download_options);

  TransferOptions transfer_options;
  transfer_options.verbose = options_.verbose;
  transfer_options.min_runtime_millis = GetMinTransferRunTimeMillis();
  transfer_options.max_runtime_millis = GetMaxTransferRunTimeMillis();
  transfer_options.min_intervals = GetMinTransferIntervals();
  transfer_options.max_intervals = GetMaxTransferIntervals();
  transfer_options.max_variance = GetMaxTransferVariance();
  transfer_options.interval_millis = GetIntervalMillis();
  if (options_.progress_millis > 0) {
    transfer_options.progress_millis = options_.progress_millis;
    transfer_options.progress_fn = [](Bucket bucket) {
      double speed_variance = variance(bucket.short_megabits,
                                       bucket.long_megabits);
      std::cout << "[+" << round(bucket.start_time / 1000.0, 0) << " ms] "
                << "Download speed: " << round(bucket.short_megabits, 2)
                << " - " << round(bucket.long_megabits, 2)
                << " Mbps (" << bucket.total_bytes << " bytes, variance "
                << round(speed_variance, 4) << ")\n";
    };
  }
  return RunTransfer(std::ref(download), cancel, transfer_options);
}

TransferResult Speedtest::RunUploadTest(std::atomic_bool *cancel) {
  if (options_.verbose) {
    std::cout << "Starting upload test to "
              << DescribeRegion(selected_region_) << ")\n";
  }
  Upload::Options upload_options;
  upload_options.verbose = options_.verbose;
  upload_options.num_transfers = GetNumUploads();
  upload_options.payload = MakeRandomData(GetUploadSizeBytes());
  upload_options.request_factory = [this](int id) -> http::Request::Ptr{
    return MakeTransferRequest(id, "/upload");
  };
  Upload upload(upload_options);

  TransferOptions transfer_options;
  transfer_options.verbose = options_.verbose;
  transfer_options.min_runtime_millis = GetMinTransferRunTimeMillis();
  transfer_options.max_runtime_millis = GetMaxTransferRunTimeMillis();
  transfer_options.min_intervals = GetMinTransferIntervals();
  transfer_options.max_intervals = GetMaxTransferIntervals();
  transfer_options.max_variance = GetMaxTransferVariance();
  transfer_options.interval_millis = GetIntervalMillis();
  if (options_.progress_millis > 0) {
    transfer_options.progress_millis = options_.progress_millis;
    transfer_options.progress_fn = [](Bucket bucket) {
      double speed_variance = variance(bucket.short_megabits,
                                       bucket.long_megabits);
      std::cout << "[+" << round(bucket.start_time / 1000.0, 0) << " ms] "
                << "Upload speed: " << round(bucket.short_megabits, 2)
                << " - " << round(bucket.long_megabits, 2)
                << " Mbps (" << bucket.total_bytes << " bytes, variance "
                << round(speed_variance, 4) << ")\n";
    };
  }
  return RunTransfer(std::ref(upload), cancel, transfer_options);
}

Ping::Result Speedtest::RunPingTest(std::atomic_bool *cancel) {
  Ping::Options ping_options;
  ping_options.verbose = options_.verbose;
  ping_options.timeout_millis = GetPingTimeoutMillis();
  ping_options.region = selected_region_;
  ping_options.num_concurrent_pings = 0;
  ping_options.request_factory = [&](const http::Url &url){
    return MakeRequest(url);
  };
  Ping ping(ping_options);
  return RunTimed(std::ref(ping), cancel, GetPingRunTimeMillis());
}

int Speedtest::GetNumDownloads() const {
  return options_.num_downloads
         ? options_.num_downloads
         : config_.num_downloads;
}

long Speedtest::GetDownloadSizeBytes() const {
  return options_.download_bytes
         ? options_.download_bytes
         : config_.download_bytes;
}

int Speedtest::GetNumUploads() const {
  return options_.num_uploads
         ? options_.num_uploads
         : config_.num_uploads;
}

long Speedtest::GetUploadSizeBytes() const {
  return options_.upload_bytes
         ? options_.upload_bytes
         : config_.upload_bytes;
}

long Speedtest::GetPingRunTimeMillis() const {
  return options_.ping_runtime_millis
         ? options_.ping_runtime_millis
         : config_.ping_runtime_millis;
}

long Speedtest::GetPingTimeoutMillis() const {
  return options_.ping_timeout_millis
         ? options_.ping_timeout_millis
         : config_.ping_timeout_millis;
}

long Speedtest::GetMinTransferRunTimeMillis() const {
  return options_.min_transfer_runtime
         ? options_.min_transfer_runtime
         : config_.min_transfer_runtime;
}

long Speedtest::GetMaxTransferRunTimeMillis() const {
  return options_.max_transfer_runtime
         ? options_.max_transfer_runtime
         : config_.max_transfer_runtime;
}

int Speedtest::GetMinTransferIntervals() const {
  return options_.min_transfer_intervals
         ? options_.min_transfer_intervals
         : config_.min_transfer_intervals;
}

int Speedtest::GetMaxTransferIntervals() const {
  return options_.max_transfer_intervals
         ? options_.max_transfer_intervals
         : config_.max_transfer_intervals;
}

double Speedtest::GetMaxTransferVariance() const {
  return options_.max_transfer_variance
         ? options_.max_transfer_variance
         : config_.max_transfer_variance;
}

long Speedtest::GetIntervalMillis() const {
  return options_.interval_millis
         ? options_.interval_millis
         : config_.interval_millis;
}

http::Request::Ptr Speedtest::MakeRequest(const http::Url &url) const {
  http::Request::Ptr request = options_.request_factory(url);
  if (!options_.user_agent.empty()) {
    request->set_user_agent(options_.user_agent);
  }
  return std::move(request);
}

http::Request::Ptr Speedtest::MakeBaseRequest(
    int id, const std::string &path) const {
  http::Url url(selected_region_.urls.front());
  url.set_path(path);
  return MakeRequest(url);
}

http::Request::Ptr Speedtest::MakeTransferRequest(
    int id, const std::string &path) const {
  http::Url url(selected_region_.urls.front().url());
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
