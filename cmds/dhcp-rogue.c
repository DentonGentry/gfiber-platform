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
 * Send fake DHCP Discover messages, and print information about any DHCP
 * server which responds. Intended to locate rogue DHCP servers on the LAN.
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct dhcp_message {
  uint8_t op;
  #define OP_BOOTREQUEST 1
  uint8_t htype;
  #define HTYPE_ETHERNET 1
  uint8_t hlen;
  uint8_t hops;
  uint32_t xid;
  uint16_t secs;
  uint16_t flags;
  #define FLAGS_BROADCAST 0x8000
  uint32_t ciaddr;
  uint32_t yiaddr;
  uint32_t siaddr;
  uint32_t giaddr;
  uint8_t chaddr[16];
  char sname[64];
  char file[128];
  uint8_t magic[4];
  uint8_t type[3];
};

int create_socket(const char *ifname)
{
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in sin;
  struct ifreq ifr;
  int enable = 1;
  struct timeval tv;

  if (s < 0) {
    perror("socket(AF_INET)");
    exit(1);
  }

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

  if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE,
                 (void *)&ifr, sizeof(ifr)) < 0) {
    perror("SO_BINDTODEVICE");
    exit(1);
  }

  if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable))) {
    perror("SO_BROADCAST");
    exit(1);
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) {
    perror("SO_REUSEADDR");
    exit(1);
  }

  tv.tv_sec = 5;
  tv.tv_usec = 0;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
    perror("SO_RCVTIMEO");
    exit(1);
  }

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

  if (ioctl(s, SIOCGIFADDR, &ifr) < 0) {
    perror("SIOCGIFADDR");
    exit(1);
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(68);

  if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("bind");
    exit(1);
  }

  return s;
}

void get_chaddr(uint8_t *chaddr, int s, const char *ifname)
{
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

  if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0) {
    perror("ioctl(SIOCGIFHWADDR)");
    exit(1);
  }

  if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
    fprintf(stderr, "%s is not Ethernet\n", ifname);
    exit(1);
  }

  memcpy(chaddr, ifr.ifr_hwaddr.sa_data, 6);
}

void send_dhcp_discover(int s, const char *ifname)
{
  struct dhcp_message msg;
  struct sockaddr_in sin;
  socklen_t slen = sizeof(sin);

  memset(&msg, 0, sizeof(msg));
  msg.op = OP_BOOTREQUEST;
  msg.htype = HTYPE_ETHERNET;
  msg.hlen = 6;
  msg.xid = 0;
  msg.flags = htons(FLAGS_BROADCAST);
  get_chaddr(msg.chaddr, s, ifname);
  snprintf(msg.sname, sizeof(msg.sname), "%s", "rogue_dhcp_server_detection");
  msg.magic[0] = 99;  /* DHCP magic number, RFC 2133 */
  msg.magic[1] = 130;
  msg.magic[2] = 83;
  msg.magic[3] = 99;
  msg.type[0] = 53;  /* option 53, DHCP type. */
  msg.type[1] = 1;  /* length = 1 */
  msg.type[2] = 1;  /* DHCPDISCOVER */

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  sin.sin_port = htons(67);

  if (sendto(s, &msg, sizeof(msg), 0,
             (const struct sockaddr *)&sin, slen) < 0) {
    perror("sendto enable");
    exit(1);
  }
}

void receive_dhcp_offers(int s)
{
  struct sockaddr_in sin;
  socklen_t slen = sizeof(sin);
  uint8_t buf[2048];

  memset(&sin, 0, sizeof(sin));
  while (recvfrom(s, buf, sizeof(buf), 0, &sin, &slen) > 0) {
    char ipbuf[64];
    inet_ntop(AF_INET, &sin.sin_addr, ipbuf, sizeof(ipbuf));
    printf("DHCP response from %s\n", ipbuf);
  }
}

void usage(const char *progname)
{
  printf("usage: %s [-i br0]\n", progname);
  printf("\t-i: name of the interface to probe for rogue DHCP servers.\n");
  exit(1);
}

int main(int argc, char **argv)
{
  int s;
  const char *interface = "br0";
  struct option long_options[] = {
    {"interface", required_argument, 0, 'i'},
    {0,          0,                 0, 0},
  };

  int c;
  while ((c = getopt_long(argc, argv, "i:", long_options, NULL)) != -1) {
    switch (c) {
    case 'i':
      interface = optarg;
      break;
    case '?':
    default:
      usage(argv[0]);
      break;
    }
  }

  s = create_socket(interface);
  send_dhcp_discover(s, interface);
  receive_dhcp_offers(s);
}
