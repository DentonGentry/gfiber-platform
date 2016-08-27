/*
 * Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _BSD_SOURCE
#include <endian.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "rcu-audio.h"
#include "remote_control_audio.pb.h"


typedef struct WAV_hdr
{
  uint32_t chunk_id;
  uint32_t chunk_size;
  uint32_t format;

  uint32_t subchunk1_id;
  uint32_t subchunk1_size;
  uint16_t audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;

  uint32_t subchunk2_id;
  uint32_t subchunk2_size;
} WAV_hdr_t;


static int usage(const char *progname)
{
  fprintf(stderr, "usage: %s [-f outfile]\n, where:", progname);
  fprintf(stderr, "\t-f outfile: file to write audio to in WAV format.\n");
  exit(1);
}


int main(int argc, char **argv)
{
  mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;
  int fd;
  struct sockaddr_un sun;
  const char *outfile = "/tmp/audio.wav";
  int outfd;
  uint8_t buf[8192];
  WAV_hdr_t hdr;
  ssize_t len, totlen=0;
  int c;
  struct timeval tv;
  const char *model = "UNKNOWN";

  memset(buf, 0, sizeof(buf));
  memset(&hdr, 0, sizeof(hdr));

  while ((c = getopt(argc, argv, "f:")) != -1) {
    switch (c) {
      case 'f':
        outfile = optarg;
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  if ((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
    perror("socket(AF_UNIX) RCU_AUDIO_PATH");
    exit(1);
  }
  memset(&sun, 0, sizeof(sun));
  sun.sun_family = AF_UNIX;
  strncpy(&sun.sun_path[1], RCU_AUDIO_PATH, sizeof(sun.sun_path) - 2);
  if (bind(fd, (const struct sockaddr *) &sun, sizeof(sun)) < 0) {
    perror("bind(AF_UNIX) RCU_AUDIO_PATH");
    exit(1);
  }

  if ((outfd = open(outfile, O_RDWR|O_CREAT|O_TRUNC, mode)) < 0) {
    fprintf(stderr, "Unable to open %s for writing.\n", outfile);
    exit(1);
  }

  if ((len = write(outfd, &hdr, sizeof(WAV_hdr_t))) != sizeof(WAV_hdr_t)) {
    fprintf(stderr, "Incorrect size for WAV header: %zd != %zd\n",
      len, sizeof(WAV_hdr_t));
    exit(1);
  }

  tv.tv_sec = 0x7fffffff;
  tv.tv_usec = 0;

  while (1) {
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0) {
      /* No more data, close the output and exit. */
      break;
    }

    len = read(fd, buf, sizeof(buf));
    if (len > 0) {
      rcaudio::AudioSamples samples;
      const char *data;
      ssize_t data_len;

      if (!samples.ParseFromArray(buf, len)) {
        if (pacing()) {
          printf("failed to parse rcaudio::AudioSamples.\n");
        }
        continue;
      }

      if (samples.audio_format() != rcaudio::AudioSamples::PCM_16BIT_16KHZ) {
        /* if we ever build a remote with a different format, we'll need
         * to keep track of it here and adjust the WAV header to match. */
        if (pacing()) {
          fprintf(stderr, "unknown audio format %d\n", samples.audio_format());
        }
        continue;
      }

      switch (samples.remote_type()) {
        case rcaudio::AudioSamples::GFRM210: model = "GFRM210"; break;
        case rcaudio::AudioSamples::GFRM100: model = "GFRM100"; break;

        case rcaudio::AudioSamples::UNDEFINED_REMOTE_TYPE:
        default:
          model = "UNKNOWN";
          break;
      }

      data = samples.audio_samples().c_str();
      data_len = samples.audio_samples().size();
      totlen += data_len;
      if (write(outfd, data, data_len) != data_len) {
        fprintf(stderr, "short write!\n");
        exit(1);
      }
    } else if (len == 0) {
      break;
    } else if (len < 0) {
      perror("read");
      exit(1);
    }
    tv.tv_sec = 2;
    tv.tv_usec = 0;
  }

  /* print the remote control type to stdout, demo script uses it. */
  puts(model);

  lseek(outfd, 0, SEEK_SET);

  #define BITS_PER_SAMPLE 16
  #define SAMPLES_PER_SECOND  16000
  /* http://soundfile.sapp.org/doc/WaveFormat/ */
  hdr.chunk_id = htole32(0x46464952);  // "RIFF"
  hdr.chunk_size = htole32(36 + totlen);
  hdr.format = htole32(0x45564157);  // "WAVE"

  hdr.subchunk1_id = htole32(0x20746d66);  // "fmt "
  hdr.subchunk1_size = htole32(16);
  hdr.audio_format = htole16(1);
  hdr.num_channels = htole16(1);
  hdr.sample_rate = htole32(SAMPLES_PER_SECOND);
  hdr.byte_rate = htole32(SAMPLES_PER_SECOND * 1 * BITS_PER_SAMPLE/8);
  hdr.block_align = htole16(1 * BITS_PER_SAMPLE/8);
  hdr.bits_per_sample = htole16(BITS_PER_SAMPLE);

  hdr.subchunk2_id = htole32(0x61746164);  // "data"
  hdr.subchunk2_size = htole32(totlen);
  if ((len = write(outfd, &hdr, sizeof(WAV_hdr_t))) != sizeof(WAV_hdr_t)) {
    fprintf(stderr, "Incorrect size for WAV header: %zd != %zd\n",
      len, sizeof(WAV_hdr_t));
    exit(1);
  }

  exit(0);
}
