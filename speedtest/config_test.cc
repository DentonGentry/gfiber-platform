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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace speedtest {
namespace {

const char *kValidConfig = R"CONFIG(
{
    "downloadSize": 10000000,
    "intervalSize": 200,
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

const char *kValidServers = R"SERVERS(
{
    "locationName": "Kansas City",
    "regionalServers": [
        "http://austin.speed.googlefiber.net/",
        "http://kansas.speed.googlefiber.net/",
        "http://provo.speed.googlefiber.net/",
        "http://stanford.speed.googlefiber.net/"
    ]
}
)SERVERS";

const char *kInvalidServers = R"SERVERS(
{
    "locationName": "Kansas City",
    "regionalServers": [
        "example.com..",
    ]
}
)SERVERS";

const char *kInvalidJson = "{{}{";

TEST(ParseConfigTest, NullConfig_Invalid) {
  EXPECT_FALSE(ParseConfig(kValidConfig, nullptr));
}

TEST(ParseConfigTest, EmptyJson_Invalid) {
  Config config;
  EXPECT_FALSE(ParseConfig("", &config));
}

TEST(ParseConfigTest, InvalidJson_Invalid) {
  Config config;
  EXPECT_FALSE(ParseConfig(kInvalidJson, &config));
}

TEST(ParseConfigTest, FullConfig_Valid) {
  Config config;
  EXPECT_TRUE(ParseConfig(kValidConfig, &config));
  EXPECT_EQ(10000000, config.download_size);
  EXPECT_EQ(20000000, config.upload_size);
  EXPECT_EQ(20, config.num_downloads);
  EXPECT_EQ(15, config.num_uploads);
  EXPECT_EQ(200, config.interval_size);
  EXPECT_EQ("Kansas City", config.location_name);
  EXPECT_EQ(10, config.min_transfer_intervals);
  EXPECT_EQ(25, config.max_transfer_intervals);
  EXPECT_EQ(5000, config.min_transfer_runtime);
  EXPECT_EQ(20000, config.max_transfer_runtime);
  EXPECT_EQ(0.08, config.max_transfer_variance);
  EXPECT_EQ(3000, config.ping_runtime);
  EXPECT_EQ(300, config.ping_timeout);
  EXPECT_EQ(3004, config.transfer_port_start);
  EXPECT_EQ(3023, config.transfer_port_end);
}

TEST(ParseServersTest, NullServers_Invalid) {
  EXPECT_FALSE(ParseServers(kValidServers, nullptr));
}

TEST(ParseServersTest, EmptyServers_Invalid) {
  std::vector<http::Url> servers;
  EXPECT_FALSE(ParseServers("", &servers));
}

TEST(ParseServersTest, InvalidJson_Invalid) {
  std::vector<http::Url> servers;
  EXPECT_FALSE(ParseServers(kInvalidJson, &servers));
}

TEST(ParseServersTest, FullServers_Valid) {
  std::vector<http::Url> servers;
  EXPECT_TRUE(ParseServers(kValidServers, &servers));
  EXPECT_THAT(servers, testing::UnorderedElementsAre(
      http::Url("http://austin.speed.googlefiber.net/"),
      http::Url("http://kansas.speed.googlefiber.net/"),
      http::Url("http://provo.speed.googlefiber.net/"),
      http::Url("http://stanford.speed.googlefiber.net/")));
}

TEST(ParseServersTest, InvalidServers_Invalid) {
  std::vector<http::Url> servers;
  EXPECT_FALSE(ParseServers(kInvalidServers, &servers));
}

}  // namespace
}  // namespace speedtest
