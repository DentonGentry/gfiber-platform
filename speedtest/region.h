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

#ifndef SPEEDTEST_REGION_H
#define SPEEDTEST_REGION_H

#include <string>
#include <vector>
#include "request.h"
#include "status.h"
#include "url.h"

namespace speedtest {

struct Region {
  std::string id;
  std::string name;
  std::vector<http::Url> urls;
};

struct RegionOptions {
  bool verbose;
  http::Request::Factory request_factory;
  bool global;
  http::Url global_url;
  std::vector<http::Url> regional_urls;
};

struct RegionResult {
  long start_time;
  long end_time;
  Status status;
  std::vector<Region> regions;
};

RegionResult LoadRegions(RegionOptions options);

std::string DescribeRegion(const Region &region);

// Parses a JSON document into a list of regions
// Returns true with the regions populated in the vector on success.
// Returns false if the JSON is invalid or regions is null.
Status ParseRegions(const std::string &json, std::vector<Region> *regions);

}  // namespace speedtest

#endif  // SPEEDTEST_REGION_H
