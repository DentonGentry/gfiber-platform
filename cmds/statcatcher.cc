// The ONU periodically multicasts out its own status, this program
// listens for those multicasts and writes the status out to a file that
// be read in by catawampus and displayed on the diagnostic page.

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

std::string multicast_addr = "FF12::8000:1";
const char *optstring = "i:f:";
std::string interface = "wan0";
std::string stat_file;
std::string tmp_file;

void usage() {
  printf("Usage: statcatcher -i <interface> -f <stat file>\n");
  exit(1);
}

int GetIfIndex(int sock, const std::string& port_name) {
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
                 reinterpret_cast<char*>(&mc_req), sizeof(mc_req)) != 0) {
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

      case 'f':
        stat_file = std::string(optarg);
        tmp_file = stat_file + ".tmp";
        break;

      default:
        usage();
        break;
    }
  }

  if (stat_file.empty() || interface.empty()) {
    usage();
    exit(1);
  }

  int sock = MakeSocket();
  pkt.resize(2048);
  int recvsize = 0;
  for (;;) {
    // process only 1 message per second to prevent a dos attack.
    sleep(1);

    pkt.resize(2048);
    recvsize = recv(sock, &pkt[0], 2048, 0);
    if (recvsize < 0) {
      perror("Failed to receive data on socket.\n");
      exit(1);
    } else {
      fprintf(stderr, "received %d bytes\n", recvsize);
    }
    pkt.resize(recvsize);

    // Deserialize message.
    devstatus::Status status;
    if (!status.ParseFromArray(&pkt[0], pkt.size())) {
      printf("failed to parse received data.");
      continue;
    }

    // This is C++0x raw string formatting.
    // NOTE(jnewlin): There are some spiffy automatic proto to json
    // converters, if we add more data we might want to get rid of this
    // simplistic converter and use something like that.  Either that or just
    // write the proto and make catawampus read the proto, there was
    // hesitation to adding proto support to cwmp.
    std::string json_out = R"({
"onu_wan_connected": %s,
"onu_acs_contacted": %s,
"onu_acs_contact_time": "%lld",
"onu_uptime": %lld,
"onu_serial": "%s",
"onu_ipv6": "%s"
})";
    FILE *f = fopen(tmp_file.c_str(), "w");
    if (!f) {
      printf("Can't open tmp file for writing.");
      exit(1);
    }

    fprintf(f, json_out.c_str(),
            status.wan_connected() ? "true" : "false",
            status.acs_contacted() ? "true" : "false",
            status.acs_contact_time(),
            status.uptime(),
            status.serial().c_str(),
            status.ipv6().c_str());
    fclose(f);

    if (rename(tmp_file.c_str(), stat_file.c_str()) != 0) {
      perror("rename tmp file failed.");
    }
  }
  return 0;
}
