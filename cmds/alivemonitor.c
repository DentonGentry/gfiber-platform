#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define CRAZY_LONG_TIME 999999 // seconds
#define DEFAULT_TIMEOUT 300    // seconds


int kill_parent = 0;
const char *keepalive_file;
pid_t p_pid;
time_t old_time;


void sighandler(int sig) {
  kill_parent = 1;
}

static void usage(char *name) {
  fprintf(stderr,
          "\n%s - Monitors a process via a keepalive file.\n"
          "\nUsage:\n"
          "  %s <keepalive_file> <first_check> <incr_checks> <timeout> "
          "<command>\n"
          "    The keepalive logic runs in cycles. A cycle begins and ends\n"
          "    with a successful check of the <keepalive_file>, i.e., it was\n"
          "    touched since the last cycle. The first check starts\n"
          "    <first_check> secs after the cycle begins. Incremental checks\n"
          "    are done at <incr_checks> intervals, until <keepalive_file>\n"
          "    was found to be updated or <timeout> is reached. In the\n"
          "    former case, the cycle restarts, while in the latter\n"
          "    (timeout) case, the process is restarted and the cycle starts\n"
          "    again.\n\n", name, name);
}

// parse the given string into an integer and check it is inside the given
// range [low..high]. If not return the provided default.
int get_value(const char* str, int low, int high, int def) {
  int val = strtol(str, NULL, 10);
  if (val < low || val > high) {
    val = def;
  }
  return val;
}

// return the current (monotonic) time in secs
unsigned long now() {
  struct timespec tp;

  if (clock_gettime(CLOCK_MONOTONIC, &tp)) {
    perror("alivemonitor: clock_gettime failed.");
    exit(1);
  }
  return tp.tv_sec;
}

// Sleep a given amount of time, then check the parent.
// Return codes:
//   2: parent exited
//   1: no change in alive status
//   0: alive!
//  -1: system error, kill parent
int sleep_check_alive(int stime) {
  struct stat fst;

  if (stime)
    sleep(stime);

  if (kill_parent)
    return -1;

  // check on the parent
  if (kill(p_pid, 0) == -1) {
    if (errno == ESRCH) {
      fprintf(stderr, "alivemonitor: parent pid %d exited.\n", p_pid);
      return 2;
    } else {
      perror("alivemonitor: kill(p_pid, 0) failed");
      return -1;
    }
  }

  memset(&fst, 0, sizeof(fst));
  if (stat(keepalive_file, &fst) != 0) {
    perror("alivemonitor: stat failed");
    return -1;
  }

  if (fst.st_mtime == old_time) {
    return 1;
  }

  // alive!
  old_time = fst.st_mtime;
  return 0;
}


int main(int argc, const char **argv) {
  int fd;
  int timeout, first_check, incr_check;
  int status;
  struct stat fst;
  mode_t old_mask;
  pid_t pid;
  long start_time;
  char *keepalive_name;

  if (argc < 6) {
    usage(basename(argv[0]));
    return -1;
  }

  signal(SIGTERM, sighandler);
  signal(SIGHUP, sighandler);

  // <keepalive_file> <first_check> <incr_checks> <timeout> <command>
  keepalive_file = argv[1];
  keepalive_name = basename(keepalive_file);
  timeout = get_value(argv[4], 1, CRAZY_LONG_TIME, DEFAULT_TIMEOUT);
  first_check = get_value(argv[2], 1, timeout, timeout);
  incr_check = get_value(argv[3], 1, timeout - first_check,
                         timeout - first_check);

  // create the keepalive file if it doesn't already exist
  memset(&fst, 0, sizeof(fst));
  if (stat(keepalive_file, &fst) != 0) {
    old_mask = umask(0000);
    fd = creat(keepalive_file, 0666);
    if (fd < 0) {
      perror("alivemonitor: creat failed");
      return -1;
    }
    // Revert the umask to default so that the child doesn't
    // inherit the changed value.
    umask(old_mask);
    close(fd);
  }
  old_time = fst.st_mtime;

  fprintf(stderr, "alivemonitor: Start monitoring '%s' with timeout=%ds, "
          "first_check=%ds, incr_check=%ds\n",
          keepalive_file, timeout, first_check, incr_check);

  // create a new process group with pgid=pid
  if (setpgid(0, 0)) {
    perror("alivemonitor: setpgid failed");
    return -1;
  }

  // spawn the child process
  p_pid = getpid();
  pid = fork();
  if (pid == -1) {
    perror("alivemonitor: fork failed");
    return -1;
  } else if (pid > 0) { // parent
    execvp(argv[5], (char *const*) (argv + 5));
    perror("alivemonitor: execv failed");
    return -1;
  }

  // from here: child

  while (!kill_parent) {

    start_time = now();

    // sleep until first check
    status = sleep_check_alive(first_check);
    if (status == 2) // parent exited
      return 0;
    else if (status < 0) // system error, kill parent
      break;
    else if (!status) // alive!
      continue;

    // no sign of life yet, run the increments
    int time_passed = now() - start_time, cnt = 1;
    while (!kill_parent && time_passed < timeout) {
      fprintf(stderr, "alivemonitor(%s):%d-No sign of life @ %d/%d secs\n",
              keepalive_name, cnt++, time_passed, timeout);
      status = sleep_check_alive(incr_check);
      if (status == 2) // parent exited
        return 0;
      else if (status <= 0) // error or alive
        break;
      time_passed = now() - start_time;
    }
    if (status != 0)
      break;
  }

  fprintf(stderr, "alivemonitor(%s):Timeout! kill parent process group %d\n",
          keepalive_name, p_pid);
  // Send kill signal to whole process group.
  if (kill(-p_pid, SIGKILL))
    perror("alivemonitor: killing parent process group failed");

  // NOTE: Code after this point will not run since we just killed ourselves

  return -1;
}
