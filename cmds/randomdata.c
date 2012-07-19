/*
 * Code in this file is taken from _helpers.c in the bup project.
 *
 * Copyright (C) 2009-2012 Avery Pennarun.
 *
 * This program may be distributed under the terms of the GNU Library General
 * Public License, version 2.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


int main(int argc, char **argv)
{
  uint32_t buf[1024/4];
  unsigned long long len, blocknum;
  unsigned int seed;
  int ret;

  if (argc != 3) {
    fprintf(stderr,
            "Usage: %s <randomseed> <numbytes>\n"
            "  Writes <numbytes> bytes of random data to stdout, using\n"
            "  srandom(<randomseed>) for repeatability.  Use a seed of 0\n"
            "  to generate a different random sequence each time.\n"
            "\n"
            "  WARNING: This program is not random enough for crypto use.\n",
            argv[0]);
    return 1;
  }

  seed = (unsigned int)strtoul(argv[1], NULL, 0);
  len = strtoull(argv[2], NULL, 0);

  if (!seed)
    srandom(time(NULL) + getpid());
  else
    srandom(seed);

  for (blocknum = 0; blocknum < len/sizeof(buf); blocknum++)
  {
    unsigned i;
    for (i = 0; i < sizeof(buf)/sizeof(buf[0]); i++)
      buf[i] = random();
    ret = write(1, buf, sizeof(buf));
    if (ret < 0) {
      perror("write");
      return 1;
    }
  }

  // handle non-multiples of sizeof(buf)
  if (len % sizeof(buf))
  {
    unsigned i;
    for (i = 0; i < sizeof(buf)/sizeof(buf[0]); i++)
      buf[i] = random();
    ret = write(1, buf, len % sizeof(buf));
    if (ret < 0) {
      perror("write");
      return 1;
    }
  }

  return 0;
}
