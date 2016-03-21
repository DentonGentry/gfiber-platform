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

#ifndef SPEEDTEST_OPTIONS_H
#define SPEEDTEST_OPTIONS_H

#include <iostream>
#include <string>
#include <vector>
#include "url.h"

namespace speedtest {

extern const char* kDefaultHost;

struct Options {
  bool usage = false;
  bool verbose = false;
  http::Url global_host;
  bool global = false;
  std::string user_agent;
  bool disable_dns_cache = false;
  int max_connections = 0;
  int progress_millis = 0;
  bool exponential_moving_average = false;

  // A value of 0 means use the speedtest config parameters
  int num_downloads = 0;
  long download_size = 0;
  int num_uploads = 0;
  long upload_size = 0;
  int min_transfer_runtime = 0;
  int max_transfer_runtime = 0;
  int min_transfer_intervals = 0;
  int max_transfer_intervals = 0;
  double max_transfer_variance = 0.0;
  int interval_millis = 0;
  int ping_runtime = 0;
  int ping_timeout = 0;

  std::vector<http::Url> hosts;
};

// Parse command line options putting results into 'options'
// Caller retains ownership of 'options'.
// Returns true on success, false on failure.
// Not threadsafe
bool ParseOptions(int argc, char *argv[], struct Options *options);

void PrintOptions(const Options &options);
void PrintOptions(std::ostream &out, const Options &options);
void PrintUsage(const char *app_path);
void PrintUsage(std::ostream &out, const char *app_name);

}  // namespace speedtest

#endif  // SPEEDTEST_OPTIONS_H
