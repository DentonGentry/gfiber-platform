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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include "region.h"

#define EXPECT_OK(statement) EXPECT_EQ(::speedtest::Status::OK, (statement))
#define EXPECT_ERROR(statement) EXPECT_NE(::speedtest::Status::OK, (statement))

namespace speedtest {
namespace {

const char *kValidConfig = R"CONFIG(
{
    "downloadSize": 10000000,
    "intervalSize": 200,
    "locationId": "mci",
    "locationName": "Kansas City",
    "maxTransferIntervals": 25,
    "maxTransferRunTime": 20000,
    "maxTransferVariance": 0.08,
    "minTransferIntervals": 10,
    "minTransferRunTime": 5000,
    "numConcurrentDownloads": 20,
    "numConcurrentUploads": 15,
    "pingRunTime": 3000,
    "pingTimeout": 300,
    "transferPortEnd": 3023,
    "transferPortStart": 3004,
    "uploadSize": 20000000
}
)CONFIG";

const char *kInvalidJson = "{{}{";

TEST(ParseConfigTest, NullConfig_Invalid) {
  EXPECT_ERROR(ParseConfig(kValidConfig, nullptr));
}

TEST(ParseConfigTest, EmptyJson_Invalid) {
  Config config;
  EXPECT_ERROR(ParseConfig("", &config));
}

TEST(ParseConfigTest, InvalidJson_Invalid) {
  Config config;
  EXPECT_ERROR(ParseConfig(kInvalidJson, &config));
}

TEST(ParseConfigTest, FullConfig_Valid) {
  Config config;
  EXPECT_OK(ParseConfig(kValidConfig, &config));
  EXPECT_EQ(10000000, config.download_bytes);
  EXPECT_EQ(20000000, config.upload_bytes);
  EXPECT_EQ(20, config.num_downloads);
  EXPECT_EQ(15, config.num_uploads);
  EXPECT_EQ(200, config.interval_millis);
  EXPECT_EQ("mci", config.location_id);
  EXPECT_EQ("Kansas City", config.location_name);
  EXPECT_EQ(10, config.min_transfer_intervals);
  EXPECT_EQ(25, config.max_transfer_intervals);
  EXPECT_EQ(5000, config.min_transfer_runtime);
  EXPECT_EQ(20000, config.max_transfer_runtime);
  EXPECT_EQ(0.08, config.max_transfer_variance);
  EXPECT_EQ(3000, config.ping_runtime_millis);
  EXPECT_EQ(300, config.ping_timeout_millis);
  EXPECT_EQ(3004, config.transfer_port_start);
  EXPECT_EQ(3023, config.transfer_port_end);
}

}  // namespace
}  // namespace speedtest
