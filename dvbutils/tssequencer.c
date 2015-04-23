/*
 * Copyright 2015 Google Inc. All rights reserved.
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

/*
 * This program checks a tansport stream for error. The TS packet payload
 * (bytes 4-188) contains a 32 bit continuous sequence number (bytes 4-8)
 * and a CRC32 checksum (bytes 184-188). The sequence number wraps around
 * at the specified maximum. The checksum is calculated on bytes 4-184.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>
#include <sched.h>

#include "common.h"

#define TS_PACKET_SIZE  188
#define PID_MASK        0x1fff
#define NULL_PID        0x1fff
#define ALL_PID         0x2000
#define SYNC_BYTE       0x47
#define EXPECTED_CRC    0x2144df1c

extern uint32_t crc32(uint32_t crc, const void *buf, size_t size);

static int set_buffer_size(int dmxfd, int buffer_size) {
  return ioctl(dmxfd, DMX_SET_BUFFER_SIZE, buffer_size);
}

static int set_pid_filter(int dmxfd, uint16_t pid, int use_dvr) {
  struct dmx_pes_filter_params filter = {};
  filter.pid = pid;
  filter.input = DMX_IN_FRONTEND;
  filter.output = use_dvr ? DMX_OUT_TS_TAP : DMX_OUT_TSDEMUX_TAP;
  filter.pes_type = DMX_PES_OTHER;
  filter.flags |= DMX_IMMEDIATE_START;
  return ioctl(dmxfd, DMX_SET_PES_FILTER, &filter);
}

static void usage(const char* prog) {
  fprintf(stderr, "Usage: %s [options]\n", prog);
  fprintf(stderr, "  Options:\n");
  fprintf(stderr, "    -a adapter Adapter device (default 0)\n");
  fprintf(stderr, "    -d demux   Demux device (default 0)\n");
  fprintf(stderr, "    -b size    Set demux buffer size (default 16MB)\n");
  fprintf(stderr, "    -i file    Read raw packet data from file\n");
  fprintf(stderr, "    -m number  Maximum sequence number (default 1000000)\n");
  fprintf(stderr, "    -o file    Save raw packet data to file\n");
  fprintf(stderr, "    -p pid     Packet ID (default all)\n");
  fprintf(stderr, "    -t timeout Exit after <timeout> seconds\n");
  fprintf(stderr, "    -c         Disable CRC32 check\n");
  fprintf(stderr, "    -q         Do not print periodic stats\n");
  fprintf(stderr, "    -r         Use realtime priority (root only)\n");
  fprintf(stderr, "    -s         Print summary on exit\n");
  exit(EXIT_FAILURE);
}

static int bad_seq_num_count = 0;
static int bad_crc_count = 0;
static int lost_packets = 0;

static void print_stats(int* pid_table, int diff_ms, int uptime_ms) {
  int i = 0;
  double diff = diff_ms / 1000.0;
  for (i = 0; i <= ALL_PID; i++) {
    int v = pid_table[i];
    if (v > 0) {
      printf("%04x %5d p/s %5d kb/s %5d kbit\n", i,
             (int)(v/diff),
             (int)(v/diff*TS_PACKET_SIZE/1024),
             (int)(v*8/diff*TS_PACKET_SIZE/1000));
      pid_table[i] = 0;
    }
  }
  printf("-PID--FREQ-----BANDWIDTH-BANDWIDTH- CRC %d SEQ %d LOST %d TIME %.1fs\n",
         bad_crc_count, bad_seq_num_count, lost_packets, uptime_ms/1000.0);
}

int main(int argc, char** argv) {
  int err = 0;
  int opt;
  int adapter = 0;
  int demux = 0;
  int max_seq_num = 1000000;
  int summary = 0;
  int quiet = 0;
  int pid = 0x2000;
  int timeout = 0;
  int buffer_size = 16*1024*1024;
  int fd = 0;
  int dmxfd = 0;
  int dvrfd = 0;
  int realtime = 0;
  int use_dvr = 0;
  int use_crc = 1;

  int infd = 0;
  char* infile = NULL;

  int outfd = 0;
  char* outfile = NULL;

  int packets = 0;
  int skipped = 0;

  int* pid_table = NULL;
  uint32_t* seq_table = NULL;

  int rbuf_size = TS_PACKET_SIZE*21;
  uint8_t* buf = NULL;

  int64_t start, t0, t1;

  while ((opt = getopt(argc, argv, "a:d:b:i:m:o:p:t:cqrsh")) != -1) {
    switch (opt) {
      case 'a':
        adapter = atoi(optarg);
        break;
      case 'd':
        demux = atoi(optarg);
        break;
      case 'b':
        buffer_size = atoi(optarg);
        break;
      case 'i':
        infile = optarg;
        break;
      case 'm':
        max_seq_num = atoi(optarg);
        break;
      case 'o':
        outfile = optarg;
        break;
      case 'p':
        pid = atoi(optarg);
        break;
      case 't':
        timeout = atoi(optarg);
        break;
      case 'c':
        use_crc = 0;
        break;
      case 'q':
        quiet = 1;
        break;
      case 'r':
        realtime = 1;
        break;
      case 's':
        summary = 1;
        break;
      default:
        usage(argv[0]);
    }
  }

  if (realtime) {
    int policy = SCHED_RR;
    struct sched_param sp = {};
    sp.sched_priority = sched_get_priority_max(policy);
    err = sched_setscheduler(0, policy, &sp);
    if (err < 0) {
      fatal("sched_setscheduler failed");
    }
  }

  if (infile) {
    infd = open(infile, O_RDONLY);
    if (infd < 0) {
      fprintf(stderr, "Failed to open input file: %s\n", infile);
    }
    fd = infd;
  }

  if (infd <= 0) {
    dmxfd = dvb_open(adapter, demux, "demux", 0);
    if (dmxfd < 0) {
      return 1;
    }
    fd = dmxfd;

    if (use_dvr) {
      dvrfd = dvb_open(adapter, 0, "dvr", 1);
      if (dvrfd < 0) {
        return 1;
      }
      fd = dvrfd;
    }

    err = set_buffer_size(dmxfd, buffer_size);
    if (err < 0) {
      fatal("Failed to set buffer size");
    }

    err = set_pid_filter(dmxfd, pid, use_dvr);
    if (err < 0) {
      fatal("Failed to set PID filter");
    }

    if (outfile != NULL) {
      outfd = creat(outfile, 0644);
      if (outfd < 0) {
        fprintf(stderr, "Failed to open output file: %s\n", outfile);
      }
    }
  }

  pid_table = calloc(ALL_PID+1, sizeof(pid_table[0]));
  seq_table = calloc(ALL_PID+1, sizeof(seq_table[0]));

  buf = malloc(rbuf_size); // Read about 4K of data

  start = t0 = time_ms();

  while (1) {
    int i, n;
    uint32_t seq_num;

    n = read(fd, buf, rbuf_size);
    if (n <= 0) {
      fprintf(stderr, "Read returned %d, stop! %s\n", n, strerror(errno));
      break;
    }

    if ((n % TS_PACKET_SIZE) != 0) {
      fatal("Read partial packet");
    }

    if (outfd > 0) {
      int w = write(outfd, buf, n);
      if (w != n) {
        fprintf(stderr, "Failed to write %d bytes\n", w);
      }

      if (timeout > 0) {
        if ((time_ms() - start) >= (timeout*1000)) {
          break;
        }
      }

      continue;
    }

    for (i = 0; i < n; i += TS_PACKET_SIZE) {
      int pkt_pid;
      uint32_t expected;
      uint8_t *pkt = buf + i;

      if (pkt[0] != SYNC_BYTE) {
        fatal("Not a valid packet");
      }

      pkt_pid = (pkt[1] << 8 | pkt[2]) & PID_MASK;

      pid_table[pkt_pid]++;
      pid_table[ALL_PID]++;
      packets++;

      seq_num = pkt[4] << 24 | pkt[5] << 16 | pkt[6] << 8 | pkt[7];

      if (skipped < 100) {
        start = t0 = time_ms();
        seq_table[pkt_pid] = seq_num;
        skipped++;
        continue;
      }

      if (pkt_pid == NULL_PID) {
        continue;
      }

      if (use_crc && crc32(0, pkt+4, TS_PACKET_SIZE-4) != EXPECTED_CRC) {
        bad_crc_count++;
        continue;
      }

      if ((pkt[3]&0x0f) != (seq_num%16)) {
        fprintf(stderr, "seq_num %d cc %d\n", seq_num, pkt[3]&0x0f);
      }

      expected = (seq_table[pkt_pid] + 1) % max_seq_num;
      if (seq_num != expected) {
        bad_seq_num_count++;
        if (seq_num > expected) {
          lost_packets += seq_num - expected;
        } else {
          uint32_t delta = (seq_num + max_seq_num) - expected;
          if (delta < 100) {
            lost_packets += delta;
          } else {
            lost_packets++;
            fprintf(stderr, "stale packet seq %d expected %d\n", seq_num, expected);
          }
        }
        if (lost_packets < bad_seq_num_count) {
          fatal("Lost packets less than bad sequence; check max sequence!");
        }
      }
      seq_table[pkt_pid] = seq_num;
    }

    if ((packets & 0x7f) == 0) {
      int diff;
      t1 = time_ms();
      diff = t1 - t0;
      if (diff >= 1000) {
        if (!quiet) {
          print_stats(pid_table, diff, t1 - start);
        }

        if (timeout > 0 && (t1 - start) >= (timeout*1000)) {
          break;
        }

        t0 = t1;
      }
    }
  }

  if (summary) {
    printf("CRC %d SEQ %d LOST %d TIME %.1fs\n", bad_crc_count,
           bad_seq_num_count, lost_packets, (time_ms()-start)/1000.0);
  }

  free(buf);
  free(seq_table);
  free(pid_table);

  if (dvrfd > 0) {
    close(dvrfd);
  }
  if (dmxfd > 0) {
    close(dmxfd);
  }
  if (outfd > 0) {
    close(outfd);
  }
  if (infd > 0) {
    close(infd);
  }

  if (bad_crc_count > 0 || bad_seq_num_count > 0) {
    return EXIT_FAILURE;
  }

  return 0;
}
