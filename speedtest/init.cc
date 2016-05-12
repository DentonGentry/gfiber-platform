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

#include "init.h"

#include "timed_runner.h"

namespace speedtest {

Init::Init(const Options &options)
    : options_(options) {
}

Init::Result Init::operator()(std::atomic_bool *cancel) {
  Init::Result result;
  result.start_time = SystemTimeMicros();

  if (!cancel) {
    result.status = Status(StatusCode::FAILED_PRECONDITION, "cancel is null");
    result.end_time = SystemTimeMicros();
    return result;
  }

  if (*cancel) {
    result.status = Status(StatusCode::ABORTED, "init aborted");
    result.end_time = SystemTimeMicros();
    return result;
  }

  RegionOptions region_options;
  region_options.verbose = options_.verbose;
  region_options.request_factory = options_.request_factory;
  region_options.global = options_.global;
  region_options.global_url = options_.global_url;
  region_options.regional_urls = options_.regional_urls;
  result.region_result = LoadRegions(region_options);
  if (!result.region_result.status.ok()) {
    result.status = result.region_result.status;
    result.end_time = SystemTimeMicros();
    if (options_.verbose) {
      std::cout << "Load regions failed: " << result.status.ToString() << "\n";
    }
    return result;
  }
  if (options_.verbose) {
    std::cout << "Load regions succeeded:\n";
    for (const Region &region : result.region_result.regions) {
      std::cout << "  " << DescribeRegion(region) << "\n";
    }
  }

  if (*cancel) {
    result.status = Status(StatusCode::ABORTED, "init aborted");
    result.end_time = SystemTimeMicros();
    return result;
  }

  FindNearest::Options find_options;
  find_options.verbose = options_.verbose;
  find_options.request_factory = options_.request_factory;
  find_options.ping_timeout_millis = options_.ping_timeout_millis;
  find_options.regions = result.region_result.regions;
  FindNearest find_nearest(find_options);
  result.find_nearest_result = RunTimed(std::ref(find_nearest), cancel, 2000);
  if (!result.find_nearest_result.status.ok()) {
    result.status = result.find_nearest_result.status;
    result.end_time = SystemTimeMicros();
    if (options_.verbose) {
      std::cout << "Find nearest failed: " << result.status.ToString() << "\n";
    }
    return result;
  }
  result.selected_region = result.find_nearest_result.selected_region;

  if (*cancel) {
    result.status = Status(StatusCode::ABORTED, "init aborted");
    result.end_time = SystemTimeMicros();
    return result;
  }

  ConfigOptions config_options;
  config_options.verbose = options_.verbose;
  config_options.request_factory = options_.request_factory;
  config_options.region_url = result.selected_region.urls.front();
  result.config_result = LoadConfig(config_options);
  if (!result.config_result.status.ok()) {
    result.status = result.config_result.status;
    if (options_.verbose) {
      std::cout << "Load config failed: " << result.status.ToString() << "\n";
    }
  } else {
    if (options_.verbose) {
      PrintConfig(result.config_result.config);
    }
    result.status = Status::OK;
    if (result.selected_region.id.empty()) {
      result.selected_region.id = result.config_result.config.location_id;
    }
    if (result.selected_region.name.empty()) {
      result.selected_region.name = result.config_result.config.location_name;
    }
  }

  result.end_time = SystemTimeMicros();
  return result;
}

}  // namespace speedtest
