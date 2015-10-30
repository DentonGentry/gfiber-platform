#include "stacktrace.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <sys/types.h>  // for waitpid
#include <sys/wait.h>  // for waitpid
#include <unistd.h>

// We need this because strlen() isn't safe to call from a signal handler.
static int safe_strlen(const char *s) {
  int i = 0;
  while (s[i] != '\0')
    ++i;
  return i;
}

#define WRITELEN(s, l) do {if (write(2, s, l)) {}} while (0)
#define WRITE(s) do {if (write(2, s, sizeof(s))) {}} while (0)
#define WRITEINT(i) { \
    char *str = format_uint(i); \
    WRITELEN(str, safe_strlen(str)); \
}


// We need this because sprintf() isn't safe to call from a signal handler.
char *format_uint(unsigned int i) {
  static char str[100];
  char *p = str + sizeof(str) - 1;
  *(--p) = '\0';
  do {
    *(--p) = '0' + (i % 10);
    i /= 10;
  } while (i > 0);
  return p;
}

static pid_t gettid(void) {
  // According to 'man gettid', this function is not in libc, so you need
  // to call syscall() yourself.  Seems to be true.
  return syscall(__NR_gettid);
}

void stacktrace(void) {
  pid_t pid, trace_tid = gettid();

  if ((pid = fork()) > 0) {
// For some reason, if we call waitpid() here, gdb (7.3.1) isn't able to
// get a valid stack trace.  If we use syscall(__NR_waitpid), then it
// works fine.
#if defined(__MIPSEL__) || defined(_MIPSEB_)
    syscall(__NR_waitpid, pid, 0);
#else
    waitpid(pid, NULL, 0);
#endif
  } else if (pid == 0) {
    char *argv[] = {(char*)"stacktrace", format_uint(trace_tid), NULL};
    execv("/usr/bin/stacktrace", argv);
  } else {
    int e = errno;
    WRITE("ERROR: fork failed?!  code=");
    WRITEINT(e);
  }
}

void stacktrace_sighandler(int sig) {
  WRITE("\nExiting thread ");
  pid_t tid = gettid();
  WRITEINT(tid);
  WRITE(" on signal ");
  WRITEINT(sig);
  WRITE("\n");

  stacktrace();

  /*
   * We have to generate a signal *other* than the one we received, because
   * signals aren't re-entrant.  But we want a proper "die!" signal so that
   * we can still get a core dump.  Stack traces are nice, but so are cores
   * sometimes.
   */
  sig = (sig == SIGSEGV) ? SIGBUS : SIGSEGV;
  signal(sig, SIG_DFL);
  kill(getpid(), sig);

  // should never get here, but just in case...
  abort();
}

void stacktrace_setup(void) {
  signal(SIGSEGV, stacktrace_sighandler);
  signal(SIGBUS, stacktrace_sighandler);
  signal(SIGFPE, stacktrace_sighandler);
  signal(SIGILL, stacktrace_sighandler);
}
