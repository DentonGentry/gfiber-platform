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

#include "options.h"

#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "url.h"

namespace speedtest {

const char* kDefaultHost = "any.speed.gfsvc.com";

namespace {

bool ParseLong(const char *s, char **endptr, long *size) {
  assert(s != nullptr);
  assert(size != nullptr);
  *size = strtol(s, endptr, 10);
  return !**endptr;
}

bool ParseSize(const char *s, long *size) {
  long result;
  char *endptr;
  bool success = ParseLong(s, &endptr, &result);
  if (result <= 0) {
    return false;
  }
  if (success) {
    *size = result;
    return true;
  }
  if (strcasecmp(endptr, "m") == 0) {
    result <<= 20;
  } else if (strcasecmp(endptr, "k") == 0) {
    result <<= 10;
  } else {
    return false;
  }
  *size = result;
  return true;
}

const int kOptMinTransferTime = 1000;
const int kOptMaxTransferTime = 1001;
const int kOptPingRuntime = 1002;
const int kOptPingTimeout = 1003;
const int kOptDisableDnsCache = 1004;
const int kOptMaxConnections = 1005;
const char *kShortOpts = "hvg:a:d:s:t:u:p:";
struct option kLongOpts[] = {
    {"help", no_argument, nullptr, 'h'},
    {"verbose", no_argument, nullptr, 'v'},
    {"global_host", required_argument, nullptr, 'g'},
    {"user_agent", required_argument, nullptr, 'a'},
    {"num_downloads", required_argument, nullptr, 'd'},
    {"download_size", required_argument, nullptr, 's'},
    {"num_uploads", required_argument, nullptr, 'u'},
    {"upload_size", required_argument, nullptr, 't'},
    {"progress", required_argument, nullptr, 'p'},
    {"min_transfer_time", required_argument, nullptr, kOptMinTransferTime},
    {"max_transfer_time", required_argument, nullptr, kOptMaxTransferTime},
    {"ping_runtime", required_argument, nullptr, kOptPingRuntime},
    {"ping_timeout", required_argument, nullptr, kOptPingTimeout},
    {"disable_dns_cache", no_argument, nullptr, kOptDisableDnsCache},
    {"max_connections", required_argument, nullptr, kOptMaxConnections},
    {nullptr, 0, nullptr, 0},
};
const int kMaxNumber = 1000;
const int kMaxProgress = 1000000;

const char *kSpeedtestHelp = R"USAGE(: run an HTTP speedtest.

If no hosts are specified, the global host is queried for a list
of servers to use, otherwise the list of supplied hosts will be
used. Each will be pinged several times and the one with the
lowest ping time will be used. If only one host is supplied, it
will be used without pinging.

Usage: speedtest [options] [host ...]
 -h, --help                This help text
 -v, --verbose             Verbose output
 -g, --global_host URL     Global host URL
 -a, --user_agent AGENT    User agent string for HTTP requests
 --disable_dns_cache       Disable global DNS cache
 --max_connections NUM     Maximum number of parallel connections

These options override the speedtest config parameters:
 -d, --num_downloads NUM   Number of simultaneous downloads
 -p, --progress TIME       Progress intervals in milliseconds
 -s, --download_size SIZE  Download size in bytes
 -t, --upload_size SIZE    Upload size in bytes
 -u, --num_uploads NUM     Number of simultaneous uploads
 --min_transfer_time TIME  Minimum transfer time in milliseconds
 --max_transfer_time TIME  Maximum transfer time in milliseconds
 --ping_time TIME          Ping time in milliseconds
 --ping_timeout TIME       Ping timeout in milliseconds;
)USAGE";

}  // namespace

bool ParseOptions(int argc, char *argv[], Options *options) {
  assert(options != nullptr);
  options->usage = false;
  options->verbose = false;
  options->global_host = http::Url(kDefaultHost);
  options->global = false;
  options->disable_dns_cache = false;
  options->max_connections = 0;
  options->num_downloads = 0;
  options->download_size = 0;
  options->num_uploads = 0;
  options->upload_size = 0;
  options->progress_millis = 0;
  options->min_transfer_time = 0;
  options->max_transfer_time = 0;
  options->ping_runtime = 0;
  options->ping_timeout = 0;
  options->hosts.clear();

  if (!options->global_host.ok()) {
    std::cerr << "Invalid global host " << kDefaultHost << "\n";
    return false;
  }

  // Manually set this to 0 to allow repeated calls
  optind = 0;
  int opt = 0, long_index = 0;
  while ((opt = getopt_long(argc, argv,
                            kShortOpts, kLongOpts, &long_index)) != -1) {
    switch (opt) {
      case 'a':
        options->user_agent = optarg;
        break;
      case 'd': {
        long number;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &number)) {
          std::cerr << "Could not parse number '" << optarg << "'\n";
          return false;
        }
        if (number < 1 || number > kMaxNumber) {
          std::cerr << "Number must be between 1 and " << kMaxNumber
                    << ", got '" << optarg << "'\n";
          return false;
        }
        options->num_downloads = static_cast<int>(number);
        break;
      }
      case 'g': {
        http::Url url(optarg);
        if (!url.ok()) {
          std::cerr << "Invalid global host " << optarg << "\n";
          return false;
        }
        options->global_host = url;
        break;
      }
      case 'h':
        options->usage = true;
        return true;
      case 'p': {
        long progress;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &progress)) {
          std::cerr << "Could not parse interval '" << optarg << "'\n";
          return false;
        }
        if (progress < 0 || progress > kMaxProgress) {
          std::cerr << "Number must be between 0 and " << kMaxProgress
                    << ", got '" << optarg << "'\n";
          return false;
        }
        options->progress_millis = static_cast<int>(progress);
        break;
      }
      case 's':
        if (!ParseSize(optarg, &options->download_size)) {
          std::cerr << "Invalid download size '" << optarg << "'\n";
          return false;
        }
        break;
      case 't':
        if (!ParseSize(optarg, &options->upload_size)) {
          std::cerr << "Invalid upload size '" << optarg << "'\n";
          return false;
        }
        break;
      case 'u': {
        long number;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &number)) {
          std::cerr << "Could not parse number '" << optarg << "'\n";
          return false;
        }
        if (number < 1 || number > kMaxNumber) {
          std::cerr << "Number must be between 1 and " << kMaxNumber
                    << ", got '" << optarg << "'\n";
          return false;
        }
        options->num_uploads = static_cast<int>(number);
        break;
      }
      case 'v':
        options->verbose = true;
        break;
      case kOptMinTransferTime: {
        long transfer_time;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &transfer_time)) {
          std::cerr << "Could not parse minimum transfer time '"
                    << optarg << "'\n";
          return false;
        }
        if (transfer_time < 0) {
          std::cerr << "Minimum transfer time must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->min_transfer_time = static_cast<int>(transfer_time);
        break;
      }
      case kOptMaxTransferTime: {
        long transfer_time;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &transfer_time)) {
          std::cerr << "Could not parse maximum transfer time '"
                    << optarg << "'\n";
          return false;
        }
        if (transfer_time < 0) {
          std::cerr << "Maximum transfer must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->max_transfer_time = static_cast<int>(transfer_time);
        break;
      }
      case kOptPingRuntime: {
        long ping_runtime;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &ping_runtime)) {
          std::cerr << "Could not parse ping time '" << optarg << "'\n";
          return false;
        }
        if (ping_runtime < 0) {
          std::cerr << "Ping time must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->ping_runtime = static_cast<int>(ping_runtime);
        break;
      }
      case kOptPingTimeout: {
        long ping_timeout;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &ping_timeout)) {
          std::cerr << "Could not parse ping timeout '" << optarg << "'\n";
          return false;
        }
        if (ping_timeout < 0) {
          std::cerr << "Ping timeout must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->ping_timeout = static_cast<int>(ping_timeout);
        break;
      }
      case kOptDisableDnsCache:
        options->disable_dns_cache = true;
        break;
      case kOptMaxConnections: {
        long max_connections;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &max_connections)) {
          std::cerr << "Could not parse max connections '" << optarg << "'\n";
          return false;
        }
        if (max_connections < 0) {
          std::cerr << "Max connections must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->max_connections = static_cast<int>(max_connections);
        break;
      }
      default:
        return false;
    }
  }
  if (optind < argc) {
    for (int i = optind; i < argc; i++) {
      http::Url url;
      if (!url.Parse(argv[i])) {
        std::cerr << "Could not parse URL '" << argv[i] << "'\n";
        return false;
      }
      if (!url.absolute()) {
        std::cerr << "URL '" << argv[i] << "' is not absolute\n";
        return false;
      }
      url.clear_path();
      url.clear_query_string();
      url.clear_fragment();
      options->hosts.emplace_back(url);
    }
  }
  if (options->hosts.empty()) {
    options->global = true;
  }
  return true;
}

void PrintOptions(const Options &options) {
  PrintOptions(std::cout, options);
}

void PrintOptions(std::ostream &out, const Options &options) {
  out << "Usage: " << (options.usage ? "true" : "false") << "\n"
      << "Verbose: " << (options.verbose ? "true" : "false") << "\n"
      << "Global host: " << options.global_host.url() << "\n"
      << "Global: " << (options.global ? "true" : "false") << "\n"
      << "User agent: " << options.user_agent << "\n"
      << "Disable DNS cache: "
      << (options.disable_dns_cache ? "true" : "false") << "\n"
      << "Max connections: " << options.max_connections << "\n"
      << "Number of downloads: " << options.num_downloads << "\n"
      << "Download size: " << options.download_size << " bytes\n"
      << "Number of uploads: " << options.num_uploads << "\n"
      << "Upload size: " << options.upload_size << " bytes\n"
      << "Min transfer time: " << options.min_transfer_time << " ms\n"
      << "Max transfer time: " << options.max_transfer_time << " ms\n"
      << "Ping runtime: " << options.ping_runtime << " ms\n"
      << "Ping timeout: " << options.ping_timeout << " ms\n"
      << "Progress interval: " << options.progress_millis << " ms\n"
      << "Hosts:\n";
  for (const http::Url &host : options.hosts) {
    out << "  " << host.url() << "\n";
  }
}

void PrintUsage(const char *app_name) {
  PrintUsage(std::cout, app_name);
}

void PrintUsage(std::ostream &out, const char *app_path) {
  assert(app_path != nullptr);
  const char *last_slash = strrchr(app_path, '/');
  const char *app_name = last_slash == nullptr ? app_path : last_slash + 1;
  out << basename(app_name) << kSpeedtestHelp;
}

}  // namespace speedtest
