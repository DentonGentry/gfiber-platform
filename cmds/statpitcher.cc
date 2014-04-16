#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "device_stats.pb.h"

#define ETH_PORT "eth0"
#define GOOG_PROTOCOL 0x8930
#define STAT_INTERVAL 60

std::string multicast_addr = "FF30::8000:1";

uint8_t mc_mac[] = { 0x01, 0x00, 0x5e, 0x00, 0x00, 0x01 };
const char *optstring = "s:i:a:";

std::string serial_number;
std::string wan_interface;
std::string acs_contact_file;

void usage() {
  printf("Usage: statpitcher -s <serial number> -i <wan interface> "
         "-a <acs_contact_file>\n");
  exit(1);
}

void ReadFile(const std::string& fname, std::string* data) {
  data->clear();

  std::ifstream s(fname);
  if (!s.is_open())
    return;
  data->assign(std::istreambuf_iterator<char>(s),
               std::istreambuf_iterator<char>());
}

int GetIfIndex(int sock) {
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  snprintf(reinterpret_cast<char*>(ifr.ifr_name),
           sizeof(ifr.ifr_name), ETH_PORT);
  if (ioctl(sock, SIOCGIFINDEX, &ifr) != 0) {
    perror("Failed to get ifindex for ethernet port.");
    exit(1);
  }
  return ifr.ifr_ifindex;
}

bool WanUp() {
  std::string if_status;
  std::string stat_file = "/sys/class/net/" + wan_interface + "/operstate";
  ReadFile(stat_file, &if_status);
  if (if_status.compare(0, 2, "up") == 0)  // compare first 2 characters.
    return true;
  return false;
}

// Returns 0 if not contacted.
// Otherwise the contact time in seconds since 1970.
int64_t AcsContacted() {
  struct stat stat_buf;
  memset(&stat_buf, 0, sizeof(stat_buf));
  if (stat(acs_contact_file.c_str(), &stat_buf) < 0)
    return 0;
  return stat_buf.st_mtime;
}

int64_t Uptime() {
  std::string data;
  float up, idle;
  ReadFile("/proc/uptime", &data);
  if (sscanf(data.c_str(), "%f %f", &up, &idle) != 2) {
    return 0;
  }
  return static_cast<int64_t>(up);
}

void MakePacket(std::vector<uint8_t>* pkt) {
  devstatus::Status status;

  int64_t acs_contact_time = AcsContacted();

  status.set_wan_connected(WanUp());
  status.set_acs_contacted(acs_contact_time != 0);
  status.set_acs_contact_time(acs_contact_time);
  status.set_uptime(Uptime());
  status.set_serial(serial_number);

  pkt->resize(status.ByteSize());
  status.SerializeToArray(&(*pkt)[0], status.ByteSize());
}

int MakeSocket() {
  int sock = socket(PF_INET6, SOCK_DGRAM, 0);
  if (sock == -1) {
    perror("can't open socket");
    exit(1);
  }
  int if_index = GetIfIndex(sock);
  if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                 reinterpret_cast<char*>(&if_index), sizeof(if_index)) != 0) {
    perror("Failed to setsockopt.");
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
        wan_interface = std::string(optarg);
        break;

      case 's':
        serial_number = std::string(optarg);
        break;

      case 'a':
        acs_contact_file = std::string(optarg);
        break;

      default:
        printf("Unknown option: %d\n", opt);
        usage();
        break;
    }
  }

  if (wan_interface.empty() || serial_number.empty() ||
      acs_contact_file.empty()) {
    usage();
    return 0;
  }

  int sock = MakeSocket();
  struct sockaddr_in6 sin6;
  memset(&sin6, 0, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_port = htons(61453);
  if (inet_pton(AF_INET6, multicast_addr.c_str(), &sin6.sin6_addr) != 1) {
    perror("inet_pton failed: ");
    exit(-1);
  }

  for (;;) {
    pkt.clear();
    MakePacket(&pkt);
    int written = sendto(sock, &pkt[0], pkt.size(), 0,
                         (struct sockaddr*)&sin6, sizeof(sin6));
    if (written < 0) {
      perror("sendto: ");
      printf("pkt.size()=%d\n", (int)pkt.size());
    }
    sleep(STAT_INTERVAL+1);
  }
  return 0;
}
