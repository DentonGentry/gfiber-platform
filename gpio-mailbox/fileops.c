#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// read a file containing a single short integer.
long read_file_long(const char *filename) {
  char buf[32] = { 0 };
  long val;
  size_t got;
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    perror(filename);
    return -1;
  }

  got = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (got < 0) {
    perror(filename);
    return -1;
  }
  buf[got] = 0;
  return strtol(buf, NULL, 10);
}

// write a file containing the given string.
int write_file_string(const char *filename, const char *content) {
  int fd = open(filename, O_TRUNC|O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
  if (fd >= 0) {
    write(fd, content, strlen(content));
    close(fd);
    return 0;
  }
  perror(filename);
  return -1;
}

int write_file_string_atomic(const char *filename, const char *content) {
  char *tmpname = malloc(strlen(filename) + 4 + 1);
  int ret;
  sprintf(tmpname, "%s.tmp", filename);
  ret = write_file_string(tmpname, content);
  if (ret >= 0) {
    ret = rename(tmpname, filename);
    free(tmpname);
    return ret;
  }
  free(tmpname);
  return -1;
}


#define WRITE_TO_FILE(filename, oldv, newv, atomic, format) \
  if (!oldv || *oldv != newv) {                     \
    char buf[128];                                  \
    snprintf(buf, sizeof(buf), format, newv);       \
    buf[sizeof(buf)-1] = 0;                         \
    if (atomic)                                     \
      write_file_string_atomic(filename, buf);      \
    else                                            \
      write_file_string(filename, buf);             \
    if (oldv)                                       \
      *oldv = newv;                                 \
  }


void __write_file_longlong(const char *filename, long long *oldv, long long newv,
                        int atomic)
{
  WRITE_TO_FILE(filename, oldv, newv, atomic, "%lld")
}

void __write_file_int(const char *filename, int *oldv, int newv, int atomic)
{
  WRITE_TO_FILE(filename, oldv, newv, atomic, "%d")
}

void __write_file_double(const char *filename, double *oldv, double newv,
                        int atomic)
{
  WRITE_TO_FILE(filename, oldv, newv, atomic, "%.2f")
}

