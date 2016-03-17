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

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <curl/curl.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "curl_env.h"
#include "url.h"

namespace http {

class Request {
 public:
  enum class UploadStatus {
    CONTINUE,
    DONE,
    ABORT
  };

  using Headers = std::multimap<std::string, std::string>;
  using QueryStringParams = std::multimap<std::string, std::string>;
  using UploadFn = std::function<UploadStatus(char *, size_t, size_t *)>;
  using DownloadFn = std::function<void(void *, size_t)>;
  using ProgressFn = std::function<bool(curl_off_t,
                                        curl_off_t,
                                        curl_off_t,
                                        curl_off_t)>;

  explicit Request(std::shared_ptr<CurlEnv> env);
  virtual ~Request();

  CURLcode Get(DownloadFn download_fn);
  CURLcode Post(UploadFn upload_fn);
  CURLcode Post(const char *data, curl_off_t data_len);

  void Reset();

  const std::string &user_agent() const { return user_agent_; }
  void set_user_agent(const std::string &user_agent) {
    user_agent_ = user_agent;
  }

  const Url &url() const { return url_; }
  void set_url(const Url &url) { url_ = url; }

  Headers &headers() { return headers_; }
  void set_header(Headers::key_type name, Headers::mapped_type value);
  void add_header(Headers::key_type name, Headers::mapped_type value);
  void clear_header(Headers::key_type name);
  void clear_headers();

  QueryStringParams &params() { return params_; }
  void set_param(QueryStringParams::key_type name,
                 QueryStringParams::mapped_type value);
  void add_param(QueryStringParams::key_type name,
                 QueryStringParams::mapped_type value);
  void clear_param(QueryStringParams::key_type name);
  void clear_params();

  // Caller retains ownership
  void set_progress_fn(ProgressFn progress_fn) { progress_fn_ = progress_fn; }
  void clear_progress_fn() { progress_fn_ = nullptr; }

  void UpdateUrl();

 private:
  void CommonSetup();
  CURLcode Execute();

  // owned
  CURL *handle_;
  struct curl_slist *curl_headers_;

  // ref-count CURL global config
  std::shared_ptr<CurlEnv> env_;
  Url url_;

  std::string user_agent_;
  Headers headers_;
  QueryStringParams params_;
  ProgressFn progress_fn_;  // unowned

  // disable
  Request(const Request &) = delete;
  void operator=(const Request &) = delete;
};

}  // namespace http

#endif  // HTTP_REQUEST_H
