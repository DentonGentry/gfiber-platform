#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include "gtest/gtest.h"
#include "log_uploader.h"
#include "utils.h"

struct log_data {
  int level;
  uint64_t timestamp;
  uint64_t seq;
  const char* cont;
  const char* text;
  const char* used_text;
};

struct parser_progress {
  int curr_entry;
  int num_entries;
  int eos_count;
  struct log_data* my_data;
  struct log_data* post_eos_data;
};

// Read all the entries and return them, when we are done return an error
// and set errno to EAGAIN. At that point it should write out the marker
// to the path we specified, but this will be called again and we should
// then return what's in the post_eos_data which will be the start marker
// line its looking for. We can then track to make sure it was called
// the proper number of times.
int read_log_struct(char* log_buffer, int len, void* user_data) {
  struct parser_progress* progress = (struct parser_progress*) user_data;
  struct log_data* use_me;
  if (progress->curr_entry >= progress->num_entries) {
    progress->eos_count++;
    if (progress->eos_count != 2) {
      errno = EAGAIN;
      return -1;
    }
    use_me = progress->post_eos_data;
  } else {
    use_me = &(progress->my_data[progress->curr_entry++]);
  }
  return snprintf(log_buffer, len, "%d,%" PRIu64 ",%" PRIu64 ",%s;%s\n",
      use_me->level, use_me->seq, use_me->timestamp, use_me->cont,
      use_me->text);
}

struct log_data test_log_data[] = {
  { 1, 1000LL, 100LL, "-", "My first log line.", NULL },
  { 4, 1001LL, 101LL, "-", "My second log line.", NULL },
  { 2, 1010LL, 102LL, "-", LOG_MARKER_START, NULL },
  { 2, 1030LL, 103LL, "?", "More fun log data\nwith extra crap!",
    "More fun log data" },
  { 5, 2030000LL, 104LL, "-", LOG_MARKER_END, NULL },
  { 3, 3030000LL, 105LL, "-", "foobah foobah foobah", NULL },
  { 4, 3040000LL, 106LL, "-", LOG_MARKER_START, NULL },
  { 9, 3050000LL, 107LL, "-", "More data after my last start marker", NULL }
};
int test_log_data_size = sizeof(test_log_data) / sizeof(struct log_data);

struct log_data eos_data = { 7, 3060000LL, 108LL, "-", LOG_MARKER_START };
const char* new_marker_logged_str = "7,108,3060000,-;" LOG_MARKER_START "\n";
char test_dev_kmsg_path[64];
char test_version_path[64];
char test_ntp_sync_path[64];
char tdir[32];

static void setup_temp_files() {
  strcpy(tdir, "logXXXXXX");
  EXPECT_TRUE(mkdtemp(tdir) != NULL);
  snprintf(test_dev_kmsg_path, sizeof(test_dev_kmsg_path), "%s/%s", tdir,
      "devkmsg");
  snprintf(test_version_path, sizeof(test_version_path), "%s/%s", tdir,
      "version");
  snprintf(test_ntp_sync_path, sizeof(test_ntp_sync_path), "%s/%s", tdir,
      "ntpsync");
}

struct log_parse_params* create_log_parse_params(struct log_data* test_data,
    int len) {
  setup_temp_files();
  write_to_file(test_version_path, "fakeversion");

  struct upload_config* config =
    (struct upload_config*) malloc(sizeof(struct upload_config));
  struct log_parse_params* params =
    (struct log_parse_params*) malloc(sizeof(struct log_parse_params));
  memset(params, 0, sizeof(struct log_parse_params));
  memset(config, 0, sizeof(struct upload_config));
  params->config = config;

  struct parser_progress* progress =
    (struct parser_progress*) malloc(sizeof(struct parser_progress));
  memset(progress, 0, sizeof(struct parser_progress));
  progress->num_entries = len;
  progress->my_data = test_data;
  progress->post_eos_data = &eos_data;

  params->user_data = progress;
  params->read_log_data = read_log_struct;
  params->dev_kmsg_path = test_dev_kmsg_path;
  params->version_path = test_version_path;
  params->ntp_synced_path = test_ntp_sync_path;
  params->total_read = 8*1024*1024;
  params->log_buffer = (char*) malloc(params->total_read);
  memset(params->log_buffer, 0, params->total_read);
  params->line_buffer_size = 8192;
  params->line_buffer = (char*) malloc(params->line_buffer_size);
  memset(params->line_buffer, 0, params->line_buffer_size);
  return params;
}

void free_log_parse_params(struct log_parse_params* params) {
  free(params->line_buffer);
  free(params->log_buffer);
  free(params->config);
  free(params);
  remove(test_ntp_sync_path);
  remove(test_dev_kmsg_path);
  remove(test_version_path);
  rmdir(tdir);
}

int verify_log_data(struct log_data* test_data, char* output_data, int len,
    int start_entry,
    int num_entries) {
  char line_test[8192];
  char cmp_buf[8192];
  int count = 0;
  for (int i = start_entry; i < start_entry + num_entries; ++i) {
    int time_sec = (int) (test_data[i].timestamp / 1000000);
    int time_usec = (int) (test_data[i].timestamp % 1000000);
    int curr = snprintf(line_test, sizeof(line_test),
        "<%d>[%5d.%06d] %s\n", test_data[i].level, time_sec, time_usec,
        (test_data[i].used_text ? test_data[i].used_text :
           test_data[i].text));
    EXPECT_LE(count + curr, len);
    strncpy(cmp_buf, &(output_data[count]), curr);
    cmp_buf[curr] = '\0';
    EXPECT_STREQ(line_test, cmp_buf);
    count += curr;
  }
  return count;
}

void verify_dev_kmsg_writes() {
  char marker_verify[8192];
  EXPECT_LT(0, read_file_as_string(test_dev_kmsg_path, marker_verify,
        sizeof(marker_verify)));
  char* marker_newline = strstr(marker_verify, "\n");
  EXPECT_TRUE(marker_newline != NULL);
  int first_line_chars = marker_newline - marker_verify + 1;
  char cmp_buf[8192];
  strncpy(cmp_buf, marker_verify, first_line_chars);
  cmp_buf[first_line_chars] = '\0';
  EXPECT_STREQ(cmp_buf, LOG_MARKER_START_LINE);
  strncpy(cmp_buf, marker_newline + 1, 6);
  cmp_buf[6] = '\0';
  EXPECT_STREQ("<7>T: ", cmp_buf);
}

// This is what happens after a reboot when we have no counter.
TEST(LogUploader, parse_logs_no_counter) {
  struct log_parse_params* params = create_log_parse_params(test_log_data,
      test_log_data_size);
  struct parser_progress* progress =
    (struct parser_progress*) params->user_data;

  char* res_buffer = parse_and_consume_log_data(params);

  // The counter was at zero, so it should look for the first
  // start marker before the last end marker and extract from there on.

  // Make sure it actually read in the whole thing.
  EXPECT_EQ(2, progress->eos_count);
  EXPECT_EQ(progress->num_entries, progress->curr_entry);

  EXPECT_EQ(107LL, params->last_log_counter);
  EXPECT_EQ(1, params->last_line_valid);
  EXPECT_STREQ(new_marker_logged_str, params->line_buffer);

  verify_dev_kmsg_writes();

  // It should have skipped the first 2 entries, but wrote out the rest
  verify_log_data(test_log_data, res_buffer, params->total_read, 2, 6);

  free_log_parse_params(params);
}

// This is what happens after a restart with a valid counter.
TEST(LogUploader, parse_logs_with_counter) {
  struct log_parse_params* params = create_log_parse_params(test_log_data,
      test_log_data_size);
  struct parser_progress* progress =
    (struct parser_progress*) params->user_data;

  params->last_log_counter = 103LL;
  char* res_buffer = parse_and_consume_log_data(params);

  // Make sure it actually read in the whole thing.
  EXPECT_EQ(2, progress->eos_count);
  EXPECT_EQ(progress->num_entries, progress->curr_entry);

  EXPECT_EQ(107LL, params->last_log_counter);
  EXPECT_EQ(1, params->last_line_valid);
  EXPECT_STREQ(new_marker_logged_str, params->line_buffer);

  verify_dev_kmsg_writes();

  // It should have skipped the first 4 entries, but wrote out the rest
  verify_log_data(test_log_data, res_buffer, params->total_read, 4, 4);

  free_log_parse_params(params);
}

TEST(LogUploader, parse_logs_with_valid_last_line) {
  struct log_parse_params* params = create_log_parse_params(test_log_data,
      test_log_data_size);
  struct parser_progress* progress =
    (struct parser_progress*) params->user_data;

  params->last_log_counter = 98LL;
  params->last_line_valid = 1;
  strcpy(params->line_buffer, "7,99,900,-;" LOG_MARKER_START "\n");
  char* res_buffer = parse_and_consume_log_data(params);

  // Make sure it actually read in the whole thing.
  EXPECT_EQ(2, progress->eos_count);
  EXPECT_EQ(progress->num_entries, progress->curr_entry);

  EXPECT_EQ(107LL, params->last_log_counter);
  EXPECT_EQ(1, params->last_line_valid);
  EXPECT_STREQ(new_marker_logged_str, params->line_buffer);

  verify_dev_kmsg_writes();

  // It should have read all the entries plus added our line_buffer
  // data at the start.
  const char* extra_first_line = "<7>[    0.000900] " LOG_MARKER_START "\n";
  ssize_t first_len = strlen(extra_first_line);
  char cmp_buf[8192];
  memcpy(cmp_buf, res_buffer, first_len);
  cmp_buf[first_len] = '\0';
  EXPECT_STREQ(extra_first_line, cmp_buf);
  verify_log_data(test_log_data, res_buffer + first_len, params->total_read, 0,
      8);

  free_log_parse_params(params);
}

TEST(LogUploader, parse_logs_with_missing_data) {
  struct log_parse_params* params = create_log_parse_params(test_log_data,
      test_log_data_size);
  struct parser_progress* progress =
    (struct parser_progress*) params->user_data;

  params->last_log_counter = 98LL;
  char* res_buffer = parse_and_consume_log_data(params);

  // Make sure it actually read in the whole thing.
  EXPECT_EQ(2, progress->eos_count);
  EXPECT_EQ(progress->num_entries, progress->curr_entry);

  EXPECT_EQ(107LL, params->last_log_counter);
  EXPECT_EQ(1, params->last_line_valid);
  EXPECT_STREQ(new_marker_logged_str, params->line_buffer);

  verify_dev_kmsg_writes();

  // It should have read all the entries plus added a line for the missing
  // entries at the start.
  const char* extra_first_line = "<7>[    0.001000] WARNING: "
    "missed 1 log entries\n";
  ssize_t first_len = strlen(extra_first_line);
  char cmp_buf[8192];
  memcpy(cmp_buf, res_buffer, first_len);
  cmp_buf[first_len] = '\0';
  EXPECT_STREQ(extra_first_line, cmp_buf);
  verify_log_data(test_log_data, res_buffer + first_len, params->total_read, 0,
      8);

  free_log_parse_params(params);
}

TEST(LogUploader, logmark_once_ntpsync) {
  setup_temp_files();
  write_to_file(test_version_path, "fakeversion");
  write_to_file(test_ntp_sync_path, "");

  EXPECT_EQ(0, logmark_once(test_dev_kmsg_path, test_version_path,
        test_ntp_sync_path));
  char out_buf[8192];
  read_file_as_string(test_dev_kmsg_path, out_buf, sizeof(out_buf));
  EXPECT_EQ(0, strncmp(out_buf, "<7>T: ", 6));
  EXPECT_TRUE(strstr(out_buf, "ntp=1\n") != NULL);
  remove(test_version_path);
  remove(test_ntp_sync_path);
  remove(test_dev_kmsg_path);
  rmdir(tdir);
}

TEST(LogUploader, logmark_once_no_ntpsync) {
  setup_temp_files();
  write_to_file(test_version_path, "fakeversion");

  EXPECT_EQ(0, logmark_once(test_dev_kmsg_path, test_version_path,
        test_ntp_sync_path));
  char out_buf[8192];
  read_file_as_string(test_dev_kmsg_path, out_buf, sizeof(out_buf));
  EXPECT_EQ(0, strncmp(out_buf, "<7>T: ", 6));
  EXPECT_TRUE(strstr(out_buf, "ntp=0\n") != NULL);
  remove(test_version_path);
  remove(test_ntp_sync_path);
  remove(test_dev_kmsg_path);
  rmdir(tdir);
}
