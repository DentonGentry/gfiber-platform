#ifndef _H_GOOGLE_PLATFORM_SPECTRALANALYZER_SPECTRAL_H_
#define _H_GOOGLE_PLATFORM_SPECTRALANALYZER_SPECTRAL_H_

#include <stdlib.h>
#include <inttypes.h>

#define MAX_PATH 1024

#define NUM_SAMPLE_BINS 56
#define MIN_SCAN_FREQ 2412
#define MAX_SCAN_FREQ 2462
#define FREQ_STEP 5

#define NUM_OVERALL_BINS 196
#define MIN_OVERALL_FREQ 2402
#define MAX_OVERALL_FREQ 2472
// This is how many bins we shift for each FREQ_STEP.
#define FREQ_STEP_BIN_OFFSET 14

typedef struct spectral_config {
  int32_t offchan_dur_millis;
  int32_t channel_switch_delay_millis;
  int32_t scan_period_millis;
  char dump_dir[MAX_PATH];
  int dump_count;
} spectral_config;


#define LOWER_POWER_BUCKET_MIN -80
#define UPPER_POWER_BUCKET_MAX -20
#define POWER_BUCKET_STEP 5
#define NUM_FREQ_BUCKETS 14
#define FREQ_BUCKET_STEP 5
#define NUM_POWER_BUCKETS 12
#define BINS_PER_BUCKET 14

typedef struct bucket_results {
  uint32_t bucket_count[NUM_FREQ_BUCKETS][NUM_POWER_BUCKETS];
  uint16_t total[NUM_FREQ_BUCKETS];
  uint64_t timestamp;
} bucket_results;

typedef struct fft_data_tlv {
  uint8_t type;
  uint16_t len;
} __attribute__((packed)) fft_data_tlv;

typedef struct fft_data {
  fft_data_tlv tlv;
  uint8_t max_exponent;
  uint16_t freq;
  int8_t rssi;
  int8_t noise;
  uint16_t max_magnitude;
  uint8_t max_index;
  uint8_t bitmap_weight;
  uint64_t timestamp;
  uint8_t fft_values[NUM_SAMPLE_BINS];
} __attribute__((packed)) fft_data;

#endif  // _H_GOOGLE_PLATFORM_SPECTRALANALYZER_SPECTRAL_H_
