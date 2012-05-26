// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)
#include <stdio.h>
#include <stdlib.h>
#include "readverity.h"

int main(int argc, char** argv) {

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <path-to-sign>\n", argv[0]);
    return 1;
  }

  if (readverity(argv[1]) < 0) {
    return 2;
  }

  return 0;
}
