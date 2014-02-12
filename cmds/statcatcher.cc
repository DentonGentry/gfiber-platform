#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "device_stats.pb.h"

std::string multicast_addr = "FF30::8000:1";
const char *optstring = "i:";
std::string interface = "wan0";

void usage() {
  printf("Usage: statcatcher -i <interface>\n");
  exit(0);
}

int GetIfIndex(int sock, std::string& port_name) {
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  snprintf(reinterpret_cast<char*>(ifr.ifr_name),
           sizeof(ifr.ifr_name), "%s", port_name.c_str());
  if (ioctl(sock, SIOCGIFINDEX, &ifr) != 0) {
    perror("Failed to get ifindex for ethernet port.");
    exit(1);
  }
  return ifr.ifr_ifindex;
}

int MakeSocket() {
  int sock = socket(PF_INET6, SOCK_DGRAM, 0);
  if (sock == -1) {
    perror("can't open socket");
    exit(1);
  }

  struct sockaddr_in6 in6;
  memset(&in6, 0, sizeof(in6));
  in6.sin6_family = PF_INET6;
  in6.sin6_port = htons(61453);
  in6.sin6_addr = in6addr_any;
  if (bind(sock, (struct sockaddr *)&in6, sizeof(in6)) < 0) {
    perror("bind failed");
    exit(1);
  }

  // Join group
  struct ipv6_mreq mc_req;
  memset(&mc_req, 0, sizeof(mc_req));
  if (inet_pton(AF_INET6,
                multicast_addr.c_str(), &mc_req.ipv6mr_multiaddr) != 1) {
    printf("multicast_addr='%s'\n", multicast_addr.c_str());
    perror("Could not convert multicast_addr");
    exit(1);
  }
  mc_req.ipv6mr_interface = GetIfIndex(sock, interface);
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                 (char*)&mc_req, sizeof(mc_req)) != 0) {
    perror("unable to join ipv6 group");
    exit(1);
  }
  return sock;
}

int main(int argc, char** argv) {
  std::vector<uint8_t> pkt;
  int opt;

  while ((opt = getopt(argc, argv, optstring)) != -1) {
    switch (opt) {
      case 'i':
        interface = std::string(optarg);
        break;

      default:
        usage();
        break;
    }
  }

  int sock = MakeSocket();
  pkt.resize(2048);
  int recvsize=0;
  for (;;) {
    pkt.resize(2048);
    recvsize = recv(sock, &pkt[0], 2048, 0);
    if (recvsize < 0) {
      perror("Failed to receive data on socket.\n");
      exit(1);
    }
    pkt.resize(recvsize);
    // TODO(jnewlin): Remove this after adding code to process received msgs.
    printf("Recieved %d bytes\n", recvsize);
    sleep(1);
  }
  return 0;
}
