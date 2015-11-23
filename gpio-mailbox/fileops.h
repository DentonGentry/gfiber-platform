#include <stdint.h>
#include <stdbool.h>

#ifndef FILEOPS_H_
#define FILEOPS_H_

// Read a file containing a single short integer.
long read_file_long(const char *filename);

// write to a file
int write_file_string(const char *filename, const char *content);
void write_file_longlong(const char *filename, long long *oldv, long long newv);
void write_file_int(const char *filename, int *oldv, int newv);
void write_file_double(const char *filename, double *oldv, double newv);

// write to a tmp file then rename "atomicly"
int write_file_string_atomic(const char *filename, const char *content);
void write_file_longlong_atomic(const char *filename, long long *oldv,
                                long long newv);
void write_file_int_atomic(const char *filename, int *oldv, int newv);
void write_file_double_atomic(const char *filename, double *oldv, double newv);

#endif  /* FILEOPS_H_ */
