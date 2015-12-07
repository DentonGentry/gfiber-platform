#ifndef SPEEDTEST_SPEEDTEST_H
#define SPEEDTEST_SPEEDTEST_H

#include <atomic>
#include <memory>
#include <string>

#include "curl_env.h"
#include "options.h"
#include "url.h"
#include "request.h"

namespace speedtest {

class Speedtest {
 public:
  explicit Speedtest(const Options &options);
  virtual ~Speedtest();

  void Run();
  void RunDownloadTest();
  void RunUploadTest();
  bool RunPingTest();

 private:
  void RunDownload(int id);
  void RunUpload(int id);
  void RunPing(size_t host_index);

  std::unique_ptr<http::Request> MakeRequest(int id, const std::string &path);

  std::shared_ptr<http::CurlEnv> env_;
  Options options_;
  http::Url url_;
  std::atomic_bool end_ping_;
  std::atomic_bool end_download_;
  std::atomic_bool end_upload_;
  std::atomic_long bytes_downloaded_;
  std::atomic_long bytes_uploaded_;
  std::vector<long> min_ping_micros_;
  const char *send_data_;

  // disable
  Speedtest(const Speedtest &) = delete;
  void operator=(const Speedtest &) = delete;
};

}  // namespace speedtest

#endif  // SPEEDTEST_SPEEDTEST_H
