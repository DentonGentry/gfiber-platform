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
#include "../common/io.h"
#include "common.h"

#if USE_WAN0
#define MY_DEST_MAC0 0xc8
#define MY_DEST_MAC1 0xb3
#define MY_DEST_MAC2 0x73
#define MY_DEST_MAC3 0x36
#define MY_DEST_MAC4 0x00
#define MY_DEST_MAC5 0x9d
#else
#define MY_DEST_MAC0 0x33
#define MY_DEST_MAC1 0x33
#define MY_DEST_MAC2 0x00
#define MY_DEST_MAC3 0x00
#define MY_DEST_MAC4 0x00
#define MY_DEST_MAC5 0x01
#endif

#define DEFAULT_TST_IF "wlan0"
#define LAN_PORT_NAME "lan0"
#define WAN_PORT_NAME "wan0"
#define BUF_SIZ 1514
#define GE_TEST_MAX_CMD 4096
#define GE_TEST_MAX_RSP 4096
#define GE_TRAFFIC_PORT "lan0"
#define GE_TRAFFIC_DST_PORT "wan0"
#define GE_TRAFFIC_REPORT_PERIOD 10
#define GE_TRAFFIC_TEST_PERIOD_SYMBOL "-p"

#define SERVER_PORT 8888
#define MAX_CMD_SIZE 256

#define GE_SEND_DELAY_IN_USEC 1000
#define GE_MAX_LAN_PORTS 4
#define GE_WAIT_AFTER_LOOPBACK_SET 5
#define GE_PKTS_SENT_BEFORE_WAIT 0xFF
#define GE_PKTS_LEN_DEFAULT 32
#define GE_BUFFER_SIZE (GE_PKTS_SENT_BEFORE_WAIT * GE_PKTS_LEN_DEFAULT)
#define GE_LOOPBACK_PASS_FACTOR 0.8  // 80%

/* Max MII register/address (we support) */
#define MII_REGISTER_MAX 31
#define MII_ADDRESS_MAX 31
#define MDIO_TIMEOUT 5000
#define PHY_MAN_BASE 0x9c200000
#define EMAC_PHY_MANAGEMENT 0x34
#define EMAC_NETWORK_STATUS 0x8
#define PHY_MAN_READ_BASE 0x60020000
#define PHY_MAN_WRITE_BASE 0x50020000
#define PHY_ADDR_MASK 0x1f
#define PHY_ADDR_POS 23
#define PHY_REG_POS 18
#define PHY_DATA_MASK 0xffff
#define EMAC_PHY_IDLE (1 << 2)
#define SPACECAST_PHY_ADDR 1

#define M88E1512_PHY_PAGE_REG 22
#define M88E1512_PHY_DEFAULT_PAGE 0
#define M88E1512_PHY_PAGE_6 6
#define M88E1512_PHY_CHECKER_CTRL_REG 18
#define M88E1512_PHY_ENABLE_STUB_TEST_BIT 3

/********************************************************************
 * gem_phy_man_rd :
 *      Performs phy management read operation.
 *******************************************************************/
static void gem_phy_man_rd(unsigned int phy_addr, unsigned int phy_reg) {
  unsigned int write_data;

  write_data =
      PHY_MAN_READ_BASE |
      ((phy_addr & (unsigned int)PHY_ADDR_MASK) << PHY_ADDR_POS) |
      ((phy_reg & (unsigned int)PHY_ADDR_MASK) << PHY_REG_POS);  // read_op
  write_physical_addr(PHY_MAN_BASE + EMAC_PHY_MANAGEMENT, write_data);
}

static void gem_phy_man_wr(unsigned int phy_addr, unsigned int phy_reg,
                          unsigned int val) {
  unsigned int write_data;

  write_data = PHY_MAN_WRITE_BASE |
               ((phy_addr & (unsigned int)PHY_ADDR_MASK) << PHY_ADDR_POS) |
               ((phy_reg & (unsigned int)PHY_ADDR_MASK) << PHY_REG_POS) |
               (val & (unsigned int)PHY_DATA_MASK);  // write_op
  write_physical_addr(PHY_MAN_BASE + EMAC_PHY_MANAGEMENT, write_data);
}

/** gem_phy_man_data
 *    Read the data section of phy management register.
 *    After a successful read opeeration the data will be stored in
 *    in this register in lower 16bits.
 */
static unsigned int gem_phy_man_data() {
  unsigned int value;

  read_physical_addr(PHY_MAN_BASE + EMAC_PHY_MANAGEMENT, &value);
  value &= PHY_DATA_MASK;
  return value;
}

static int gem_phy_man_idle() {
  unsigned int value;

  read_physical_addr(PHY_MAN_BASE + EMAC_NETWORK_STATUS, &value);
  return ((value & EMAC_PHY_IDLE) == EMAC_PHY_IDLE);
}

static int gem_phy_timeout(int timeout) {
  while (!gem_phy_man_idle()) {
    if (timeout-- <= 0) {
      printf("Phy MDIO read/write timeout\n");
      return -1;
    }
  }
  return 0;
}

/** PHY read function
 * Reads a 16bit value from a MII register
 *
 * @param[in] mdev        Pointer to MII device structure
 * @param[in] phy_addr
 * @param[in] phy_reg
 *
 * @return  16bit value on success, a negative value (-1) on error
 */
static int c2000_phy_read(int phy_addr, int phy_reg) {
  int value;

  if ((phy_addr > MII_ADDRESS_MAX) || (phy_reg > MII_REGISTER_MAX)) return -1;

  gem_phy_man_rd(phy_addr, phy_reg);
  if (gem_phy_timeout(MDIO_TIMEOUT)) return -1;

  value = gem_phy_man_data();

  return value;
}

/** PHY write function
 * Writes a 16bit value to a MII register
 *
 * @param[in] mdev      Pointer to MII device structure
 * @param[in] phy_addr
 * @param[in] phy_reg
 * @param[in] value     Value to be written to Phy
 *
 * @return              On success returns 0, a negative value (-1) on error
 */
static int c2000_phy_write(int phy_addr, int phy_reg, int value) {
  if ((phy_addr > MII_ADDRESS_MAX) || (phy_reg > MII_REGISTER_MAX)) return -1;

  gem_phy_man_wr(phy_addr, phy_reg, value);
  if (gem_phy_timeout(MDIO_TIMEOUT)) return -1;

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
        if ((i & GE_PKTS_SENT_BEFORE_WAIT) == 0) {
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
        if ((i & GE_PKTS_SENT_BEFORE_WAIT) == 0) {
          usleep(xfer_wait);
        }
      }
    }
  }
  close(sockfd);
}

static void phy_read_usage(void) {
  printf("phy_read <register>\n");
  printf("Example:\n");
  printf("phy_read 22\n");
  printf("read PHY register 22\n");
}

int phy_read(int argc, char *argv[]) {
  int reg, data;

  if (argc != 2) {
    phy_read_usage();
    return -1;
  }
  reg = strtol(argv[1], NULL, 10);
  data = c2000_phy_read(SPACECAST_PHY_ADDR, reg);
  printf("Reg %d: 0x%x\n", reg, data);
  return 0;
}

static void phy_write_usage(void) {
  printf("phy_write <register> <data>\n");
  printf("Example:\n");
  printf("phy_write 22 2\n");
  printf("write 2 to PHY register 22\n");
}

int phy_write(int argc, char *argv[]) {
  int reg, data;

  if (argc != 3) {
    phy_write_usage();
    return -1;
  }
  reg = strtol(argv[1], NULL, 10);
  data = strtoul(argv[2], NULL, 16);
  c2000_phy_write(SPACECAST_PHY_ADDR, reg, data);
  printf("Write PHY Reg %d: 0x%x\n", reg, data);
  return 0;
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

int setup_loopback(int port) {
  char command[GE_TEST_MAX_CMD];
  FILE *fp;

  // ssdk_sh hardcoded wan0 to be up in order to work
  system_cmd("ifup wan0");
  // printf("Set up loop back on PHY %d\n", port);
  sprintf(command, "ssdk_sh >> %s\n", TMP_FILE);
  fp = popen(command, "w");
  fprintf(fp, "debug phy set %d 0x1d 0xb\n", port);
  fprintf(fp, "debug phy set %d 0x1e 0x3c40\n", port);
  fprintf(fp, "debug phy set %d 0x1d 0x11\n", port);
  fprintf(fp, "debug phy set %d 0x1e 0x7553\n", port);
  fprintf(fp, "debug phy set %d 0x10 0x0800\n", port);
  fprintf(fp, "debug phy set %d 0x0 0x8140\n", port);
  if (port == 4) {
    fprintf(fp, "debug phy set %d 0x0 0x0140\n", port);
  }
  fprintf(fp, "quit\n");
  pclose(fp);
  system_cmd("ifdown wan0");

  return 0;
}

int take_down_loopback(int port) {
  char command[GE_TEST_MAX_CMD];
  FILE *fp;

  system_cmd("ifup wan0");
  sprintf(command, "ssdk_sh >> %s\n", TMP_FILE);
  fp = popen(command, "w");
  fprintf(fp, "debug phy set %d 0x1d 0xb\n", port);
  fprintf(fp, "debug phy set %d 0x1e 0xbc40\n", port);
  fprintf(fp, "debug phy set %d 0x1d 0x11\n", port);
  fprintf(fp, "debug phy set %d 0x1e 0x7552\n", port);
  fprintf(fp, "debug phy set %d 0x10 0x0862\n", port);
  fprintf(fp, "debug phy set %d 0x0 0x1000\n", port);
  fprintf(fp, "quit\n");
  pclose(fp);
  system_cmd("ifdown wan0");
  // printf("Take down loop back on PHY %d\n",port);

  return 0;
}

int atheros_drv_init() {
  char command[GE_TEST_MAX_CMD];
  FILE *fp;

  system_cmd("ifup wan0");
  sprintf(command, "ssdk_sh >> %s\n", TMP_FILE);
  fp = popen(command, "w");
  fprintf(fp, "debug reg set 0x0624 0x3f3f3f 4\n");
  fprintf(fp, "debug reg set 0x0004 0x6800000 4\n");
  fprintf(fp, "debug reg set 0x0008 0x1000000 4\n");
  fprintf(fp, "debug reg set 0x000c 0x20000 4\n");
  fprintf(fp, "debug reg set 0x0090 0 4\n");
  fprintf(fp, "debug reg set 0x0094 0 4\n");
  fprintf(fp, "debug reg set 0x007c 0xfe 4\n");
  fprintf(fp, "debug reg get 0x0 4\n");

  fprintf(fp, "debug phy set 4 0x1d 0x0\n");
  fprintf(fp, "debug phy get 4 0x1e\n");
  fprintf(fp, "debug phy set 4 0x1d 0x0\n");
  fprintf(fp, "debug phy set 4 0x1e 0x82ee\n");
  fprintf(fp, "debug phy set 4 0x1d 0x5\n");
  fprintf(fp, "debug phy get 4 0x1e\n");
  fprintf(fp, "debug phy set 4 0x1d 0x5\n");
  fprintf(fp, "debug phy set 4 0x1e 0x1d47\n");

  fprintf(fp, "debug reg set 0x0970 0x2a666666 4\n");
  fprintf(fp, "debug reg set 0x0974 0xc6 4\n");
  fprintf(fp, "debug reg set 0x0978 0x2a008888 4\n");
  fprintf(fp, "debug reg set 0x097c 0xc6 4\n");
  fprintf(fp, "debug reg set 0x0980 0x2a008888 4\n");
  fprintf(fp, "debug reg set 0x0984 0xc6 4\n");
  fprintf(fp, "debug reg set 0x0988 0x2a008888 4\n");
  fprintf(fp, "debug reg set 0x098C 0xc6 4\n");
  fprintf(fp, "debug reg set 0x0990 0x2a008888 4\n");
  fprintf(fp, "debug reg set 0x0994 0xc6 4\n");
  fprintf(fp, "debug reg set 0x0998 0x2a666666 4\n");
  fprintf(fp, "debug reg set 0x099C 0xc6 4\n");
  fprintf(fp, "debug reg set 0x09a0 0x2a666666 4\n");
  fprintf(fp, "debug reg set 0x09a4 0xc6 4\n");
  fprintf(fp, "debug reg set 0x0050 0xffb7ffb7 4\n");
  fprintf(fp, "debug reg set 0x0054 0xffb7ffb7 4\n");
  fprintf(fp, "debug reg set 0x0058 0xffb7ffb7 4\n");
  fprintf(fp, "quit\n");
  pclose(fp);
  system_cmd("ifdown wan0");

  return 0;
}

int atheros_phy_init() {
  char command[GE_TEST_MAX_CMD];
  FILE *fp;

  system_cmd("ifup wan0");
  sprintf(command, "ssdk_sh >> %s\n", TMP_FILE);
  fp = popen(command, "w");
  // PHY 0
  fprintf(fp, "debug phy set 0 4 0xDE0\n");
  fprintf(fp, "debug phy set 0 9 0x0200\n");
  fprintf(fp, "debug phy set 0 0 0x9000\n");
  fprintf(fp, "debug phy get 0 0\n");
  fprintf(fp, "debug phy set 0 13 3\n");
  fprintf(fp, "debug phy set 0 14 0x8007\n");
  fprintf(fp, "debug phy set 0 13 0x4003\n");
  fprintf(fp, "debug phy set 0 14 0x8315\n");
  fprintf(fp, "debug phy set 0 13 3\n");
  fprintf(fp, "debug phy set 0 14 0x800d\n");
  fprintf(fp, "debug phy set 0 13 0x4003\n");
  fprintf(fp, "debug phy set 0 14 0x103f\n");
  fprintf(fp, "debug phy set 0 0x1d 0x3d\n");
  fprintf(fp, "debug phy set 0 0x1e 0x6860\n");
  // PHY 1
  fprintf(fp, "debug phy set 1 4 0xDE0\n");
  fprintf(fp, "debug phy set 1 9 0x0200\n");
  fprintf(fp, "debug phy set 1 0 0x9000\n");
  fprintf(fp, "debug phy get 1 0\n");
  fprintf(fp, "debug phy set 1 13 3\n");
  fprintf(fp, "debug phy set 1 14 0x8007\n");
  fprintf(fp, "debug phy set 1 13 0x4003\n");
  fprintf(fp, "debug phy set 1 14 0x8315\n");
  fprintf(fp, "debug phy set 1 13 3\n");
  fprintf(fp, "debug phy set 1 14 0x800d\n");
  fprintf(fp, "debug phy set 1 13 0x4003\n");
  fprintf(fp, "debug phy set 1 14 0x103f\n");
  fprintf(fp, "debug phy set 1 0x1d 0x3d\n");
  fprintf(fp, "debug phy set 1 0x1e 0x6860\n");
  // PHY 2
  fprintf(fp, "debug phy set 2 4 0xDE0\n");
  fprintf(fp, "debug phy set 2 9 0x0200\n");
  fprintf(fp, "debug phy set 2 0 0x9000\n");
  fprintf(fp, "debug phy get 2 0\n");
  fprintf(fp, "debug phy set 2 13 3\n");
  fprintf(fp, "debug phy set 2 14 0x800d\n");
  fprintf(fp, "debug phy set 2 13 0x4003\n");
  fprintf(fp, "debug phy set 2 14 0x103f\n");
  fprintf(fp, "debug phy set 2 0x1d 0x3d\n");
  fprintf(fp, "debug phy set 2 0x1e 0x6860\n");
  // PHY 3
  fprintf(fp, "debug phy set 3 4 0xDE0\n");
  fprintf(fp, "debug phy set 3 9 0x0200\n");
  fprintf(fp, "debug phy set 3 0 0x9000\n");
  fprintf(fp, "debug phy get 3 0\n");
  fprintf(fp, "debug phy set 3 13 3\n");
  fprintf(fp, "debug phy set 3 14 0x800d\n");
  fprintf(fp, "debug phy set 3 13 0x4003\n");
  fprintf(fp, "debug phy set 3 14 0x103f\n");
  fprintf(fp, "debug phy set 3 0x1d 0x3d\n");
  fprintf(fp, "debug phy set 3 0x1e 0x6860\n");

  // PHY 4
  fprintf(fp, "debug phy set 4 4 0xDE0\n");
  fprintf(fp, "debug phy set 4 9 0x0200\n");
  fprintf(fp, "debug phy set 4 0 0x9000\n");
  fprintf(fp, "debug phy get 4 0\n");
  fprintf(fp, "debug phy set 4 13 3\n");
  fprintf(fp, "debug phy set 4 14 0x800d\n");
  fprintf(fp, "debug phy set 4 13 0x4003\n");
  fprintf(fp, "debug phy set 4 14 0x103f\n");
  fprintf(fp, "debug phy set 4 0x1d 0x3d\n");
  fprintf(fp, "debug phy set 4 0x1e 0x6860\n");

  fprintf(fp, "debug phy set 4 0x1d 0x12\n");
  fprintf(fp, "debug phy set 4 0x1e 0x4c0c\n");
  fprintf(fp, "debug phy set 4 0x1d 0x0\n");
  fprintf(fp, "debug phy set 4 0x1e 0x82ee\n");
  fprintf(fp, "debug phy set 4 0x1d 0x5\n");
  fprintf(fp, "debug phy set 4 0x1e 0x3d46\n");
  fprintf(fp, "debug phy set 4 0x1d 0xb\n");
  fprintf(fp, "debug phy set 4 0x1e 0xbc20\n");

  fprintf(fp, "quit\n");
  pclose(fp);
  system_cmd("ifdown wan0");
  // printf("Take down loop back on PHY %d\n",port);

  return 0;
}

static void atheros_init_usage(void) {
  printf("atheros_init\n");
  printf("Example:\n");
  printf("atheros_init\n");
  printf("initialize atheros chipset\n");
}

int atheros_init(int argc, char *argv[]) {
  if ((argc != 1) || (argv[0] == NULL)) {
    atheros_init_usage();
    return -1;
  }
  return atheros_drv_init();
}

static void phy_init_usage(void) {
  printf("phy_init\n");
  printf("Example:\n");
  printf("phy_init\n");
  printf("initialize all PHY port (0 to 4)\n");
}

int phy_init(int argc, char *argv[]) {
  if ((argc != 1) || (argv[0] == NULL)) {
    phy_init_usage();
    return -1;
  }
  return atheros_phy_init();
}

int get_ip_stat(int *rx_pkts, int *tx_pkts, int *rx_errs, int *tx_errs,
                char *name) {
  char command[4096], rsp[MAX_CMD_SIZE];
  FILE *fp;
  unsigned int j;

  strcpy(command, "ifstat ");
  strcat(command, name);
  fp = popen(command, "r");
  while (fscanf(fp, "%s", rsp) != EOF) {
    if (!strncmp(rsp, name, strlen(name))) {
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      sscanf(rsp, "%d", rx_pkts);
      for (j = 0; j < strlen(rsp); ++j) {
        if (rsp[j] == 'K') {
          *rx_pkts *= 1000;
          break;
        }
      }
      // RX Pkts rate
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      // TX
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      sscanf(rsp, "%d", tx_pkts);
      for (j = 0; j < strlen(rsp); ++j) {
        if (rsp[j] == 'K') {
          *tx_pkts *= 1000;
          break;
        }
      }
      // TX Pkts rate
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      // RX Data
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      // RX Data Rate
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      // TX Data
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      // TX Data Rate
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      // RX Errs
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      sscanf(rsp, "%d", rx_errs);
      // RX Drop
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      // TX Errs
      if (fscanf(fp, "%s", rsp) <= 0) return -1;
      sscanf(rsp, "%d", tx_errs);
      break;
    }
  }
  pclose(fp);

  return 0;
}

int get_if_ip(char *name, unsigned int *ip) {
  char command[GE_TEST_MAX_CMD], rsp[GE_TEST_MAX_RSP];
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

static void send_eth_usage(void) {
  printf(
      "send_eth <source if> <dest if> <num>"
      " [-t <delay between pkts send>]\n");
  printf("Example:\n");
  printf("send_eth lan0 wan0 100\n");
  printf("send 100 msg from lan0 to wan0\n");
}

int send_eth(int argc, char *argv[]) {
  int n;
  char if_name[IFNAMSIZ];
  char out_name[IFNAMSIZ];
  unsigned char dst_mac[6] = {0, 0, 0, 0, 0, 0};
  unsigned int xfer_wait = GE_SEND_DELAY_IN_USEC;

  /* Get interface name */
  if (argc == 6) {
    if (strcmp(argv[4], "-t") == 0) {
      xfer_wait = strtoul(argv[5], NULL, 10);
    } else {
      send_eth_usage();
      return -1;
    }
  } else if (argc != 4) {
    send_eth_usage();
    return -1;
  }

  strcpy(if_name, argv[1]);
  strcpy(out_name, argv[2]);
  n = strtol(argv[3], NULL, 10);

  send_mac_pkt(if_name, out_name, BUF_SIZ, xfer_wait, n, NULL);

  printf("Sent %d pkt of size %d from %s to %02x:%02x:%02x:%02x:%02x:%02x\n", n,
         BUF_SIZ, argv[1], dst_mac[0], dst_mac[1], dst_mac[2], dst_mac[3],
         dst_mac[4], dst_mac[5]);

  return 0;
}

static void geloopback_usage(void) {
  printf(
      "geloopback <PHY ports in bit mask (hex)> <num> "
      "[-b <pkt byte size (max %d)>] "
      "[-t <time delay in micro-second between pkt send>]\n",
      BUF_SIZ);
  printf("Example:\n");
  printf("geloopback 0x1F 100\n");
  printf("loopback PHY port 0, 1, 2, 3 ,4 with 100 msgs\n");
  printf("geloopback 0xF 100 -b 256 -t 250\n");
  printf(
      "loopback PHY port 0, 1, 2, 3 with 100 msgs of size 256 bytes "
      "and 250 us delay\n");
}

int geloopback(int argc, char *argv[]) {
  int n, port, pass_num, j;
  unsigned int xfer_len = GE_PKTS_LEN_DEFAULT, xfer_wait = 250, rx_wait = 2;
  char if_name[IFNAMSIZ];
  char out_name[IFNAMSIZ];
  // char cmd[MAX_CMD_SIZE];
  int rx_pkts, tx_pkts, rx_errs, tx_errs;
  unsigned int portMask;
  static const int kLoopbackRetries = 3;

  /* Get interface name */
  if ((argc != 3) && (argc != 5) && (argc != 7)) {
    printf("%s invalid params\n", FAIL_TEXT);
    geloopback_usage();
    return -1;
  }

#if 0
  // Increase the buffer size
  sprintf(cmd, "echo %d > /proc/sys/net/core/rmem_max", GE_BUFFER_SIZE);
  system_cmd(cmd);
  sprintf(cmd, "echo %d > /proc/sys/net/core/rmem_default", GE_BUFFER_SIZE);
  system_cmd(cmd);
  sprintf(cmd, "echo %d > /proc/sys/net/core/wmem_max", GE_BUFFER_SIZE);
  system_cmd(cmd);
  sprintf(cmd, "echo %d > /proc/sys/net/core/wmem_default", GE_BUFFER_SIZE);
  system_cmd(cmd);
#endif

  portMask = strtoul(argv[1], NULL, 16);
  n = strtoul(argv[2], NULL, 10);
  pass_num = (int)(n * GE_LOOPBACK_PASS_FACTOR);

  if (argc >= 5) {
    unsigned int tmp;
    tmp = strtoul(argv[4], NULL, 10);
    if (strcmp(argv[3], "-b") == 0) {
      if (tmp > BUF_SIZ) {
        printf("%s invalid params\n", FAIL_TEXT);
        geloopback_usage();
        return -1;
      }
      xfer_len = tmp;

      if (argc == 7) {
        if (strcmp(argv[5], "-t") == 0) {
          xfer_wait = strtoul(argv[6], NULL, 10);
        } else {
          printf("%s invalid params\n", FAIL_TEXT);
          geloopback_usage();
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
            geloopback_usage();
            return -1;
          }
        } else {
          printf("%s invalid params\n", FAIL_TEXT);
          geloopback_usage();
          return -1;
        }
      }
    } else {
      printf("%s invalid params\n", FAIL_TEXT);
      geloopback_usage();
      return -1;
    }
  }

  strcpy(if_name, LAN_PORT_NAME);
  strcpy(out_name, LAN_PORT_NAME);

  printf("Sending %d packets of size %d delay %d\n", n, xfer_len, xfer_wait);

  for (port = 0; port < GE_MAX_LAN_PORTS; ++port) {
    if (portMask & (1 << port)) {
      for (j = 0; j < kLoopbackRetries; ++j) {
        setup_loopback(port);
        sleep(GE_WAIT_AFTER_LOOPBACK_SET);
        rx_pkts = 0;
        get_ip_stat(&rx_pkts, &tx_pkts, &rx_errs, &tx_errs, LAN_PORT_NAME);
        system_cmd("uptime");

        send_mac_pkt(if_name, out_name, xfer_len, xfer_wait, n, NULL);
        system_cmd("uptime");
        rx_pkts = 0;
        sleep(rx_wait);
        get_ip_stat(&rx_pkts, &tx_pkts, &rx_errs, &tx_errs, LAN_PORT_NAME);
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

static void send_if_to_mac_usage(void) {
  printf(
      "send_if_to_mac <if> <dest MAC> <num> [-b <pkt byte size (max %d)>] "
      "[-t <time delay in micro-second between pkt send>]\n",
      BUF_SIZ);
  printf("Example:\n");
  printf("send_if_to_mac moca0 f8:8f:ca:00:16:04 100\n");
  printf("send 100 msg from interface moca0 to f8:8f:ca:00:16:04\n");
  printf("send_if_to_mac moca0 f8:8f:ca:00:16:04 100 -b 256 -t 250\n");
  printf(
      "send to interface moca0 with 100 msgs of size 256 bytes and 250 us "
      "delay\n");
}

int send_if_to_mac(int argc, char *argv[]) {
  int n;
  unsigned int xfer_len = 16, xfer_wait = 0;
  char if_name[IFNAMSIZ];
  unsigned char dst_mac[6] = {0, 0, 0, 0, 0, 0};

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

/* This is for Marvell 88E1512 only */
int lan_lpbk(int argc, char *argv[]) {
  int data;
  bool loopback_on = false;

  if (argc != 2) {
    lan_lpbk_usage();
    return -1;
  }

  if (strcmp(argv[1], "on") == 0) {
    loopback_on = true;
  } else if (strcmp(argv[1], "off") == 0) {
    loopback_on = false;
  } else {
    lan_lpbk_usage();
    return -1;
  }
  if (c2000_phy_write(SPACECAST_PHY_ADDR, M88E1512_PHY_PAGE_REG,
                      M88E1512_PHY_PAGE_6) < 0) {
    printf("PHY write to reg %d of data 0x%x failed\n", M88E1512_PHY_PAGE_REG,
           M88E1512_PHY_PAGE_6);
    return -1;
  }
  data = c2000_phy_read(SPACECAST_PHY_ADDR, M88E1512_PHY_CHECKER_CTRL_REG);
  if (loopback_on) {
    data |= (1 << M88E1512_PHY_ENABLE_STUB_TEST_BIT);
    printf("Ethernet port external loopback enabled\n");
  } else {
    data &= ~(1 << M88E1512_PHY_ENABLE_STUB_TEST_BIT);
    printf("Ethernet port external loopback disabled\n");
  }
  if (c2000_phy_write(SPACECAST_PHY_ADDR, M88E1512_PHY_CHECKER_CTRL_REG, data) <
      0) {
    printf("PHY write to reg %d of data 0x%x failed\n",
           M88E1512_PHY_CHECKER_CTRL_REG, data);
    return -1;
  }
  if (c2000_phy_write(SPACECAST_PHY_ADDR, M88E1512_PHY_PAGE_REG,
                      M88E1512_PHY_DEFAULT_PAGE) < 0) {
    printf("PHY write to reg %d of data 0x%x failed\n", M88E1512_PHY_PAGE_REG,
           M88E1512_PHY_DEFAULT_PAGE);
    return -1;
  }
  return 0;
}

/* This is for QCA switch */
int qca_lan_lpbk(int argc, char *argv[]) {
  bool loopback_on = false;

  if (argc != 2) {
    lan_lpbk_usage();
    return -1;
  }

  if (strcmp(argv[1], "on") == 0) {
    loopback_on = true;
  } else if (strcmp(argv[1], "off") == 0) {
    loopback_on = false;
  } else {
    lan_lpbk_usage();
    return -1;
  }
  system_cmd("ifup wan0");
  if (loopback_on) {
    system_cmd("ssdk_sh debug reg set 0x660 0x34007e 4");
    system_cmd("ssdk_sh debug reg set 0x66C 0x34007e 4");
    system_cmd("ssdk_sh debug reg set 0x678 0x34007e 4");
    system_cmd("ssdk_sh debug reg set 0x684 0x34007e 4");
    printf("All lan ports looped back to external\n");
  } else {
    system_cmd("ssdk_sh debug reg set 0x660 0x14007e 4");
    system_cmd("ssdk_sh debug reg set 0x66C 0x14007e 4");
    system_cmd("ssdk_sh debug reg set 0x678 0x14007e 4");
    system_cmd("ssdk_sh debug reg set 0x684 0x14007e 4");
    printf("All lan ports loopback turned off\n");
  }
  system_cmd("ifdown wan0");
  return 0;
}

static void set_lan_snake_usage(void) {
  printf("set_lan_snake\n");
  printf("Example:\n");
  printf("set_lan_snake\n");
  printf("Traffic generator -> P1; and P1/2 are the same VLAN.\n");
  printf("P2 is connected to P3 via cable\n");
  printf("P3/4 are the same VLAN. P4 -> traffic receiver\n");
}

int set_lan_snake(int argc, char *argv[]) {
  if ((argc != 1) || (argv[0] == NULL)) {
    set_lan_snake_usage();
    return -1;
  }
  system_cmd("ifup wan0");
  // Take the LAN ports out of the default LAN VLAN 0
  system_cmd("ssdk_sh portVlan member del 0 1 > /tmp/t");
  system_cmd("ssdk_sh portVlan member del 0 2 > /tmp/t");
  system_cmd("ssdk_sh portVlan member del 0 3 > /tmp/t");
  system_cmd("ssdk_sh portVlan member del 0 4 > /tmp/t");
  // Assign new VLAN ID
  system_cmd("ssdk_sh portVlan member update 1 0x4 > /tmp/t");
  system_cmd("ssdk_sh portVlan member update 2 0x2  > /tmp/t");
  system_cmd("ssdk_sh portVlan member update 3 0x10 > /tmp/t");
  system_cmd("ssdk_sh portVlan member update 4 0x8 > /tmp/t");
  system_cmd("ssdk_sh portVlan defaultCVid set 1 1 > /tmp/t");
  system_cmd("ssdk_sh portVlan defaultCVid set 2 1 > /tmp/t");
  system_cmd("ssdk_sh portVlan defaultCVid set 3 2 > /tmp/t");
  system_cmd("ssdk_sh portVlan defaultCVid set 4 2 > /tmp/t");
  system_cmd("ifdown wan0");
  return 0;
}

static void ge_traffic_usage(void) {
  printf("ge_traffic <test duration> [<%s print period>]\n",
         GE_TRAFFIC_TEST_PERIOD_SYMBOL);
  printf("- duration >=1 or -1 (forever)\n");
  printf("- traffic sent from lan0 to wan0\n");
  printf("- print period > 0\n");
}

int ge_traffic(int argc, char *argv[]) {
  char cmd[MAX_CMD_SIZE];
  int duration, num = -1;
  int pid, pid1, pid2;
  int printPeriod = GE_TRAFFIC_REPORT_PERIOD;
  static const unsigned char dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  if ((argc != 2) && (argc != 4)) {
    ge_traffic_usage();
    return -1;
  }

  duration = strtol(argv[1], NULL, 0);
  if ((duration < -1) || (duration == 0)) {
    ge_traffic_usage();
    return -1;
  }

  if (argc == 4) {
    if (strcmp(argv[2], GE_TRAFFIC_TEST_PERIOD_SYMBOL) != 0) {
      ge_traffic_usage();
      return -1;
    }

    printPeriod = strtoul(argv[3], NULL, 0);
    if (printPeriod == 0) {
      ge_traffic_usage();
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
    send_mac_pkt(GE_TRAFFIC_PORT, NULL, BUF_SIZ, 0, num, dst_mac);
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
    send_mac_pkt(GE_TRAFFIC_PORT, NULL, BUF_SIZ, 0, num, dst_mac);
    exit(0);
  }
  // Parent process
  pid2 = pid;

  while (duration != 0) {
    if (duration >= 0) {
      if (duration <= printPeriod) {
        sleep(duration);
        duration = 0;
        kill(pid1, SIGKILL);
        kill(pid2, SIGKILL);
        // printf("Killed processes %d and %d\n", pid1, pid2);
      } else {
        duration -= printPeriod;
        sleep(printPeriod);
      }
    } else {
      sleep(printPeriod);
    }
    sprintf(cmd, "ifstat %s %s", GE_TRAFFIC_PORT, GE_TRAFFIC_DST_PORT);
    system_cmd(cmd);
  }

  return 0;
}
