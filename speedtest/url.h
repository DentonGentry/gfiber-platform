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

#ifndef HTTP_URL_H
#define HTTP_URL_H

#include <string>

namespace http {

class Url;

bool operator==(const Url &url1, const Url &url2);
std::ostream &operator<<(std::ostream &os, const Url &url);

// Partial implementation of a URL parser. This is needed because URLs need
// to be manipulated for creating URLs for Speedtest, which is otherwise
// awkward.
//
// This is implemented as a series of state machines rather than being
// regex based as the C++11 regex implementation, at least in GCC 4.8.x,
// is incomplete and buggy. Plus regular expressions to properly parse URLs
// are complex.
//
// TODO(wshields): authority
// TODO(wshields): pct-encoding
// TODO(wshields): strict IPV6 parsing
// TODO(wshields): validate setters
// TODO(wshields): move query string param handling here
class Url {
 public:
  Url();
  Url(const Url &other);
  explicit Url(const std::string &url);
  Url &operator=(const Url &other);

  bool Parse(const std::string &url);

  bool ok() const { return parsed_; }
  bool absolute() const { return absolute_; }

  const std::string &scheme() const { return scheme_; }
  void set_scheme(const std::string &scheme);

  const std::string &host() const { return host_; }
  void set_host(const std::string &host);

  int port() const { return port_; }
  void set_port(int port);

  const std::string &path() const { return path_; }
  void set_path(const std::string &path);
  void clear_path();

  const std::string &query_string() const { return query_string_; }
  void set_query_string(const std::string &query_string);
  void clear_query_string();

  const std::string &fragment() const { return fragment_; }
  void set_fragment(const std::string &fragment);
  void clear_fragment();

  std::string url() const;

  friend bool operator==(const Url &url1, const Url &url2);
  friend std::ostream &operator<<(std::ostream &os, const Url &url);

 private:
  using Iter = std::string::const_iterator;

  bool Scheme();
  bool IPv6();
  bool Host();
  bool Port();
  bool AbsolutePath();
  bool QueryString();
  bool Fragment();

  void UpdateAbsolute();

  bool parsed_;
  bool absolute_;
  Iter current_;
  Iter end_;
  std::string scheme_;
  std::string host_;
  int port_;
  std::string path_;
  std::string query_string_;
  std::string fragment_;
};

}  // namespace http

#endif  // HTTP_URL_H
