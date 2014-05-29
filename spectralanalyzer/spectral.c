#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

#include "spectral.h"

static void load_default_config(spectral_config* config);

static int load_config(spectral_config* config);

static void usage(const char* progname);

static int parse_args(spectral_config* config, int argc, char* const argv[]);

static void verify_config(spectral_config* config);

static int execute_full_scan(spectral_config* config, bucket_results* buckets);

static int send_control(const char* message);

static int go_offchannel(const spectral_config* config, int freq);

static char* collect_results(ssize_t* total);

static void append_results(const char* raw_data, ssize_t size,
                              bucket_results* buckets);

static void post_buckets(const bucket_results* result);

static int calc_square_sum(const uint8_t* data, int num, uint8_t exp);

static void dump_raw_data(spectral_config* config, char* data, ssize_t size);

// Only for reading non-negative numbers, returns -1 if file doesn't exist.
static int32_t read_file_as_int(char *file_path);

int main(int argc, char* const argv[]) {
  spectral_config config;
  bucket_results buckets;
  int rv;
  int ignore_fs_config = 0;
  setvbuf(stdout, (char *) NULL, _IOLBF, 0);

  printf("Starting background spectral scanner\n");
  load_default_config(&config);
  if (argc > 1) {
    if (parse_args(&config, argc, argv) < 0) {
      usage(argv[0]);
      return 1;
    }
    // This allows for using it in a way that overrides what's
    // configured as the settings in the filesystem.
    ignore_fs_config = 1;
    verify_config(&config);
  }
  while (1) {
    if (!ignore_fs_config) {
      if (load_config(&config))
        verify_config(&config);
    }

    if (config.scan_period_millis == 0) {
      // This is how we can disable it, but setting the overall
      // scan period to zero. We then recheck our config in 6 minutes.
      usleep(360000);
      continue;
    }

    rv = execute_full_scan(&config, &buckets);

    if (rv >= 0) {
      post_buckets(&buckets);
    } else {
      fprintf(stderr, "Failure with full scan\n");
    }

    usleep(config.scan_period_millis * 1000);
  }
  return 0;
}

static void load_default_config(spectral_config* config) {
  config->offchan_dur_millis = 100;
  config->scan_period_millis = 300000;
  config->channel_switch_delay_millis = 1000;
  config->dump_dir[0] = '\0';
  config->dump_count = 0;
}

static int load_config(spectral_config* config) {
  int rv = 0;
  int32_t config_value = read_file_as_int("/tmp/spectral_offchannel_duration");
  if (config_value >= 0) {
    config->offchan_dur_millis = config_value;
    rv =1;
  }
  config_value = read_file_as_int("/tmp/spectral_offchannel_switch_delay");
  if (config_value >= 0) {
    config->channel_switch_delay_millis = config_value;
    rv = 1;
  }
  config_value = read_file_as_int("/tmp/spectral_scan_period");
  if (config_value >= 0) {
    config->scan_period_millis = config_value;
    rv = 1;
  }
  return rv;
}

static void usage(const char* progname) {
  printf("%s [--offchan_dur dur] [--scan_period period] "
      "[--dump_dir dir] [--channel_switch_delay delay]\n", progname);
  exit(1);
}

static int parse_args(spectral_config* config, int argc, char* const argv[]) {
  int opt = 0, len;
  static struct option long_options[] = {
    { "offchan_dur", required_argument, 0, 'o' },
    { "scan_period", required_argument, 0, 's' },
    { "channel_switch_delay", required_argument, 0, 'c' },
    { "dump_dir", required_argument, 0, 'd' },
    { 0, 0, 0, 0}
  };

  while (1) {
    opt = getopt_long(argc, argv, "", long_options, NULL);
    if (opt == -1)
      break;

    switch (opt) {
      case 'o':
        config->offchan_dur_millis = atoi(optarg);
        break;
      case 's':
        config->scan_period_millis = atoi(optarg);
        break;
      case 'c':
        config->channel_switch_delay_millis = atoi(optarg);
        break;
      case 'd':
       len = snprintf(config->dump_dir, sizeof(config->dump_dir) - 1,
           "%s", optarg);
       if (len >= sizeof(config->dump_dir) - 1) {
         fprintf(stderr, "Dump path is too long\n");
         return -1;
       }
       if (config->dump_dir[len - 1] != '/') {
         config->dump_dir[len] = '/';
         config->dump_dir[len + 1] = '\0';
       }
       break;
     default:
       return -1;
    }
  }
  if (optind < argc)
    return -1; // extraneous non-option arguments
  return 0;
}

static void verify_config(spectral_config* config) {
  if (config->offchan_dur_millis <= 0) {
    fprintf(stderr, "Invalid offchan_dur_millis in spectral config %d\n",
        config->offchan_dur_millis);
    config->offchan_dur_millis = 100;
  }
  if (config->scan_period_millis < 0) {
    fprintf(stderr,
        "Invalid scan_period_millis in spectral config %d\n",
        config->scan_period_millis);
    config->scan_period_millis = 300000;
  }
  if (config->channel_switch_delay_millis <= 0) {
    fprintf(stderr,
        "Invalid channel_switch_delay in spectral config %d\n",
        config->channel_switch_delay_millis);
    config->channel_switch_delay_millis = 1000;
  }
  printf("Loaded spectral config offchan_dur %d scan_period %d"
      " channel_switch %d dump_dir %s\n",
      config->offchan_dur_millis, config->scan_period_millis,
      config->channel_switch_delay_millis, config->dump_dir);
}

static int execute_full_scan(spectral_config* config, bucket_results* buckets) {
  int freq, rv;
  ssize_t total_size;
  char* raw_data;

  if (buckets)
    memset(buckets, 0, sizeof(*buckets));

  for (freq = MIN_SCAN_FREQ; freq <= MAX_SCAN_FREQ; freq += FREQ_STEP) {
    // First we need to enable background scanning.
    rv = send_control("background");
    if (rv < 0) {
      fprintf(stderr, "Failed to set background scanning\n");
      return -1;
    }

    // Switch the AP into offchannel mode briefly.
    rv = go_offchannel(config, freq);
    if (rv < 0) {
      fprintf(stderr, "Failed to do offchannel scan\n");
      return -1;
    }

    // Trigger the actual scan.
    rv = send_control("trigger");
    if (rv < 0) {
      fprintf(stderr, "Failure triggering the spectral scan\n");
      return -1;
    }

    // Wait for the scan to complete.
    usleep(config->offchan_dur_millis * 1000);

    // Disable scanning.
    rv = send_control("disable");
    if (rv < 0) {
      fprintf(stderr, "Failure disabling the spectral scan\n");
      return -1;
    }

    total_size = 0;
    raw_data = collect_results(&total_size);
    if (!raw_data) {
      fprintf(stderr, "Failure collecting scan data results\n");
      return -1;
    }

    append_results(raw_data, total_size, buckets);
    if (config->dump_dir[0]) {
      dump_raw_data(config, raw_data, total_size);
    }
    free(raw_data);

    // Wait to do another cycle
    usleep(config->channel_switch_delay_millis * 1000);
  }
  return 0;
}

static void dump_raw_data(spectral_config* config, char* data, ssize_t size) {
  int dump_fd;
  ssize_t written;
  char target_file[MAX_PATH + 20];

  snprintf(target_file, sizeof(target_file), "%sspectral-%d", config->dump_dir,
      config->dump_count);
  config->dump_count++;
  dump_fd = open(target_file, O_WRONLY | O_TRUNC | O_CREAT, 0666);
  if (dump_fd < 0) {
    fprintf(stderr, "Could not open dump file at %s errno %d\n",
        target_file, errno);
    return;
  }
  written = write(dump_fd, data, size);
  if (size != written) {
    fprintf(stderr, "Incomplete write to dump file %s wrote %zd length %zd\n",
        target_file, written, size);
  }
  if (close(dump_fd) < 0) {
    fprintf(stderr, "Failure closing dump file %s\n", target_file);
  }
}

static int send_control(const char* message) {
  int len, scan_ctl_fd, rv;
  ssize_t count;

  scan_ctl_fd = open(
      "/sys/kernel/debug/ieee80211/phy0/ath9k/spectral_scan_ctl",
      O_WRONLY | O_TRUNC);
  if (scan_ctl_fd < 0) {
    fprintf(stderr, "Failure opening spectral_scan_ctl: %d\n", errno);
    return -1;
  }
  len = strlen(message);
  count = write(scan_ctl_fd, message, len);
  rv = close(scan_ctl_fd);
  if (len != count) {
    fprintf(stderr, "Incomplete write to spectral_scan_ctl written %zd len %d",
        count, len);
    return -1;
  }
  if (rv < 0) {
    fprintf(stderr, "Failure closing spectral_scan_ctl: %d\n", errno);
    return -1;
  }
  return 0;
}

static int go_offchannel(const spectral_config* config, int freq) {
  char* iwcmd[7];
  char freq_str[16];
  char dwell_str[16];
  pid_t pid;
  int fd, maxfd;

  printf("Performing wifi spectral scan at freq %d\n", freq);
  iwcmd[0] = "iw";
  iwcmd[1] = "dev";
  iwcmd[2] = "wlan0";
  iwcmd[3] = "offchannel";
  snprintf(freq_str, sizeof(freq_str), "%d", freq);
  iwcmd[4] = freq_str;
  snprintf(dwell_str, sizeof(dwell_str), "%d", config->offchan_dur_millis);
  iwcmd[5] = dwell_str;
  iwcmd[6] = NULL;
  pid = fork();
  if (pid != -1) {
    if (!pid) { // child
      // Close all descriptors except stdin, stdout, stderr
      maxfd = sysconf(_SC_OPEN_MAX);
      for (fd = 3; fd < maxfd; ++fd) {
        close(fd);
      }
      execvp(iwcmd[0], (char* const *)iwcmd);
      // It won't reach this unless there was an error
      fprintf(stderr, "Error executing %s errno: %d\n", iwcmd[0], errno);
      exit(1);
    } else {
      // Wait for it to finish
      waitpid(pid, NULL, 0);
      return 0;
    }
  } else {
    fprintf(stderr, "Error executing child process errno: %d\n", errno);
    return -1;
  }
}

static char* collect_results(ssize_t* total) {
  char *buf = NULL, *buf_tmp = NULL;
  const ssize_t read_size = 4096;
  ssize_t count;
  int result_fd = open(
      "/sys/kernel/debug/ieee80211/phy0/ath9k/spectral_scan0",
      O_RDONLY);
  if (result_fd < 0) {
    fprintf(stderr, "Error opening spectral_scan0 of %d\n", errno);
    return NULL;
  }
  *total = 0;
  while (1) {
    buf_tmp = realloc(buf, *total + read_size);
    if (!buf_tmp) {
      fprintf(stderr, "Failure reallocating read buffer\n");
      if (buf)
        free(buf);
      *total = 0;
      close(result_fd);
      return NULL;
    }
    buf = buf_tmp;
    count = read(result_fd, buf + *total, read_size);
    *total += count;
    if (count < 0) {
      fprintf(stderr, "Failure reading spectral_scan0 of %d\n", errno);
      *total = 0;
      free(buf);
      close(result_fd);
      return NULL;
    }
    if (count < read_size)
      break;
  }
  close(result_fd);
  //printf("Succesfully read %d bytes of scan data\n", *total);
  return buf;
}

static void append_results(const char* raw_data, ssize_t size,
                          bucket_results* buckets) {
  uint16_t freq;
  uint64_t timestamp;
  int i, bin_offset, square_sum, base_value;
  int power_bucket_index, freq_bucket_index;
  float bucket_max, curr_signal;
  fft_data_tlv* tlv;
  fft_data* curr_fft_data;
  size_t data_len;
  const char* pos = raw_data;
  while (pos - raw_data < size) {
    tlv = (fft_data_tlv*) pos;
    data_len = sizeof(fft_data_tlv) + be16toh(tlv->len);
    pos += data_len;
    if (tlv->type != 1) {
      // 1 is the type code for the HT20 data we understand.
      fprintf(stderr, "Invalid type code in scan data of %d\n", tlv->type);
      continue;
    }
    if (data_len != sizeof(fft_data)) {
      fprintf(stderr, "Invalid data length in scan data of %zd\n", data_len);
      continue;
    }
    curr_fft_data = (fft_data*) tlv;
    // Fix the byte ordering on the data fields larger than 8 bits.
    freq = be16toh(curr_fft_data->freq);
    timestamp = be64toh(curr_fft_data->timestamp);

    if (buckets) {
      buckets->timestamp = timestamp;
    }
    bin_offset = FREQ_STEP_BIN_OFFSET * (freq - MIN_SCAN_FREQ) / FREQ_STEP;
    if (bin_offset < 0 || bin_offset >= NUM_OVERALL_BINS) {
      fprintf(stderr, "Invalid frequency bin %d from freq %d\n",
          bin_offset, freq);
      continue;
    }
    square_sum = calc_square_sum(curr_fft_data->fft_values,
                                     NUM_SAMPLE_BINS,
                                     curr_fft_data->max_exponent);
    if (square_sum == 0) {
      // Sometimes the data is all empty so the square sum is zero,
      // we can't do anything correct in this case since this is just
      // bad data coming from the driver so just skip it.
      // This happens every so often..no need to log it.
      //fprintf(stderr, "Skipping data chunk due to zero sum\n");
      continue;
    }
    for (i = 0; i < NUM_SAMPLE_BINS; ++i) {
      base_value = curr_fft_data->fft_values[i] << curr_fft_data->max_exponent;
      if (base_value == 0)
        base_value = 1;
      curr_signal = curr_fft_data->noise + curr_fft_data->rssi +
        20 * log10f(base_value) - 10 * log10f(square_sum);
      if (buckets) {
        if (i % BINS_PER_BUCKET == 0)
          bucket_max = -200;
        if (curr_signal > bucket_max)
          bucket_max = curr_signal;
        if ((i + 1) % BINS_PER_BUCKET == 0) {
          // We have the max for this bucket, update the info
          freq_bucket_index = (bin_offset + i + 1 - BINS_PER_BUCKET) /
              BINS_PER_BUCKET;
          if (bucket_max <= LOWER_POWER_BUCKET_MIN)
            power_bucket_index = 0;
          else if (bucket_max >= UPPER_POWER_BUCKET_MAX)
            power_bucket_index = NUM_POWER_BUCKETS - 1;
          else
            power_bucket_index = (((int) bucket_max) - LOWER_POWER_BUCKET_MIN) /
              POWER_BUCKET_STEP;
          buckets->bucket_count[freq_bucket_index][power_bucket_index]++;
          buckets->total[freq_bucket_index]++;
        }
      }
    }
  }
}

static int calc_square_sum(const uint8_t* data, int num, uint8_t exp) {
  int i, curr, rv = 0;
  for (i = 0; i < num; ++i) {
    curr =  data[i] << exp;
    rv += (curr * curr);
  }
  return rv;
}

static void post_buckets(const bucket_results* result) {
  int i, j, k, perct, overall_total, bucket_total;
  // This prints out the buckets as 5 MHz intervals and doesn't coalesce
  // the data into each Wifi channel itself. It may be useful again so I'm
  // going to leave it here just in case.
  if (0) {
    for (i = 0; i < NUM_FREQ_BUCKETS; i++) {
      if (result->total[i]) {
        printf("%d MHz\n", MIN_OVERALL_FREQ + (i * FREQ_BUCKET_STEP));
        for (j = 0; j < NUM_POWER_BUCKETS; ++j) {
          perct = result->bucket_count[i][j] * 100 / result->total[i];
          if (perct > 0)
            printf(" %3d", perct);
          else
            printf("    ");
        }
        printf("\n");
      }
    }
  }

  // We start at 2 because we are going over all the wifi channels
  // here and the first two will get added into the first channel.
  // We stop one short of the end because that one will get included in
  // channel 11.
  for (i = 2; i < NUM_FREQ_BUCKETS - 1; i++) {
    printf("fft-%2d:", i - 1);
    overall_total = 0;
    for (k = -2; k < 2; k++) {
      overall_total += result->total[i + k];
    }
    for (j = 0; j < NUM_POWER_BUCKETS; ++j) {
      bucket_total = 0;
      for (k = -2; k < 2; k++) {
        bucket_total += result->bucket_count[i + k][j];
      }
      perct = bucket_total * 100 / overall_total;
      if (perct > 0)
        printf(" %3d", perct);
      else
        printf("    ");
    }
    printf("\n");
  }
}

int32_t read_file_as_int(char* file_path) {
  char buf[64];
  int num_read, fd = open(file_path, O_RDONLY);
  if (fd < 0)
    return -1;
  num_read = read(fd, buf, sizeof(buf) -1);
  if (num_read > 0) {
    buf[num_read] = '\0';
    return atoi(buf);
  }
  return -1;
}
