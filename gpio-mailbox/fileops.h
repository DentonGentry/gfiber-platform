#include <stdint.h>
#include <stdbool.h>

#ifndef FILEOPS_H_
#define FILEOPS_H_

// Read a file containing a single short integer.
long read_file_long(const char *filename);

void __write_file_longlong(const char *filename, long long *oldv,
                           long long newv, int atomic);

void __write_file_int(const char *filename, int *oldv, int newv, int atomic);

void __write_file_double(const char *filename, double *oldv, double newv,
                         int atomic);

static void write_file_longlong_atomic(const char *filename, long long *oldv,
                                       long long newv ) {
  __write_file_longlong(filename, oldv, newv, 1);
}

static void write_file_int_atomic(const char *filename, int *oldv, int newv) {
  __write_file_int(filename, oldv, newv, 1);
}

static void write_file_double_atomic(const char *filename, double *oldv,
                                     double newv) {
  __write_file_double(filename, oldv, newv, 1);
}

static void write_file_longlong(const char *filename,
                                long long *oldv, long long newv ) {
  __write_file_longlong(filename, oldv, newv, 0);
}

static void write_file_int(const char *filename, int *oldv,
                           int newv) {
  __write_file_int(filename, oldv, newv, 0);
}

static void write_file_double(const char *filename, double *oldv,
                              double newv) {
  __write_file_double(filename, oldv, newv, 0);
}


#endif  /* FILEOPS_H_ */
