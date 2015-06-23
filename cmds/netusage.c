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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>


// number of samples to print on each log line.
// if you change this there is a printf below which must be adjusted.
#define SAMPLES 8


static uint64_t mono_usecs(void)
{
  struct timespec ts;
  uint64_t usec;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    perror("clock_gettime(CLOCK_MONOTONIC)");
    exit(1);
  }
  usec = ts.tv_sec * 1000000ULL;
  usec += ts.tv_nsec / 1000ULL;
  return usec;
}


int netlink_socket()
{
  int s;
  struct sockaddr_nl snl;

  if ((s = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
    perror("socket AF_NETLINK failed");
    exit(1);
  }

  memset(&snl, 0, sizeof(snl));
  snl.nl_family = AF_NETLINK;
  snl.nl_pid = getpid();
  if (bind(s, (struct sockaddr *) &snl, sizeof(snl))) {
    perror("bind AF_NETLINK failed");
    exit(1);
  }

  return s;
}


void sendreq(int s, const char *ifname)
{
  struct {
    struct nlmsghdr nh;
    struct ifinfomsg ifi;
    char attr[RTA_SPACE(IFNAMSIZ)];
  } req;
  struct sockaddr_nl snl;
  struct rtattr *rta;

  if (strlen(ifname) > IFNAMSIZ) {
    fprintf(stderr, "interface name is too long.\n");
    exit(1);
  }

  memset(&req, 0, sizeof(req));
  req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(req.ifi));
  req.nh.nlmsg_type = RTM_GETLINK;
  req.nh.nlmsg_flags = NLM_F_REQUEST;
  req.nh.nlmsg_seq = 1;
  req.ifi.ifi_family = AF_PACKET;

  rta = (struct rtattr *)&req.attr;
  rta->rta_type = IFLA_IFNAME;
  rta->rta_len = RTA_LENGTH(strlen(ifname));
  snprintf(RTA_DATA(rta), IFNAMSIZ, "%s", ifname);

  req.nh.nlmsg_len = NLMSG_ALIGN(req.nh.nlmsg_len) + RTA_ALIGN(rta->rta_len);

  memset(&snl, 0, sizeof(snl));
  snl.nl_family = AF_NETLINK;
  if (sendto(s, &req, req.nh.nlmsg_len, 0,
             (struct sockaddr *)&snl, sizeof(snl)) < 0) {
    perror("sendto AF_NETLINK failed");
    exit(1);
  }
}


void recvresp(int s, uint32_t *tx_bytes, uint32_t *rx_bytes,
    uint32_t *tx_pkts, uint32_t *rx_unipkts, uint32_t *rx_multipkts)
{
  ssize_t len;
  unsigned char buf[4096];
  struct nlmsghdr *nh = (struct nlmsghdr *)buf;
  struct ifinfomsg *ifmsg;
  struct rtattr *attr;
  int attrlen;

  len = recv(s, buf, sizeof(buf), 0);
  while (NLMSG_OK(nh, len)) {
    struct rtnl_link_stats *stats;

    if (nh->nlmsg_type == NLMSG_ERROR) {
      fprintf(stderr, "NLMSG_ERROR\n");
      exit(1);
    }

    ifmsg = NLMSG_DATA(nh);
    attr = IFLA_RTA(ifmsg);
    attrlen = NLMSG_PAYLOAD(nh, sizeof(struct ifinfomsg));
    while (RTA_OK(attr, attrlen)) {
      if (attr->rta_type == IFLA_STATS) {
        stats = (struct rtnl_link_stats *)RTA_DATA(attr);
        *rx_bytes = stats->rx_bytes;
        *tx_bytes = stats->tx_bytes;
        *tx_pkts = stats->tx_packets;
        *rx_unipkts = stats->rx_packets - stats->multicast;
        *rx_multipkts = stats->multicast;
      }

      attr = RTA_NEXT(attr, attrlen);
    }

    nh = NLMSG_NEXT(nh, len);
  }
}


void usage(const char *progname)
{
  fprintf(stderr, "usage: %s -i foo0\n", progname);
  fprintf(stderr, "\t-i foo0: network interface to monitor.\n");

  exit(1);
}


int main(int argc, char **argv)
{
  int c;
  const char *interface = NULL;
  int s = netlink_socket();
  uint64_t start;
  uint32_t old_tx_bytes, old_rx_bytes, old_tx_pkts;
  uint32_t old_rx_unipkts, old_rx_multipkts;
  int i = 0;

  while ((c = getopt(argc, argv, "i:")) >= 0) {
    switch (c) {
      case 'i':
        interface = optarg;
        break;
      default:
      case '?':
        usage(argv[0]);
        break;
    }
  }

  if (!interface) {
    usage(argv[0]);
  }

  setlinebuf(stdout);
  start = mono_usecs();
  sendreq(s, interface);
  recvresp(s, &old_tx_bytes, &old_rx_bytes, &old_tx_pkts, &old_rx_unipkts,
      &old_rx_multipkts);

  while (1) {
    uint64_t timestamp;
    double delta;
    uint32_t tx_bytes, rx_bytes, tx_pkts, rx_unipkts, rx_multipkts;
    double tx_kbps[SAMPLES];
    double rx_kbps[SAMPLES];
    double tx_pps[SAMPLES];
    double rx_uni_pps[SAMPLES];
    double rx_multi_pps[SAMPLES];

    sleep(1);
    timestamp = mono_usecs();
    sendreq(s, interface);
    recvresp(s, &tx_bytes, &rx_bytes, &tx_pkts, &rx_unipkts, &rx_multipkts);

    delta = (timestamp - start) / 1000000.0;
    tx_kbps[i] = (8.0 * (tx_bytes - old_tx_bytes) / 1000.0) / delta;
    rx_kbps[i] = (8.0 * (rx_bytes - old_rx_bytes) / 1000.0) / delta;
    tx_pps[i] = (tx_pkts - old_tx_pkts) / delta;
    rx_uni_pps[i] = (rx_unipkts - old_rx_unipkts) / delta;
    rx_multi_pps[i] = (rx_multipkts - old_rx_multipkts) / delta;
    i++;

    if (i == SAMPLES) {
      printf("%s TX Kbps %.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
          interface,
          tx_kbps[0], tx_kbps[1], tx_kbps[2], tx_kbps[3],
          tx_kbps[4], tx_kbps[5], tx_kbps[6], tx_kbps[7]);
      printf("%s RX Kbps %.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
          interface,
          rx_kbps[0], rx_kbps[1], rx_kbps[2], rx_kbps[3],
          rx_kbps[4], rx_kbps[5], rx_kbps[6], rx_kbps[7]);
      printf("%s TX pps %.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
          interface,
          tx_pps[0], tx_pps[1], tx_pps[2], tx_pps[3],
          tx_pps[4], tx_pps[5], tx_pps[6], tx_pps[7]);
      printf("%s RX unipps %.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
          interface,
          rx_uni_pps[0], rx_uni_pps[1], rx_uni_pps[2], rx_uni_pps[3],
          rx_uni_pps[4], rx_uni_pps[5], rx_uni_pps[6], rx_uni_pps[7]);
      printf("%s RX multipps %.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
          interface,
          rx_multi_pps[0], rx_multi_pps[1], rx_multi_pps[2], rx_multi_pps[3],
          rx_multi_pps[4], rx_multi_pps[5], rx_multi_pps[6], rx_multi_pps[7]);
      i = 0;
    }

    old_tx_bytes = tx_bytes;
    old_rx_bytes = rx_bytes;
    old_tx_pkts = tx_pkts;
    old_rx_unipkts = rx_unipkts;
    old_rx_multipkts = rx_multipkts;
    start = timestamp;
  }
}
