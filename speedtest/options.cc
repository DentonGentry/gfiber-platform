#include "options.h"

#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include "url.h"

namespace speedtest {
namespace {

const char *kDefaultHost = "speedtest.googlefiber.net";

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

const char *kShortOpts = "d:u:t:n:p:vh";
struct option kLongOpts[] = {
    {"help", no_argument, nullptr, 'h'},
    {"number", required_argument, nullptr, 'n'},
    {"download_size", required_argument, nullptr, 'd'},
    {"upload_size", required_argument, nullptr, 'u'},
    {"progress", required_argument, nullptr, 'p'},
    {"time", required_argument, nullptr, 't'},
    {"verbose", no_argument, nullptr, 'v'},
    {nullptr, 0, nullptr, 0},
};
const int kDefaultNumber = 10;
const int kDefaultDownloadSize = 10000000;
const int kDefaultUploadSize = 10000000;
const int kDefaultTimeMillis = 5000;
const int kMaxNumber = 1000;
const int kMaxProgress = 1000000;

}  // namespace

bool ParseOptions(int argc, char *argv[], Options *options) {
  assert(options != nullptr);
  options->number = kDefaultNumber;
  options->download_size = kDefaultDownloadSize;
  options->upload_size = kDefaultUploadSize;
  options->time_millis = kDefaultTimeMillis;
  options->progress_millis = 0;
  options->verbose = false;
  options->usage = false;
  options->hosts.clear();

  // Manually set this to 0 to allow repeated calls
  optind = 0;

  int opt = 0, long_index = 0;
  while ((opt = getopt_long(argc, argv,
                            kShortOpts, kLongOpts, &long_index)) != -1) {
    switch (opt) {
      case 'd':
        if (!ParseSize(optarg, &options->download_size)) {
          std::cerr << "Invalid download size '" << optarg << "'\n";
          return false;
        }
        break;
      case 'u':
        if (!ParseSize(optarg, &options->upload_size)) {
          std::cerr << "Invalid upload size '" << optarg << "'\n";
          return false;
        }
        break;
      case 't':
        options->time_millis = atoi(optarg);
        break;
      case 'v':
        options->verbose = true;
        break;
      case 'h':
        options->usage = true;
        return true;
      case 'n': {
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
        options->number = static_cast<int>(number);
        break;
      }
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
    options->hosts.emplace_back(http::Url(kDefaultHost));
  }
  return true;
}

void PrintOptions(const Options &options) {
  PrintOptions(std::cout, options);
}

void PrintOptions(std::ostream &out, const Options &options) {
  out << "Number: " << options.number << "\n"
      << "Upload size: " << options.upload_size << "\n"
      << "Download size: " << options.download_size << "\n"
      << "Time: " << options.time_millis << " ms\n"
      << "Progress interval: " << options.progress_millis << " ms\n"
      << "Verbose: " << (options.verbose ? "true" : "false") << "\n"
      << "Usage: " << (options.usage ? "true" : "false") << "\n"
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
  out << basename(app_name) << ": run an HTTP speedtest\n\n"
      << "Usage: speedtest [options] <host> ...\n"
      << " -h, --help                This help text\n"
      << " -n, --number NUM          Number of simultaneous transfers\n"
      << " -d, --download_size SIZE  Download size in bytes\n"
      << " -u, --upload_size SIZE    Upload size in bytes\n"
      << " -t, --time TIME           Time per test in milliseconds\n"
      << " -p, --progress TIME       Progress intervals in milliseconds\n"
      << " -v, --verbose             Verbose output\n";
}

}  // namespace speedtest
