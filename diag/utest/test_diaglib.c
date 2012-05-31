/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagd tester related
 *
 */

/*--------------------------------------------------------------------------
 *
 * Includes
 *
 *--------------------------------------------------------------------------
 */
#include <asm/types.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "diagLibApis.h"

void test(int bufSize) {
  char *buffer = NULL;
  int rc;

  printf ("C API diag_get_info() Test: bufSize = %d\n", bufSize);

  if (bufSize > 0) {
    buffer = malloc(bufSize);
  }

  rc  = diag_get_info(buffer, bufSize);

  if (rc == DIAG_LIB_RC_OK) {
    printf ("%s\n", buffer);
  } else {
    printf ("diag_get_info() return error =  %d\n", rc);
  }

  if (buffer != NULL) {
    free (buffer);
    buffer = NULL;
  }
}


int main(int argc, char** argv) {
  int bufSize;

  if(argc != 2) {
    printf("Usage:test_diaglib <buffer size>\n");
    printf("      <buffer size> >= 4096\n");
    return 0;
  }

  sscanf(argv[1], "%d", &bufSize);
  test(bufSize);

  return 0;
}
