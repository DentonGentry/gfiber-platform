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

#include "options.h"

#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "url.h"

namespace speedtest {
namespace {

const char* kDefaultHost = "any.speed.gfsvc.com";

}  // namespace

namespace {

bool ParseLong(const char *s, char **endptr, long *number) {
  assert(s);
  assert(endptr);
  assert(number != nullptr);
  *number = strtol(s, endptr, 10);
  return !**endptr;
}

bool ParseDouble(const char *s, char **endptr, double *number) {
  assert(s);
  assert(endptr);
  assert(number != nullptr);
  *number = strtod(s, endptr);
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

const int kOptDisableDnsCache = 1000;
const int kOptMaxConnections = 1001;
const int kOptReportResults = 1002;
const int kOptSkipDownload = 1003;
const int kOptSkipUpload = 1004;
const int kOptSkipPing = 1005;
const int kOptNoReportResults = 1006;
const int kOptServerId = 1007;

const int kOptMinTransferTime = 1100;
const int kOptMaxTransferTime = 1101;
const int kOptMinTransferIntervals = 1102;
const int kOptMaxTransferIntervals = 1103;
const int kOptMaxTransferVariance = 1104;
const int kOptIntervalMillis = 1105;
const int kOptPingRuntime = 1106;
const int kOptPingTimeout = 1107;
const int kOptExponentialMovingAverage = 1108;

const char *kShortOpts = "hvg:a:d:s:t:u:p:";

struct option kLongOpts[] = {
    {"help", no_argument, nullptr, 'h'},
    {"verbose", no_argument, nullptr, 'v'},
    {"global_url", required_argument, nullptr, 'g'},
    {"user_agent", required_argument, nullptr, 'a'},
    {"disable_dns_cache", no_argument, nullptr, kOptDisableDnsCache},
    {"max_connections", required_argument, nullptr, kOptMaxConnections},
    {"progress_millis", required_argument, nullptr, 'p'},
    {"skip_download", no_argument, nullptr, kOptSkipDownload},
    {"skip_upload", no_argument, nullptr, kOptSkipUpload},
    {"skip_ping", no_argument, nullptr, kOptSkipPing},
    {"report_results", no_argument, nullptr, kOptReportResults},
    {"noreport_results", no_argument, nullptr, kOptNoReportResults},

    {"num_downloads", required_argument, nullptr, 'd'},
    {"download_size", required_argument, nullptr, 's'},
    {"num_uploads", required_argument, nullptr, 'u'},
    {"upload_size", required_argument, nullptr, 't'},
    {"min_transfer_runtime", required_argument, nullptr, kOptMinTransferTime},
    {"max_transfer_runtime", required_argument, nullptr, kOptMaxTransferTime},
    {"min_transfer_intervals", required_argument, nullptr,
        kOptMinTransferIntervals},
    {"max_transfer_intervals", required_argument, nullptr,
        kOptMaxTransferIntervals},
    {"max_transfer_variance", required_argument, nullptr,
        kOptMaxTransferVariance},
    {"interval_millis", required_argument, nullptr, kOptIntervalMillis},
    {"ping_runtime", required_argument, nullptr, kOptPingRuntime},
    {"ping_timeout", required_argument, nullptr, kOptPingTimeout},
    {"exponential_moving_average", no_argument, nullptr,
        kOptExponentialMovingAverage},
    {"serverid", required_argument, nullptr, kOptServerId},  // ignored
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
 -h, --help                    This help text
 -v, --verbose                 Verbose output
 -g, --global_url URL         Global host URL
 -a, --user_agent AGENT        User agent string for HTTP requests
 -p, --progress_millis NUM     Delay in milliseconds between updates
 --disable_dns_cache           Disable global DNS cache
 --max_connections NUM         Maximum number of parallel connections
 --skip_download               Skip the download test
 --skip_upload                 Skip the upload test
 --skip_ping                   Skip the ping test
 --[no]report_results          Whether to report Speedtest results to server

These options override the speedtest config parameters:
 -d, --num_downloads NUM       Number of simultaneous downloads
 -s, --download_size SIZE      Download size in bytes
 -t, --upload_size SIZE        Upload size in bytes
 -u, --num_uploads NUM         Number of simultaneous uploads
 --min_transfer_runtime TIME   Minimum transfer time in milliseconds
 --max_transfer_runtime TIME   Maximum transfer time in milliseconds
 --min_transfer_intervals NUM  Short moving average intervals
 --max_transfer_intervals NUM  Long moving average intervals
 --max_transfer_variance NUM   Max difference between moving averages
 --interval_millis TIME        Interval size in milliseconds
 --ping_runtime TIME           Ping runtime in milliseconds
 --ping_timeout TIME           Ping timeout in milliseconds
 --exponential_moving_average  Use exponential instead of simple moving average
)USAGE";

}  // namespace

bool ParseOptions(int argc, char *argv[], Options *options) {
  assert(options != nullptr);
  options->usage = false;
  options->verbose = false;
  options->global_url = http::Url(kDefaultHost);
  options->global = false;
  options->user_agent = "";
  options->progress_millis = 0;
  options->disable_dns_cache = false;
  options->max_connections = 0;
  options->exponential_moving_average = false;
  options->skip_download = false;
  options->skip_upload = false;
  options->skip_ping = false;
  options->report_results = true;

  options->num_downloads = 0;
  options->download_bytes = 0;
  options->num_uploads = 0;
  options->upload_bytes = 0;
  options->min_transfer_runtime = 0;
  options->max_transfer_runtime = 0;
  options->min_transfer_intervals = 0;
  options->max_transfer_intervals = 0;
  options->max_transfer_variance = 0.0;
  options->interval_millis = 0;
  options->ping_runtime_millis = 0;
  options->ping_timeout_millis = 0;

  options->regional_urls.clear();

  if (!options->global_url.ok()) {
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
        options->global_url = url;
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
        options->progress_millis = progress;
        break;
      }
      case 's':
        if (!ParseSize(optarg, &options->download_bytes)) {
          std::cerr << "Invalid download size '" << optarg << "'\n";
          return false;
        }
        break;
      case 't':
        if (!ParseSize(optarg, &options->upload_bytes)) {
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
      case kOptReportResults:
        options->report_results = true;
        break;
      case kOptSkipDownload:
        options->skip_download = true;
        break;
      case kOptSkipUpload:
        options->skip_upload = true;
        break;
      case kOptSkipPing:
        options->skip_ping = true;
        break;
      case kOptNoReportResults:
        options->report_results = false;
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
          std::cerr << "Minimum transfer runtime must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->min_transfer_runtime = transfer_time;
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
          std::cerr << "Maximum transfer runtime must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->max_transfer_runtime = transfer_time;
        break;
      }
      case kOptMinTransferIntervals: {
        long intervals;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &intervals)) {
          std::cerr << "Could not parse minimum transfer intervals '"
                    << optarg << "'\n";
          return false;
        }
        if (intervals < 0) {
          std::cerr << "Minimum transfer intervals must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->min_transfer_intervals = static_cast<int>(intervals);
        break;
      }
      case kOptMaxTransferIntervals: {
        long intervals;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &intervals)) {
          std::cerr << "Could not parse maximum transfer intervals '"
                    << optarg << "'\n";
          return false;
        }
        if (intervals < 0) {
          std::cerr << "Maximum transfer intervals must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->max_transfer_intervals = static_cast<int>(intervals);
        break;
      }
      case kOptMaxTransferVariance: {
        double variance;
        char *endptr;
        if (!ParseDouble(optarg, &endptr, &variance)) {
          std::cerr << "Could not parse variance '" << optarg << "'\n";
          return false;
        }
        if (variance < 0) {
          std::cerr << "Variances must be nonnegative, got " << optarg << "'\n";
          return false;
        }
        options->max_transfer_variance = variance;
        break;
      }
      case kOptIntervalMillis: {
        long interval_millis;
        char *endptr;
        if (!ParseLong(optarg, &endptr, &interval_millis)) {
          std::cerr << "Could not parse interval time '" << optarg << "'\n";
          return false;
        }
        if (interval_millis < 0) {
          std::cerr << "Interval time must be nonnegative, got "
                    << optarg << "'\n";
          return false;
        }
        options->interval_millis = interval_millis;
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
        options->ping_runtime_millis = ping_runtime;
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
        options->ping_timeout_millis = ping_timeout;
        break;
      }
      case kOptExponentialMovingAverage:
        options->exponential_moving_average = true;
        break;
      case kOptServerId:
        // --serverid is accepted but ignored, for backwards compatibility.
        break;
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
      options->regional_urls.emplace_back(url);
    }
  }
  if (options->regional_urls.empty()) {
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
      << "Global host: " << options.global_url.url() << "\n"
      << "Global: " << (options.global ? "true" : "false") << "\n"
      << "User agent: " << options.user_agent << "\n"
      << "Progress interval: " << options.progress_millis << " ms\n"
      << "Disable DNS cache: "
      << (options.disable_dns_cache ? "true" : "false") << "\n"
      << "Max connections: " << options.max_connections << "\n"
      << "Skip download: "
      << (options.skip_download ? "true" : "false") << "\n"
      << "Skip upload: "
      << (options.skip_upload ? "true" : "false") << "\n"
      << "Skip ping: "
      << (options.skip_ping ? "true" : "false") << "\n"
      << "Report results: "
      << (options.report_results ? "true" : "false") << "\n"
      << "Number of downloads: " << options.num_downloads << "\n"
      << "Download size: " << options.download_bytes << " bytes\n"
      << "Number of uploads: " << options.num_uploads << "\n"
      << "Upload size: " << options.upload_bytes << " bytes\n"
      << "Min transfer runtime: " << options.min_transfer_runtime << " ms\n"
      << "Max transfer runtime: " << options.max_transfer_runtime << " ms\n"
      << "Min transfer intervals: " << options.min_transfer_intervals << "\n"
      << "Max transfer intervals: " << options.max_transfer_intervals << "\n"
      << "Max transfer variance: " << options.max_transfer_variance << "\n"
      << "Interval size: " << options.interval_millis << " ms\n"
      << "Ping runtime: " << options.ping_runtime_millis << " ms\n"
      << "Ping timeout: " << options.ping_timeout_millis << " ms\n"
      << "Exponential moving average: "
      << (options.exponential_moving_average ? "true" : "false") << "\n"
      << "Hosts:\n";
  for (const http::Url &host : options.regional_urls) {
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
