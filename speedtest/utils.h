#ifndef SPEEDTEST_UTILS_H
#define SPEEDTEST_UTILS_H

#include <string>

namespace speedtest {

// Return relative time in microseconds
// This isn't convertible to an absolute date and time
long SystemTimeMicros();

// Return a string representation of n
std::string to_string(long n);

// return an integer value from the string str.
int stoi(const std::string& str);

}  // namespace speedtst

#endif  // SPEEDTEST_UTILS_H
