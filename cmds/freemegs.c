#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MEG (1024 * 1024)

unsigned long long monotime()
{
  struct timespec ts;
  unsigned long long msec;

  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    perror("clock_gettime");
    exit(1);
  }

  msec = ts.tv_nsec / 1000000;
  msec += ts.tv_sec * 1000;
  return msec;
}

int main(int argc, char **argv)
{
  int megs;
  unsigned int offset;
  char *mem;
  unsigned long long start, end, delta;

  srandom(0x12345678);

  for (megs = 0; megs < 4096; megs++) {
    start = monotime();

    if ((mem = malloc(MEG)) == NULL)
      break;

    for (offset = 0; offset < MEG; offset += 4) {
      /* Fill the space with a pseudo-random sequence, to ensure it
       * does not compress well. One purpose of this program is to
       * evaluate the effectiveness of zram, a compressed swap */
      *(unsigned int *)(mem + offset) = random();
    }

    end = monotime();
    delta = end - start;

    if (delta > 1000) {
      printf("Allocated %d Megabytes\n", megs);
      exit (0);
    }

    printf("Allocated Megabyte #%3d in %lld msec\n", megs+1, delta);
    fflush(stdout);
  }

  exit(0);
}
