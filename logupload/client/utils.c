#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>

#include "utils.h"

ssize_t read_file_as_string(const char* file_path, char* data, int length) {
  if (!file_path || !data)
    return -1;
  int fd = open(file_path, O_RDONLY);
  if (fd < 0)
    return -1;
  ssize_t num_read = read(fd, data, length - 1);
  close(fd);
  if (num_read < 0)
    return -1;
  data[num_read] = '\0';
  return num_read;
}

void rstrip(char* string) {
  ssize_t l;
  if (!string)
    return;
  l = strlen(string) - 1;
  while (l >= 0 && isspace(string[l]))
    string[l--] = '\0';
}

int path_exists(const char* path) {
  return access(path, F_OK) == 0 ? 1 : 0;
}

uint64_t read_file_as_uint64(const char* file_path) {
  char buf[64];
  ssize_t num_read;
  int fd = open(file_path, O_RDONLY);
  if (fd < 0)
    return 0;
  num_read = read(fd, buf, sizeof(buf) -1);
  close(fd);
  if (num_read > 0) {
    buf[num_read] = '\0';
    return strtoull(buf, NULL, 10);
  }
  return 0;
}

int write_file_as_uint64(const char* file_path, uint64_t counter) {
  int fd = open(file_path, O_WRONLY | O_CREAT, RW_FILE_PERMISSIONS);
  if (fd < 0) {
    fprintf(stderr, "could not open file %s: %s\n", file_path, strerror(errno));
    return -1;
  }
  char data[64];
  int len = snprintf(data, sizeof(data), "%" PRIu64 "\n", counter);
  ssize_t num_written = write(fd, data, len);
  close(fd);
  if (num_written < len) {
    fprintf(stderr, "failed writing to file %s:  %s\n", file_path,
        strerror(errno));
    return -1;
  }
  return 0;
}

ssize_t write_to_file(const char* file_path, const char* data) {
  int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, RW_FILE_PERMISSIONS);
  if (fd < 0) {
    fprintf(stderr, "could not open %s for writing: %s\n", file_path,
        strerror(errno));
    return -1;
  }
  ssize_t len = strlen(data);
  ssize_t num_written = write(fd, data, len);
  close(fd);
  if (num_written < len) {
    fprintf(stderr, "failed writing to %s: %s\n", file_path, strerror(errno));
    return -1;
  }
  return num_written;
}

int parse_line_data(char* line, struct line_data* data) {
  // The format used here from the kernel is:
  // level,sequence,time,cont;text\n
  //   dictionary values
  // We ignore the 'cont' part because we just treat each line as its
  // own rather than worrying about continuation. We also don't care about
  // the dictionary data because that's generally already part of the text.
  char* comma_1 = strchr(line, ',');
  if (!comma_1) {
    return -1;
  }
  char* comma_2 = strchr(comma_1 + 1, ',');
  if (!comma_2) {
    return -1;
  }
  char* semi = strchr(comma_2 + 1, ';');
  if (!semi) {
    return -1;
  }
  char* newline = strchr(semi, '\n');
  if (!newline) {
    return -1;
  }

  // Now we know it has the lexical structure we are looking far, parse it
  // out into our struct.
  data->level = atoi(line);
  data->seq = strtoull(comma_1 + 1, NULL, 10);
  data->ts_nsec = strtoull(comma_2 + 1, NULL, 10);
  data->text = semi + 1;
  *(newline + 1) = '\0'; // terminate the string after the newline
  return 0;
}

int deflate_inplace(z_stream *strm, unsigned char* buf,
    unsigned long len, unsigned long *out_len) {
  int rv, used_out, used_in;
  unsigned char temp[1024]; // big enough to hold the header which is 11 bytes,
                            // plus more so we can eat into our data quicker

  deflateEnd(strm);
  rv = deflateInit(strm, 1);
  if (rv != Z_OK)
    return rv;

  // Set the input/output data fields in strm. We first use the temp buffer
  // to start the process, then after that we will re-use what's consumed
  // in our own buffer.
  strm->next_in = buf;
  strm->avail_in = len;
  strm->next_out = temp;
  strm->avail_out = sizeof(temp);

  // Do the first compression step
  rv = deflate(strm, Z_FINISH);
  if (rv == Z_STREAM_ERROR)
    return rv;
  used_out = strm->next_out - temp;
  used_in = len - strm->avail_in;
  if (used_out > used_in) {
    // Our output data is larger than the input data consumed, sad panda :(
    return Z_BUF_ERROR;
  }

  // Copy the data back to our buffer and use that from now on.
  memcpy(buf, temp, used_out);
  strm->next_out = buf + used_out;
  while (rv == Z_OK) {
    // Update how much room we have in the output buffer, there may be a point
    // where its consumed all the input data and we just need to provide more
    // output data as well.
    if (strm->avail_in) {
      strm->avail_out = strm->next_in - strm->next_out;
    } else {
      // All input data is consumed so we have the full buffer size to work
      // with here.
      strm->avail_out = buf + *out_len - strm->next_out;
    }
    rv = deflate(strm, Z_FINISH);
  }
  if (rv == Z_STREAM_END) {
    // Successfully did the compression.
    *out_len = strm->next_out - buf;
    return Z_OK;
  }

  // Failure.
  return rv;
}
