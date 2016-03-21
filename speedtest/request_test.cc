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

#include "request.h"

#include <gtest/gtest.h>
#include <memory>
#include "curl_env.h"

namespace http {
namespace {

class RequestTest : public testing::Test {
 protected:
  std::shared_ptr<CurlEnv> env;
  std::unique_ptr<Request> request;

  void SetUp() override {
    env = CurlEnv::NewCurlEnv({});
    request = env->NewRequest(http::Url("http://example.com/foo"));
  }

  void VerifyQueryString(const char *expected,
                         Request::QueryStringParams params) {
    request->params() = params;
    request->UpdateUrl();
    EXPECT_EQ(expected, request->url().query_string());
  }

  void VerifyUrl(const char *expected,
                 const char *url,
                 Request::QueryStringParams params) {
    request->set_url(Url(url));
    request->params() = params;
    request->UpdateUrl();
    EXPECT_EQ(expected, request->url().url());
  }
};

TEST_F(RequestTest, QueryString_Empty_Ok) {
  VerifyQueryString("", {});
}

TEST_F(RequestTest, QueryString_SingleParam_Ok) {
  VerifyQueryString("abc=def", {{"abc", "def"}});
}

TEST_F(RequestTest, QueryString_MultipleValues_Ok) {
  VerifyQueryString("abc=def&abc=ghi", {{"abc", "def"}, {"abc", "ghi"}});
}

TEST_F(RequestTest, QueryString_TwoParams_Ok) {
  VerifyQueryString("abc=def&def=ghi", {{"abc", "def"}, {"def", "ghi"}});
}

TEST_F(RequestTest, QueryString_Escape_Ok) {
  VerifyQueryString("%2B%3D%26%20=%20%26%3D%2B", {{"+=& ", " &=+"}});
}

TEST_F(RequestTest, Url_Empty_Ok) {
  VerifyUrl("", "", {});
}

TEST_F(RequestTest, Url_NoParams_Ok) {
  VerifyUrl("http://example.com/", "http://example.com", {});
}

TEST_F(RequestTest, Url_OneParam_Ok) {
  VerifyUrl("http://example.com/?abc=def",
            "http://example.com",
            {{"abc", "def"}});
}

TEST_F(RequestTest, Url_TwoParams_Ok) {
  VerifyUrl("http://example.com/?abc=def&def=ghi",
            "http://example.com",
            {{"abc", "def"}, {"def", "ghi"}});
}

TEST_F(RequestTest, Url_OneParamTwoValues_Ok) {
  VerifyUrl("http://example.com/?abc=def&abc=def",
            "http://example.com",
            {{"abc", "def"}, {"abc", "def"}});
}

TEST_F(RequestTest, Url_EscapeParam_Ok) {
  VerifyUrl("http://example.com/?%2B%3D%26%20=%20%26%3D%2B",
            "http://example.com",
            {{"+=& ", " &=+"}});
}

}  // namespace
}  // namespace http
