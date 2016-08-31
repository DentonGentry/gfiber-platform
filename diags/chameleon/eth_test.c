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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "../common/io.h"
#include "../common/util.h"
#include "common.h"
#include "mdio.h"

#define ETH_PORT_NAME "eth0"
#define MAX_NET_IF 2
#define BUF_SIZ 1536
#define ETH_TEST_MAX_CMD 4096
#define ETH_TEST_MAX_RSP 4096
#define ETH_TRAFFIC_PORT "wan0"
#define ETH_TRAFFIC_REPORT_PERIOD 60
#define ETH_TRAFFIC_MAX_REPORT_PERIOD 300
#define ETH_TRAFFIC_TEST_PERIOD_SYMBOL "-p"
// 100 Mb/s
#define ETH_TRAFFIC_PER_PERIOD_MAX \
  (((unsigned int)ETH_TRAFFIC_MAX_REPORT_PERIOD) * ((unsigned int)13107200))

#define SERVER_PORT 8888
#define MAX_CMD_SIZE 256
#define SCAN_CMD_FORMAT "%256s"
#define MAX_INT 0x7FFFFFFF

#define ETH_SEND_DELAY_IN_USEC 1000
#define ETH_MAX_LAN_PORTS 2
#define ETH_WAIT_AFTER_LOOPBACK_SET 5
#define ETH_PKTS_SENT_BEFORE_WAIT 0xFF
#define ETH_PKTS_LEN_DEFAULT 128
#define ETH_BUFFER_SIZE (ETH_PKTS_SENT_BEFORE_WAIT * ETH_PKTS_LEN_DEFAULT)
#define ETH_LOOPBACK_PASS_FACTOR 0.8  // 80%
#define ETH_TEST_FLUSH_NUM 5

#define ETH_RX_NAME "RX"
#define ETH_TX_NAME "TX"
#define ETH_PACKETS_NAME "packets:"
#define ETH_ERRORS_NAME "errors:"
#define ETH_BYTES_NAME "bytes:"
#define ONE_MEG (1024 * 1024)

#define ETH_DEBUG_PORT_ADDR_REG 0x1D
#define ETH_DEBUG_PORT_DATA_REG 0x1E
#define ETH_EXT_LPBK_PORT_ADDR_OFFSET 0xB
#define ETH_EXT_LPBK_PORT_SET_DATA 0x3C40
#define ETH_EXT_LPBK_PORT_CLEAR_DATA 0xBC00
#define ETH_STAT_CLEAR_CMD "ifstat > /dev/null"
#define ETH_STAT_CMD "ifstat %s | sed '1,3d;5d'"
#define ETH_STAT_RX_POS 5
#define ETH_STAT_TX_POS 7
#define ETH_STAT_WAIT_PERIOD 1  // sec
#define ETH_STAT_PERCENT_MARGIN 95

#define ETH0_SMI_REG 0xF1072004

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
  unsigned char dst_mac[6] = {0, 0, 0, 0, 0, 0};

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

int net_stat(unsigned int *rx_bytes, unsigned int *tx_bytes, char *name) {
  static unsigned int tx_stat = 0;
  static unsigned int rx_stat = 0;
  char command[MAX_CMD_SIZE], rsp[MAX_CMD_SIZE];
  unsigned int tmp;

  if (strcmp(name, ETH_PORT_NAME) != 0) {
    return -1;
  }

  snprintf(command, sizeof(command),
           "cat /sys/class/net/%s/statistics/tx_bytes", name);
  if (scan_command(command, rsp, NULL) == 0) *tx_bytes = strtoul(rsp, NULL, 10);

  snprintf(command, sizeof(command),
           "cat /sys/class/net/%s/statistics/rx_bytes", name);
  if (scan_command(command, rsp, NULL) == 0) *rx_bytes = strtoul(rsp, NULL, 10);

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
  char command[MAX_CMD_SIZE], rsp[MAX_CMD_SIZE];

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
  char command[ETH_TEST_MAX_CMD], rsp[ETH_TEST_MAX_RSP];
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

static void phy_read_usage(void) {
  printf("phy_read <ifname> <reg>\n");
  printf("Example:\n");
  printf("phy_read %s 2\n", ETH_PORT_NAME);
}

int phy_read(int argc, char *argv[]) {
  int rc = 0;
  unsigned int tmp, reg, val = 0;

  if (argc != 3) {
    phy_read_usage();
    return -1;
  }

  if (strcmp(argv[1], ETH_PORT_NAME) != 0) {
    printf("Currently support only port %s\n", ETH_PORT_NAME);
    return -1;
  }

  reg = strtol(argv[2], NULL, 0);
  tmp = reg;
  reg = (reg << 21) + (1 << 26);
  rc = write_physical_addr(ETH0_SMI_REG, reg);
  rc += read_physical_addr(ETH0_SMI_REG, &val);
  val &= 0xFFFF;
  printf("PHY %s Reg 0x%x is 0x%x\n", argv[1], tmp, val);
  return 0;
}

static void phy_write_usage(void) {
  printf("phy_write <ifname> <reg> <val>\n");
  printf("Example:\n");
  printf("phy_write i%s 22 0x6\n", ETH_PORT_NAME);
}

int phy_write(int argc, char *argv[]) {
  int rc = 0;
  int tmp, reg, val;

  if (argc != 4) {
    phy_write_usage();
    return -1;
  }

  if (strcmp(argv[1], ETH_PORT_NAME) != 0) {
    printf("Currently support only port %s\n", ETH_PORT_NAME);
    return -1;
  }

  reg = strtol(argv[2], NULL, 0);
  tmp = reg;
  val = strtol(argv[3], NULL, 16);
  reg = (reg << 21) + (0 << 26);
  val &= 0xFFFF;
  rc = write_physical_addr(ETH0_SMI_REG, reg);
  rc += write_physical_addr(ETH0_SMI_REG, (reg + val));
  printf("PHY %s Reg 0x%x = 0x%x\n", argv[1], tmp, val);
  return 0;
}

static void send_if_usage(void) {
  printf("send_if <source if> <num> [-t <delay between pkts send>]\n");
  printf("Example:\n");
  printf("send_if lan0 100\n");
  printf("send 100 msg out of lan0\n");
}

int send_if(int argc, char *argv[]) {
  int n;
  char if_name[IFNAMSIZ];
  unsigned int xfer_wait = ETH_SEND_DELAY_IN_USEC;
  unsigned char dst_mac[6] = {0, 0, 0, 0, 0, 0};

  /* Get interface name */
  if (argc == 5) {
    if (strcmp(argv[3], "-t") == 0) {
      xfer_wait = strtoul(argv[4], NULL, 10);
    } else {
      send_if_usage();
      return -1;
    }
  } else if (argc != 3) {
    send_if_usage();
    return -1;
  }

  strcpy(if_name, argv[1]);
  n = strtol(argv[2], NULL, 10);

  send_mac_pkt(if_name, NULL, BUF_SIZ, xfer_wait, n, dst_mac);

  printf("Sent %d pkt of size %d from %s to %s\n", n, BUF_SIZ, argv[1],
         argv[2]);

  return 0;
}

static void loopback_test_usage(void) {
  printf(
      "loopback_test <interface> <duration in secs> "
      "[<%s print-period in secs>]\n",
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

  if ((argc != 3) && (argc != 5)) {
    loopback_test_usage();
    return -1;
  }

  if (strcmp(argv[1], ETH_PORT_NAME) != 0) {
    printf("Invalid Ethernet Interface %s\n", argv[1]);
    return -1;
  }

  duration = strtol(argv[2], NULL, 0);
  if ((duration < -1) || (duration == 0)) {
    loopback_test_usage();
    return -1;
  }

  if (argc == 5) {
    if (strcmp(argv[3], ETH_TRAFFIC_TEST_PERIOD_SYMBOL) != 0) {
      loopback_test_usage();
      return -1;
    }

    print_period = strtoul(argv[4], NULL, 0);
    if (((print_period == 0) && (duration < 0)) || (print_period < 0) ||
        (print_period > ETH_TRAFFIC_MAX_REPORT_PERIOD)) {
      loopback_test_usage();
      return -1;
    }
    if (print_period == 0) {
      print_every_period = false;
      print_period = ETH_TRAFFIC_REPORT_PERIOD;
    }
  }

  // eth_external_loopback(argv[1], true);
  system_cmd("ethtool -s " ETH_PORT_NAME " autoneg off duplex full speed 100");
  sleep(2);

  net_stat(&rx_bytes, &tx_bytes, argv[1]);

  pid = fork();
  if (pid < 0) {
    printf("Server fork error %d, errno %d\n", pid, errno);
    return -1;
  }
  if (pid == 0) {
    // Child process
    send_mac_pkt(argv[1], NULL, pkt_len, 0, num, dst_mac);
    exit(0);
  }
  // Parent process
  pid1 = pid;

  while (duration != 0) {
    if (duration >= 0) {
      if (duration <= print_period) {
        problem = !sleep_and_check_carrier(duration, argv[1]);
        print_period = duration;
        duration = 0;
        kill(pid1, SIGKILL);
        // printf("Killed processes %d and %d\n", pid1, pid2);
      } else {
        duration -= print_period;
        problem = !sleep_and_check_carrier(print_period, argv[1]);
      }
    } else {
      problem = !sleep_and_check_carrier(print_period, argv[1]);
    }

    if (duration > 0) kill(pid1, SIGSTOP);
    sleep(ETH_STAT_WAIT_PERIOD);
    net_stat(&rx_bytes, &tx_bytes, argv[1]);
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
             argv[1], throughput, tx_bytes, rx_bytes);
    }
    problem = false;
  }

  // eth_external_loopback(argv[1], false);
  system_cmd("ethtool -s " ETH_PORT_NAME " autoneg on");

  average_throughput /= ((float)collected_count);
  printf("%s overall %s: %3.3f Mb/s\n",
         (traffic_problem) ? FAIL_TEXT : PASS_TEXT, argv[1],
         average_throughput);

  return 0;
}
