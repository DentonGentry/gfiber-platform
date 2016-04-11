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
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_packet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


#define DHCP_SERVER_PORT  67
#define DHCP_CLIENT_PORT  68

struct dhcp_message {
  /* DHCP packet */
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
  uint8_t end;
} __attribute__ ((__packed__));


struct dhcp_packet {
  struct ether_header eth;
  struct ip ip;
  struct udphdr udp;
  struct dhcp_message dhcp;
} __attribute__ ((__packed__));


struct udp_checksum_helper {
  uint32_t ip_src;
  uint32_t ip_dst;
  uint8_t rsvd;
  uint8_t ip_p;
  uint16_t udp_len;
  struct udphdr udp;
  struct dhcp_message dhcp;
} __attribute__ ((__packed__));


void bind_socket_to_device(int s, const char *ifname)
{
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

  if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE,
                 (void *)&ifr, sizeof(ifr)) < 0) {
    perror("SO_BINDTODEVICE");
    exit(1);
  }
}


int create_udp_socket(const char *ifname)
{
  int s;
  struct sockaddr_in sin;
  int enable = 1;
  int ipttl = 2;
  struct timeval tv;

  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket(SOCK_DGRAM)");
    exit(1);
  }

  bind_socket_to_device(s, ifname);

  if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable))) {
    perror("SO_BROADCAST");
    exit(1);
  }

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) {
    perror("SO_REUSEADDR");
    exit(1);
  }

  tv.tv_sec = 15;
  tv.tv_usec = 0;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
    perror("SO_RCVTIMEO");
    exit(1);
  }

  if (setsockopt(s, IPPROTO_IP, IP_TTL, &ipttl, sizeof(ipttl))) {
    perror("IP_TTL");
    exit(1);
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr=htonl(INADDR_ANY);
  sin.sin_port = htons(DHCP_CLIENT_PORT);

  if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("bind");
    exit(1);
  }

  return s;
}


int create_raw_socket(const char *ifname)
{
  int s;
  int enable = 1;

  if ((s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
    perror("socket(PF_PACKET)");
    exit(1);
  }

  bind_socket_to_device(s, ifname);

  if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable))) {
    perror("SO_BROADCAST");
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

uint16_t ipsum(const uint8_t *data, int len)
{
  uint32_t sum = 0;
  const uint16_t *p = (const uint16_t *)data;

  while (len > 1) {
    sum += *p++;
    len -= 2;
  }

  if (len) {
    const uint8_t *p8 = (const uint8_t *)p;
    sum += (uint16_t) *p8;
  }

  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  return ~sum;
}


int getifindex(int s, const char *ifname)
{
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

  if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
    char errbuf[128];
    snprintf(errbuf, sizeof(errbuf), "SIOCGIFINDEX %s", ifname);
    perror(errbuf);
    exit(1);
  }

  return ifr.ifr_ifindex;
}


void insert_udp_checksum(struct dhcp_packet *pkt)
{
  struct udp_checksum_helper csum_helper;

  memset(&csum_helper, 0, sizeof(csum_helper));
  memcpy(&csum_helper.udp, &pkt->udp, sizeof(csum_helper.udp));
  memcpy(&csum_helper.dhcp, &pkt->dhcp, sizeof(csum_helper.dhcp));
  csum_helper.ip_src = pkt->ip.ip_src.s_addr;
  csum_helper.ip_dst = pkt->ip.ip_dst.s_addr;
  csum_helper.ip_p = pkt->ip.ip_p;
  csum_helper.udp_len = pkt->udp.len;
  pkt->udp.check = ipsum((const uint8_t *)&csum_helper, sizeof(csum_helper));
}


void send_dhcp_discover(int udp_sock, const char *ifname)
{
  int s = create_raw_socket(ifname);
  struct dhcp_packet pkt;
  struct sockaddr_ll sll;
  socklen_t slen = sizeof(sll);
  struct sockaddr_in sin;

  memset(&pkt, 0, sizeof(pkt));
  memset(&pkt.eth.ether_dhost, 0xff, sizeof(pkt.eth.ether_dhost));
  get_chaddr(pkt.eth.ether_shost, s, ifname);
  pkt.eth.ether_type = htons(ETH_P_IP);

  pkt.ip.ip_v = 4;
  pkt.ip.ip_hl = 5;
  pkt.ip.ip_ttl = 2;
  pkt.ip.ip_p = 17;
  inet_pton(AF_INET, "0.0.0.0", &pkt.ip.ip_src);
  inet_pton(AF_INET, "255.255.255.255", &pkt.ip.ip_dst);
  pkt.ip.ip_len = htons(sizeof(pkt.ip) + sizeof(pkt.udp) + sizeof(pkt.dhcp));
  pkt.ip.ip_sum = ipsum((const uint8_t *)&pkt.ip, sizeof(pkt.ip));

  pkt.udp.source = htons(DHCP_CLIENT_PORT);
  pkt.udp.dest = htons(DHCP_SERVER_PORT);
  pkt.udp.len = htons(sizeof(pkt.udp) + sizeof(pkt.dhcp));
  pkt.udp.check = htons(0);

  pkt.dhcp.op = OP_BOOTREQUEST;
  pkt.dhcp.htype = HTYPE_ETHERNET;
  pkt.dhcp.hlen = 6;
  pkt.dhcp.xid = htonl(time(NULL));
  pkt.dhcp.secs = htons(1);
  pkt.dhcp.flags = htons(FLAGS_BROADCAST);
  get_chaddr(pkt.dhcp.chaddr, s, ifname);
  snprintf(pkt.dhcp.sname, sizeof(pkt.dhcp.sname), "%s",
      "rogue_dhcp_server_detection");
  pkt.dhcp.magic[0] = 99;  /* DHCP magic number, RFC 2133 */
  pkt.dhcp.magic[1] = 130;
  pkt.dhcp.magic[2] = 83;
  pkt.dhcp.magic[3] = 99;
  pkt.dhcp.type[0] = 53;  /* option 53, DHCP type. */
  pkt.dhcp.type[1] = 1;  /* length = 1 */
  pkt.dhcp.type[2] = 1;  /* DHCPDISCOVER */
  pkt.dhcp.end = 0xff;  /* End option */

  insert_udp_checksum(&pkt);

  memset(&sll, 0, sizeof(sll));
  sll.sll_family = AF_PACKET;
  memset(&sll.sll_addr, 0xff, ETH_ALEN);
  sll.sll_halen = ETH_ALEN;
  sll.sll_ifindex = getifindex(s, ifname);
  sll.sll_pkttype = PACKET_BROADCAST;

  if (sendto(s, &pkt, sizeof(pkt), 0,
             (const struct sockaddr *)&sll, slen) < 0) {
    perror("sendto");
    exit(1);
  }

  close(s);

  /*
   * We send two DHCP requests. The PF_PACKET socket above
   * sends a packet with a soruce IP address of 0.0.0.0, and
   * sends it straight to the Ethernet link such that the
   * local dnsmasq does not see it.
   *
   * We send another one here using a PF_INET socket, which
   * will have a source IP address of this node, and which will
   * also be copied to the local dnsmasq.
   */

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  sin.sin_port = htons(DHCP_SERVER_PORT);
  slen = sizeof(sin);

  if (sendto(udp_sock, &pkt.dhcp, sizeof(pkt.dhcp), 0,
             (const struct sockaddr *)&sin, slen) < 0) {
    perror("sendto");
    exit(1);
  }
}


static int cmp_in_addr_p(const void *p1, const void *p2)
{
  const struct in_addr *i1 = (const struct in_addr *)p1;
  const struct in_addr *i2 = (const struct in_addr *)p2;

  if (i1->s_addr == i2->s_addr) {
    return 0;
  } else if (i1->s_addr < i2->s_addr) {
    return -1;
  } else {
    return 1;
  }
}


void receive_dhcp_offers(int s)
{
  struct sockaddr_in sin;
  socklen_t slen = sizeof(sin);
  uint8_t pktbuf[2048];
  #define MAX_RESPONSES 4
  struct in_addr responses[MAX_RESPONSES];
  int nresponses = 0;

  memset(&sin, 0, sizeof(sin));
  memset(responses, 0, sizeof(responses));

  while (recvfrom(s, pktbuf, sizeof(pktbuf), 0, &sin, &slen) > 0) {
    int duplicate = 0;
    int i;

    if (nresponses >= MAX_RESPONSES) {
      break;
    }

    for (i = 0; i < MAX_RESPONSES; ++i) {
      if (responses[i].s_addr == sin.sin_addr.s_addr) {
        duplicate = 1;
        break;
      }
    }

    if (!duplicate) {
      responses[nresponses].s_addr = sin.sin_addr.s_addr;
      nresponses++;
    }
  }

  if (nresponses == 0) {
    printf("Received 0 DHCP responses.\n");
  } else {
    char outbuf[(MAX_RESPONSES * (INET_ADDRSTRLEN + 1)) + 1];
    int i;

    qsort(responses, nresponses, sizeof(responses[0]), cmp_in_addr_p);

    outbuf[0] = '\0';
    for (i = 0; i < nresponses; ++i) {
      int len = strlen(outbuf);
      int lim = sizeof(outbuf) - len;

      if (i > 0) {
        strcat(outbuf, ",");
        len = strlen(outbuf);
        lim = sizeof(outbuf) - len;
      }

      inet_ntop(AF_INET, &responses[i], outbuf + len, lim);
    }

    /*
     * Yes, this will print "Received 1 DHCP responses". It
     * complicates any matching code to make the 's' optional,
     * for no benefit. OCD will have to find a way to cope.
     */
    printf("Received %d DHCP responses from: %s\n", nresponses, outbuf);
  }
}

void usage(const char *progname)
{
  fprintf(stderr, "usage: %s [-i br0]\n", progname);
  fprintf(stderr, "\t-i: name of the interface to probe for DHCP servers.\n");
  exit(1);
}

int main(int argc, char **argv)
{
  const char *interface = "br0";
  struct option long_options[] = {
    {"interface", required_argument, 0, 'i'},
    {0,           0,                 0, 0},
  };
  int s, c;

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

  setlinebuf(stdout);
  s = create_udp_socket(interface);
  send_dhcp_discover(s, interface);
  receive_dhcp_offers(s);
}
