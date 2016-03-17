#ifndef _H_LOGUPLOAD_CLIENT_LOG_UPLOADER_H_
#define _H_LOGUPLOAD_CLIENT_LOG_UPLOADER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "kvextract.h"

#define LOG_MARKER_START "*LOG_UPLOAD_START*"
#define LOG_MARKER_END "*LOG_UPLOAD_END*"
#define LOG_MARKER_START_LINE "<7>*LOG_UPLOAD_START*\n"
#define LOG_MARKER_END_LINE "<7>*LOG_UPLOAD_END*\n"

struct upload_config {
  char server[1024];
  char logtype[MAX_KV_LENGTH];
  int upload_all;
  int use_stdout;
  int use_stdin;
  int freq;
  char upload_target[1024];
};

struct log_parse_params {
  struct upload_config* config;
  void* user_data;
  int (*read_log_data)(char* buffer, int len, void* user_data);
  const char* dev_kmsg_path;
  const char* version_path;
  const char* ntp_synced_path;
  uint64_t last_log_counter;
  char* log_buffer;
  unsigned long total_read; // in sizeof(log_buffer), out bytes used
  char* line_buffer;
  int line_buffer_size;
  int last_line_valid;
};

// Returns a pointer to the start of the valid log data which will be
// either the log_buffer pointer itself, or somewhere within its
// range.
char* parse_and_consume_log_data(struct log_parse_params* params);

// This writes the data to the log that logmark-once would normally do, but
// we do it here instead so we can have better time precision since the
// 'date' command we have doesn't support subsecond accuracy but the logs server
// will parse it.
int logmark_once(const char* output_path, const char* version_path,
    const char* ntp_sync_path);

// Rewrite any MAC addresses of the form 00:11:22:33:44:55 (or similar)
// as anonids like ABCDEF.
unsigned long suppress_mac_addresses(char *line, ssize_t len);

// initialize a random key for anonymization.
void default_consensus_key();

#ifdef __cplusplus
}
#endif

#endif  // _H_LOGUPLOAD_CLIENT_LOG_UPLOADER_H_
