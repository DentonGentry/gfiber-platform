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

/*
 * Derived from hid-example.c, license:
 *
 * Hidraw Userspace Example
 *
 * Copyright (c) 2010 Alan Ott <alan@signal11.us>
 * Copyright (c) 2010 Signal 11 Software
 *
 * The code may be used by anyone for any purpose,
 * and can serve as a starting point for developing
 * applications using hidraw.
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <vector>

#include "rcu-audio.h"
#include "remote_control_audio.pb.h"


int main(int argc, char **argv)
{
  int in = -1, out = -1, connected = 0;
  const char *device;
  struct sockaddr_un sun;
  char name[16];
  char address[64];

  if (argc != 2) {
    fprintf(stderr, "usage: %s /dev/hidraw#\n", argv[0]);
    exit(1);
  }
  device = argv[1];

  if ((in = open(device, O_RDWR)) < 0) {
    perror("open /dev/hidraw");
    exit(1);
  }

  memset(&sun, 0, sizeof(sun));
  sun.sun_family = AF_UNIX;
  strncpy(&sun.sun_path[1], RCU_AUDIO_PATH, sizeof(sun.sun_path) - 2);

  if (ioctl(in, HIDIOCGRAWNAME(sizeof(name)), name) < 0) {
    perror("HIDIOCGRAWNAME");
    exit(1);
  }
  if (strcmp(name, "GFRM100") != 0) {
    fprintf(stderr, "%s is not a GFRM100. Exiting.\n", device);
    exit(0);
  }

  if (ioctl(in, HIDIOCGRAWPHYS(sizeof(address)), address) < 0) {
    perror("HIDIOCGRAWPHYS");
    exit(1);
  }

  /* this process will be started out of the hotplug script when a new
   * remote appears. We either exit if not a GFRM100, or daemonize to
   * let the hotplug script continue. */
  if (daemon(0, 1)) {
    perror("daemon()");
    exit(1);
  }

  while (1) {
    uint8_t data[2048];
    size_t len = read(in, data, sizeof(data));

    if (len < 0) {
      fprintf(stderr, "GFRM100 has disconnected. Exiting.\n");
      exit(0);
    }

    if (data[0] != 0xf7) {
      /* Not an audio packet */
      continue;
    }

    if (data[1] == 0x01 && len > 4) {
      rcaudio::AudioSamples samples;
      std::vector<uint8_t> pkt;

      samples.set_rc_address(address);
      samples.set_audio_format(rcaudio::AudioSamples::PCM_16BIT_16KHZ);
      samples.set_remote_type(rcaudio::AudioSamples::GFRM100);

      /*
       * data[0] == 0xf7
       * data[1] == 0x01
       * data[2] and data[3] are a count of the number of samples.
       * data[4] == first byte of audio data.
       */
      samples.set_audio_samples(data + 4, len - 4);

      pkt.resize(samples.ByteSize());
      samples.SerializeToArray(&pkt[0], samples.ByteSize());

      if (out < 0) {
        out = get_socket_or_die();
      }

      if (!connected) {
        if (connect(out, (const struct sockaddr *) &sun, sizeof(sun)) == 0) {
          connected = 1;
        } else {
          sleep(2);  /* rate limit how often we retry. */
        }
      }

      if (connected) {
        if (send(out, &pkt[0], pkt.size(), 0) != (ssize_t)pkt.size()) {
          fprintf(stderr, "Audio send failed, will reconnect.\n");
          close(out);
          out = -1;
          connected = 0;
        }
      }
    }
  }
  return 0;
}
