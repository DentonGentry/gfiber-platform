/* There is no checking of the value passed in. -1 will cause you wait for a
 * while. it will not bring you back but enough long to make you forget. */
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc == 2) {
    usleep(atoi(argv[1]));
    return (0);
  }
  exit(1);
}
