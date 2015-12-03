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
