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

#include "config.h"

#include <curl/curl.h>
#include "errors.h"
#include "request.h"
#include "utils.h"

// For some reason, the libjsoncpp package installs to /usr/include/jsoncpp/json
// instead of /usr{,/local}/include/json
#include <jsoncpp/json/json.h>

namespace speedtest {

ConfigResult LoadConfig(ConfigOptions options) {
  ConfigResult result;
  result.start_time = SystemTimeMicros();
  if (!options.request_factory) {
    result.status = Status(StatusCode::INVALID_ARGUMENT,
                           "request factory not set");
    result.end_time = SystemTimeMicros();
    return result;
  }

  http::Url config_url(options.region_url);
  config_url.set_path("/fiber/config");
  if (options.verbose) {
    std::cout << "Loading config from " << config_url.url() << "\n";
  }
  http::Request::Ptr request = options.request_factory(config_url);
  request->set_url(config_url);
  request->set_timeout_millis(500);
  std::string json;
  CURLcode code = request->Get([&](void *data, size_t size){
    json.assign(static_cast<const char *>(data), size);
  });
  if (code != CURLE_OK) {
    result.status = Status(StatusCode::INTERNAL, http::ErrorString(code));
  } else {
    result.status = ParseConfig(json, &result.config);
  }
  result.end_time = SystemTimeMicros();
  return result;
}

Status ParseConfig(const std::string &json, Config *config) {
  if (!config) {
    return Status(StatusCode::FAILED_PRECONDITION, "Config is null");
  }

  Json::Reader reader;
  Json::Value root;
  if (!reader.parse(json, root, false)) {
    return Status(StatusCode::INVALID_ARGUMENT, "Failed to parse config JSON");
  }

  config->download_bytes = root["downloadSize"].asInt();
  config->upload_bytes = root["uploadSize"].asInt();
  config->interval_millis = root["intervalSize"].asInt();
  config->location_id = root["locationId"].asString();
  config->location_name = root["locationName"].asString();
  config->min_transfer_intervals = root["minTransferIntervals"].asInt();
  config->max_transfer_intervals = root["maxTransferIntervals"].asInt();
  config->min_transfer_runtime = root["minTransferRunTime"].asInt();
  config->max_transfer_runtime = root["maxTransferRunTime"].asInt();
  config->max_transfer_variance = root["maxTransferVariance"].asDouble();
  config->num_uploads = root["numConcurrentUploads"].asInt();
  config->num_downloads = root["numConcurrentDownloads"].asInt();
  config->ping_runtime_millis = root["pingRunTime"].asInt();
  config->ping_timeout_millis = root["pingTimeout"].asInt();
  config->transfer_port_start = root["transferPortStart"].asInt();
  config->transfer_port_end = root["transferPortEnd"].asInt();
  config->average_type = root["averageType"].asString();
  return Status::OK;
}

void PrintConfig(const Config &config) {
  PrintConfig(std::cout, config);
}

void PrintConfig(std::ostream &out, const Config &config) {
  out << "Download size: " << config.download_bytes << " bytes\n"
      << "Upload size: " << config.upload_bytes << " bytes\n"
      << "Interval size: " << config.interval_millis << " ms\n"
      << "Location ID: " << config.location_id << "\n"
      << "Location name: " << config.location_name << "\n"
      << "Min transfer intervals: " << config.min_transfer_intervals << "\n"
      << "Max transfer intervals: " << config.max_transfer_intervals << "\n"
      << "Min transfer runtime: " << config.min_transfer_runtime << " ms\n"
      << "Max transfer runtime: " << config.max_transfer_runtime << " ms\n"
      << "Max transfer variance: " << config.max_transfer_variance << "\n"
      << "Number of downloads: " << config.num_downloads << "\n"
      << "Number of uploads: " << config.num_uploads << "\n"
      << "Ping runtime: " << config.ping_runtime_millis << " ms\n"
      << "Ping timeout: " << config.ping_timeout_millis << " ms\n"
      << "Transfer port start: " << config.transfer_port_start << "\n"
      << "Transfer port end: " << config.transfer_port_end << "\n"
      << "Average type: " << config.average_type << "\n";
}

}  // namespace
