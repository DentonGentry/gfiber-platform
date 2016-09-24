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
/* Note: though this specific file is licensed under the Apache license,
 * it exists in order to interface with the RAS_LIB.c implementation
 * provided by TI which is not licensed under Apache. */

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "rcu-audio.h"
#include "remote_control_audio.pb.h"
#include "RAS_lib.h"

#define TI_AUDIO_PATH "\0rc_audio_ti"

int main(int argc, char **argv)
{
  int is = -1, os = -1, connected = 0;
  struct sockaddr_un sun;
  struct sockaddr_in sin;
  uint8 prev = 0;
  int msgs = 0, missed = 0, errors = 0;

  is = get_socket_or_die();
  memset(&sun, 0, sizeof(sun));
  sun.sun_family = AF_UNIX;
  snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", TI_AUDIO_PATH);
  if (bind(is, (const struct sockaddr *) &sun, sizeof(sun)) < 0) {
    perror("bind(AF_UNIX)");
    exit(1);
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(RCU_AUDIO_PORT);
  sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  while (1) {
    uint8 ibuf[MAX_INPUT_BUF_SIZE + 6 + 1 + 4];
    ssize_t ilen;

    ilen = recv(is, ibuf, sizeof(ibuf), 0);
    if (ilen < 23) {
      /* end of an audio file, prepare for the next one */
      printf("Finished audio stream; msgs = %d, missed = %d, errors = %d\n",
          msgs, missed, errors);
      msgs = missed = errors = 0;
      RAS_Init(RAS_NO_PEC);
    } else {
      uint8_t remote_type = ibuf[6];
      uint8 expected = (prev + 1) & 0x1f;
      uint8 seqnum = (ibuf[7] >> 3) & 0x1f;
      uint8 *data;
      uint16 data_len;
      int16 obuf[4 * MAX_INPUT_BUF_SIZE];
      uint16 olen;

      if (seqnum != expected) {
        missed++;
      }
      prev = seqnum;
      msgs++;

      /*
       * We skip over:
       *   the first 6 bytes, which is the BDADDR of the remote.
       *   the remote type byte (0 == GFRM210)
       *   the sequence number byte
       *
       * The first three bytes of payload are some kind of RAS header.
       * RAS_Decode() says the data pointer must include the three bytes
       * of header, but the length must not include those 3 bytes.
       * So the length is decremented by 6+1+1+3 for a total of 11.
       */
      data = &ibuf[6 + 1 + 1];
      data_len = ilen - (6 + 1 + 1 + 3);
      if (RAS_Decode(RAS_DECODE_TI_TYPE1, data, data_len, obuf, &olen) == 0) {
        rcaudio::AudioSamples samples;
        char bdaddr[18];
        std::vector<uint8_t> pkt;

        snprintf(bdaddr, sizeof(bdaddr),
            "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
            ibuf[0], ibuf[1], ibuf[2], ibuf[3], ibuf[4], ibuf[5]);

        samples.set_rc_address(bdaddr);
        if (remote_type == 0) {
          samples.set_audio_format(rcaudio::AudioSamples::PCM_16BIT_16KHZ);
          samples.set_remote_type(rcaudio::AudioSamples::GFRM210);
        } else {
          samples.set_audio_format(rcaudio::AudioSamples::UNDEFINED_AUDIO_FORMAT);
          samples.set_remote_type(rcaudio::AudioSamples::UNDEFINED_REMOTE_TYPE);
        }
        samples.set_audio_samples(obuf, olen);

        pkt.resize(samples.ByteSize());
        samples.SerializeToArray(&pkt[0], samples.ByteSize());

        if (os < 0) {
          os = get_socket_or_die();
        }

        if (!connected) {
          if (connect(os, (const struct sockaddr *) &sin, sizeof(sin)) == 0) {
            connected = 1;
          } else {
            sleep(2);  /* rate limit how often we try */
          }
        }

        if (connected) {
          if (send(os, &pkt[0], pkt.size(), 0) != (ssize_t)pkt.size()) {
            fprintf(stderr, "Audio send failed, will reconnect.\n");
            connected = 0;
            close(os);
            os = -1;
          }
        }
      } else {
        if (pacing()) {
          printf("RAS_Decode(RAS_DECODE_TI_TYPE1) failed\n");
        }
        errors++;
      }
    }
  }
}
