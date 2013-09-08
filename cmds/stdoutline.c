/*
 * Shared library to force stdout to be unbuffered.
 * This is useful when working with binaries we cannot
 * otherwise modify; LD_PRELOAD can force this
 * library to be loaded and execute its constructor.
 */

#include <stdio.h>

__attribute__((constructor))
void stdoutnobuffer()
{
  if (setvbuf(stdout, NULL, _IOLBF, 0) == 0) {
    printf("stdout set to line buffering.\n");
  } else {
    fprintf(stderr, "Unable to make stdout line buffered.\n");
  }
}
