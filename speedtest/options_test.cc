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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <mutex>
#include <string.h>
#include <vector>
#include "options.h"

namespace speedtest {
namespace {

char *CopyString(const char *s) {
  size_t len = strlen(s);
  char *ret = new char[len + 1];
  strcpy(ret, s);
  return ret;
}

std::mutex options_lock;

void TestOptions(bool valid,
                 std::vector<const char *> args,
                 Options *options) {
  int argc = args.size() + 1;
  char **argv = new char *[argc];
  argv[0] = CopyString("speedtest");
  int i = 1;
  for (const char *s : args) {
    argv[i++] = CopyString(s);
  }
  {
    std::lock_guard<std::mutex> lock(options_lock);
    EXPECT_EQ(valid, ParseOptions(argc, argv, options));
  }
  for (int i = 0; i < argc; i++) {
    delete argv[i];
  }
  delete[] argv;
}

void TestOptions(bool valid, std::vector<const char *> args) {
  Options options;
  TestOptions(valid, args, &options);
}

void TestValidOptions(std::vector<const char *> args, Options *options) {
  TestOptions(true, args, options);
}

void TestValidOptions(std::vector<const char *> args) {
  TestOptions(true, args);
}

void TestInvalidOptions(std::vector<const char *> args) {
  TestOptions(false, args);
}

TEST(OptionsTest, UnknownOption_Invalid) {
  TestInvalidOptions({"-x"});
}

TEST(OptionsTest, MissingArgs_Invalid) {
  TestInvalidOptions({"-d", "-u"});
}

TEST(OptionsTest, Empty_ValidDefault) {
  Options options;
  TestValidOptions({}, &options);
  EXPECT_FALSE(options.verbose);
  EXPECT_FALSE(options.usage);
}

TEST(OptionsTest, Usage_Valid) {
  {
    Options options;
    TestValidOptions({"-h"}, &options);
    EXPECT_TRUE(options.usage);
  }
  {
    Options options;
    TestValidOptions({"--help"}, &options);
    EXPECT_TRUE(options.usage);
  }
}

TEST(OptionsTest, OneHost_Valid) {
  Options options;
  TestValidOptions({"efgh"}, &options);
  EXPECT_THAT(options.hosts, testing::ElementsAre(http::Url("efgh")));
}

TEST(OptionsTest, FullShort_Valid) {
  Options options;
  TestValidOptions({"-d", "5122",
                    "-u", "7653",
                    "-t", "123",
                    "-n", "15",
                    "-p", "500"},
                    &options);
  EXPECT_EQ(5122, options.download_size);
  EXPECT_EQ(7653, options.upload_size);
  EXPECT_EQ(123, options.time_millis);
  EXPECT_EQ(15, options.number);
  EXPECT_EQ(500, options.progress_millis);
}

TEST(OptionsTest, FullLong_Valid) {
  Options options;
  TestValidOptions({"--download_size", "5122",
                    "--upload_size", "7653",
                    "--time", "123",
                    "--progress", "1000",
                    "--number", "12"},
                    &options);
  EXPECT_EQ(5122, options.download_size);
  EXPECT_EQ(7653, options.upload_size);
  EXPECT_EQ(123, options.time_millis);
  EXPECT_EQ(12, options.number);
  EXPECT_EQ(1000, options.progress_millis);
  EXPECT_THAT(options.hosts, testing::ElementsAre(http::Url(kDefaultHost)));
}

}  // namespace
}  // namespace speedtest
