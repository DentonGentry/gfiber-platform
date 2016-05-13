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

#include "result.h"

#include "url.h"

namespace speedtest {
namespace {

template <typename T>
void PopulateDuration(Json::Value &json, const T &t) {
  json["startMillis"] = static_cast<Json::Value::Int64>(t.start_time);
  json["endMillis"] = static_cast<Json::Value::Int64>(t.end_time);
}

}  // namespace

void PopulateParameters(Json::Value &json, const Config &config) {
  json["downloadSize"] =
      static_cast<Json::Value::Int64>(config.download_bytes);
  json["uploadSize"] =
      static_cast<Json::Value::Int64>(config.upload_bytes);
  json["intervalSize"] =
      static_cast<Json::Value::Int64>(config.interval_millis);
  json["locationId"] = config.location_id;
  json["locationName"] = config.location_name;
  json["minTransferIntervals"] = config.min_transfer_intervals;
  json["maxTransferIntervals"] = config.max_transfer_intervals;
  json["minTransferRunTime"] =
      static_cast<Json::Value::Int64>(config.min_transfer_runtime);
  json["maxTransferRunTime"] =
      static_cast<Json::Value::Int64>(config.max_transfer_runtime);
  json["maxTransferVariance"] = config.max_transfer_variance;
  json["numConcurrentDownloads"] = config.num_downloads;
  json["numConcurrentUploads"] = config.num_uploads;
  json["pingRunTime"] =
      static_cast<Json::Value::Int64>(config.ping_runtime_millis);
  json["pingTimeout"] =
      static_cast<Json::Value::Int64>(config.ping_timeout_millis);
  json["transferPortStart"] = config.transfer_port_start;
  json["transferPortEnd"] = config.transfer_port_end;
  json["averageType"] = config.average_type;
}

void PopulateConfigResult(Json::Value &json,
                          const ConfigResult &config_result) {
  PopulateDuration(json, config_result);
  PopulateParameters(json["parameters"], config_result.config);
}

void PopulateFindNearest(Json::Value &json,
                         const FindNearest::Result &find_nearest) {
  PopulateDuration(json, find_nearest);
  json["pingResults"] = Json::Value(Json::arrayValue);
  for (const Ping::Result &ping_result : find_nearest.ping_results) {
    Json::Value ping;
    ping["id"] = ping_result.region.id;
    ping["url"] = ping_result.region.urls.front().url();
    if (ping_result.received > 0) {
      ping["minPingMillis"] =
          static_cast<Json::Value::Int64>(ping_result.min_ping_micros);
    }
    json["pingResults"].append(ping);
  }
}

void PopulateInitResult(Json::Value &json,
                        const Init::Result &init_result) {
  PopulateDuration(json, init_result);
  PopulateConfigResult(json["configResult"], init_result.config_result);
  if (!init_result.find_nearest_result.ping_results.empty()) {
    PopulateFindNearest(json["findNearest"], init_result.find_nearest_result);
  }
  json["selectedRegion"] = init_result.selected_region.id;
}

void PopulateTransfer(Json::Value &json,
                      const TransferResult &transfer_result) {
  PopulateDuration(json, transfer_result);
  json["speedMbps"] = transfer_result.speed_mbps;
  json["totalBytes"] =
      static_cast<Json::Value::Int64>(transfer_result.total_bytes);
  json["buckets"] = Json::Value(Json::arrayValue);
  for (const Bucket &bucket : transfer_result.buckets) {
    Json::Value bucket_json;
    bucket_json["totalBytes"] =
        static_cast<Json::Value::Int64>(bucket.total_bytes);
    bucket_json["longSpeedMbps"] = bucket.long_megabits;
    bucket_json["shortSpeedMbps"] = bucket.short_megabits;
    bucket_json["offsetMillis"] = bucket.start_time / 1000.0d;
    json["buckets"].append(bucket_json);
  }
}

void PopulatePingResult(Json::Value &json, const Ping::Result &ping_result) {
  PopulateDuration(json, ping_result);
  json["id"] = ping_result.region.id;
  json["url"] = ping_result.region.urls.front().url();
  if (ping_result.received > 0) {
    json["minPingMillis"] =
        static_cast<Json::Value::Int64>(ping_result.min_ping_micros);
  }
}

void PopulateSpeedtest(Json::Value &json,
                       const Speedtest::Result &speedtest_result) {
  PopulateDuration(json, speedtest_result);
  PopulateInitResult(json["initResult"], speedtest_result.init_result);
  if (speedtest_result.download_run) {
    PopulateTransfer(json["downloadResult"], speedtest_result.download_result);
  }
  if (speedtest_result.upload_run) {
    PopulateTransfer(json["uploadResult"], speedtest_result.upload_result);
  }
  if (speedtest_result.ping_run) {
    PopulatePingResult(json["pingResult"], speedtest_result.ping_result);
  }
  json["endState"] = "COMPLETE";
}

}  // namespace speedtest
