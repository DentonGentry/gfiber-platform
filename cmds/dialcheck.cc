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
 * dialcheck
 *
 * Check for nearby devices supporting the DIAL protocol.
 */

#include <arpa/inet.h>
#include <asm/types.h>
#include <ctype.h>
#include <getopt.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>
#include <set>
#include <tr1/unordered_map>


typedef std::set<std::string> ResultsSet;
int ssdp_timeout_secs = 10;
int alarm_timeout_secs = 15;


/* SSDP Discover packet */
int ssdp_port = 1900;
int ssdp_loop = 0;
#define SSDP_IP4 "239.255.255.250"
#define SSDP_IP6 "FF02::C"
const char discover_template[] = "M-SEARCH * HTTP/1.1\r\n"
    "HOST: %s:%d\r\n"
    "MAN: \"ssdp:discover\"\r\n"
    "MX: 2\r\n"
    "USER-AGENT: dialcheck/1.0\r\n"
    "ST: urn:dial-multiscreen-org:service:dial:1\r\n\r\n";


int get_ipv4_ssdp_socket()
{
  int s;
  int reuse = 1;
  struct sockaddr_in sin;
  struct ip_mreq mreq;
  struct ip_mreqn mreqn;

  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket SOCK_DGRAM");
    exit(1);
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse))) {
    perror("setsockopt SO_REUSEADDR");
    exit(1);
  }

  if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP,
        &ssdp_loop, sizeof(ssdp_loop))) {
    perror("setsockopt IP_MULTICAST_LOOP");
    exit(1);
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(ssdp_port);
  sin.sin_addr.s_addr = INADDR_ANY;
  if (bind(s, (struct sockaddr*)&sin, sizeof(sin))) {
    perror("bind");
    exit(1);
  }

  memset(&mreqn, 0, sizeof(mreqn));
  mreqn.imr_ifindex = if_nametoindex("br0");
  if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof(mreqn))) {
    perror("IP_MULTICAST_IF");
    exit(1);
  }

  memset(&mreq, 0, sizeof(mreq));
  mreq.imr_multiaddr.s_addr = inet_addr(SSDP_IP4);
  if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
        (char *)&mreq, sizeof(mreq))) {
    perror("IP_ADD_MEMBERSHIP");
    exit(1);
  }

  return s;
}


void send_ssdp_ip4_request(int s)
{
  struct sockaddr_in sin;
  char buf[1024];
  ssize_t len;

  snprintf(buf, sizeof(buf), discover_template, SSDP_IP4, ssdp_port);
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(ssdp_port);
  sin.sin_addr.s_addr = inet_addr(SSDP_IP4);
  len = strlen(buf);
  if (sendto(s, buf, len, 0, (struct sockaddr*)&sin, sizeof(sin)) != len) {
    perror("sendto multicast IPv4");
    exit(1);
  }
}


int get_ipv6_ssdp_socket()
{
  int s;
  int reuse = 1;
  int loop = 0;
  struct sockaddr_in6 sin6;
  struct ipv6_mreq mreq;
  int idx;
  int hops;

  if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
    perror("socket SOCK_DGRAM");
    exit(1);
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse))) {
    perror("setsockopt SO_REUSEADDR");
    exit(1);
  }

  if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop))) {
    perror("setsockopt IPV6_MULTICAST_LOOP");
    exit(1);
  }

  memset(&sin6, 0, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_port = htons(ssdp_port);
  sin6.sin6_addr = in6addr_any;
  if (bind(s, (struct sockaddr*)&sin6, sizeof(sin6))) {
    perror("bind");
    exit(1);
  }

  idx = if_nametoindex("br0");
  if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, &idx, sizeof(idx))) {
    perror("IP_MULTICAST_IF");
    exit(1);
  }

  hops = 2;
  if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops))) {
    perror("IPV6_MULTICAST_HOPS");
    exit(1);
  }

  memset(&mreq, 0, sizeof(mreq));
  mreq.ipv6mr_interface = idx;
  if (inet_pton(AF_INET6, SSDP_IP6, &mreq.ipv6mr_multiaddr) != 1) {
    fprintf(stderr, "ERR: inet_pton(%s) failed", SSDP_IP6);
    exit(1);
  }
  if (setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
    perror("ERR: setsockopt(IPV6_JOIN_GROUP)");
    exit(1);
  }

  return s;
}


void send_ssdp_ip6_request(int s)
{
  struct sockaddr_in6 sin6;
  char buf[1024];
  ssize_t len;

  snprintf(buf, sizeof(buf), discover_template, SSDP_IP6, ssdp_port);
  memset(&sin6, 0, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_port = htons(ssdp_port);
  if (inet_pton(AF_INET6, SSDP_IP6, &sin6.sin6_addr) != 1) {
    fprintf(stderr, "ERR: inet_pton(%s) failed", SSDP_IP6);
    exit(1);
  }
  len = strlen(buf);
  if (sendto(s, buf, len, 0, (struct sockaddr*)&sin6, sizeof(sin6)) != len) {
    perror("sendto multicast IPv6");
    exit(1);
  }
}


std::string handle_ssdp_response(int s, int family)
{
  char buffer[4096];
  char ipbuf[INET6_ADDRSTRLEN];
  ssize_t pktlen;
  struct sockaddr from;
  socklen_t len = sizeof(from);

  pktlen = recvfrom(s, buffer, sizeof(buffer), 0, &from, &len);
  if (pktlen <= 0) {
    return std::string("");
  }

  if (family == AF_INET) {
    struct sockaddr_in *sin = (struct sockaddr_in *)&from;
    inet_ntop(AF_INET, &sin->sin_addr, ipbuf, sizeof(ipbuf));
    return std::string(ipbuf);
  } else if (family == AF_INET6) {
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&from;
    inet_ntop(AF_INET6, &sin6->sin6_addr, ipbuf, sizeof(ipbuf));
    return std::string(ipbuf);
  }

  return std::string("");
}


/* Wait for SSDP NOTIFY messages to arrive. */
ResultsSet listen_for_responses(int s4, int s6)
{
  ResultsSet results;
  struct timeval tv;
  fd_set rfds;
  int maxfd = (s4 > s6) ? s4 : s6;

  memset(&tv, 0, sizeof(tv));
  tv.tv_sec = ssdp_timeout_secs;
  tv.tv_usec = 0;

  FD_ZERO(&rfds);
  FD_SET(s4, &rfds);
  FD_SET(s6, &rfds);

  while (select(maxfd + 1, &rfds, NULL, NULL, &tv) > 0) {
    if (FD_ISSET(s4, &rfds)) {
      std::string ip = handle_ssdp_response(s4, AF_INET);
      if (!ip.empty()) {
        results.insert(ip);
      }
    }
    if (FD_ISSET(s6, &rfds)) {
      std::string ip = handle_ssdp_response(s6, AF_INET6);
      if (!ip.empty()) {
        results.insert(ip);
      }
    }

    FD_ZERO(&rfds);
    FD_SET(s4, &rfds);
    FD_SET(s6, &rfds);
  }

  return results;
}


void usage(char *progname) {
  fprintf(stderr, "usage: %s [-t port]\nwhere:\n", progname);
  fprintf(stderr, "\t-t port:  test mode, send to localhost port\n");
  exit(1);
}


int main(int argc, char **argv)
{
  int c;
  int s4, s6;

  setlinebuf(stdout);
  alarm(alarm_timeout_secs);

  while ((c = getopt(argc, argv, "t:")) != -1) {
    switch(c) {
      case 't':
        ssdp_timeout_secs = 1;
        ssdp_port = atoi(optarg);
        ssdp_loop = 1;
        break;
      default: usage(argv[0]); break;
    }
  }

  s4 = get_ipv4_ssdp_socket();
  send_ssdp_ip4_request(s4);
  s6 = get_ipv6_ssdp_socket();
  send_ssdp_ip6_request(s6);
  ResultsSet IPs = listen_for_responses(s4, s6);

  std::string output("DIAL responses from: ");
  for (ResultsSet::const_iterator ii = IPs.begin(); ii != IPs.end(); ++ii) {
    output.append(*ii);
    output.append(" ");
  }
  std::cout << output << std::endl;

  exit(0);
}
