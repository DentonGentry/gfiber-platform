#ifndef _H_LOGUPLOAD_CLIENT_UTILS_H_
#define _H_LOGUPLOAD_CLIENT_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <zlib.h>

#define RW_FILE_PERMISSIONS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH |\
    S_IWOTH)

struct line_data {
  unsigned int level;
  uint64_t ts_nsec;
  uint64_t seq;
  char* text;
};

// Reads a file and puts the contents into the passed in string, returns
// number of chars read, -1 if failure occurs.
ssize_t read_file_as_string(const char* file_path, char* data, int length);

// Writes the specified data to the file_path, returns number of bytes written
// or -1 if failure. If all the data is not written, -1 is returned.
ssize_t write_to_file(const char* file_path, const char* data);

// Only for reading non-negative numbers, returns 0 if file doesn't exist or
// there is any other reason the file cannot be read.
uint64_t read_file_as_uint64(const char* file_path);

// Writes a uint64 value to the specified file path.
// Returns 0 on success.
int write_file_as_uint64(const char* file_path, uint64_t counter);

// Strip trailing whitespace from a string.
void rstrip(char* string);

// Returns 1 if the path exists, zero otherwise.
int path_exists(const char* path);

// Parses a line of kernel log data into the struct.
int parse_line_data(char* line, struct line_data* data);

// This will compress the data inplace in memory. This will have a problem
// if the data is incompressible; but that will never happen in our
// situation because we are compressing text data which will always lead
// to a size reduction. In the unlikely event we can't do this cleanly
// in place this function will return Z_BUF_ERROR.
// out_len should be set to the max size we can use in buf when this is
// called.
int deflate_inplace(z_stream *strm, unsigned char* buf,
    unsigned long len, unsigned long *out_len);

#ifdef __cplusplus
}
#endif

#endif  // _H_LOGUPLOAD_CLIENT_UTILS_H_
