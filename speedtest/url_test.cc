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

#include "url.h"

namespace http {
namespace {

class InvalidUrlTest : public testing::TestWithParam<const char *> {
};

TEST_P(InvalidUrlTest, Invalid) {
  Url url;
  EXPECT_FALSE(url.Parse(GetParam()));
  EXPECT_FALSE(url.ok());
}

INSTANTIATE_TEST_CASE_P(
    InvalidUrls,
    InvalidUrlTest,
    testing::Values(
        "http://",
        "//",
        "http://foo//",
        "https://example.com:/",
        "http://example.com:234567",
        "2600:55::00ad:d001",
        "2600:55::00ad:d001]",
        "[2600:55::00ad:d001",
        "[2600:55::00ad:d001]:"
    ));

void VerifyOk(const std::string &raw_url,
              bool absolute,
              const std::string &scheme,
              const std::string &host,
              int port,
              const std::string &path,
              const std::string &query_string,
              const std::string &fragment,
              const std::string &normal_url) {
  Url url;
  url.Parse(raw_url);
  EXPECT_TRUE(url.ok());
  EXPECT_EQ(absolute, url.absolute());
  EXPECT_EQ(scheme, url.scheme());
  EXPECT_EQ(host, url.host());
  EXPECT_EQ(port, url.port());
  EXPECT_EQ(path, url.path());
  EXPECT_EQ(query_string, url.query_string());
  EXPECT_EQ(fragment, url.fragment());
  EXPECT_EQ(normal_url, url.url());
}

TEST(UrlTest, Empty_NotOk) {
  Url url;
  EXPECT_FALSE(url.ok());
  EXPECT_EQ("", url.scheme());
  EXPECT_EQ("", url.host());
  EXPECT_EQ(0, url.port());
  EXPECT_EQ("", url.path());
  EXPECT_EQ("", url.query_string());
  EXPECT_EQ("", url.fragment());
}

TEST(UrlTest, HostOnly_Ok) {
  VerifyOk("www.example.com",
           true,
           "http",
           "www.example.com",
           80,
           "/",
           "",
           "",
           "http://www.example.com/");
}

TEST(UrlTest, HostOnlyTrailingSlash_Ok) {
  VerifyOk("www.example.com/",
           true,
           "http",
           "www.example.com",
           80,
           "/",
           "",
           "",
           "http://www.example.com/");
}

TEST(UrlTest, HostPort_Ok) {
  VerifyOk("www.example.com:3111",
           true,
           "http",
           "www.example.com",
           3111,
           "/",
           "",
           "",
           "http://www.example.com:3111/");
}

TEST(UrlTest, HostPortTrailingSlash_Ok) {
  VerifyOk("www.example.com:3111/",
           true,
           "http",
           "www.example.com",
           3111,
           "/",
           "",
           "",
           "http://www.example.com:3111/");
}

TEST(UrlTest, SchemeAndHost_Ok) {
  VerifyOk("https://www.example.com",
           true,
           "https",
           "www.example.com",
           443,
           "/",
           "",
           "",
           "https://www.example.com/");
}

TEST(UrlTest, SchemeAndHostTrailingSlash_Ok) {
  VerifyOk("https://www.example.com/",
           true,
           "https",
           "www.example.com",
           443,
           "/",
           "",
           "",
           "https://www.example.com/");
}

TEST(UrlTest, SchemeHostPort_Ok) {
  VerifyOk("http://www.example.com:7001",
           true,
           "http",
           "www.example.com",
           7001,
           "/",
           "",
           "",
           "http://www.example.com:7001/");
}

TEST(UrlTest, SchemeHostPortTrailingSlash_Ok) {
  VerifyOk("http://www.example.com:7001/",
           true,
           "http",
           "www.example.com",
           7001,
           "/",
           "",
           "",
           "http://www.example.com:7001/");
}

TEST(UrlTest, AbsolutePathOnly_Ok) {
  VerifyOk("/path/to/resource",
           false,
           "",
           "",
           0,
           "/path/to/resource",
           "",
           "",
           "/path/to/resource");
}

TEST(UrlTest, HostPath_Ok) {
  VerifyOk("foo/bar/path",
           true,
           "http",
           "foo",
           80,
           "/bar/path",
           "",
           "",
           "http://foo/bar/path");
}

TEST(UrlTest, SchemaHostQueryString_Ok) {
  VerifyOk("http://localhost?foo=bar&a=b",
           true,
           "http",
           "localhost",
           80,
           "/",
           "foo=bar&a=b",
           "",
           "http://localhost/?foo=bar&a=b");
}

TEST(UrlTest, SchemaHostSlashQueryString_Ok) {
  VerifyOk("http://localhost/?foo=bar&abc=def",
           true,
           "http",
           "localhost",
           80,
           "/",
           "foo=bar&abc=def",
           "",
           "http://localhost/?foo=bar&abc=def");
}

TEST(UrlTest, SchemaHostPathQueryString_Ok) {
  VerifyOk("http://localhost/cgi-bin/download?foo=bar",
           true,
           "http",
           "localhost",
           80,
           "/cgi-bin/download",
           "foo=bar",
           "",
           "http://localhost/cgi-bin/download?foo=bar");
}

TEST(UrlTest, Fragment_Ok) {
  VerifyOk("#foo",
           false,
           "",
           "",
           0,
           "",
           "",
           "foo",
           "#foo");
}

TEST(UrlTest, HostFragment_Ok) {
  VerifyOk("www.example.com#foo",
           true,
           "http",
           "www.example.com",
           80,
           "/",
           "",
           "foo",
           "http://www.example.com/#foo");
}

TEST(UrlTest, HostPortSlashFragment_Ok) {
  VerifyOk("https://www.example.com:3011/#foo",
           true,
           "https",
           "www.example.com",
           3011,
           "/",
           "",
           "foo",
           "https://www.example.com:3011/#foo");
}

TEST(UrlTest, IPv6_Ok) {
  VerifyOk("[e712:ff00:3::ad]",
           true,
           "http",
           "[e712:ff00:3::ad]",
           80,
           "/",
           "",
           "",
           "http://[e712:ff00:3::ad]/");
}

TEST(UrlTest, IPv6Slash_Ok) {
  VerifyOk("[e712:ff00:3::ad]/",
           true,
           "http",
           "[e712:ff00:3::ad]",
           80,
           "/",
           "",
           "",
           "http://[e712:ff00:3::ad]/");
}

TEST(UrlTest, IPv6Path_Ok) {
  VerifyOk("[e712:ff00:3::ad]/foo/bar",
           true,
           "http",
           "[e712:ff00:3::ad]",
           80,
           "/foo/bar",
           "",
           "",
           "http://[e712:ff00:3::ad]/foo/bar");
}

TEST(UrlTest, IPv6Port_Ok) {
  VerifyOk("[e712:ff00:3::ad]:3303",
           true,
           "http",
           "[e712:ff00:3::ad]",
           3303,
           "/",
           "",
           "",
           "http://[e712:ff00:3::ad]:3303/");
}

TEST(UrlTest, IPv6PortSlash_Ok) {
  VerifyOk("[e712:ff00:3::ad]:3303/",
           true,
           "http",
           "[e712:ff00:3::ad]",
           3303,
           "/",
           "",
           "",
           "http://[e712:ff00:3::ad]:3303/");
}

TEST(UrlTest, IPv6PortPath_Ok) {
  VerifyOk("[e712:ff00:3::ad]:3303/abc/def",
           true,
           "http",
           "[e712:ff00:3::ad]",
           3303,
           "/abc/def",
           "",
           "",
           "http://[e712:ff00:3::ad]:3303/abc/def");
}

TEST(UrlTest, SchemeIPv6_Ok) {
  VerifyOk("https://[e712:ff00:3::ad]",
           true,
           "https",
           "[e712:ff00:3::ad]",
           443,
           "/",
           "",
           "",
           "https://[e712:ff00:3::ad]/");
}

TEST(UrlTest, SchemeIPv6Slash_Ok) {
  VerifyOk("https://[e712:ff00:3::ad]/",
           true,
           "https",
           "[e712:ff00:3::ad]",
           443,
           "/",
           "",
           "",
           "https://[e712:ff00:3::ad]/");
}

TEST(UrlTest, SchemeIPv6Path_Ok) {
  VerifyOk("https://[e712:ff00:3::ad]/def/ghi/",
           true,
           "https",
           "[e712:ff00:3::ad]",
           443,
           "/def/ghi/",
           "",
           "",
           "https://[e712:ff00:3::ad]/def/ghi/");
}

TEST(UrlTest, SchemeIPv6Port_Ok) {
  VerifyOk("https://[e712:ff00:3::ad]:3303",
           true,
           "https",
           "[e712:ff00:3::ad]",
           3303,
           "/",
           "",
           "",
           "https://[e712:ff00:3::ad]:3303/");
}

TEST(UrlTest, SchemeIPv6PortSlash_Ok) {
  VerifyOk("https://[e712:ff00:3::ad]:3303/",
           true,
           "https",
           "[e712:ff00:3::ad]",
           3303,
           "/",
           "",
           "",
           "https://[e712:ff00:3::ad]:3303/");
}

TEST(UrlTest, SchemeIPv6PortPath_Ok) {
  VerifyOk("https://[e712:ff00:3::ad]:3303/dir",
           true,
           "https",
           "[e712:ff00:3::ad]",
           3303,
           "/dir",
           "",
           "",
           "https://[e712:ff00:3::ad]:3303/dir");
}

TEST(UrlTest, FullHost_Ok) {
  VerifyOk("http://www.example.com:7889/path/to/foo?a=b&c=d#foo",
           true,
           "http",
           "www.example.com",
           7889,
           "/path/to/foo",
           "a=b&c=d",
           "foo",
           "http://www.example.com:7889/path/to/foo?a=b&c=d#foo");
}

TEST(UrlTest, FullIPv6_Ok) {
  VerifyOk("http://[26e5:0030:2:4::efad:0001:200e]:2345/path?a=b&c=d#foo",
           true,
           "http",
           "[26e5:0030:2:4::efad:0001:200e]",
           2345,
           "/path",
           "a=b&c=d",
           "foo",
           "http://[26e5:0030:2:4::efad:0001:200e]:2345/path?a=b&c=d#foo");
}

}  // namespace
}  // namespace http
