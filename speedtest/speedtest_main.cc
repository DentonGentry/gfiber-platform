#include <cstdlib>
#include <iostream>
#include <memory>
#include "options.h"
#include "speedtest.h"

int main(int argc, char *argv[]) {
  speedtest::Options options;
  if (!speedtest::ParseOptions(argc, argv, &options) || options.usage) {
    speedtest::PrintUsage(argv[0]);
    std::exit(1);
  }
  if (options.verbose) {
    speedtest::PrintOptions(options);
  }
  speedtest::Speedtest speed(options);
  speed.Run();
  return 0;
}
