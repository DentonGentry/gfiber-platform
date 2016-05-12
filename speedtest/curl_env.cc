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

#include "curl_env.h"

#include <cstdlib>
#include <iostream>
#include "errors.h"

namespace http {
namespace {

void LockFn(CURL *handle,
            curl_lock_data data,
            curl_lock_access access,
            void *userp) {
  CurlEnv *env = static_cast<CurlEnv *>(userp);
  env->Lock(data);
}

void UnlockFn(CURL *handle, curl_lock_data data, void *userp) {
  CurlEnv *env = static_cast<CurlEnv *>(userp);
  env->Unlock(data);
}

}  // namespace

std::shared_ptr<CurlEnv> CurlEnv::NewCurlEnv(const Options &options) {
  return std::shared_ptr<CurlEnv>(new CurlEnv(options));
}

CurlEnv::CurlEnv(const Options &options)
    : options_(options),
      set_max_connections_(false),
      share_(nullptr) {
  CURLcode status;
  {
    std::lock_guard <std::mutex> lock(curl_mutex_);
    status = curl_global_init(options_.curl_options);
  }
  if (status != 0) {
    std::cerr << "Curl initialization failed: " << ErrorString(status);
    std::exit(1);
  }
  if (!options_.disable_dns_cache) {
    share_ = curl_share_init();
    curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(share_, CURLSHOPT_USERDATA, this);
    curl_share_setopt(share_, CURLSHOPT_LOCKFUNC, &LockFn);
    curl_share_setopt(share_, CURLSHOPT_UNLOCKFUNC, &UnlockFn);
  }
}

CurlEnv::~CurlEnv() {
  curl_share_cleanup(share_);
  share_ = nullptr;
  curl_global_cleanup();
}

Request::Ptr CurlEnv::NewRequest(const Url &url) {
  // curl_global_init is not threadsafe and calling curl_easy_init may
  // implicitly call it so we need to mutex lock on creating all requests
  // to ensure the global initialization is done in a threadsafe manner.
  std::lock_guard <std::mutex> lock(curl_mutex_);

  // We use an aliasing constructor on a shared_ptr to keep a reference to
  // CurlEnv as when the refcount drops to 0 we want to do global cleanup.
  // So the CURL handle for this shared_ptr is _unmanaged_ and the Request
  // object is responsible for cleaning it up, which involves calling
  // curl_easy_cleanup().
  //
  // This way Request doesn't need to know about CurlEnv at all, while
  // all Request instances will still keep an implicit reference to
  // CurlEnv.
  std::shared_ptr<CURL> handle(shared_from_this(), curl_easy_init());

  // For some reason libcurl sets the max connections on a handle.
  // According to the docs, doing so when there are open connections may
  // close them so we maintain this boolean so as to set the maximum
  // number of connections on the connection pool associated with this
  // handle before any connections are opened.
  if (!set_max_connections_ && options_.max_connections > 0) {
    curl_easy_setopt(handle.get(),
                     CURLOPT_MAXCONNECTS,
                     options_.max_connections);
    set_max_connections_ = true;
  }

  curl_easy_setopt(handle.get(), CURLOPT_SHARE, share_);
  curl_easy_setopt(handle.get(), CURLOPT_NOSIGNAL, 1);
  return std::unique_ptr<Request>(new Request(handle, url));
}

void CurlEnv::Lock(curl_lock_data lock_type) {
  if (lock_type == CURL_LOCK_DATA_DNS) {
    // It is ill-advised to call lock directly but libcurl uses
    // separate lock/unlock functions.
    dns_mutex_.lock();
  }
}

void CurlEnv::Unlock(curl_lock_data lock_type) {
  if (lock_type == CURL_LOCK_DATA_DNS) {
    // It is ill-advised to call lock directly but libcurl uses
    // separate lock/unlock functiounknns.
    dns_mutex_.unlock();
  }
}

}  // namespace http
