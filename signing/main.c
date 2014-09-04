// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "readverity.h"

static void usage() {
    fprintf(stderr, "Usage: readverity [-s] <path-to-sign>\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {

  int size_mode = 0;
  int opt;

  while ((opt = getopt(argc, argv, "s")) != -1) {
    switch (opt) {
      case 's':
        size_mode = 1;
        break;

      default:
        usage();
        break;
    }
  }

  if (optind != (argc - 1)) {
    usage();
  }

  if (size_mode) {
    if (readVerityHashSize(argv[optind])) {
      return 2;
    }
  } else {
    if (readVerityParams(argv[optind])) {
      return 2;
    }
  }

  return 0;
}
