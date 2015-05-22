/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <netdb.h>
#include <linux/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include "gpio.h"
#include "../common/util.h"
#include "common.h"

#define DEFAULT_TST_IF "lan0"
#define LAN_PORT_NAME "lan0"
#define WAN_PORT_NAME "wan0"
#define MAX_NET_IF 2
#define BUF_SIZ 1536
#define ETH_TEST_MAX_CMD 4096
#define ETH_TEST_MAX_RSP 4096
#define ETH_TRAFFIC_PORT "wan0"
#define ETH_TRAFFIC_DST_PORT "lan0"
#define ETH_TRAFFIC_REPORT_PERIOD 60
#define ETH_TRAFFIC_MAX_REPORT_PERIOD 300
#define ETH_TRAFFIC_TEST_PERIOD_SYMBOL "-p"
// 100 Mb/s
#define ETH_TRAFFIC_PER_PERIOD_MAX \
  (((unsigned int)ETH_TRAFFIC_MAX_REPORT_PERIOD) * ((unsigned int)13107200))

#define SERVER_PORT 8888
#define MAX_CMD_SIZE 256
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
#define ETH_LAN_IF_PORT 0
#define ETH_WAN_IF_PORT 4
#define ETH_DEBUG_CMD "diags ethreg -i %s -p %d 0x%x=0x%x > /dev/null"
#define ETH_STAT_CLEAR_CMD "ifstat > /dev/null"
#define ETH_STAT_CMD "ifstat %s | sed '1,3d;5d'"
#define ETH_STAT_RX_POS 5
#define ETH_STAT_TX_POS 7

// WindCharger external loopback is set and cleared via Port Debug Registers.
// Debug port address offset register is 0x1D and RW port is 0x1E. It needs to
// First set the address offset, then read/write the data RW port. The
// following are functions to set up/take down external loopback.

void eth_set_debug_reg(char *if_name, unsigned short port, unsigned short addr,
                       unsigned short data) {
  char cmd[MAX_CMD_SIZE];

  snprintf(cmd, MAX_CMD_SIZE, ETH_DEBUG_CMD, if_name, port, addr, data);
  system_cmd(cmd);
}

int eth_external_loopback(char *if_name, bool set_not_clear) {
  unsigned short data = ETH_EXT_LPBK_PORT_SET_DATA;

  if (!set_not_clear) data = ETH_EXT_LPBK_PORT_CLEAR_DATA;

  if (strcmp(if_name, LAN_PORT_NAME) == 0) {
    eth_set_debug_reg(if_name, ETH_LAN_IF_PORT, ETH_DEBUG_PORT_ADDR_REG,
                      ETH_EXT_LPBK_PORT_ADDR_OFFSET);
    eth_set_debug_reg(if_name, ETH_LAN_IF_PORT, ETH_DEBUG_PORT_DATA_REG, data);
  } else if (strcmp(if_name, WAN_PORT_NAME) == 0) {
    eth_set_debug_reg(if_name, ETH_WAN_IF_PORT, ETH_DEBUG_PORT_ADDR_REG,
                      ETH_EXT_LPBK_PORT_ADDR_OFFSET);
    eth_set_debug_reg(if_name, ETH_WAN_IF_PORT, ETH_DEBUG_PORT_DATA_REG, data);
  } else {
    return -1;
  }
  return 0;
}

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

static void send_ip_usage(void) {
  printf("send_ip <address> <port> <num>\n");
  printf("Example:\n");
  printf("send_ip  192.168.1.1 10000 1\n");
  printf("send 1 msg to ip address 192.168.1.1 port 10000\n");
}

int send_ip(int argc, char *argv[]) {
  int sockfd, portno, i, n;
  struct sockaddr_in serv_addr;
  char *my_msg = "This is a test";
  unsigned int ipaddr[4];
  uint32_t ia;

  if (argc != 4) {
    send_ip_usage();
    return -1;
  }

  sscanf(argv[1], "%u.%u.%u.%u", &(ipaddr[0]), &(ipaddr[1]), &(ipaddr[2]),
         &(ipaddr[3]));
  ia = (ipaddr[3] << 24) | (ipaddr[2] << 16) | (ipaddr[1] << 8) | ipaddr[0];
  portno = strtoul(argv[2], NULL, 0);
  n = strtoul(argv[3], NULL, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = portno;
  serv_addr.sin_addr.s_addr = (__be32)(ia);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  if (sockfd < 0) {
    printf("Cannot create socket. sockfd = %d\n", sockfd);
    return -1;
  }

  for (i = 0; i < n; ++i) {
    if (sendto(sockfd, my_msg, strlen(my_msg), 0, (struct sockaddr *)&serv_addr,
               sizeof(serv_addr)) < 0) {
      printf("Cannot send msg to socket %s\n", my_msg);
      return -1;
    }
  }

  printf("send %d packets to %u.%u.%u.%u:0x%08x port %d\n", n, ipaddr[0],
         ipaddr[1], ipaddr[2], ipaddr[3], serv_addr.sin_addr.s_addr, portno);

  return 0;
}

int net_stat(unsigned int *rx_bytes, unsigned int *tx_bytes, char *name) {
  static const char *kIfName[MAX_NET_IF] = {LAN_PORT_NAME, WAN_PORT_NAME};
  static unsigned int tx_stat[MAX_NET_IF] = {0, 0};
  static unsigned int rx_stat[MAX_NET_IF] = {0, 0};
  char command[MAX_CMD_SIZE], rsp[MAX_CMD_SIZE];
  unsigned int index, tmp;
  FILE *fp;

  for (index = 0; index < MAX_NET_IF; ++index) {
    if (strcmp(name, kIfName[index]) == 0) {
      break;
    }
  }
  if (index >= MAX_NET_IF) return -1;

  sprintf(command, "cat /sys/class/net/%s/statistics/tx_bytes", name);
  fp = popen(command, "r");
  if (fp != NULL) {
    if (fscanf(fp, "%s", rsp) != EOF) {
      *tx_bytes = strtoul(rsp, NULL, 10);
    }
    pclose(fp);
  }

  sprintf(command, "cat /sys/class/net/%s/statistics/rx_bytes", name);
  fp = popen(command, "r");
  if (fp != NULL) {
    if (fscanf(fp, "%s", rsp) != EOF) {
      *rx_bytes = strtoul(rsp, NULL, 10);
    }
    pclose(fp);
  }

  if (*tx_bytes >= tx_stat[index]) {
    *tx_bytes -= tx_stat[index];
    tx_stat[index] += *tx_bytes;
  } else {
    tmp = *tx_bytes;
    // tx_bytes is uint.
    *tx_bytes += (0xffffffff - tx_stat[index]);
    tx_stat[index] = tmp;
  }

  if (*rx_bytes >= rx_stat[index]) {
    *rx_bytes -= rx_stat[index];
    rx_stat[index] += *rx_bytes;
  } else {
    tmp = *rx_bytes;
    // rx_bytes is uint.
    *rx_bytes += (0xffffffff - rx_stat[index]);
    rx_stat[index] = tmp;
  }
  return 0;
}

// Not needed for now
#if 0
int get_ip_stat(unsigned int *rx_bytes, unsigned int *tx_bytes, char *name) {
  char command[MAX_CMD_SIZE], rsp[MAX_CMD_SIZE];
  FILE *fp;
  int pos = 0;
  unsigned long long long_long_tmp;
  unsigned int uint_tmp;

  *rx_bytes = 0;
  *tx_bytes = 0;
  sprintf(command, ETH_STAT_CMD, name);
  fp = popen(command, "r");
  while (fscanf(fp, "%s", rsp) != EOF) {
    if ((pos == ETH_STAT_RX_POS) || (pos == ETH_STAT_TX_POS)) {
      long_long_tmp = strtoull(rsp, NULL, 0);
      if (rsp[strlen(rsp) - 1] == 'K') {
        long_long_tmp *= 1000;
      } else if (rsp[strlen(rsp) - 1] == 'M') {
        long_long_tmp *= (1024 * 1024);
      }
      uint_tmp = long_long_tmp & 0xffffffff;
      if (uint_tmp & 0x80000000) {
        uint_tmp = 0xffffffff - uint_tmp;
      }
      if (pos == ETH_STAT_RX_POS)
        *rx_bytes = uint_tmp;
      else
        *tx_bytes = uint_tmp;
    }
    ++pos;
  }
  pclose(fp);

  return 0;
}
#endif

// Return 0 if lost carrier. Otherwise, 1
int get_carrier_state(char *name) {
  char command[MAX_CMD_SIZE], rsp[MAX_CMD_SIZE];
  FILE *fp;

  sprintf(command, "cat /sys/class/net/%s/carrier", name);
  fp = popen(command, "r");
  if (fp != NULL) {
    if (fscanf(fp, "%s", rsp) != EOF) {
      if (strcmp(rsp, "0") == 0) {
        pclose(fp);
        return 0;
      }
    }
    pclose(fp);
  }
  return 1;
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
  static const char *kIpName = "inet";
  FILE *fp;
  bool found = false;

  sprintf(command, "ip addr show %s", name);
  fp = popen(command, "r");
  while (fscanf(fp, "%s", rsp) != EOF) {
    if (!strcmp(rsp, kIpName)) {
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      if (sscanf(rsp, "%u.%u.%u.%u", ip, (ip + 1), (ip + 2), (ip + 3)) <= 0) {
        return -1;
      }
      found = true;
      break;
    }
  }
  pclose(fp);

  if (!found) {
    return -1;
  }

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

static void send_if_to_if_usage(void) {
  printf(
      "send_if_to_if <src if> <dest if> <secs> [-b <pkt byte size (max %d)>] "
      "[-t <time delay in micro-second between pkt send>]\n",
      BUF_SIZ);
  printf("Example:\n");
  printf("send_if_to_if wan0 lan0 10\n");
  printf("send 10 seconds from interface wan0 to lan0\n");
  printf("send_if_to_if wan0 lan0 10 -b 256 -t 250\n");
  printf(
      "send from interface wan0 to lan0 for 10 seconds of size 256 bytes and"
      " 250 us delay\n");
}

int send_if_to_if(int argc, char *argv[]) {
  int n;
  unsigned int xfer_len = ETH_PKTS_LEN_DEFAULT, xfer_wait = 0;
  char if_name[IFNAMSIZ], dst_name[IFNAMSIZ];
  unsigned int src_rx_bytes, src_tx_bytes, dst_rx_bytes, dst_tx_bytes;
  int pid;

  /* Get interface name */
  if ((argc != 4) && (argc != 6) && (argc != 8)) {
    send_if_to_if_usage();
    return -1;
  }

  strcpy(if_name, argv[1]);
  strcpy(dst_name, argv[2]);

  if (argc >= 6) {
    unsigned int tmp;
    tmp = strtoul(argv[5], NULL, 10);
    if (strcmp(argv[4], "-b") == 0) {
      if (tmp > BUF_SIZ) {
        send_if_to_if_usage();
        return -1;
      }
      xfer_len = tmp;

      if (argc == 8) {
        if (strcmp(argv[6], "-t") == 0) {
          xfer_wait = strtoul(argv[7], NULL, 10);
        } else {
          send_if_to_if_usage();
          return -1;
        }
      }
    } else if (strcmp(argv[4], "-t") == 0) {
      xfer_wait = tmp;
      if (argc == 8) {
        if (strcmp(argv[6], "-b") == 0) {
          xfer_len = strtoul(argv[7], NULL, 10);
          if (xfer_len > BUF_SIZ) {
            send_if_to_if_usage();
            return -1;
          }
        } else {
          send_if_to_if_usage();
          return -1;
        }
      }
    } else {
      send_if_to_if_usage();
      return -1;
    }
  }

  n = strtoul(argv[3], NULL, 10);

  net_stat(&src_rx_bytes, &src_tx_bytes, if_name);
  net_stat(&dst_rx_bytes, &dst_tx_bytes, dst_name);
  pid = fork();
  if (pid < 0) {
    printf("Server fork error %d, errno %d\n", pid, errno);
    return -1;
  }
  if (pid == 0) {
    // Child process
    send_mac_pkt(if_name, dst_name, xfer_len, xfer_wait, -1, NULL);
    exit(0);
  }
  // Parent process
  sleep(n);
  kill(pid, SIGKILL);
  net_stat(&src_rx_bytes, &src_tx_bytes, if_name);
  net_stat(&dst_rx_bytes, &dst_tx_bytes, dst_name);

  if (dst_rx_bytes >= src_tx_bytes) {
    printf("Sent %d seconds from %s(%d) to %s(%d) rate %3.3f Mb/s\n", n,
           if_name, src_tx_bytes, dst_name, dst_rx_bytes,
           (((float)dst_rx_bytes) * 8.0) / ((float)(n * ONE_MEG)));
  } else {
    printf("%s Sent %d seconds from %s(%d) to %s(%d)\n", FAIL_TEXT, n, if_name,
           src_tx_bytes, dst_name, dst_rx_bytes);
  }

  return 0;
}

static void send_if_to_mac_usage(void) {
  printf(
      "send_if_to_mac <if> <dest MAC> <num> [-b <pkt byte size (max %d)>] "
      "[-t <time delay in micro-second between pkt send>]\n",
      BUF_SIZ);
  printf("Example:\n");
  printf("send_if_to_mac lan0 f8:8f:ca:00:16:04 100\n");
  printf("send 100 msg from interface lan0 to f8:8f:ca:00:16:04\n");
  printf("send_if_to_mac lan0 f8:8f:ca:00:16:04 100 -b 256 -t 250\n");
  printf(
      "send to interface lan0 with 100 msgs of size 256 bytes and 250 us "
      "delay\n");
}

int send_if_to_mac(int argc, char *argv[]) {
  int n;
  unsigned int xfer_len = 16, xfer_wait = 0;
  char if_name[IFNAMSIZ];
  unsigned char dst_mac[6] = {0, 0, 0, 0, 0, 0};
  // int rx_bytes, tx_bytes, rx_errs, tx_errs;

  /* Get interface name */
  if ((argc != 4) && (argc != 6) && (argc != 8)) {
    send_if_to_mac_usage();
    return -1;
  }

  strcpy(if_name, argv[1]);

  dst_mac[0] = strtoul(&argv[2][0], NULL, 16);
  dst_mac[1] = strtoul(&argv[2][3], NULL, 16);
  dst_mac[2] = strtoul(&argv[2][6], NULL, 16);
  dst_mac[3] = strtoul(&argv[2][9], NULL, 16);
  dst_mac[4] = strtoul(&argv[2][12], NULL, 16);
  dst_mac[5] = strtoul(&argv[2][15], NULL, 16);

  if (argc >= 6) {
    unsigned int tmp;
    tmp = strtoul(argv[5], NULL, 10);
    if (strcmp(argv[4], "-b") == 0) {
      if (tmp > BUF_SIZ) {
        send_if_to_mac_usage();
        return -1;
      }
      xfer_len = tmp;

      if (argc == 8) {
        if (strcmp(argv[6], "-t") == 0) {
          xfer_wait = strtoul(argv[7], NULL, 10);
        } else {
          send_if_to_mac_usage();
          return -1;
        }
      }
    } else if (strcmp(argv[4], "-t") == 0) {
      xfer_wait = tmp;
      if (argc == 8) {
        if (strcmp(argv[6], "-b") == 0) {
          xfer_len = strtoul(argv[7], NULL, 10);
          if (xfer_len > BUF_SIZ) {
            send_if_to_mac_usage();
            return -1;
          }
        } else {
          send_if_to_mac_usage();
          return -1;
        }
      }
    } else {
      send_if_to_mac_usage();
      return -1;
    }
  }

  n = strtoul(argv[3], NULL, 10);

  send_mac_pkt(if_name, NULL, xfer_len, xfer_wait, n, dst_mac);

  printf(
      "Sent %d packets from IF %s to "
      "%02x:%02x:%02x:%02x:%02x:%02x\n",
      n, if_name, dst_mac[0], dst_mac[1], dst_mac[2], dst_mac[3], dst_mac[4],
      dst_mac[5]);

  return 0;
}

static void test_both_ports_usage(void) {
  printf("test_both_ports <duration in secs> [<%s print-period in secs>]\n",
         ETH_TRAFFIC_TEST_PERIOD_SYMBOL);
  printf("- duration >=1 or -1 (forever)\n");
  printf("- print-period >= 0 and <= %d\n", ETH_TRAFFIC_MAX_REPORT_PERIOD);
  printf("- traffic sent between lan0 and wan0\n");
  printf("- print-period > 0 if duration > 0\n");
  printf("- print-period = 0 prints only the summary\n");
}

int test_both_ports(int argc, char *argv[]) {
  int duration, num = -1, pid, pid1, pid2;
  int print_period = ETH_TRAFFIC_REPORT_PERIOD;
  unsigned int pkt_len = ETH_PKTS_LEN_DEFAULT, lan0_rx, lan0_tx;
  unsigned int wan0_rx, wan0_tx;
  bool print_every_period = true;
  bool failed = false, overall_failed = false;
  ;

  if ((argc != 2) && (argc != 4)) {
    test_both_ports_usage();
    return -1;
  }

  duration = strtol(argv[1], NULL, 0);
  if ((duration < -1) || (duration == 0)) {
    test_both_ports_usage();
    return -1;
  }

  if (argc == 4) {
    if (strcmp(argv[2], ETH_TRAFFIC_TEST_PERIOD_SYMBOL) != 0) {
      test_both_ports_usage();
      return -1;
    }

    print_period = strtoul(argv[3], NULL, 0);
    if (((print_period == 0) && (duration < 0)) || (print_period < 0) ||
        (print_period > ETH_TRAFFIC_MAX_REPORT_PERIOD)) {
      test_both_ports_usage();
      return -1;
    }
    if (print_period == 0) {
      print_every_period = false;
      print_period = ETH_TRAFFIC_REPORT_PERIOD;
    }
  }

  net_stat(&wan0_rx, &wan0_tx, ETH_TRAFFIC_PORT);
  net_stat(&lan0_rx, &lan0_tx, ETH_TRAFFIC_DST_PORT);

  pid = fork();
  if (pid < 0) {
    printf("Server fork error %d, errno %d\n", pid, errno);
    return -1;
  }
  if (pid == 0) {
    // Child process
    send_mac_pkt(ETH_TRAFFIC_PORT, ETH_TRAFFIC_DST_PORT, pkt_len, 0, num, NULL);
    exit(0);
  }
  // Parent process
  pid1 = pid;

  pid = fork();
  if (pid < 0) {
    printf("Server fork error %d, errno %d\n", pid, errno);
    return -1;
  }
  if (pid == 0) {
    // Child process
    send_mac_pkt(ETH_TRAFFIC_DST_PORT, ETH_TRAFFIC_PORT, pkt_len, 0, num, NULL);
    exit(0);
  }
  // Parent process
  pid2 = pid;

  while (duration != 0) {
    if (duration >= 0) {
      if (duration <= print_period) {
        failed = !sleep_and_check_carrier(duration, ETH_TRAFFIC_PORT);
        print_period = duration;
        duration = 0;
        kill(pid1, SIGKILL);
        kill(pid2, SIGKILL);
        // printf("Killed processes %d and %d\n", pid1, pid2);
      } else {
        duration -= print_period;
        failed = !sleep_and_check_carrier(print_period, ETH_TRAFFIC_PORT);
      }
    } else {
      failed = !sleep_and_check_carrier(print_period, ETH_TRAFFIC_PORT);
    }
    if (print_every_period) {
      if (duration > 0) {
        kill(pid1, SIGSTOP);
        kill(pid2, SIGSTOP);
      }
      net_stat(&wan0_rx, &wan0_tx, ETH_TRAFFIC_PORT);
      net_stat(&lan0_rx, &lan0_tx, ETH_TRAFFIC_DST_PORT);
      if ((lan0_rx == 0) || (wan0_rx == 0) || (lan0_tx == 0) || (wan0_tx == 0))
        failed = true;
      // Due to two processes are stopped one after another, need some
      // margin to compare RX vs TX. Set it to 1% for now
      if (lan0_rx < ((wan0_tx / 100) * 99)) failed = true;
      if (wan0_rx < ((lan0_tx / 100) * 99)) failed = true;
      // When the cable is disconnected and connected again, got bogus data
      if ((lan0_rx > ETH_TRAFFIC_PER_PERIOD_MAX) ||
          (wan0_rx > ETH_TRAFFIC_PER_PERIOD_MAX))
        failed = true;
      if (failed) {
        printf("Failed: %s (%d,%d) <-> %s (%d,%d)\n", ETH_TRAFFIC_PORT, wan0_tx,
               wan0_rx, ETH_TRAFFIC_DST_PORT, lan0_tx, lan0_rx);
      } else {
        printf("Passed: %s %3.3f Mb/s (%d,%d) <-> %s %3.3f Mb/s (%d,%d)\n",
               ETH_TRAFFIC_PORT,
               (((float)wan0_tx) * 8) / (float)(print_period * ONE_MEG),
               wan0_tx, wan0_rx, ETH_TRAFFIC_DST_PORT,
               (((float)lan0_tx) * 8) / (float)(print_period * ONE_MEG),
               lan0_tx, lan0_rx);
      }
      overall_failed |= failed;
      failed = false;
      if (duration > 0) {
        kill(pid1, SIGCONT);
        kill(pid2, SIGCONT);
      }
    }
  }
  if (overall_failed) printf("%s Ethernet port test\n", FAIL_TEXT);

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

  if ((strcmp(argv[1], LAN_PORT_NAME) != 0) &&
      (strcmp(argv[1], WAN_PORT_NAME) != 0)) {
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

  eth_external_loopback(argv[1], true);

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
    net_stat(&rx_bytes, &tx_bytes, argv[1]);
    if (duration > 0) kill(pid1, SIGCONT);
    ++collected_count;
    // Give 1% margin
    if ((rx_bytes == 0) || (((tx_bytes / 100) * 99) > rx_bytes)) {
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

  eth_external_loopback(argv[1], false);

  average_throughput /= ((float)collected_count);
  if (traffic_problem) {
    printf("%s overall %s: %3.3f Mb/s\n", FAIL_TEXT, argv[1],
           average_throughput);
  } else {
    printf("%s overall %s: %3.3f Mb/s\n", PASS_TEXT, argv[1],
           average_throughput);
  }

  return 0;
}
