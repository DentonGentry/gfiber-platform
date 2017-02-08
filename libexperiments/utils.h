#ifndef _LIBEXPERIMENTS_UTILS_H_
#define _LIBEXPERIMENTS_UTILS_H_

#include <fcntl.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <ostream>
#include <vector>

//
// Subset of utils functions copied from vendor/google/mcastcapture/utils/.
//

// Namespace is needed to avoid conflicts when other modules with identical
// function names are linking against libexperiments.so.
namespace libexperiments_utils {

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

const int64_t kUsecsPerSec = 1000000LL;
const int64_t kNsecsPerSec = 1000000000LL;
const int64_t kNsecsPerUsec = 1000LL;

static inline int64_t secs_to_usecs(int64_t secs) {
  return secs * kUsecsPerSec;
}

void log(const char* cstr, ...)
    __attribute__((format(printf, 1, 2)));

void log_perror(int err, const char* cstr, ...)
    __attribute__((format(printf, 2, 3)));

// Measures elapsed time in usecs.
uint64_t us_elapse(uint64_t start_time_us);

void us_sleep(uint64_t usecs);

static inline bool file_exists(const char *name) {
  return access(name, F_OK) == 0;
}

static inline bool directory_exists(const char *path) {
  struct stat dir_stat;
  if (stat(path, &dir_stat) != 0) {
    return false;
  }
  return S_ISDIR(dir_stat.st_mode);
}

static inline bool touch_file(const char *name) {
  if (!name)
    return false;

  int fd = open(name, O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    log_perror(errno, "Cannot create file '%s':", name);
    return false;
  }
  close(fd);
  return true;
}

static inline bool rm_file(const char *name) {
  if (!name)
    return false;

  if (remove(name) < 0) {
    log_perror(errno, "Cannot rm file '%s':", name);
    return false;
  }
  return true;
}

static inline bool mv_file(const char *from_name, const char *to_name) {
  if (!from_name || !to_name)
    return false;

  // Note this uses rename() and hence only works on the same filesystem.
  if (rename(from_name, to_name) < 0) {
    log_perror(errno, "Cannot mv file '%s' to '%s':", from_name, to_name);
    return false;
  }
  return true;
}

// This function runs the command cmd (but not in a shell), providing the
// return code, stdout, and stderr. It also allows to specify the stdin,
// and a command timeout value (timeout_usec, use <0 to block indefinitely).
// Note that the timeout is fired only if the command stops producing
// either stdout or stderr. A process that periodically produces output
// will never be killed.
// Returns an extended error code:
// - 0 if everything is successful,
//  -1 if any step fails,
//  -2 if timeout, and
//  -3 if the command output (stdout+stderr) is too large (kMaxRunCmdOutput).
int run_cmd(const std::vector<std::string> &cmd, const std::string &in,
            int *status,
            std::ostream *out,
            std::ostream *err,
            int64_t timeout_usec);

//
// String printf functions, ported from stringprintf.cc/h
//

// Return a C++ string
std::string StringPrintf(const char* format, ...)
    __attribute__((format(printf, 1, 2)));

}  // namespace libexperiments_utils

#endif  // _LIBEXPERIMENTS_UTILS_H_
