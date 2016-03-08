#include "utils.h"

#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

namespace libexperiments_utils {

void log(const char* cstr, ...) {
  va_list va;
  va_start(va, cstr);
  vprintf(cstr, va);
  va_end(va);
  printf("\n");
  fflush(stdout);
}

void log_perror(int err, const char* cstr, ...) {
  va_list va;
  va_start(va, cstr);
  vprintf(cstr, va);
  va_end(va);
  char strerrbuf[1024] = {'\0'};
  printf("'%s'[%d]\n", strerror_r(err, strerrbuf, sizeof(strerrbuf)), err);
  fflush(stdout);
}

uint64_t us_elapse(uint64_t start_time_us) {
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return tv.tv_sec * kUsecsPerSec + tv.tv_nsec / kNsecsPerUsec - start_time_us;
}

void us_sleep(uint64_t usecs) {
  uint64_t nsecs = kNsecsPerUsec * usecs;
  struct timespec tv;
  // tv_nsec field must be [0..kNsecsPerSec-1]
  tv.tv_sec = nsecs / kNsecsPerSec;
  tv.tv_nsec = nsecs % kNsecsPerSec;
  nanosleep(&tv, NULL);
}

// Maximum output (stdout+stderr) accepted by run_cmd()
const int kMaxRunCmdOutput = 4 * 1024 * 1024;

static int nice_snprintf(char *str, size_t size, const char *format, ...) {
  va_list ap;
  int bi;
  va_start(ap, format);
  // http://stackoverflow.com/a/100991
  bi = vsnprintf(str, size, format, ap);
  va_end(ap);
  if (bi > size) {
    // From printf(3):
    // "snprintf() [returns] the number of characters (not including the
    // trailing '\0') which would have been written to the final string
    // if enough space had been available" [printf(3)]
    bi = size;
  }
  return bi;
}

/* snprintf's a run_cmd command */
int snprintf_cmd(char *buf, int bufsize, const std::vector<std::string> &cmd) {
  int bi = 0;

  // ensure we always return something valid
  buf[0] = '\0';
  for (const auto &item : cmd) {
    bool blanks = (item.find_first_of(" \n\r\t") != std::string::npos);
    if (blanks)
      bi += nice_snprintf(buf+bi, bufsize-bi, "\"");
    for (int i = 0; i < item.length() && bi < bufsize; ++i) {
      if (isprint(item.at(i)))
        bi += nice_snprintf(buf+bi, bufsize-bi, "%c", item.at(i));
      else if (item.at(i) == '\n')
        bi += nice_snprintf(buf+bi, bufsize-bi, "\\n");
      else
        bi += nice_snprintf(buf+bi, bufsize-bi, "\\x%02x", item.at(i));
    }
    if (blanks)
      bi += nice_snprintf(buf+bi, bufsize-bi, "\"");
    bi += nice_snprintf(buf+bi, bufsize-bi, " ");
  }
  return bi;
}

int run_cmd(const std::vector<std::string> &cmd, const std::string &in,
            int *status,
            std::ostream *out,
            std::ostream *err,
            int64_t timeout_usec) {
  if (cmd.empty() || cmd[0].empty()) {
    *status = -1;
    return -1;
  }

  int pipe_in[2];
  int pipe_out[2];
  int pipe_err[2];
  int pid;

  // init the 3 pipes
  int ret;
  for (auto the_pipe : { pipe_in, pipe_out, pipe_err }) {
    if ((ret = pipe(the_pipe)) < 0) {
      log_perror(errno, "run_cmd:Error-pipe failed-");
      return -1;
    }
  }

  char cmd_buf[1024];
  snprintf_cmd(cmd_buf, sizeof(cmd_buf), cmd);
  log("run_cmd:running command: %s", cmd_buf);

  pid = fork();
  if (pid == 0) {
    // child: set stdin/stdout/stderr
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_err[1], STDERR_FILENO);

    // close unused pipe ends
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_err[0]);
    close(pipe_in[0]);
    close(pipe_out[1]);
    close(pipe_err[1]);

    // convert strings to "const char *" and "char * []"
    const char *file = cmd[0].c_str();
    char *argv[cmd.size() + 1];
    for (int i = 0; i < cmd.size(); ++i)
      argv[i] = const_cast<char *>(cmd[i].c_str());
    argv[cmd.size()] = NULL;
    // run command
    execvp(file, argv);
    // exec() functions return only if an error has occurred
    _exit(errno);
  }

  // parent: close unused pipe ends
  close(pipe_in[0]);
  close(pipe_out[1]);
  close(pipe_err[1]);
  // process stdin
  if (!in.empty()) {
    if ((ret = write(pipe_in[1], in.c_str(), in.length())) < in.length()) {
      log_perror(errno, "run_cmd:Error-write() failed-");
      // kill the child
      kill(pid, SIGKILL);
      wait(NULL);
      return -4;
    }
  }
  close(pipe_in[1]);

  // start reading stdout/stderr
  struct FancyPipe {
      int fd;
      std::ostream *stream_ptr;
  } fancypipes[] = {
      { pipe_out[0], out },
      { pipe_err[0], err },
  };
  fd_set fdread;
  char buf[1024];

  int total_output = 0;
  int retcode = 0;
  while (fancypipes[0].fd >= 0 || fancypipes[1].fd >= 0) {
    if (total_output > kMaxRunCmdOutput) {
      log("run_cmd:Error-command output is too large (%i bytes > %i)",
          total_output, kMaxRunCmdOutput);
      // kill the child
      kill(pid, SIGKILL);
      retcode = -3;
      break;
    }
    FD_ZERO(&fdread);
    int max_fd = -1;
    struct timeval tv, *timeout = NULL;
    if (timeout_usec >= 0) {
      tv.tv_sec = timeout_usec / 1000000;
      tv.tv_usec = (timeout_usec % 1000000) * 1000000;
      timeout = &tv;
    }
    for (const auto &fancypipe : fancypipes) {
      if (fancypipe.fd >= 0) {
        FD_SET(fancypipe.fd, &fdread);
        max_fd = MAX(max_fd, fancypipe.fd);
      }
    }
    int select_ret = select(max_fd + 1, &fdread, NULL, NULL, timeout);
    if (select_ret == 0) {
      // timeout
      log("run_cmd:Error-command timed out");
      // kill the child
      kill(pid, SIGKILL);
      retcode = -2;
      break;
    } else if (select_ret == -1) {
      if (errno == EINTR) {
        // interrupted by signal
        retcode = -1;
        break;
      }
      log_perror(errno, "run_cmd:Error-pipe select failed-");
      retcode = -1;
      break;
    }
    for (auto &fancypipe : fancypipes) {
      if (fancypipe.fd >= 0 && FD_ISSET(fancypipe.fd, &fdread)) {
        ssize_t len;
        len = read(fancypipe.fd, buf, sizeof(buf));
        if (len <= 0) {
          close(fancypipe.fd);
          fancypipe.fd = -1;
          if (len < 0)
            retcode = -1;
          continue;
        }
        total_output += len;
        if (fancypipe.stream_ptr)
          fancypipe.stream_ptr->write(buf, len);
      }
    }
  }

  *status = 0;
  wait(status);
  // interpret child exit status
  if (WIFEXITED(*status))
    *status = WEXITSTATUS(*status);
  return retcode;
}

//
// String printf functions, ported from stringprintf.cc/h
//

void StringAppendV(std::string* dst, const char* format, va_list ap) {
  // First try with a small fixed size buffer
  static const int kSpaceLength = 1024;
  char space[kSpaceLength];

  // It's possible for methods that use a va_list to invalidate
  // the data in it upon use.  The fix is to make a copy
  // of the structure before using it and use that copy instead.
  va_list backup_ap;
  va_copy(backup_ap, ap);
  int result = vsnprintf(space, kSpaceLength, format, backup_ap);
  va_end(backup_ap);

  if (result < kSpaceLength) {
    if (result >= 0) {
      // Normal case -- everything fit.
      dst->append(space, result);
      return;
    }
    if (result < 0) {
      // Just an error.
      return;
    }
  }

  // Increase the buffer size to the size requested by vsnprintf,
  // plus one for the closing \0.
  int length = result+1;
  char* buf = new char[length];

  // Restore the va_list before we use it again
  va_copy(backup_ap, ap);
  result = vsnprintf(buf, length, format, backup_ap);
  va_end(backup_ap);

  if (result >= 0 && result < length) {
    // It fit
    dst->append(buf, result);
  }
  delete[] buf;
}

std::string StringPrintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

}  // namespace libexperiments_utils
