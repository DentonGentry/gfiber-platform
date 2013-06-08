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

#define CHECK_INTERVAL 300 // seconds

int kill_child = 0;

void sighandler(int sig) {
  kill_child = 1;
}

int main(int argc, const char **argv) {
  time_t old_time;
  struct stat fst;
  int rc, status = -1;
  int fd;
  mode_t old_mask;

  if (argc < 3) {
    fprintf(stderr, "Usage: %s <keepalive_file> <command>\n"
            " This tool spawns the command given as argument in a separate"
            " process and it keeps checking on both the process and its"
            " <keepalive_file> every %d seconds, to see if the process is still"
            " running and the file was updated in the meantime.\n",
            argv[0], CHECK_INTERVAL);
    return -1;
  }

  signal(SIGTERM, sighandler);
  signal(SIGHUP, sighandler);

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
  // spawn the child process
  pid_t pid = fork();
  switch (pid) {
  case -1:
    perror("alivemonitor: fork failed");
    return -1;
  case 0:
    if (setpgid(0, 0)) {
      perror("alivemonitor: setpgid failed");
      return -1;
    }
    execvp(argv[2], (char *const*) (argv + 2));
    perror("alivemonitor: execv failed");
    return -1;
  default:
    break;
  }

  sleep(CHECK_INTERVAL);
  while (!kill_child) {
    rc = waitpid(pid, &status, WNOHANG);
    switch (rc) {
    case -1:
      perror("alivemonitor: waitpid failed");
      return -1;
    case 0:
      // child hasn't changed its status
      break;
    default:
      fprintf(stderr, "alivemonitor: child pid %d exited with status %d\n",
              pid, status);
      return WEXITSTATUS(status);
    }

    memset(&fst, 0, sizeof(fst));
    if (stat(argv[1], &fst) != 0) {
      perror("alivemonitor: stat failed");
      break;
    }

    if (fst.st_mtime == old_time) {
      fprintf(stderr, "alivemonitor: keepalive file has not been changed"
              " in the last %d seconds; last access: %s",
              CHECK_INTERVAL, ctime(&old_time));
      break;
    }
    old_time = fst.st_mtime;
    sleep(CHECK_INTERVAL);
  }

  fprintf(stderr, "alivemonitor: will kill child pid %d\n", pid);
  // Send kill signal to whole process group.
  kill(-pid, SIGKILL);
  return -1;
}
