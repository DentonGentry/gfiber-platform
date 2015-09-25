#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "log_uploader.h"
#include "utils.h"

int logmark_once(const char* output_path, const char* version_path,
    const char* ntp_sync_path) {
  char buf[1024];
  char version[64];
  struct tm timeinfo;
  struct timespec curr_time;
  if (read_file_as_string(version_path, version, sizeof(version)) < 0) {
    return -1;
  }
  rstrip(version);
  memset(&timeinfo, 0, sizeof(timeinfo));
  memset(&curr_time, 0, sizeof(curr_time));
  clock_gettime(CLOCK_REALTIME, &curr_time);
  localtime_r(&curr_time.tv_sec, &timeinfo);
  snprintf(buf, sizeof(buf), "<7>T: %s %d.%03d %02d/%02d %02d:%02d:%02d "
      "ntp=%d\n",
      version, (int)curr_time.tv_sec, (int)(curr_time.tv_nsec/1000000L),
      timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
      timeinfo.tm_sec, path_exists(ntp_sync_path));
  int rv = write_to_file(output_path, buf);
  if (rv < 0) {
    perror(output_path);
    return -1;
  }
  return 0;
}

char* parse_and_consume_log_data(struct log_parse_params* params) {
  unsigned long log_buffer_size = params->total_read;
  int wrote_start_marker = 0;
  char *last_start_marker, *last_start_before_end_marker;
  int check_for_markers = (params->last_log_counter == 0);
  struct line_data parsed_line;
  memset(&parsed_line, 0, sizeof(parsed_line));
  ssize_t num_read;
  if (check_for_markers)
    last_start_marker = last_start_before_end_marker = NULL;
  params->total_read = 0;

  while (1) {
    // Make sure we have room in our main buffer for another line if we
    // don't then just stop reading now. This should never really happen,
    // but we should protect against it anyways.
    // Use 2x so that we can also log data about missing entries if we
    // need to do that as well.
    if (params->total_read + 2*params->line_buffer_size >= log_buffer_size) {
      break;
    }
    if (!params->last_line_valid) {
      // Read the next line from /dev/kmsg
      num_read = params->read_log_data(params->line_buffer,
          params->line_buffer_size, params->user_data);
    } else {
      // We have our start marker in the line buffer from the last run
      // of the loop, just leave it there and we'll parse it.
      params->last_line_valid = 0;
      num_read = 0;
    }
    if (num_read < 0) {
      if (errno == EAGAIN) {
        if (!wrote_start_marker) {
          // Write out the start marker.
          if (write_to_file(params->dev_kmsg_path, LOG_MARKER_START_LINE) < 0) {
            perror("start marker");
            return NULL;
          }
          if (logmark_once(params->dev_kmsg_path, params->version_path,
                params->ntp_synced_path) < 0) {
            fprintf(stderr, "failed to execute logmark-once properly\n");
            return NULL;
          }
          wrote_start_marker = 1;
          continue;
        } else {
          // We have finished reading everything, exit the loop.
          // But this should never really happen because we should find
          // out start marker instead and exit that way.
          break;
        }
      } else if (errno == EPIPE) {
        // This means the last message we read is no longer there,
        // it will then reset us to the beginning so just continue on
        // and we will log that we lost messages. This also will happen
        // the first time /dev/kmsg is opened.
        continue;
      } else if (errno == EINVAL) {
        // This should never happen because we use an 8192 size buf which is
        // the same size as the kernel uses for the temp buf to copy the result
        // to. So if this happens...that likely means there is kernel
        // corruption.
        perror("kernel memory possibly corrupted, devkmsg_read");
        continue;
      } else {
        perror("/dev/kmsg");
        return NULL;
      }
    }

    // Parse the data on the line to get the extra information
    if (parse_line_data(params->line_buffer, &parsed_line)) {
      // We don't want to be fatal if we fail to parse a line for some
      // reason because we'd then repeatedly fail on that line when we
      // were restarted. Instead just use it the way it is.
      memcpy(params->log_buffer + params->total_read, params->line_buffer,
          num_read);
      params->total_read += num_read;
    } else {
      if (wrote_start_marker && strstr(parsed_line.text, LOG_MARKER_START)) {
        // We just read back in our start marker that we wrote out. Leave
        // it in the buffer and stop reading at this point.
        params->last_line_valid = 1;
        break;
      }
      // Successfully parsed a log line, check to see if we're at the right
      // spot relative to the counter.
      int time_sec = (int) (parsed_line.ts_nsec / 1000000);
      int time_usec = (int) (parsed_line.ts_nsec % 1000000);
      if (params->last_log_counter > 0) {
        if (parsed_line.seq > params->last_log_counter + 1) {
          // Put this in what we upload so when viewing it then we
          // know there was loss and its interleaved at the correct spot.
          params->total_read += snprintf(params->log_buffer +
              params->total_read, log_buffer_size - params->total_read,
              "<7>[%5d.%06d] WARNING: missed %" PRIu64 " log entries\n",
              time_sec, time_usec,
              parsed_line.seq - params->last_log_counter - 1);
        } else if (!params->config->upload_all &&
            parsed_line.seq <= params->last_log_counter) {
          // Skip this entry because we've already read it and uploaded
          // it. This'll happen if we crash and are restarted.
          continue;
        }
      }
      // If we don't have our tracking working then we need to look at the
      // markers in the log to avoid uploading tons of duplicate data.
      // This should only happen after a reboot.
      if (check_for_markers) {
        if (strstr(parsed_line.text, LOG_MARKER_END) && last_start_marker) {
          last_start_before_end_marker = last_start_marker;
          last_start_marker = NULL;
        } else if (strstr(parsed_line.text, LOG_MARKER_START)) {
          last_start_marker = params->log_buffer + params->total_read;
        }
      }

      params->total_read += snprintf(params->log_buffer +
          params->total_read, log_buffer_size - params->total_read,
          "<%d>[%5d.%06d] %s", parsed_line.level, time_sec, time_usec,
          parsed_line.text);
      params->last_log_counter = parsed_line.seq;
    }
  }
  params->log_buffer[params->total_read] = '\0';

  if (check_for_markers && last_start_before_end_marker) {
      // We have duplicate data from an upload (should have been before
      // a reboot which just occurred). Only upload what's been there
      // since the last start marker before the last end marker. We will
      // still upload duplicate data, but it would only be what was logged
      // during the timeframe of us writing the start marker and consuming
      // all the log data which should be a very short timeframe.
      int dupe_size = last_start_before_end_marker - params->log_buffer;
      params->total_read -= dupe_size;
      return last_start_before_end_marker;
  }
  return params->log_buffer;
}
