#ifndef SPEEDTEST_OPTIONS_H
#define SPEEDTEST_OPTIONS_H

#include <iostream>
#include <string>
#include <vector>
#include "url.h"

namespace speedtest {

extern const char* kDefaultHost;

struct Options {
  std::vector<http::Url> hosts;
  int number;
  long download_size;
  long upload_size;
  int time_millis;
  int progress_millis;
  bool verbose;
  bool usage;
};

// Parse command line options putting results into 'options'
// Caller retains ownership of 'options'.
// Returns true on success, false on failure.
// Not threadsafe
bool ParseOptions(int argc, char *argv[], struct Options *options);

void PrintOptions(const Options &options);
void PrintOptions(std::ostream &out, const Options &options);
void PrintUsage(const char *app_path);
void PrintUsage(std::ostream &out, const char *app_name);

}  // namespace speedtest

#endif  // SPEEDTEST_OPTIONS_H
