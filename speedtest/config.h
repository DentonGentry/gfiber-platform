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

#ifndef SPEEDTEST_CONFIG_H
#define SPEEDTEST_CONFIG_H

#include <iostream>
#include <string>
#include <vector>
#include "region.h"
#include "request.h"
#include "status.h"
#include "url.h"

namespace speedtest {

struct Config {
  long download_bytes = 0;
  long upload_bytes = 0;
  long interval_millis = 0;
  std::string location_id;
  std::string location_name;
  int min_transfer_intervals = 0;
  int max_transfer_intervals = 0;
  long min_transfer_runtime = 0;
  long max_transfer_runtime = 0;
  double max_transfer_variance = 0;
  int num_downloads = 0;
  int num_uploads = 0;
  long ping_runtime_millis = 0;
  long ping_timeout_millis = 0;
  int transfer_port_start = 0;
  int transfer_port_end = 0;
  std::string average_type;
};

struct ConfigOptions {
  bool verbose;
  http::Request::Factory request_factory;
  http::Url region_url;
};

struct ConfigResult {
  long start_time;
  long end_time;
  Status status;
  Config config;
};

ConfigResult LoadConfig(ConfigOptions options);

// Parses a JSON document into a config struct.
// Returns true with the config struct populated on success.
// Returns false if the JSON is invalid or config is null.
Status ParseConfig(const std::string &json, Config *config);

void PrintConfig(const Config &config);
void PrintConfig(std::ostream &out, const Config &config);

}  // namespace speedtest

#endif // SPEEDTEST_CONFIG_H
