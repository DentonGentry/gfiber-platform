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

#include "curl_env.h"

#include <cstdlib>
#include <iostream>
#include <curl/curl.h>
#include "errors.h"
#include "request.h"

namespace http {

CurlEnv::CurlEnv() {
  init(CURL_GLOBAL_NOTHING);
}

CurlEnv::CurlEnv(int init_options) {
  init(init_options);
}

CurlEnv::~CurlEnv() {
  curl_global_cleanup();
}

std::unique_ptr<Request> CurlEnv::NewRequest() {
  return std::unique_ptr<Request>(new Request(shared_from_this()));
}

void CurlEnv::init(int init_flags) {
  CURLcode status = curl_global_init(init_flags);
  if (status != 0) {
    std::cerr << "Curl initialization failed: " << ErrorString(status);
    std::exit(1);
  }
}

}  // namespace http
