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

const char *kValidRegions = R"REGIONS(
{
    "regions": [
        {
            "id": "aus",
            "name": "Austin",
            "url": "http://austin.speed.googlefiber.net/"
        },
        {
            "id": "mci",
            "name": "Kansas City",
            "url": [
                "http://kansas.speed.googlefiber.net/"
            ]
        },
        {
            "id": "slc",
            "name": "Provo",
            "url": [
                "http://provo.speed.googlefiber.net/"
            ]
        },
        {
            "id": "sfo",
            "name": "Stanford",
            "url": [
                "http://stanford.speed.googlefiber.net/"
            ]
        }
    ]
}
)REGIONS";

const char *kRegionMissingId = R"REGIONS(
{
    "regions": [
        {
            "name": "Kansas City",
            "url": [
                "http://kansas.speed.googlefiber.net/"
            ]
        },
    ]
}
)REGIONS";

const char *kRegionMissingName = R"REGIONS(
{
    "regions": [
        {
            "id": "mci",
            "url": [
                "http://kansas.speed.googlefiber.net/"
            ]
        },
    ]
}
)REGIONS";

const char *kRegionMissingUrl = R"REGIONS(
{
    "regions": [
        {
            "id": "mci",
            "name": "Kansas City",
        },
    ]
}
)REGIONS";

const char *kRegionEmptyUrl = R"REGIONS(
{
    "regions": [
        {
            "id": "mci",
            "name": "Kansas City",
            "url": [
            ]
        },
    ]
}
)REGIONS";

const char *kRegionMultipleUrls = R"REGIONS(
{
    "regions": [
        {
            "id": "mci",
            "name": "Kansas City",
            "url": [
                "http://kansas1.speed.googlefiber.net/",
                "http://kansas2.speed.googlefiber.net/"
            ]
        }
    ]
}
)REGIONS";

const char *kRegionInvalidUrl = R"REGIONS(
{
    "regions": [
        {
            "id": "mci",
            "name": "Kansas City",
            "url": [
                "example.com..",
            ]
        },
    ]
}
)REGIONS";

const char *kInvalidJson = "{{}{";

std::vector<std::string> RegionList(const std::vector<Region> &regions) {
  std::vector<std::string> region_list;
  for (const Region &region : regions) {
    std::stringstream ss;
    ss << region.id << ", " << region.name;
    for (const http::Url &url : region.urls) {
      ss << ", " << url.url();
    }
    region_list.emplace_back(ss.str());
  }
  return region_list;
}

TEST(ParseRegionsTest, NullRegions_Invalid) {
  EXPECT_ERROR(ParseRegions(kValidRegions, nullptr));
}

TEST(ParseRegionsTest, EmptyRegions_Invalid) {
  std::vector<Region> regions;
  EXPECT_ERROR(ParseRegions("", &regions));
}

TEST(ParseRegionsTest, InvalidJson_Invalid) {
  std::vector<Region> regions;
  EXPECT_ERROR(ParseRegions(kInvalidJson, &regions));
}

TEST(ParseRegionsTest, FullRegions_Valid) {
  std::vector<Region> regions;
  EXPECT_OK(ParseRegions(kValidRegions, &regions));
  EXPECT_THAT(RegionList(regions), testing::UnorderedElementsAre(
      "aus, Austin, http://austin.speed.googlefiber.net/",
      "mci, Kansas City, http://kansas.speed.googlefiber.net/",
      "slc, Provo, http://provo.speed.googlefiber.net/",
      "sfo, Stanford, http://stanford.speed.googlefiber.net/"));
}

TEST(ParseRegionsTest, MissingId_Invalid) {
  std::vector<Region> regions;
  EXPECT_ERROR(ParseRegions(kRegionMissingId, &regions));
}

TEST(ParseRegionsTest, MissingName_Invalid) {
  std::vector<Region> regions;
  EXPECT_ERROR(ParseRegions(kRegionMissingName, &regions));
}

TEST(ParseRegionsTest, MissingUrl_Invalid) {
  std::vector<Region> regions;
  EXPECT_ERROR(ParseRegions(kRegionMissingUrl, &regions));
}

TEST(ParseRegionsTest, EmptyUrl_Invalid) {
  std::vector<Region> regions;
  EXPECT_ERROR(ParseRegions(kRegionEmptyUrl, &regions));
}

TEST(ParseRegionsTest, InvalidRegionUrl_Invalid) {
  std::vector<Region> regions;
  EXPECT_ERROR(ParseRegions(kRegionInvalidUrl, &regions));
}

TEST(ParseRegionsTest, MultipleUrls_Valid) {
  std::vector<Region> regions;
  EXPECT_OK(ParseRegions(kRegionMultipleUrls, &regions));
  EXPECT_THAT(RegionList(regions), testing::UnorderedElementsAre(
      "mci, Kansas City, http://kansas1.speed.googlefiber.net/, "
      "http://kansas2.speed.googlefiber.net/"));
}

}  // namespace
}  // namespace speedtest
