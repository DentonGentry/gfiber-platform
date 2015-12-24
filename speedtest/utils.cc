#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>

namespace speedtest {

long SystemTimeMicros() {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    std::cerr << "Error reading system time\n";
    std::exit(1);
  }
  return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

std::string to_string(long n)
{
  std::ostringstream s;
  s << n;
  return s.str();
}

int stoi(const std::string& str)
{
  int rc;
  std::istringstream n(str);

  if (!(n >> rc)) {
    std::cerr << "Not a number: " << str;
    std::exit(1);
  }

  return rc;
}

}  // namespace speedtest
