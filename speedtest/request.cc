#include "request.h"

#include <assert.h>
#include <cstdlib>
#include <iostream>

#include "errors.h"

namespace http {
namespace {

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  Request::DownloadFn *cb = static_cast<Request::DownloadFn *>(userp);
  size_t len = size * nmemb;
  if (cb && *cb) {
    (*cb)(contents, len);
  }
  return size * nmemb;
}

size_t ReadCallback(char *buffer, size_t size, size_t nmemb, void *userp) {
  Request::UploadFn *cb = static_cast<Request::UploadFn *>(userp);
  size_t len = size * nmemb;
  if (!cb || !*cb) {
    return CURL_READFUNC_ABORT;
  }
  size_t bytes_sent = 0;
  Request::UploadStatus status = (*cb)(buffer, len, &bytes_sent);
  if (status == Request::UploadStatus::ABORT) {
    return CURL_READFUNC_ABORT;
  } else if (status == Request::UploadStatus::DONE) {
    return 0;
  }
  return bytes_sent;
}

int ProgressCallback(void *clientp,
                     curl_off_t dltotal,
                     curl_off_t dlnow,
                     curl_off_t ultotal,
                     curl_off_t ulnow) {
  Request::ProgressFn *cb = static_cast<Request::ProgressFn *>(clientp);
  if (cb) {
    return (*cb)(dltotal, dlnow, ultotal, ulnow);
  }
  return 0;
}

const int kDefaultQueryStringSize = 200;

}  // namespace

Request::Request(std::shared_ptr<CurlEnv> env)
    : curl_headers_(nullptr),
      env_(env) {
  assert(env_);
  handle_ = curl_easy_init();
  if (!handle_) {
    std::cerr << "Failed to create handle\n";
    std::exit(1);
  }
}

Request::~Request() {
  if (curl_headers_) {
    curl_slist_free_all(curl_headers_);
    curl_headers_ = nullptr;
  }
  curl_easy_cleanup(handle_);
}

CURLcode Request::Get(DownloadFn download_fn) {
  CommonSetup();
  if (download_fn) {
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, &WriteCallback);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &download_fn);
  }
  return Execute();
}

CURLcode Request::Post(UploadFn upload_fn) {
  CommonSetup();
  curl_easy_setopt(handle_, CURLOPT_UPLOAD, 1);
  curl_easy_setopt(handle_, CURLOPT_READFUNCTION, &ReadCallback);
  curl_easy_setopt(handle_, CURLOPT_READDATA, &upload_fn);
  return Execute();
}

CURLcode Request::Post(const char *data, curl_off_t data_len) {
  CommonSetup();
  curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE_LARGE, data_len);
  curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, data);
  return Execute();
}

void Request::Reset() {
  curl_easy_reset(handle_);
  clear_progress_fn();
  clear_headers();
  clear_params();
  if (curl_headers_) {
    curl_slist_free_all(curl_headers_);
    curl_headers_ = nullptr;
  }
}

void Request::set_header(Headers::key_type name, Headers::mapped_type value) {
  clear_header(name);
  add_header(name, value);
}

void Request::add_header(Headers::key_type name, Headers::mapped_type value) {
  headers_.insert(std::make_pair(name, value));
}

void Request::clear_header(Headers::key_type name) {
  headers_.erase(name);
}

void Request::clear_headers() {
  headers_.clear();
}

void Request::set_param(QueryStringParams::key_type name,
                        QueryStringParams::mapped_type value) {
  clear_param(name);
  add_param(name, value);
}

void Request::add_param(QueryStringParams::key_type name,
                        QueryStringParams::mapped_type value) {
  params_.insert(std::make_pair(name, value));
}

void Request::clear_param(QueryStringParams::key_type name) {
  params_.erase(name);
}

void Request::clear_params() {
  params_.clear();
}

void Request::UpdateUrl() {
  std::string query_string;
  query_string.reserve(kDefaultQueryStringSize);
  for (QueryStringParams::const_iterator iter = params_.begin();
       iter != params_.end();
       ++iter) {
    if (!query_string.empty()) {
      query_string.append("&");
    }
    char *name = curl_easy_escape(handle_,
                                  iter->first.data(),
                                  iter->first.length());
    char *value = curl_easy_escape(handle_,
                                   iter->second.data(),
                                   iter->second.length());
    query_string.append(name);
    query_string.append("=");
    query_string.append(value);
    curl_free(name);
    curl_free(value);
  }
  url_.set_query_string(query_string);
}

void Request::CommonSetup() {
  UpdateUrl();
  std::string request_url = url_.url();
  curl_easy_setopt(handle_, CURLOPT_URL, request_url.c_str());
  curl_easy_setopt(handle_, CURLOPT_USERAGENT, user_agent_.c_str());
  if (progress_fn_) {
    curl_easy_setopt(handle_, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION, &ProgressCallback);
    curl_easy_setopt(handle_, CURLOPT_XFERINFODATA, &progress_fn_);
  }
  if (!headers_.empty()) {
    struct curl_slist *headers = nullptr;
    for (Headers::const_iterator iter = headers_.begin();
         iter != headers_.end();
         ++iter) {
      std::string header(iter->first);
      header.append(": ");
      header.append(iter->second);
      headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers);
  }
}

CURLcode Request::Execute() {
  return curl_easy_perform(handle_);
}

}  // namespace http
