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

#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <openssl/md5.h>
#include <openssl/hmac.h>


const char SOFT[] = "AEIOUY" "V";
const char HARD[] = "BCDFGHJKLMNPQRSTVWXYZ" "AEIOU";
const char *consensus_key_file = "/tmp/waveguide/consensus_key";
#define CONSENSUS_KEY_LEN 16
uint8_t consensus_key[CONSENSUS_KEY_LEN] = {0};
#define MAC_ADDR_LEN 17

void default_consensus_key()
{
  int fd;

  if ((fd = open("/dev/urandom", O_RDONLY)) >= 0) {
    ssize_t siz = sizeof(consensus_key);
    if (read(fd, consensus_key, siz) != siz) {
      /* https://xkcd.com/221/ */
      memset(consensus_key, time(NULL), siz);
    }
    close(fd);
  }
}

/* Read the waveguide consensus_key, if any. Returns 0 if
 * a key was present, 1 if not or something fails. */
int get_consensus_key()
{
  int fd, rc = 1;
  uint8_t new_key[sizeof(consensus_key)];

  if ((fd = open(consensus_key_file, O_RDONLY)) < 0) {
    return 1;
  }

  if (read(fd, new_key, sizeof(new_key)) == sizeof(new_key)) {
    memcpy(consensus_key, new_key, sizeof(consensus_key));
    rc = 0;
  }
  close(fd);

  return rc;
}

/* Given a value from 0..4095, encode it as a cons+vowel+cons sequence. */
void trigraph(int num, char *out)
{
  int ns = sizeof(SOFT) - 1;
  int nh = sizeof(HARD) - 1;
  int c1, c2, c3;

  c3 = num % nh;
  c2 = (num / nh) % ns;
  c1 = num / nh / ns;
  out[0] = HARD[c1];
  out[1] = SOFT[c2];
  out[2] = HARD[c3];
}

int hex_chr_to_int(char hex) {
  switch(hex) {
    case '0' ... '9':
      return hex - '0';
    case 'a' ... 'f':
      return hex - 'a' + 10;
    case 'A' ... 'F':
      return hex - 'A' + 10;
  }

  return 0;
}

/*
 * Convert a string of the form "00:11:22:33:44:55" to
 * a binary array 001122334455.
 */
void get_binary_mac(const char *mac, uint8_t *out) {
  int i;
  for (i = 0; i < MAC_ADDR_LEN; i += 3) {
    *out = (hex_chr_to_int(mac[i]) << 4) | hex_chr_to_int(mac[i+1]);
    out++;
  }
}


void get_anonid_for_mac(const char *mac, char *out) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = sizeof(digest);
  uint8_t macbin[6];
  uint32_t num;

  get_binary_mac(mac, macbin);
  HMAC(EVP_md5(), consensus_key, CONSENSUS_KEY_LEN, macbin, sizeof(macbin),
      digest, &digest_len);
  num = (digest[0] << 16) | (digest[1] << 8) | digest[2];
  trigraph((num >> 12) & 0x0fff, out);
  trigraph((num      ) & 0x0fff, out + 3);
}


void usage(const char *progname)
{
  fprintf(stderr, "usage: %s: -a ##:##:##:##:##:## [-k consensus_key]\n",
      progname);
  fprintf(stderr, "\t-a addr: MAC address to generate an anonid for\n");
  fprintf(stderr, "\t-k key: Use a specific consensus_key. "
      "Default is to read it from %s\n", consensus_key_file);
  exit(1);
}


int main(int argc, char **argv)
{
  struct option long_options[] = {
    {"addr",          required_argument, 0, 'a'},
    {"consensus_key", required_argument, 0, 'k'},
    {0,          0,                 0, 0},
  };
  const char *addr = NULL;
  char anonid[7];
  size_t lim;
  int c;

  setlinebuf(stdout);
  alarm(30);

  if (get_consensus_key()) {
    default_consensus_key();
  }

  while ((c = getopt_long(argc, argv, "a:k:", long_options, NULL)) != -1) {
    switch (c) {
    case 'a':
      addr = optarg;
      break;
    case 'k':
      lim = (sizeof(consensus_key) > strlen(optarg)) ? strlen(optarg) :
        sizeof(consensus_key);
      memset(consensus_key, 0, sizeof(consensus_key));
      memcpy(consensus_key, optarg, lim);
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  if (addr == NULL) {
    usage(argv[0]);
  }

  memset(anonid, 0, sizeof(anonid));
  get_anonid_for_mac(addr, anonid);
  printf("%s\n", anonid);

  exit(0);
}
