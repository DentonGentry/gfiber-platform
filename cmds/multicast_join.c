/*
 * Copyright 2014 Google Inc. All rights reserved.
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
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


static void usage(const char *progname)
{
  fprintf(stderr, "\nUsage: %s 239.0.0.1 [224.0.0.2] [...]]\n", progname);
  exit(1);
}


int main(int argc, char **argv)
{
  int i, s;
  struct in_addr sin;

  if (argc < 2) {
    usage(argv[0]);
  }

  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  for (i = 1; i < argc; ++i) {
    struct ip_mreq mreq;
    char errbuf[256];

    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET, argv[i], &sin) != 1) {
      snprintf(errbuf, sizeof(errbuf), "inet_pton(%s)", argv[i]);
      perror(errbuf);
      exit(1);
    }
    mreq.imr_multiaddr = sin;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) {
      snprintf(errbuf, sizeof(errbuf), "IP_ADD_MEMBERSHIP %s", argv[i]);
      perror(errbuf);
      exit(1);
    }
  }

  while (1) {
    pause();
  }
  // NOTREACHED
}
