#include "speedtest.h"

#include <assert.h>
#include <chrono>
#include <cstring>
#include <limits>
#include <random>
#include <thread>
#include <iomanip>

#include "errors.h"
#include "utils.h"

namespace speedtest {

Speedtest::Speedtest(const Options &options)
    : options_(options),
      bytes_downloaded_(0),
      bytes_uploaded_(0) {
  assert(!options_.hosts.empty());
  env_ = std::make_shared<http::CurlEnv>();
  std::random_device rd;
  std::default_random_engine random_engine(rd());
  std::uniform_int_distribution<char> uniform_dist(1, 255);
  char *data = new char[options_.upload_size];
  for (int i = 0; i < options_.upload_size; ++i) {
    data[i] = uniform_dist(random_engine);
  }
  send_data_ = data;
}

Speedtest::~Speedtest() {
  delete[] send_data_;
}

void Speedtest::Run() {
  if (!RunPingTest()) {
    std::cout << "No servers responded. Exiting\n";
    return;
  }
  RunDownloadTest();
  RunUploadTest();
}

void Speedtest::RunDownloadTest() {
  end_download_ = false;
  long start_time = SystemTimeMicros();
  bytes_downloaded_ = 0;
  std::thread threads[options_.number];
  for (int i = 0; i < options_.number; ++i) {
    threads[i] = std::thread([=]() {
      RunDownload(i);
    });
  }
  std::thread timer([&]{
    std::this_thread::sleep_for(
        std::chrono::milliseconds(options_.time_millis));
    end_download_ = true;
  });
  timer.join();
  for (auto &thread : threads) {
    thread.join();
  }
  long end_time = speedtest::SystemTimeMicros();

  double running_time = (end_time - start_time) / 1000000.0;
  double megabits = bytes_downloaded_ * 8 / 1000000.0 / running_time;
  std::cout << "Downloaded " << bytes_downloaded_
            << " bytes in " << running_time * 1000 << " ms ("
            << megabits << " Mbps)\n";
}

void Speedtest::RunDownload(int id) {
  auto download = MakeRequest(id, "/download");
  http::Request::DownloadFn noop = [](void *, size_t) {};
  while (!end_download_) {
    long downloaded = 0;
    download->set_param("i", std::to_string(id));
    download->set_param("size", std::to_string(options_.download_size));
    download->set_param("time", std::to_string(SystemTimeMicros()));
    download->set_progress_fn([&](curl_off_t,
                                  curl_off_t dlnow,
                                  curl_off_t,
                                  curl_off_t) -> bool {
      if (dlnow > downloaded) {
        bytes_downloaded_ += dlnow - downloaded;
        downloaded = dlnow;
      }
      return end_download_;
    });
    download->Get(noop);
    download->Reset();
  }
}

void Speedtest::RunUploadTest() {
  end_upload_ = false;
  long start_time = SystemTimeMicros();
  bytes_uploaded_ = 0;
  std::thread threads[options_.number];
  for (int i = 0; i < options_.number; ++i) {
    threads[i] = std::thread([=]() {
      RunUpload(i);
    });
  }
  std::thread timer([&]{
    std::this_thread::sleep_for(
        std::chrono::milliseconds(options_.time_millis));
    end_upload_ = true;
  });
  timer.join();
  for (auto &thread : threads) {
    thread.join();
  }
  long end_time = speedtest::SystemTimeMicros();

  double running_time = (end_time - start_time) / 1000000.0;
  double megabits = bytes_uploaded_ * 8 / 1000000.0 / running_time;
  std::cout << "Uploaded " << bytes_uploaded_
            << " bytes in " << running_time * 1000 << " ms ("
            << megabits << " Mbps)\n";
}

void Speedtest::RunUpload(int id) {
  auto upload = MakeRequest(id, "/upload");
  while (!end_upload_) {
    long uploaded = 0;
    upload->set_progress_fn([&](curl_off_t,
                                curl_off_t,
                                curl_off_t,
                                curl_off_t ulnow) -> bool {
      if (ulnow > uploaded) {
        bytes_uploaded_ += ulnow - uploaded;
        uploaded = ulnow;
      }
      return end_upload_;
    });

    // disable the Expect header as the server isn't expecting it (perhaps
    // it should?). If the server isn't then libcurl waits for 1 second
    // before sending the data anyway. So sending this header eliminated
    // the 1 second delay.
    upload->set_header("Expect", "");
    upload->Post(send_data_, options_.upload_size);
    upload->Reset();
  }
}

bool Speedtest::RunPingTest() {
  end_ping_ = false;
  size_t num_hosts = options_.hosts.size();
  std::thread threads[num_hosts];
  min_ping_micros_.clear();
  min_ping_micros_.resize(num_hosts);
  for (size_t i = 0; i < num_hosts; ++i) {
    threads[i] = std::thread([=]() {
      RunPing(i);
    });
  }
  std::thread timer([&]{
    std::this_thread::sleep_for(
        std::chrono::milliseconds(options_.time_millis));
    end_ping_ = true;
  });
  timer.join();
  for (auto &thread : threads) {
    thread.join();
  }
  if (options_.verbose) {
    std::cout << "Pinged " << num_hosts << " "
              << (num_hosts == 1 ? "host" : "hosts:") << "\n";
  }
  size_t min_index = 0;
  for (size_t i = 0; i < num_hosts; ++i) {
    if (options_.verbose) {
      std::cout << "  " << options_.hosts[i].url() << ": ";
      if (min_ping_micros_[i] == std::numeric_limits<long>::max()) {
        std::cout << "no packets received";
      } else {
        double ping_ms = min_ping_micros_[i] / 1000.0;
        if (ping_ms < 10) {
          std::cout << std::fixed << std::setprecision(1);
        } else {
          std::cout << std::fixed << std::setprecision(0);
        }
        std::cout << ping_ms << " ms";
      }
      std::cout << "\n";
    }
    if (min_ping_micros_[i] < min_ping_micros_[min_index]) {
      min_index = i;
    }
  }
  if (min_ping_micros_[min_index] == std::numeric_limits<long>::max()) {
    // no servers respondeded
    return false;
  }
  url_ = options_.hosts[min_index];
  std::cout << "Host for Speedtest: " << url_.url() << " (";
  double ping_ms = min_ping_micros_[min_index] / 1000.0;
  if (ping_ms < 10) {
    std::cout << std::fixed << std::setprecision(1);
  } else {
    std::cout << std::fixed << std::setprecision(0);
  }
  std::cout << ping_ms << " ms)\n";
  return true;
}

void Speedtest::RunPing(size_t index) {
  http::Request::DownloadFn noop = [](void *, size_t) {};
  min_ping_micros_[index] = std::numeric_limits<long>::max();
  http::Url url(options_.hosts[index]);
  url.set_path("/ping");
  auto ping = env_->NewRequest();
  ping->set_url(url);
  while (!end_ping_) {
    long req_start = SystemTimeMicros();
    if (ping->Get(noop) == CURLE_OK) {
      long req_end = SystemTimeMicros();
      long ping_time = req_end - req_start;
      min_ping_micros_[index] = std::min(min_ping_micros_[index], ping_time);
    }
    ping->Reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

std::unique_ptr<http::Request> Speedtest::MakeRequest(int id,
                                                      const std::string &path) {
  auto request = env_->NewRequest();
  http::Url url(url_);
  int port = (id % 20) + url.port() + 1;
  url.set_port(port);
  url.set_path(path);
  request->set_url(url);
  return std::move(request);
}

}  // namespace speedtest
