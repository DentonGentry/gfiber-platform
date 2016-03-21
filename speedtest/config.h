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
#include "url.h"

namespace speedtest {

struct Config {
  int download_size = 0;
  int upload_size = 0;
  int interval_millis = 0;
  std::string location_name;
  int min_transfer_intervals = 0;
  int max_transfer_intervals = 0;
  int min_transfer_runtime = 0;
  int max_transfer_runtime = 0;
  double max_transfer_variance = 0;
  int num_downloads = 0;
  int num_uploads = 0;
  int ping_runtime = 0;
  int ping_timeout = 0;
  int transfer_port_start = 0;
  int transfer_port_end = 0;
};

// Parses a JSON document into a config struct.
// Returns true with the config struct populated on success.
// Returns false if the JSON is invalid or config is null.
bool ParseConfig(const std::string &json, Config *config);

// Parses a JSON document into a list of server URLs
// Returns true with the servers populated in the vector on success.
// Returns false if the JSON is invalid or servers is null.
bool ParseServers(const std::string &json, std::vector<http::Url> *servers);

void PrintConfig(const Config &config);
void PrintConfig(std::ostream &out, const Config &config);

}  // namespace speedtest

#endif //SPEEDTEST_CONFIG_H
