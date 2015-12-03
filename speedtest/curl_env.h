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
