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

#include "config.h"

// For some reason, the libjsoncpp package installs to /usr/include/jsoncpp/json
// instead of /usr{,/local}/include/json
#include <jsoncpp/json/json.h>

namespace speedtest {

bool ParseConfig(const std::string &json, Config *config) {
  if (!config) {
    return false;
  }

  Json::Reader reader;
  Json::Value root;
  if (!reader.parse(json, root, false)) {
    return false;
  }

  config->download_size = root["downloadSize"].asInt();
  config->upload_size = root["uploadSize"].asInt();
  config->interval_size = root["intervalSize"].asInt();
  config->location_name = root["locationName"].asString();
  config->min_transfer_intervals = root["minTransferIntervals"].asInt();
  config->max_transfer_intervals = root["maxTransferIntervals"].asInt();
  config->min_transfer_runtime = root["minTransferRunTime"].asInt();
  config->max_transfer_runtime = root["maxTransferRunTime"].asInt();
  config->max_transfer_variance = root["maxTransferVariance"].asDouble();
  config->num_uploads = root["numConcurrentUploads"].asInt();
  config->num_downloads = root["numConcurrentDownloads"].asInt();
  config->ping_runtime = root["pingRunTime"].asInt();
  config->ping_timeout = root["pingTimeout"].asInt();
  config->transfer_port_start = root["transferPortStart"].asInt();
  config->transfer_port_end = root["transferPortEnd"].asInt();
  return true;
}

bool ParseServers(const std::string &json, std::vector<http::Url> *servers) {
  if (!servers) {
    return false;
  }

  Json::Reader reader;
  Json::Value root;
  if (!reader.parse(json, root, false)) {
    return false;
  }

  for (const auto &it : root["regionalServers"]) {
    http::Url url(it.asString());
    if (!url.ok()) {
      return false;
    }
    servers->emplace_back(url);
  }
  return true;
}

void PrintConfig(const Config &config) {
  PrintConfig(std::cout, config);
}

void PrintConfig(std::ostream &out, const Config &config) {
  out << "Download size: " << config.download_size << " bytes\n"
      << "Upload size: " << config.upload_size << " bytes\n"
      << "Interval size: " << config.interval_size << " ms\n"
      << "Location name: " << config.location_name << "\n"
      << "Min transfer intervals: " << config.min_transfer_intervals << "\n"
      << "Max transfer intervals: " << config.max_transfer_intervals << "\n"
      << "Min transfer runtime: " << config.min_transfer_runtime << " ms\n"
      << "Max transfer runtime: " << config.max_transfer_runtime << " ms\n"
      << "Max transfer variance: " << config.max_transfer_variance << "\n"
      << "Number of downloads: " << config.num_downloads << "\n"
      << "Number of uploads: " << config.num_uploads << "\n"
      << "Ping runtime: " << config.ping_runtime << " ms\n"
      << "Ping timeout: " << config.ping_timeout << " ms\n"
      << "Transfer port start: " << config.transfer_port_start << "\n"
      << "Transfer port end: " << config.transfer_port_end << "\n";
}

}  // namespace
