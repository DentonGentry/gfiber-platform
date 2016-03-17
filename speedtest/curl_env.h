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

#ifndef HTTP_CURL_ENV_H
#define HTTP_CURL_ENV_H

#include <memory>

namespace http {

class Request;

// Curl initialization to cleanup automatically
class CurlEnv : public std::enable_shared_from_this<CurlEnv> {
 public:
  CurlEnv();
  explicit CurlEnv(int init_options);
  virtual ~CurlEnv();

  std::unique_ptr<Request> NewRequest();

 private:
  void init(int flags);

  // disable
  CurlEnv(const CurlEnv &other) = delete;

  void operator=(const CurlEnv &other) = delete;
};

}  // namespace http

#endif  // HTTP_CURL_ENV_H
