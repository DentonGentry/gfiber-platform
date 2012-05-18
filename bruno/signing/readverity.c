#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERITY_START "[VERITY-START]"
#define VERITY_STOP  "[VERITY-STOP]"
#define HEADER_SIZE 16
#define BLOCK_SIZE  4096
#define INFO_LENGTH BLOCK_SIZE-HEADER_SIZE

int main(int argc, char** argv) {
  FILE *fd = NULL;
  char buffer[INFO_LENGTH];
  size_t rd_cnt;
  char* start;
  char* stop;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s path\n", argv[0]);
    exit(-1);
  }

  fd = fopen(argv[1], "rb");
  if (NULL == fd) {
    fprintf(stderr, "Failed to open file %s\n", argv[1]);
    fclose(fd);
    exit(-1);
  }

  fseek(fd, HEADER_SIZE, SEEK_SET);
  rd_cnt = fread(buffer, 1, INFO_LENGTH, fd);
  if (rd_cnt != INFO_LENGTH) {
    fprintf(stderr, "Failed to read, read %u and expected %u\n",
            (unsigned int)rd_cnt, (unsigned int)INFO_LENGTH);
    fclose(fd);
    exit(-1);
  }
  fclose(fd);

  start = strstr(buffer, VERITY_START);
  if (!start) {
    fprintf(stderr, "Cannot find the verity table\n");
    exit(-1);
  }

  start += strlen(VERITY_START);
  stop = strstr(start, VERITY_STOP);
  if (start && stop) {
    stop[0] = '\0';
    fprintf(stdout, "%s", start);
  }

  return 0;
}
