// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERITY_START "[VERITY-START]"
#define VERITY_STOP  "[VERITY-STOP]"
#define HEADER_SIZE 16
#define BLOCK_SIZE  4096
#define INFO_LENGTH ((BLOCK_SIZE)-(HEADER_SIZE))

int readverity(const char* fname) {
  FILE *fd = NULL;
  char buffer[INFO_LENGTH];
  size_t rd_cnt;
  char* start;
  char* stop;

  fd = fopen(fname, "rb");
  if (NULL == fd) {
    perror(fname);
    return -1;
  }

  fseek(fd, HEADER_SIZE, SEEK_SET);
  rd_cnt = fread(buffer, 1, INFO_LENGTH, fd);
  if (rd_cnt != INFO_LENGTH) {
    fprintf(stderr, "Failed to read, read %u and expected %u\n",
            (unsigned int)rd_cnt, (unsigned int)INFO_LENGTH);
    fclose(fd);
    return -1;
  }
  fclose(fd);

  start = strstr(buffer, VERITY_START);
  if (!start) {
    fprintf(stderr, "Cannot find verity table start\n");
    return -1;
  }

  start += strlen(VERITY_START);
  stop = strstr(start, VERITY_STOP);
  if (!stop) {
    fprintf(stderr, "Cannot find verity table stop\n");
    return -1;
  }

  stop[0] = '\0';
  fprintf(stdout, "%s", start);
  return 0;
}
