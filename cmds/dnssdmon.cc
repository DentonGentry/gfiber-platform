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
 * dnssdmon
 * Listen for DNS-SD packets containing TXT fields which help to identify
 * the device. For example, some iOS devices send a model string:
 *    My iPad._device-info._tcp.local: type TXT, class IN, cache flush
 *          Name: My iPad._device-info._tcp.local
 *          Type: TXT (Text strings) (16)
 *          .000 0000 0000 0001 = Class: IN (0x0001)
 *          1... .... .... .... = Cache flush: True
 *          Time to live: 4500
 *          Data length: 12
 *          TXT Length: 11
 *          TXT: model=J81AP
 */

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <assert.h>
#include <ctype.h>
#include <net/if.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <tr1/unordered_map>

#include "l2utils.h"
#include "modellookup.h"


#define MDNS_PORT 5353
#define MDNS_IPV4 "224.0.0.251"
#define MDNS_IPV6 "ff02::fb"
#define MIN(a,b) (((a)<(b))?(a):(b))


typedef std::tr1::unordered_map<std::string, std::string> HostsMapType;
HostsMapType hosts;


/* Return monotonically increasing time in seconds. */
static time_t monotime(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec;
}


static void strncpy_limited(char *dst, size_t dstlen,
    const char *src, size_t srclen)
{
  size_t i;
  size_t lim = (srclen >= (dstlen - 1)) ? (dstlen - 2) : srclen;

  for (i = 0; i < lim; ++i) {
    unsigned char s = src[i];
    if (isspace(s) || s == ';') {
      dst[i] = ' ';  // deliberately convert newline to space
    } else if (isprint(s)) {
      dst[i] = s;
    } else {
      dst[i] = '_';
    }
  }
  dst[lim] = '\0';
}


void add_hostmap_entry(std::string macaddr, std::string model)
{
  HostsMapType::iterator found = hosts.find(macaddr);
  if (found != hosts.end()) {
    return;
  }

  hosts[macaddr] = model;
}


int get_ifindex(const char *ifname)
{
  int fd;
  struct ifreq ifr;
  size_t nlen = strlen(ifname);

  if ((fd = socket(AF_PACKET, SOCK_DGRAM, 0)) < 0) {
    perror("ERR: socket");
    exit(1);
  }

  if (nlen >= sizeof(ifr.ifr_name)) {
    fprintf(stderr, "ERR: interface name %s is too long\n", ifname);
    exit(1);
  }

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, nlen);
  ifr.ifr_name[nlen] = '\0';

  if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
    perror("ERR: SIOCGIFINDEX");
    exit(1);
  }

  close(fd);
  return ifr.ifr_ifindex;
}  /* get_ifindex */


void init_mdns_socket_common(int s, const char *ifname)
{
  struct ifreq ifr;
  unsigned int enable = 1;

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
  if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) {
    perror("ERR: SO_BINDTODEVICE");
    exit(1);
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
    perror("ERR: SO_REUSEADDR");
    exit(1);
  }
}


int init_mdns_socket_ipv4(const char *ifname)
{
  int s;
  struct sockaddr_in sin;
  struct ip_mreq mreq;

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("ERR: socket");
    exit(1);
  }
  init_mdns_socket_common(s, ifname);

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(MDNS_PORT);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (struct sockaddr *)&sin, sizeof(sin))) {
    perror("ERR: bind");
    exit(1);
  }

  memset(&mreq, 0, sizeof(mreq));
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (inet_pton(AF_INET, MDNS_IPV4, &mreq.imr_multiaddr) != 1) {
    fprintf(stderr, "ERR: inet_pton(%s)", MDNS_IPV4);
    exit(1);
  }
  if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    perror("ERR: setsockopt(IP_ADD_MEMBERSHIP)");
    exit(1);
  }

  return s;
}


int init_mdns_socket_ipv6(const char *ifname, int ifindex)
{
  int s;
  struct sockaddr_in6 sin6;
  struct ipv6_mreq mreq;
  int off = 0;

  if ((s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("ERR: socket(AF_INET6)");
    exit(1);
  }
  init_mdns_socket_common(s, ifname);

  if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&off, sizeof(off))) {
    perror("ERR: setsockopt(IPV6_V6ONLY)");
    exit(1);
  }

  memset(&sin6, 0, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_port = htons(MDNS_PORT);
  sin6.sin6_addr = in6addr_any;
  if (bind(s, (struct sockaddr *)&sin6, sizeof(sin6))) {
    perror("ERR: bind");
    exit(1);
  }

  memset(&mreq, 0, sizeof(mreq));
  mreq.ipv6mr_interface = ifindex;
  if (inet_pton(AF_INET6, MDNS_IPV6, &mreq.ipv6mr_multiaddr) != 1) {
    fprintf(stderr, "ERR: inet_pton(%s) failed", MDNS_IPV6);
    exit(1);
  }
  if (setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
    perror("ERR: setsockopt(IPV6_JOIN_GROUP)");
    exit(1);
  }

  return s;
}


/*
 * Search a DNS TXT record for fields which look like a model description.
 * MacOS and iOS devices send "model=...", various peripherals send
 * "ty=..."  (presumably for 'type').
 */
int parse_txt_for_model(const unsigned char *rdata, unsigned int rdlen,
    char *model, unsigned int modellen)
{
  const unsigned char *p = rdata;

  while (rdlen > 0) {
    unsigned int txtlen = p[0];
    /*
     * TXT record format is:
     * Length1 (1 byte)
     * String1 (variable length)
     * Length2 (1 byte)
     * String2 (variable length)
     * etc.
     */
    p++;
    rdlen--;
    if (txtlen > rdlen) {
      fprintf(stderr, "ERR: Malformed TXT record\n");
      return -1;
    }

    if (txtlen > 6 && strncmp((const char *)p, "model=", 6) == 0) {
      strncpy_limited(model, modellen, (const char *)(p + 6), txtlen - 6);
      return 0;
    }
    if (txtlen > 3 && strncmp((const char *)p, "ty=", 3) == 0) {
      strncpy_limited(model, modellen, (const char *)(p + 3), txtlen - 3);
      return 0;
    }
    rdlen -= txtlen;
    p += txtlen;
  }

  return 1;
}

void process_mdns(int s)
{
  ssize_t len;
  ns_msg msg;
  unsigned int i, n, rr_count;
  ns_sect sections[] = {ns_s_an, ns_s_ar};  // Answers and Additional Records
  struct sockaddr_storage from;
  socklen_t fromlen = sizeof(from);
  char ipstr[INET6_ADDRSTRLEN];
  uint8_t buf[4096];

  if ((len = recvfrom(s, buf, sizeof(buf), 0,
      (struct sockaddr *)&from, &fromlen)) < 0) {
    return;
  }

  if (from.ss_family == AF_INET) {
    inet_ntop(AF_INET, &(((struct sockaddr_in *)&from)->sin_addr),
        ipstr, sizeof(ipstr));
  } else if (from.ss_family == AF_INET6) {
    inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&from)->sin6_addr),
        ipstr, sizeof(ipstr));
  }

  if (ns_initparse(buf, len, &msg) < 0) {
    fprintf(stderr, "ERR: ns_initparse\n");
    return;
  }

  for (i = 0; i < (sizeof(sections) / sizeof(sections[0])); ++i) {
    ns_sect sect = sections[i];
    rr_count = ns_msg_count(msg, sect);
    for (n = 0; n < rr_count; ++n) {
      ns_rr rr;
      if (ns_parserr(&msg, sect, n, &rr)) {
        fprintf(stderr, "ERR: unable to parse RR type=%s n=%d\n",
            (sect == ns_s_an) ? "ns_s_an" : "ns_s_ar", n);
        continue;
      }

      if (ns_rr_type(rr) == ns_t_txt) {
        const unsigned char *rdata = ns_rr_rdata(rr);
        unsigned int rdlen = ns_rr_rdlen(rr);
        char model[64];
        if (parse_txt_for_model(rdata, rdlen, model, sizeof(model)) == 0) {
          std::string mac = get_l2addr_for_ip(std::string(ipstr));
          add_hostmap_entry(mac, model);
        }
      }
    }
  }
}


void listen_for_mdns(const char *ifname, int ifindex, time_t seconds)
{
  int s4, s6;
  time_t start, now;
  struct timeval tv;
  fd_set readfds;
  int maxfd;

  now = start = monotime();
  s4 = init_mdns_socket_ipv4(ifname);
  s6 = init_mdns_socket_ipv6(ifname, ifindex);
  maxfd = ((s4 > s6) ? s4 : s6) + 1;

  do {
    FD_ZERO(&readfds);
    FD_SET(s4, &readfds);
    FD_SET(s6, &readfds);
    memset(&tv, 0, sizeof(tv));

    tv.tv_sec = (seconds - (now - start)) + 1;
    if (select(maxfd, &readfds, NULL, NULL, &tv) < 0) {
      perror("ERR: select");
      return;
    }

    if (FD_ISSET(s4, &readfds)) {
      process_mdns(s4);
    }
    if (FD_ISSET(s6, &readfds)) {
      process_mdns(s6);
    }

    now = monotime();
  } while ((now - start) < seconds);
}


void usage(char *progname)
{
  fprintf(stderr, "usage: %s [-i ifname] [-t seconds]\n", progname);
  fprintf(stderr, "\t-i ifname - interface to use (default: br0)\n");
  fprintf(stderr, "\t-t seconds - number of seconds to run before exiting.\n");
  exit(1);
}


int main(int argc, char *argv[])
{
  int opt;
  const char *ifname = "br0";
  time_t seconds = 30 * 60;
  int ifindex;
  L2Map l2map;

  setlinebuf(stdout);

  while ((opt = getopt(argc, argv, "i:t:")) != -1) {
    switch (opt) {
      case 'i':
        ifname = optarg;
        break;
      case 't':
        seconds = atoi(optarg);
        if (seconds < 0) usage(argv[0]);
        break;
      default:
        usage(argv[0]);
        break;
    }
  }

  alarm(seconds * 2);

  if ((ifindex = get_ifindex(ifname)) < 0) {
    fprintf(stderr, "ERR: get_ifindex(%s).\n", ifname);
    exit(1);
  }

  /* block for an extended period, listening for DNS-SD */
  listen_for_mdns(ifname, ifindex, seconds);

  for (HostsMapType::const_iterator ii = hosts.begin();
      ii != hosts.end(); ++ii) {
    std::string macaddr = ii->first;
    std::string model = ii->second;

    if (model.size() > 0) {
      const struct model_strings *l;
      std::string genus, species;

      genus = species = model;
      if ((l = model_lookup(model.c_str(), model.length())) != NULL) {
        genus = l->genus;
        species = l->species;
      }
      std::cout << "dnssd " << macaddr << " "
        << genus << ";" << species << std::endl;
    }
  }

  exit(0);
}
