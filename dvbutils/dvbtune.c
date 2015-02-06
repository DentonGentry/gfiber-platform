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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <strings.h>
#include <time.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

static int dvb_open(int adapter, int device, const char* type) {
  char s[100];
  if (snprintf(s, sizeof(s), "/dev/dvb/adapter%d/%s%d",
               adapter, type, device) < 0) {
    return -EINVAL;
  }
  return open(s, O_RDWR);
}

static int dvb_fe_set_properties(int fefd, int mod, int ifreq, int sr,
                                 int fec, int voltage, int tone) {
  int num = 0;
  struct dtv_properties ps;
  struct dtv_property props[DTV_IOCTL_MAX_MSGS];

  int pairs[][2] = {
    {DTV_FREQUENCY, ifreq},
    {DTV_MODULATION, mod},
    {DTV_INVERSION, INVERSION_AUTO},
    {DTV_SYMBOL_RATE, sr},
    {DTV_INNER_FEC, fec},
    {DTV_VOLTAGE, voltage},
    {DTV_TONE, tone},
    {DTV_PILOT, PILOT_AUTO},
    {DTV_ROLLOFF, ROLLOFF_AUTO},
    // TODO: Add command option for delivery system.
    {DTV_DELIVERY_SYSTEM, SYS_DVBS2},
    {DTV_TUNE, 1},
  };

  for (num = 0; num < ARRAY_SIZE(pairs); num++) {
    props[num].cmd = pairs[num][0];
    props[num].u.data = pairs[num][1];
  }

  ps.num = num;
  ps.props = props;
  return ioctl(fefd, FE_SET_PROPERTY, &ps);
}

// Returns 0 on timeout, 1 on event, and < 0 on error.
static int dvb_fe_get_event(int fefd, struct dvb_frontend_event* ev, int timeout) {
  int err;
  struct pollfd fds = {.fd = fefd, .events = POLLPRI};
  if (!ev) {
    return -EINVAL;
  }
  err = poll(&fds, 1, timeout);
  if (err > 0) {
    if ((fds.revents & POLLPRI) != 0) {
      err = ioctl(fefd, FE_GET_EVENT, ev);
      if (err >= 0) {
        return 1;
      }
    }
    return 0;
  }
  return err;
}

static int str2fec(const char* s) {
  int fec = FEC_NONE;
  if (strcasecmp(s, "none") == 0) {
    fec = FEC_NONE;
  } else if (strcasecmp(s, "auto") == 0) {
    fec = FEC_AUTO;
  } else {
    int fec = strtol(s, NULL, 0);
    switch (fec) {
      case 12:
        fec = FEC_1_2;
        break;
      case 23:
        fec = FEC_2_3;
        break;
      case 34:
        fec = FEC_3_4;
        break;
      case 45:
        fec = FEC_4_5;
        break;
      case 56:
        fec = FEC_5_6;
        break;
      case 67:
        fec = FEC_6_7;
        break;
      case 78:
        fec = FEC_7_8;
        break;
      case 35:
        fec = FEC_3_5;
        break;
      case 910:
        fec = FEC_9_10;
        break;
      default:
        fec = FEC_AUTO;
    }
  }
  return fec;
}

static int64_t time_ms() {
  struct timespec tv = {0};
  if (clock_gettime(CLOCK_MONOTONIC, &tv) < 0) {
    return -1;
  }
  return tv.tv_sec * 1000LL + tv.tv_nsec / 1000000;
}

// Return 1 if locked and 0 otherwise.
static int wait_for_lock(int fefd, int timeout) {
  int locked = 0;
  int64_t t0 = time_ms();
  struct dvb_frontend_event ev;

  while (1) {
    int err = dvb_fe_get_event(fefd, &ev, 100);
    if (err < 0) {
      if (EOVERFLOW == errno) {
        continue;
      }
      perror("dvb_fe_get_event");
      break;
    }

    if (err > 0 && (ev.status & FE_HAS_LOCK) != 0) {
      printf("Status %#x Locked!\n", ev.status);
      locked = 1;
      break;
    }

    if ((time_ms() - t0) > timeout) {
      printf("Status %#x No lock!\n", ev.status);
      break;
    }
  }

  return locked;
}

static void usage(const char* prog) {
  fprintf(stderr, "Usage: %s [options]\n", prog);
  fprintf(stderr, "    -a Adapter Adapter device (default 0)\n");
  fprintf(stderr, "    -d Device  Front end device (default 0)\n");
  fprintf(stderr, "    -i Freq    Intermediate frequency in kHz (required)\n");
  fprintf(stderr, "    -r Rate    Symbol rate in 1000's (required)\n");
  fprintf(stderr, "    -c FEC     Forward Error Correction code\n");
  fprintf(stderr, "    -p <v|h>   Polarization voltage (default off)\n");
  fprintf(stderr, "    -t         Turn on 22kHz tone\n");
  fprintf(stderr, "    -w timeout Milliseconds to wait for lock\n");
  fprintf(stderr, "    -x         Exit after tuning\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
  int err;
  int opt;
  int adapter = 0;
  int ifreq_khz = 0;
  // TODO: Add command option for modulation.
  int mod = PSK_8;
  int sr_k = 0;
  int fec = FEC_AUTO;
  int voltage = SEC_VOLTAGE_OFF;
  int tone = SEC_TONE_OFF;
  int dev = 0;
  int fefd = 0;
  int required_args = 0;
  int loop = 1;
  int timeout = 2000;

  while ((opt = getopt(argc, argv, "a:d:i:p:r:c:w:txh")) != -1) {
    switch (opt) {
      case 'a':
        adapter = atoi(optarg);
        break;
      case 'd':
        dev = atoi(optarg);
        break;
      case 'i':
        ifreq_khz = atoi(optarg);
        required_args++;
        break;
      case 'r':
        sr_k = atoi(optarg);
        required_args++;
        break;
      case 'c':
        fec = str2fec(optarg);
        break;
      case 'p': {
        char p = optarg[0];
        if (p == 'h' || p == 'H') {
          voltage = SEC_VOLTAGE_18;
        } else if (p == 'v' || p == 'V') {
          voltage = SEC_VOLTAGE_13;
        }
        break;
      }
      case 't':
        tone = SEC_TONE_ON;
        break;
      case 'w': {
        int t = atoi(optarg);
        if (t > 0) {
          timeout = t;
        }
        break;
      }
      case 'x':
        loop = 0;
        break;
      default:
        usage(argv[0]);
    }
  }

  if (required_args < 2) {
    usage(argv[0]);
  }

  fefd = dvb_open(adapter, dev, "frontend");
  if (fefd < 0) {
    return 1;
  }

  err = dvb_fe_set_properties(fefd, mod, ifreq_khz, sr_k*1000, fec, voltage, tone);
  if (err < 0) {
    perror("dvb_fe_set_properties");
  }

  if (err == 0) {
    // Check lock status at least once.
    do {
      if (wait_for_lock(fefd, timeout)) {
        break;
      }
    } while (loop);

    // Loop to keep driver active.
    while (loop) {
      struct dvb_frontend_event ev;
      dvb_fe_get_event(fefd, &ev, 1000);
    }
  }

  close(fefd);

  return err;
}
