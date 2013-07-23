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

#define DEFAULT_CHECK_INTERVAL 300 // seconds

int kill_parent = 0;

void sighandler(int sig) {
  kill_parent = 1;
}

int main(int argc, const char **argv) {
  time_t old_time;
  struct stat fst;
  int fd;
  mode_t old_mask;
  int check_interval;

  if (argc < 4) {
    fprintf(stderr,
            "Usage: %s <keepalive_file> <check_interval> <command>\n"
            " This tool spawns the command given as argument in a separate"
            " process and it keeps checking on both the process and its"
            " <keepalive_file> every <check_interval> seconds, to see if"
            " the process is still running and the file was updated in the"
            " meantime.\n", argv[0]);
    return -1;
  }

  signal(SIGTERM, sighandler);
  signal(SIGHUP, sighandler);

  check_interval = strtol(argv[2], NULL, 10);
  if (check_interval <= 0) check_interval = DEFAULT_CHECK_INTERVAL;

  // create the keepalive file if it doesn't already exist
  memset(&fst, 0, sizeof(fst));
  if (stat(argv[1], &fst) != 0) {
    old_mask = umask(0000);
    fd = creat(argv[1], 0666);
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

  fprintf(stderr, "alivemonitor: Start monitoring '%s' via '%s' every "
          "%d secs.\n", argv[3], argv[1], check_interval);

  // create a new process group with pgid=pid
  if (setpgid(0, 0)) {
    perror("alivemonitor: setpgid failed");
    return -1;
  }

  // spawn the child process
  pid_t p_pid = getpid();
  pid_t pid = fork();
  if (pid == -1) {
    perror("alivemonitor: fork failed");
    return -1;
  } else if (pid > 0) { // parent
    execvp(argv[3], (char *const*) (argv + 3));
    perror("alivemonitor: execv failed");
    return -1;
  }

  // from here: child

  sleep(check_interval);
  while (!kill_parent) {
    // check on the parent
    if (kill(p_pid, 0) == -1) {
      if (errno == ESRCH) {
        fprintf(stderr, "alivemonitor: parent pid %d exited.\n", p_pid);
        return 0;
      } else {
        perror("alivemonitor: kill(p_pid, 0) failed");
        break;
      }
    }

    memset(&fst, 0, sizeof(fst));
    if (stat(argv[1], &fst) != 0) {
      perror("alivemonitor: stat failed");
      break;
    }

    if (fst.st_mtime == old_time) {
      fprintf(stderr, "alivemonitor: keepalive file has not been changed"
              " in the last %d seconds; last access: %s\n",
              check_interval, ctime(&old_time));
      break;
    }
    old_time = fst.st_mtime;
    sleep(check_interval);
  }

  fprintf(stderr, "alivemonitor: kill parent process group %d\n", p_pid);
  // Send kill signal to whole process group.
  if (kill(-p_pid, SIGKILL))
    perror("alivemonitor: killing parent process group failed");

  // NOTE: Code after this point will not run since we just killed ourselves

  return -1;
}
