// Copyright 2012 Google Inc. All Rights Reserved.
// Author: kedong@google.com (Ke Dong)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERITY_START "[VERITY-START]"
#define VERITY_STOP  "[VERITY-STOP]"
#define VERITY_START_SIZE  "[VERITY-START-SIZE]"
#define VERITY_STOP_SIZE   "[VERITY-STOP-SIZE]"
#define HEADER_SIZE 16
#define BLOCK_SIZE  4096
#define INFO_LENGTH ((BLOCK_SIZE)-(HEADER_SIZE))

static int readHeader(FILE *fd, char *buffer, size_t buffer_size) {
  size_t rd_cnt;

  if (buffer_size < INFO_LENGTH) {
    fprintf(stderr, "buffer not large enough.  supplied=%d need=%d\n",
            buffer_size, INFO_LENGTH);
    return -1;
  }

  fseek(fd, HEADER_SIZE, SEEK_SET);
  rd_cnt = fread(buffer, 1, buffer_size, fd);
  if (rd_cnt != buffer_size) {
    perror("fread");
    fprintf(stderr, "  read %u and expected %u\n",
            (unsigned int)rd_cnt, (unsigned int)buffer_size);
    return -1;
  }
  return 0;
}

int readVerityHashSize(const char* fname) {
  int file_size;
  char buffer[INFO_LENGTH+1];
  FILE *fd = NULL;
  char *start=NULL;
  char *stop=NULL;

  memset(buffer, 0, sizeof(buffer));
  fd = fopen(fname, "rb");
  if (NULL == fd) {
    perror(fname);
    return -1;
  }

  fseek(fd, 0, SEEK_END);
  file_size = ftell(fd);
  if (file_size < 0) {
    perror(fname);
    fclose(fd);
    return -1;
  }

  rewind(fd);
  if (readHeader(fd, buffer, sizeof(buffer)-1)) {
    fprintf(stderr, "Cannot read the verity header.\n");
    fclose(fd);
    return -1;
  }
  fclose(fd);

  start = strstr(buffer, VERITY_START_SIZE);
  if (!start) {
    fprintf(stderr, "Cannot find verity-size start\n");
    return -1;
  }

  start += strlen(VERITY_START_SIZE);
  stop = strstr(start, VERITY_STOP_SIZE);
  if (!stop) {
    fprintf(stderr, "Cannot find verity-size stop\n");
    return -1;
  }
  stop[0] = 0;
  fprintf(stdout, "%s", start);
  return 0;
}


int readVerityParams(const char* fname) {
  FILE *fd = NULL;
  char buffer[INFO_LENGTH+1];
  char* start;
  char* stop;

  memset(buffer, 0, sizeof(buffer));

  fd = fopen(fname, "rb");
  if (NULL == fd) {
    perror(fname);
    return -1;
  }

  if (readHeader(fd, buffer, sizeof(buffer)-1)) {
    fprintf(stderr, "Cannot read the verity header.\n");
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
