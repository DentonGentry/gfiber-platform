#include "utils.h"

#include <cstdlib>
#include <iostream>

namespace speedtest {

long SystemTimeMicros() {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    std::cerr << "Error reading system time\n";
    std::exit(1);
  }
  return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

}  // namespace speedtest
