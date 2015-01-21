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

#define DEFAULT_TST_IF "eth0"
#define LAN_PORT_NAME "eth0"
#define WAN_PORT_NAME "eth1"
#define BUF_SIZ 1500
#define ETH_TEST_MAX_CMD 4096
#define ETH_TEST_MAX_RSP 4096
#define ETH_TRAFFIC_PORT "eth1"
#define ETH_TRAFFIC_DST_PORT "eth0"
#define ETH_TRAFFIC_REPORT_PERIOD 60
#define ETH_TRAFFIC_MAX_REPORT_PERIOD 300
#define ETH_TRAFFIC_TEST_PERIOD_SYMBOL "-p"

#define SERVER_PORT 8888
#define MAX_CMD_SIZE 256
#define MAX_INT 0x7FFFFFFF

#define ETH_SEND_DELAY_IN_USEC 1000
#define ETH_MAX_LAN_PORTS 2
#define ETH_WAIT_AFTER_LOOPBACK_SET 5
#define ETH_PKTS_SENT_BEFORE_WAIT 0xFF
#define ETH_PKTS_LEN_DEFAULT 32
#define ETH_BUFFER_SIZE (ETH_PKTS_SENT_BEFORE_WAIT * ETH_PKTS_LEN_DEFAULT)
#define ETH_LOOPBACK_PASS_FACTOR 0.8  // 80%
#define ETH_TEST_FLUSH_NUM 5

#define ETH_RX_NAME "RX"
#define ETH_TX_NAME "TX"
#define ETH_PACKETS_NAME "packets:"
#define ETH_ERRORS_NAME "errors:"
#define ETH_BYTES_NAME "bytes:"
#define ONE_MEG (1024 * 1024)

static void send_ip_usage(void) {
  printf("send_ip <address> <port> <num>\n");
  printf("Example:\n");
  printf("send_ip  192.168.1.1 10000 1\n");
  printf("send 1 msg to ip address 192.168.1.1 port 10000\n");
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

  sleep(1);

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

int setup_loopback(int port) {
  if (port < 0 || port > ETH_MAX_LAN_PORTS) return -1;
  return 0;
}

int take_down_loopback(int port) {
  if (port < 0 || port > ETH_MAX_LAN_PORTS) return -1;
  return 0;
}

int get_ip_stat(int *rx_bytes, int *tx_bytes, int *rx_errs, int *tx_errs,
                char *name) {
  char command[4096], rsp[MAX_CMD_SIZE];
  FILE *fp;
  int pkt_name_len = strlen(ETH_BYTES_NAME);
  int error_name_len = strlen(ETH_ERRORS_NAME);

  sprintf(command, "ifconfig %s", name);
  fp = popen(command, "r");
  while (fscanf(fp, "%s", rsp) != EOF) {
    if (strcmp(rsp, ETH_RX_NAME) == 0) {
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      if (strncmp(rsp, ETH_BYTES_NAME, pkt_name_len) == 0) {
        *rx_bytes = strtoul(&rsp[pkt_name_len], NULL, 0);
        continue;
      } else if (strncmp(rsp, ETH_PACKETS_NAME, pkt_name_len) == 0) {
        if (fscanf(fp, "%s", rsp) <= 0) return -1;
        if (strncmp(rsp, ETH_ERRORS_NAME, error_name_len) == 0) {
          *rx_errs = strtoul(&rsp[error_name_len], NULL, 0);
        }
      }
    } else if (strcmp(rsp, ETH_TX_NAME) == 0) {
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      if (strncmp(rsp, ETH_BYTES_NAME, pkt_name_len) == 0) {
        *tx_bytes = strtoul(&rsp[pkt_name_len], NULL, 0);
        continue;
      } else if (strncmp(rsp, ETH_PACKETS_NAME, pkt_name_len) == 0) {
        if (fscanf(fp, "%s", rsp) <= 0) return -1;
        if (strncmp(rsp, ETH_ERRORS_NAME, error_name_len) == 0) {
          *tx_errs = strtoul(&rsp[error_name_len], NULL, 0);
        }
      }
    }
  }
  pclose(fp);

  return 0;
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
  printf("send_if eth0 100\n");
  printf("send 100 msg out of eth0\n");
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

static void loopback_usage(void) {
  printf(
      "loopback <PHY ports in bit mask (hex)> <num> "
      "[-b <pkt byte size (max %d)>] "
      "[-t <time delay in micro-second between pkt send>]\n",
      BUF_SIZ);
  printf("Example:\n");
  printf("loopback 0x1F 100\n");
  printf("loopback PHY port 0, 1, 2, 3 ,4 with 100 msgs\n");
  printf("loopback 0xF 100 -b 256 -t 250\n");
  printf(
      "loopback PHY port 0, 1, 2, 3 with 100 msgs of size 256 bytes "
      "and 250 us delay\n");
}

int loopback(int argc, char *argv[]) {
  int n, port, pass_num, j;
  unsigned int xfer_len = ETH_PKTS_LEN_DEFAULT, xfer_wait = 250, rx_wait = 2;
  char if_name[IFNAMSIZ];
  char out_name[IFNAMSIZ];
  // char cmd[MAX_CMD_SIZE];
  int rx_pkts, tx_pkts, rx_errs, tx_errs;
  int prev_rx_pkts, prev_tx_pkts, prev_rx_errs, prev_tx_errs;
  unsigned int portMask;
  static const int kLoopbackRetries = 3;

  /* Get interface name */
  if ((argc != 3) && (argc != 5) && (argc != 7)) {
    printf("%s invalid params\n", FAIL_TEXT);
    loopback_usage();
    return -1;
  }

#if 0
  // Increase the buffer size
  sprintf(cmd, "echo %d > /proc/sys/net/core/rmem_max", ETH_BUFFER_SIZE);
  system_cmd(cmd);
  sprintf(cmd, "echo %d > /proc/sys/net/core/rmem_default", ETH_BUFFER_SIZE);
  system_cmd(cmd);
  sprintf(cmd, "echo %d > /proc/sys/net/core/wmem_max", ETH_BUFFER_SIZE);
  system_cmd(cmd);
  sprintf(cmd, "echo %d > /proc/sys/net/core/wmem_default", ETH_BUFFER_SIZE);
  system_cmd(cmd);
#endif

  portMask = strtoul(argv[1], NULL, 16);
  n = strtoul(argv[2], NULL, 10);
  pass_num = (int)(n * ETH_LOOPBACK_PASS_FACTOR);

  if (argc >= 5) {
    unsigned int tmp;
    tmp = strtoul(argv[4], NULL, 10);
    if (strcmp(argv[3], "-b") == 0) {
      if (tmp > BUF_SIZ) {
        printf("%s invalid params\n", FAIL_TEXT);
        loopback_usage();
        return -1;
      }
      xfer_len = tmp;

      if (argc == 7) {
        if (strcmp(argv[5], "-t") == 0) {
          xfer_wait = strtoul(argv[6], NULL, 10);
        } else {
          printf("%s invalid params\n", FAIL_TEXT);
          loopback_usage();
          return -1;
        }
      }
    } else if (strcmp(argv[3], "-t") == 0) {
      xfer_wait = tmp;
      if (argc == 7) {
        if (strcmp(argv[5], "-b") == 0) {
          xfer_len = strtoul(argv[6], NULL, 10);
          if (xfer_len > BUF_SIZ) {
            printf("%s invalid params\n", FAIL_TEXT);
            loopback_usage();
            return -1;
          }
        } else {
          printf("%s invalid params\n", FAIL_TEXT);
          loopback_usage();
          return -1;
        }
      }
    } else {
      printf("%s invalid params\n", FAIL_TEXT);
      loopback_usage();
      return -1;
    }
  }

  strcpy(if_name, LAN_PORT_NAME);
  strcpy(out_name, LAN_PORT_NAME);

  printf("Sending %d packets of size %d delay %d\n", n, xfer_len, xfer_wait);

  for (port = 0; port < ETH_MAX_LAN_PORTS; ++port) {
    if (portMask & (1 << port)) {
      for (j = 0; j < kLoopbackRetries; ++j) {
        setup_loopback(port);
        sleep(ETH_WAIT_AFTER_LOOPBACK_SET);
        rx_pkts = 0;
        get_ip_stat(&prev_rx_pkts, &prev_tx_pkts, &prev_rx_errs, &prev_tx_errs,
                    LAN_PORT_NAME);
        system_cmd("uptime");

        send_mac_pkt(if_name, out_name, xfer_len, xfer_wait, n, NULL);
        system_cmd("uptime");
        rx_pkts = 0;
        sleep(rx_wait);
        get_ip_stat(&rx_pkts, &tx_pkts, &rx_errs, &tx_errs, LAN_PORT_NAME);
        rx_pkts -= prev_rx_pkts;
        tx_pkts -= prev_tx_pkts;
        rx_errs -= prev_rx_errs;
        tx_errs -= prev_tx_errs;
        take_down_loopback(port);
        if ((rx_pkts >= pass_num) && (tx_pkts >= pass_num) && (rx_errs == 0) &&
            (tx_errs == 0)) {
          printf("PHY %d passed loop back test. Sent %d:%d, Received %d\n",
                 port, n, tx_pkts, rx_pkts);
          fflush(stdout);
          break;
        } else {
          if (j == (kLoopbackRetries - 1)) {
            printf(
                "%s PHY %d failed loop back test. Sent %d:%d, Received %d, "
                "Errs %d:%d\n",
                FAIL_TEXT, port, n, tx_pkts, rx_pkts, tx_errs, rx_errs);
          }
          fflush(stdout);
        }
      }
    }
  }

  return 0;
}

static void setup_eth_ports_for_traffic() {
  system_cmd("brctl delif br0 eth0");
  system_cmd("brctl delif br0 eth1");
}

static void send_if_to_if_usage(void) {
  printf(
      "send_if_to_if <src if> <dest if> <secs> [-b <pkt byte size (max %d)>] "
      "[-t <time delay in micro-second between pkt send>]\n",
      BUF_SIZ);
  printf("Example:\n");
  printf("send_if_to_if eth1 eth0 10\n");
  printf("send 10 seconds from interface eth1 to eth0\n");
  printf("send_if_to_if eth1 eth0 10 -b 256 -t 250\n");
  printf(
      "send from interface eth1 to eth0 for 10 seconds of size 256 bytes and"
      " 250 us delay\n");
}

int send_if_to_if(int argc, char *argv[]) {
  int n;
  unsigned int xfer_len = 128, xfer_wait = 0;
  char if_name[IFNAMSIZ], dst_name[IFNAMSIZ];
  int src_rx_pkts, src_tx_pkts, src_rx_errs, src_tx_errs;
  int dst_rx_pkts, dst_tx_pkts, dst_rx_errs, dst_tx_errs;
  int out_pkts, in_pkts, prev_out_pkts, prev_in_pkts;
  int prev_in_errs, prev_out_errs;
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

  setup_eth_ports_for_traffic();

  get_ip_stat(&src_rx_pkts, &src_tx_pkts, &src_rx_errs, &src_tx_errs, if_name);
  get_ip_stat(&dst_rx_pkts, &dst_tx_pkts, &dst_rx_errs, &dst_tx_errs, dst_name);
  prev_out_pkts = src_tx_pkts;
  prev_in_pkts = dst_rx_pkts;
  prev_out_errs = src_tx_errs;
  prev_in_errs = dst_rx_errs;
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
  get_ip_stat(&src_rx_pkts, &src_tx_pkts, &src_rx_errs, &src_tx_errs, if_name);
  get_ip_stat(&dst_rx_pkts, &dst_tx_pkts, &dst_rx_errs, &dst_tx_errs, dst_name);
  out_pkts = src_tx_pkts - prev_out_pkts;
  in_pkts = dst_rx_pkts - prev_in_pkts;

  if (in_pkts >= out_pkts) {
    printf("Sent %d seconds from %s(%d:%d) to %s(%d:%d) rate %3.3f Mb/s\n", n,
           if_name, out_pkts, src_tx_errs - prev_out_errs, dst_name, in_pkts,
           dst_rx_errs - prev_in_errs,
           (((float)in_pkts) * 8.0) / ((float)(n * ONE_MEG)));
  } else {
    printf("%s Sent %d seconds from %s(%d) to %s(%d)\n", FAIL_TEXT, n, if_name,
           out_pkts, dst_name, in_pkts);
  }

  return 0;
}

static void send_if_to_mac_usage(void) {
  printf(
      "send_if_to_mac <if> <dest MAC> <num> [-b <pkt byte size (max %d)>] "
      "[-t <time delay in micro-second between pkt send>]\n",
      BUF_SIZ);
  printf("Example:\n");
  printf("send_if_to_mac eth0 f8:8f:ca:00:16:04 100\n");
  printf("send 100 msg from interface eth0 to f8:8f:ca:00:16:04\n");
  printf("send_if_to_mac eth0 f8:8f:ca:00:16:04 100 -b 256 -t 250\n");
  printf(
      "send to interface eth0 with 100 msgs of size 256 bytes and 250 us "
      "delay\n");
}

int send_if_to_mac(int argc, char *argv[]) {
  int n;
  unsigned int xfer_len = 16, xfer_wait = 0;
  char if_name[IFNAMSIZ];
  unsigned char dst_mac[6] = {0, 0, 0, 0, 0, 0};
  // int rx_pkts, tx_pkts, rx_errs, tx_errs;

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

static void lan_lpbk_usage(void) {
  printf("lan_lpbk <on/off>\n");
  printf("Example:\n");
  printf("lan_lpbk on\n");
  printf("set all lan ports loop back to external\n");
}

int lan_lpbk(int argc, char *argv[]) {
  if (argc != 2 || argv == NULL) {
    lan_lpbk_usage();
    return -1;
  }
  return 0;
}

static void traffic_usage(void) {
  printf("traffic <test duration> [<%s print period>]\n",
         ETH_TRAFFIC_TEST_PERIOD_SYMBOL);
  printf("- duration >=1 or -1 (forever)\n");
  printf("- traffic sent from eth0 to eth1\n");
  printf("- print period > 0\n");
}

int traffic(int argc, char *argv[]) {
  char cmd[MAX_CMD_SIZE];
  int duration, num = -1;
  int pid, pid1, pid2;
  int print_period = ETH_TRAFFIC_REPORT_PERIOD;
  static const unsigned char dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  if ((argc != 2) && (argc != 4)) {
    traffic_usage();
    return -1;
  }

  duration = strtol(argv[1], NULL, 0);
  if ((duration < -1) || (duration == 0)) {
    traffic_usage();
    return -1;
  }

  if (argc == 4) {
    if (strcmp(argv[2], ETH_TRAFFIC_TEST_PERIOD_SYMBOL) != 0) {
      traffic_usage();
      return -1;
    }

    print_period = strtoul(argv[3], NULL, 0);
    if (print_period == 0) {
      traffic_usage();
      return -1;
    }
  }

  pid = fork();
  if (pid < 0) {
    printf("Server fork error %d, errno %d\n", pid, errno);
    return -1;
  }
  if (pid == 0) {
    // Child process
    send_mac_pkt(ETH_TRAFFIC_PORT, NULL, BUF_SIZ, 0, num, dst_mac);
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
    send_mac_pkt(ETH_TRAFFIC_PORT, NULL, BUF_SIZ, 0, num, dst_mac);
    exit(0);
  }
  // Parent process
  pid2 = pid;

  while (duration != 0) {
    if (duration >= 0) {
      if (duration <= print_period) {
        sleep(duration);
        duration = 0;
        kill(pid1, SIGKILL);
        kill(pid2, SIGKILL);
        // printf("Killed processes %d and %d\n", pid1, pid2);
      } else {
        duration -= print_period;
        sleep(print_period);
      }
    } else {
      sleep(print_period);
    }
    sprintf(cmd, "ifconfig %s", ETH_TRAFFIC_PORT);
    system_cmd(cmd);
    sprintf(cmd, "ifconfig %s", ETH_TRAFFIC_DST_PORT);
    system_cmd(cmd);
  }

  return 0;
}

static void test_both_ports_usage(void) {
  printf("test_both_ports <duration in secs> [<%s print-period in secs>]\n",
         ETH_TRAFFIC_TEST_PERIOD_SYMBOL);
  printf("- duration >=1 or -1 (forever)\n");
  printf("- print-period >= 0 and <= %d\n", ETH_TRAFFIC_MAX_REPORT_PERIOD);
  printf("- traffic sent between eth0 and eth1\n");
  printf("- print-period > 0 if duration > 0\n");
  printf("- print-period = 0 prints only the summary\n");
}

int test_both_ports(int argc, char *argv[]) {
  // char cmd[MAX_CMD_SIZE];
  int duration, num = -1, collected_count = 0;
  int pid, pid1, pid2;
  int print_period = ETH_TRAFFIC_REPORT_PERIOD;
  unsigned int pkt_len = 128;
  int src_rx_pkts, src_tx_pkts, src_rx_errs, src_tx_errs;
  int dst_rx_pkts, dst_tx_pkts, dst_rx_errs, dst_tx_errs;
  int prev_src_rx, prev_dst_rx, src_diff, dst_diff;
  int prev_src_tx_errs, prev_dst_tx_errs, prev_src_rx_errs, prev_dst_rx_errs;
  bool print_every_period = true;
  float src_average_throughput = 0.0, dst_average_throughput = 0.0;
  float src_throughput, dst_throughput;
  int src_total_errs = 0, dst_total_errs = 0;

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
    if (((print_period == 0) && (duration < 0)) || 
        (print_period < 0) || (print_period > ETH_TRAFFIC_MAX_REPORT_PERIOD)) {
      test_both_ports_usage();
      return -1;
    }
    if (print_period == 0) {
      print_every_period = false;
      print_period = ETH_TRAFFIC_REPORT_PERIOD;
    }
  }

  setup_eth_ports_for_traffic();
  // For some reason, there is always one error after deleted from the bridge.
  // We need to send a few packets to flush the errors first.
  send_mac_pkt(ETH_TRAFFIC_PORT, ETH_TRAFFIC_DST_PORT, pkt_len, 0,
               ETH_TEST_FLUSH_NUM, NULL);
  send_mac_pkt(ETH_TRAFFIC_DST_PORT, ETH_TRAFFIC_PORT, pkt_len, 0,
               ETH_TEST_FLUSH_NUM, NULL);

  get_ip_stat(&src_rx_pkts, &src_tx_pkts, &src_rx_errs, &src_tx_errs,
              ETH_TRAFFIC_PORT);
  get_ip_stat(&dst_rx_pkts, &dst_tx_pkts, &dst_rx_errs, &dst_tx_errs,
              ETH_TRAFFIC_DST_PORT);
  prev_src_rx = src_rx_pkts;
  prev_dst_rx = dst_rx_pkts;
  prev_src_rx_errs = src_rx_errs;
  prev_dst_rx_errs = dst_rx_errs;
  prev_src_tx_errs = src_tx_errs;
  prev_dst_tx_errs = dst_tx_errs;

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
        sleep(duration);
        print_period = duration;
        duration = 0;
        kill(pid1, SIGKILL);
        kill(pid2, SIGKILL);
        // printf("Killed processes %d and %d\n", pid1, pid2);
      } else {
        duration -= print_period;
        sleep(print_period);
      }
    } else {
      sleep(print_period);
    }
#if 0
    sprintf(cmd, "ifconfig %s", ETH_TRAFFIC_PORT);
    system_cmd(cmd);
    sprintf(cmd, "ifconfig %s", ETH_TRAFFIC_DST_PORT);
    system_cmd(cmd);
#endif
    get_ip_stat(&src_rx_pkts, &src_tx_pkts, &src_rx_errs, &src_tx_errs,
                ETH_TRAFFIC_PORT);
    get_ip_stat(&dst_rx_pkts, &dst_tx_pkts, &dst_rx_errs, &dst_tx_errs,
                ETH_TRAFFIC_DST_PORT);
    if (src_rx_pkts >= prev_src_rx) {
      src_diff = src_rx_pkts - prev_src_rx;
    } else {
      src_diff = (MAX_INT - prev_src_rx) + src_rx_pkts;
    }
    if (dst_rx_pkts >= prev_dst_rx) {
      dst_diff = dst_rx_pkts - prev_dst_rx;
    } else {
      dst_diff = (MAX_INT - prev_dst_rx) + dst_rx_pkts;
    }
    src_throughput = (((float)src_diff) * 8) / (float)(print_period * ONE_MEG);
    dst_throughput = (((float)dst_diff) * 8) / (float)(print_period * ONE_MEG);
    src_average_throughput += src_throughput;
    dst_average_throughput += dst_throughput;
    prev_src_rx_errs = src_rx_errs - prev_src_rx_errs;
    prev_dst_rx_errs = dst_rx_errs - prev_dst_rx_errs;
    prev_src_tx_errs = src_tx_errs - prev_src_tx_errs;
    prev_dst_tx_errs = dst_tx_errs - prev_dst_tx_errs;
    src_total_errs += prev_src_rx_errs + prev_dst_tx_errs;
    dst_total_errs += prev_dst_rx_errs + prev_src_tx_errs;
    ++collected_count;
    if (print_every_period) {
      printf("%s->%s: %3.3f Mb/s (%d:%d), %s->%s: %3.3f Mb/s (%d:%d)\n",
             ETH_TRAFFIC_DST_PORT, ETH_TRAFFIC_PORT, src_throughput, src_diff,
             prev_src_rx_errs + prev_dst_tx_errs, ETH_TRAFFIC_PORT,
             ETH_TRAFFIC_DST_PORT, dst_throughput, dst_diff, prev_dst_rx_errs +
             prev_src_tx_errs);
    }
    prev_src_rx = src_rx_pkts;
    prev_dst_rx = dst_rx_pkts;
    prev_src_rx_errs = src_rx_errs;
    prev_dst_rx_errs = dst_rx_errs;
    prev_src_tx_errs = src_tx_errs;
    prev_dst_tx_errs = dst_tx_errs;
  }

  src_average_throughput = src_average_throughput / ((float) collected_count);
  dst_average_throughput = dst_average_throughput / ((float) collected_count);
  if (src_total_errs > 0 || dst_total_errs > 0) {
    printf("%s %s->%s: %3.3f Mb/s (%d), %s-%s: %3.3f Mb/s (%d)\n",
           FAIL_TEXT, ETH_TRAFFIC_DST_PORT, ETH_TRAFFIC_PORT,
           src_average_throughput, src_total_errs, ETH_TRAFFIC_PORT,
           ETH_TRAFFIC_DST_PORT, dst_average_throughput, dst_total_errs);
  } else {
    printf("%s %s->%s: %3.3f Mb/s (%d), %s-%s: %3.3f Mb/s (%d)\n",
           PASS_TEXT, ETH_TRAFFIC_DST_PORT, ETH_TRAFFIC_PORT,
           src_average_throughput, src_total_errs, ETH_TRAFFIC_PORT,
           ETH_TRAFFIC_DST_PORT, dst_average_throughput, dst_total_errs);
  }

  return 0;
}
