/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <linux/types.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>
#include "../common/util.h"
#include "../common/io.h"
#include "common.h"

#define WAN_PORT_NAME "eth1_1"
#define ETH_TEST_LINE_MAX 4096
#define TX_BYTES_PATH "cat /sys/class/net/" WAN_PORT_NAME "/statistics/tx_bytes"
#define RX_BYTES_APTH "cat /sys/class/net/" WAN_PORT_NAME "/statistics/rx_bytes"
#define ETH_TRAFFIC_TEST_PERIOD_SYMBOL "-p"
#define ETH_TRAFFIC_MAX_REPORT_PERIOD 50
#define ETH_TRAFFIC_MAX_GE_REPORT_PERIOD 15
#define ETH_TRAFFIC_REPORT_PERIOD 50
#define ETH_PKTS_LEN_DEFAULT 32
#define ETH_STAT_WAIT_PERIOD 1  // sec
#define BUF_SIZ 1536
#define ETH_PKTS_SENT_BEFORE_WAIT 0xFF
#define SCAN_CMD_FORMAT "%256s"
// 1G
#define ETH_TRAFFIC_PER_PERIOD_MAX \
  (((unsigned int)ETH_TRAFFIC_MAX_REPORT_PERIOD) * ((unsigned int)131072000))
#define ONE_MEG (1024 * 1024)
#define ETH_STAT_PERCENT_MARGIN 95

void send_mac_pkt(char *if_name, char *out_name, unsigned int xfer_len,
                  unsigned int xfer_wait, int n,
                  const unsigned char *dst_mac1) {
  int sockfd, i;
  struct ifreq if_idx;
  struct ifreq if_mac, out_mac;
  int tx_len = 0;
  char sendbuf[BUF_SIZ];
  struct ether_header *eh = (struct ether_header *)sendbuf;
  struct sockaddr_ll socket_address;
  unsigned char dst_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  /* Open RAW socket to send on */
  if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
    perror("socket");
  }

  /* Get the index of the interface to send on */
  memset(&if_idx, 0, sizeof(if_idx));
  safe_strncpy(if_idx.ifr_name, if_name, IFNAMSIZ - 1);
  if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
    perror("SIOCGIFINDEX");
  }
  /* Get the MAC address of the interface to send on */
  memset(&out_mac, 0, sizeof(out_mac));
  if (out_name != NULL) {
    safe_strncpy(out_mac.ifr_name, out_name, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFHWADDR, &out_mac) < 0) {
      perror("out SIOCGIFHWADDR");
    }
  }
  memset(&if_mac, 0, sizeof(if_mac));
  safe_strncpy(if_mac.ifr_name, if_name, IFNAMSIZ - 1);
  if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
    perror("SIOCGIFHWADDR");
  }
  if (out_name != NULL) {
    dst_mac[0] = ((uint8_t *)&out_mac.ifr_hwaddr.sa_data)[0];
    dst_mac[1] = ((uint8_t *)&out_mac.ifr_hwaddr.sa_data)[1];
    dst_mac[2] = ((uint8_t *)&out_mac.ifr_hwaddr.sa_data)[2];
    dst_mac[3] = ((uint8_t *)&out_mac.ifr_hwaddr.sa_data)[3];
    dst_mac[4] = ((uint8_t *)&out_mac.ifr_hwaddr.sa_data)[4];
    dst_mac[5] = ((uint8_t *)&out_mac.ifr_hwaddr.sa_data)[5];
  } else if (dst_mac1 != NULL) {
    dst_mac[0] = dst_mac1[0];
    dst_mac[1] = dst_mac1[1];
    dst_mac[2] = dst_mac1[2];
    dst_mac[3] = dst_mac1[3];
    dst_mac[4] = dst_mac1[4];
    dst_mac[5] = dst_mac1[5];
  } else {
    printf("Invalid out_name and dst_mac.\n");
    return;
  }

  /* Construct the Ethernet header */
  // memset(sendbuf, 0, BUF_SIZ);
  for (i = 0; i < BUF_SIZ; ++i) {
    sendbuf[i] = 0xA5;
  }
  /* Ethernet header */
  eh->ether_shost[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
  eh->ether_shost[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
  eh->ether_shost[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
  eh->ether_shost[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
  eh->ether_shost[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
  eh->ether_shost[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
  eh->ether_dhost[0] = dst_mac[0];
  eh->ether_dhost[1] = dst_mac[1];
  eh->ether_dhost[2] = dst_mac[2];
  eh->ether_dhost[3] = dst_mac[3];
  eh->ether_dhost[4] = dst_mac[4];
  eh->ether_dhost[5] = dst_mac[5];
  /* Ethertype field */
  eh->ether_type = htons(ETH_P_IP);
  tx_len += sizeof(struct ether_header);
  // printf("TX MAC %02x:%02x:%02x:%02x:%02x:%02x RX MAC %02x:%02x:%02x:%02x:%02x:%02x\n", eh->ether_shost[0],eh->ether_shost[1],eh->ether_shost[2],eh->ether_shost[3],eh->ether_shost[4],eh->ether_shost[5],eh->ether_dhost[0],eh->ether_dhost[1],eh->ether_dhost[2],eh->ether_dhost[3],eh->ether_dhost[4],eh->ether_dhost[5]);

  /* Packet data */
  sendbuf[tx_len++] = 0xde;
  sendbuf[tx_len++] = 0xad;
  sendbuf[tx_len++] = 0xbe;
  sendbuf[tx_len++] = 0xef;

  /* Index of the network device */
  socket_address.sll_ifindex = if_idx.ifr_ifindex;
  /* Address length*/
  socket_address.sll_halen = ETH_ALEN;
  /* Destination MAC */
  socket_address.sll_addr[0] = dst_mac[0];
  socket_address.sll_addr[1] = dst_mac[1];
  socket_address.sll_addr[2] = dst_mac[2];
  socket_address.sll_addr[3] = dst_mac[3];
  socket_address.sll_addr[4] = dst_mac[4];
  socket_address.sll_addr[5] = dst_mac[5];

  /* Send packet */
  if (n < 0) {
    while (1) {
      if (sendto(sockfd, sendbuf, xfer_len, 0,
                 (struct sockaddr *)&socket_address,
                 sizeof(struct sockaddr_ll)) < 0) {
        printf("Send failed at msg %d\n", i);
        break;
      }
      if (xfer_wait > 0) {
        if ((i & ETH_PKTS_SENT_BEFORE_WAIT) == 0) {
          usleep(xfer_wait);
        }
      }
    }
  } else {
    for (i = 0; i < n; ++i) {
      if (sendto(sockfd, sendbuf, xfer_len, 0,
                 (struct sockaddr *)&socket_address,
                 sizeof(struct sockaddr_ll)) < 0) {
        printf("Send failed at msg %d\n", i);
        break;
      }
      if (xfer_wait > 0) {
        if ((i & ETH_PKTS_SENT_BEFORE_WAIT) == 0) {
          usleep(xfer_wait);
        }
      }
    }
  }
  close(sockfd);
}

static void phy_read_usage(void) {
  printf("phy_read <ifname> <reg>\n");
  printf("Example:\n");
  printf("phy_read lan0 2\n");
}

int phy_read(int argc, char *argv[]) {
  int reg, val;

  if (argc != 3) {
    phy_read_usage();
    return -1;
  }

  reg = strtol(argv[2], NULL, 0);
  mdio_init();
  mdio_set_interface(argv[1]);
  val = mdio_read(reg);
  mdio_done();

  if (val < 0) {
    printf("Read PHY %s reg %d failed\n", argv[1], reg);
    return -1;
  }
  printf("PHY %s Reg %d = 0x%x\n", argv[1], reg, val);
  return 0;
}

static void phy_write_usage(void) {
  printf("phy_write <ifname> <reg> <val>\n");
  printf("Example:\n");
  printf("phy_write lan0 22 0x6\n");
}

int phy_write(int argc, char *argv[]) {
  int reg, val, rc;

  if (argc != 4) {
    phy_write_usage();
    return -1;
  }

  reg = strtol(argv[2], NULL, 0);
  val = strtol(argv[3], NULL, 16);
  mdio_init();
  mdio_set_interface(argv[1]);
  rc = mdio_write(reg, val);
  mdio_done();

  if (rc < 0) {
    printf("Write PHY %s reg %d val 0x%x failed\n", argv[1], reg, val);
    return -1;
  }
  printf("PHY %s Reg %d = 0x%x\n", argv[1], reg, val);
  return 0;
}

/* If extra is not NULL, rsp is returned as the string followed extra */
int scan_command(char *command, char *rsp, char *extra) {
  FILE *fp;
  fp = popen(command, "r");
  if (fp != NULL) {
    if (extra != NULL) {
      while (fscanf(fp, "%s", rsp) != EOF) {
        if (!strcmp(rsp, extra)) {
          if (fscanf(fp, "%s", rsp) <= 0)
            return -1;
          else
            return 0;
        }
      }
    } else {
      fscanf(fp, SCAN_CMD_FORMAT, rsp);
    }
    pclose(fp);
  } else {
    return -1;
  }
  return 0;
}

int net_stat(unsigned int *rx_bytes, unsigned int *tx_bytes) {
  static unsigned int tx_stat = 0;
  static unsigned int rx_stat = 0;
  char rsp[ETH_TEST_LINE_MAX];
  unsigned int tmp;

  for (tmp = 0; tmp < 2; ++tmp) {
    // system_cmd(RX_BYTES_APTH);
    // system_cmd(TX_BYTES_PATH);
    scan_command(RX_BYTES_APTH, rsp, NULL);
    scan_command(TX_BYTES_PATH, rsp, NULL);
    sleep(1);
  }
  if (scan_command(RX_BYTES_APTH, rsp, NULL) == 0) *rx_bytes = strtoul(rsp, NULL, 10);
  if (scan_command(TX_BYTES_PATH, rsp, NULL) == 0) *tx_bytes = strtoul(rsp, NULL, 10);
  // printf("STAT: TX %d RX %d\n", *tx_bytes, *rx_bytes);

  if (*tx_bytes >= tx_stat) {
    *tx_bytes -= tx_stat;
    tx_stat += *tx_bytes;
  } else {
    tmp = *tx_bytes;
    // tx_bytes is uint. It will continue to increment till wrap around
    // When it wraps around, the current value will be less than the
    // previous one. That is why this logic kicked in.
    *tx_bytes += (0xffffffff - tx_stat);
    tx_stat = tmp;
  }

  if (*rx_bytes >= rx_stat) {
    *rx_bytes -= rx_stat;
    rx_stat += *rx_bytes;
  } else {
    tmp = *rx_bytes;
    // rx_bytes is uint. It will continue to increment till wrap around
    // When it wraps around, the current value will be less than the
    // previous one. That is why this logic kicked in.
    *rx_bytes += (0xffffffff - rx_stat);
    rx_stat = tmp;
  }
  return 0;
}

// Return 0 if lost carrier. Otherwise, 1
int get_carrier_state(char *name) {
  char command[ETH_TEST_LINE_MAX], rsp[ETH_TEST_LINE_MAX];

  snprintf(command, sizeof(command), "cat /sys/class/net/%s/carrier", name);
  if (scan_command(command, rsp, NULL) == 0) {
    if (strcmp(rsp, "0") != 0) return 1;
  }
  return 0;
}

// This is the same as sleep but monitor the link carrier every second
// Return true if the carrier is good every second. Otherwise false
bool sleep_and_check_carrier(int duration, char *if_name) {
  bool good_carrier = true;
  int i;
  for (i = 0; i < duration; ++i) {
    if (get_carrier_state(if_name) == 0) good_carrier = false;
    sleep(1);
  }
  return good_carrier;
}

int get_if_ip(char *name, unsigned int *ip) {
  char command[ETH_TEST_LINE_MAX], rsp[ETH_TEST_LINE_MAX];
  bool found = false;

  snprintf(command, sizeof(command), "ip addr show %s", name);
  if (scan_command(command, rsp, "inet") == 0) {
    if (sscanf(rsp, "%u.%u.%u.%u", ip, (ip + 1), (ip + 2), (ip + 3)) <= 0) {
      return -1;
    }
    found = true;
  }

  if (!found) {
    return -1;
  }

  return 0;
}

static void loopback_test_usage(void) {
  printf(
      "loopback_test <duration in secs> [<%s print-period in secs>]\n",
      ETH_TRAFFIC_TEST_PERIOD_SYMBOL);
  printf("- duration >=1 or -1 (forever)\n");
  printf("- print-period >= 0 and <= %d\n", ETH_TRAFFIC_MAX_REPORT_PERIOD);
  printf("- print-period > 0 if duration > 0\n");
  printf("- print-period = 0 prints only the summary\n");
}

int loopback_test(int argc, char *argv[]) {
  int duration, num = -1, collected_count = 0;
  int pid, pid1, print_period = ETH_TRAFFIC_REPORT_PERIOD;
  unsigned int pkt_len = ETH_PKTS_LEN_DEFAULT, rx_bytes, tx_bytes;
  bool print_every_period = true, traffic_problem = false, problem = false;
  float average_throughput = 0.0, throughput;
  unsigned char dst_mac[6] = {0, 0, 0, 0, 0, 0};
  bool gig_traffic = false;

  if ((argc < 2) || (argc > 5)) {
    printf("Invalid number of parameters: %d\n", argc);
    loopback_test_usage();
    return -1;
  }

  duration = strtol(argv[1], NULL, 0);
  if ((duration < -1) || (duration == 0)) {
    printf("Invalid duration %d:%s\n", duration, argv[1]);
    loopback_test_usage();
    return -1;
  }

  if (argc == 3) {
    if (strcmp(argv[2], "-g") != 0) {
      printf("Invalid option %s\n", argv[4]);
      loopback_test_usage();
      return -1;
    } else {
      gig_traffic = true;
    }
  }

  if (argc >= 4) {
    if (strcmp(argv[2], ETH_TRAFFIC_TEST_PERIOD_SYMBOL) != 0) {
      printf("Invalid option %s\n", argv[2]);
      loopback_test_usage();
      return -1;
    }

    print_period = strtoul(argv[3], NULL, 0);
    if (((print_period == 0) && (duration < 0)) || (print_period < 0) ||
        (print_period > ETH_TRAFFIC_MAX_REPORT_PERIOD)) {
      printf("Invalid print period: %d:%s\n", print_period, argv[3]);
      loopback_test_usage();
      return -1;
    }
    if (print_period == 0) {
      print_every_period = false;
      print_period = ETH_TRAFFIC_REPORT_PERIOD;
    }
  }

  if (argc == 5) {
    if (strcmp(argv[4], "-g") != 0) {
      printf("Invalid option %s\n", argv[4]);
      loopback_test_usage();
      return -1;
    } else {
      gig_traffic = true;
    }
  }

  net_stat(&rx_bytes, &tx_bytes);

  if (gig_traffic) {
    /*
    system_cmd("ethtool -s eth1_0 autoneg off");
    sleep(1);
    system_cmd("ethtool -s eth1_0 autoneg on");
    sleep(1);
    system_cmd("ethtool -s eth1_0 autoneg off");
    sleep(1);
    system_cmd("ethtool -s eth1_0 autoneg on");
    sleep(1);
    system_cmd("ethtool -s eth1_0 autoneg off");
    sleep(1);
    */
    if (print_period > ETH_TRAFFIC_MAX_GE_REPORT_PERIOD)
      print_period = ETH_TRAFFIC_MAX_GE_REPORT_PERIOD;
    system_cmd("ethtool -s eth1_1 autoneg off duplex full speed 1000");
    // printf("1G loopback\n");
    // Need to set crossover
  } else {
    system_cmd("ethtool -s eth1_1 autoneg off duplex full speed 10");
    // printf("10M loopback\n");
  }
  // system_cmd("brctl delif br0 eth1_0");
  system_cmd("brctl delif br0 eth1_1");
  // system_cmd("brctl delif br0 wifi0");
  // system_cmd("ethtool -s " WAN_PORT_NAME " autoneg off duplex full speed 10");
  // printf("ethtool -s eth1_0 autoneg off\n");
  sleep(9);

  pid = fork();
  if (pid < 0) {
    printf("Server fork error %d, errno %d\n", pid, errno);
    return -1;
  }
  if (pid == 0) {
    // Child process
    send_mac_pkt(WAN_PORT_NAME, NULL, pkt_len, 1000, num, dst_mac);
    // send_mac_pkt(WAN_PORT_NAME, "eth1_0", pkt_len, 0, num, dst_mac);
    exit(0);
  }
  // Parent process
  pid1 = pid;

  while (duration != 0) {
    if (duration >= 0) {
      if (duration <= print_period) {
        problem = !sleep_and_check_carrier(duration, WAN_PORT_NAME);
        print_period = duration;
        duration = 0;
        kill(pid1, SIGKILL);
        // printf("Killed processes %d and %d\n", pid1, pid2);
      } else {
        duration -= print_period;
        problem = !sleep_and_check_carrier(print_period, WAN_PORT_NAME);
      }
    } else {
      problem = !sleep_and_check_carrier(print_period, WAN_PORT_NAME);
    }

    if (duration > 0) kill(pid1, SIGSTOP);
    sleep(ETH_STAT_WAIT_PERIOD);
    net_stat(&rx_bytes, &tx_bytes);
    printf("carrier %d: TX %d RX %d\n", !(problem), tx_bytes, rx_bytes);
    if (duration > 0) kill(pid1, SIGCONT);
    ++collected_count;
    // Give 1% margin
    if ((rx_bytes == 0) ||
        (((tx_bytes / 100) * ETH_STAT_PERCENT_MARGIN) > rx_bytes)) {
      problem = true;
    }
    if ((rx_bytes > ETH_TRAFFIC_PER_PERIOD_MAX) ||
        (tx_bytes > ETH_TRAFFIC_PER_PERIOD_MAX)) {
      problem = true;
    }
    traffic_problem |= problem;
    if (!problem) {
      throughput = (((float)rx_bytes) * 8) / (float)(print_period * ONE_MEG);
      average_throughput += throughput;
    } else {
      throughput = 0.0;
    }
    if (print_every_period) {
      printf("%s %s: %3.3f Mb/s (%d:%d)\n", (problem) ? FAIL_TEXT : PASS_TEXT,
             WAN_PORT_NAME, throughput, tx_bytes, rx_bytes);
    }
    problem = false;
  }

  if (gig_traffic) {
    // Need to enable crossover
  }

  average_throughput /= ((float)collected_count);
  printf("%s overall %s: %3.3f Mb/s\n",
         (traffic_problem) ? FAIL_TEXT : PASS_TEXT, argv[1],
         average_throughput);

  system_cmd("ethtool -s " WAN_PORT_NAME " autoneg on");
  // system_cmd("brctl addif br0 " WAN_PORT_NAME);
  // printf("ethtool -s eth1_0 autoneg on\n");
  return 0;
}
