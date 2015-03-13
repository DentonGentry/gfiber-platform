// This does the same work as the setsid command but waits
// for the child to exit and propates the childs exit status
// back to the caller.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv) {

  if (argc < 2) {
    printf("gsetsid [program] [args]\n");
    return 1;
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    perror("fork");
    return 1;
  }

  if (child_pid) {
    // This is the parent, wait for the child to exit.
    int status;
    if (waitpid(child_pid, &status, 0) < 0) {
      perror("waitpid");
      exit(1);
    }
    exit(WEXITSTATUS(status));
  }

  if (setsid() < 0) {
    perror("setsid");
    exit(1);
  }

  execvp(argv[1], &argv[1]);
  // exec never return unless there was an error
  perror("execvp");
  return 1;
}

