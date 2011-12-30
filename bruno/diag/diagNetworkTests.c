/*
 * Copyright 2011 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics related functions
 * 1. GENET Loopback test
 *    a) Internal loopback
 *    b) External loopback
 */

/*--------------------------------------------------------------------------
 *
 * Includes
 *
 *--------------------------------------------------------------------------
 */

#include "diagdIncludes.h"
#include "diagNetworkTests.h"


/* -------------------------------------------------------------------------
 *
 * Internal defines, types and variables
 *
 *--------------------------------------------------------------------------
 */

int sockfd = 0;             /*Socket descriptor*/
void* buffer = NULL;
uint32_t total_sent_packets = 0;
uint32_t total_recv_packets = 0;
uint32_t total_missed_packets = 0;
bool    iff_promisc = false;

const char *loopbackTestTitle = "Internal Loopback Test:";


/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */

/*
 * To check loopback test result, check network error statistics
 * (exclude link up/down cnts) of the specified network interface.
 *
 * Input:
 * pNetIf_name      - (IN) Starting address of the network interface's name.
 * pbErrorDetected
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * DIAGD_RC_ERR - Can't find the specified network interface
 */
int diag_Check_Log_netStats_LoopbackTest(
  char *pNetIf_name, diag_netIf_info_t **pNetIf, uint8_t *pbErrorDetected)
{
  int                 rtn = DIAGD_RC_OK;
  diag_netIf_info_t  *pLocalNetIf  = NULL;
  diag_netif_stats_t *pDelta;

  DIAGD_TRACE("%s: enter", __func__);

  do {

    *pNetIf = NULL;
    *pbErrorDetected = false;


    /* Get the starting address of the specified network interface. */
    diag_GetStartingAddr_NetIfInfo(pNetIf_name, &pLocalNetIf);
    if (pLocalNetIf == NULL) {
      rtn = DIAGD_RC_ERR;
      break;
    }

    *pNetIf = pLocalNetIf;

    /* Check if there is any error detected in the loopback test */
    pDelta = &pLocalNetIf->delta_stats;

    /* For the time being, if any error occurred, consider loopback failed
     * and log the test result.
     */
    if (pDelta->tx_errors || pDelta->rx_errors ||
        pDelta->rx_crc_errors || pDelta->rx_frame_errors ||
        pDelta->rx_length_errors) {
      /* Loopback test failed*/
      *pbErrorDetected = true;
    }

  } while (false);

  DIAGD_TRACE("%s: exit", __func__);

  return(rtn);

} /* end of diag_Check_Log_netStats_LoopbackTest */



/*
 * Set to the loopback mode per "loopbackType" setting
 *
 * Input:
 * pNetIf_name  - Point to network interface for loopback test
 * loopbackType - DIAG_LOOPBACK_TYPE_INTERNAL or DIAG_LOOPBACK_TYPE_EXTERNAL
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    Failed
 */
static int diag_SetLoopbackMode(char *pNetIf_name, uint8_t loopbackType)
{
  int       rtn = DIAGD_RC_ERR;
  uint16_t  phyRegData;

  do {
    /* Check which net interface to loopback */
    if (strcmp(pNetIf_name, "eth0") == 0) {
      if (loopbackType == DIAG_LOOPBACK_TYPE_INTERNAL) {
        /* It is internal loopback.
         * 1) Read the register.
         */
        if (diagRd_54612_PhyReg(PHY3450_CTRL_REG, &phyRegData) != DIAGD_RC_OK) {
          DIAGD_DEBUG("diagRd_54612_PhyReg() failed.\n");
          break;
        }

        /* 2) Set Disable auto-negotiation and Enable internal Loopback.
         * 3) Write the reg back
         */
        phyRegData = (phyRegData & ~PHY3450_CTRL_AUTO_ENG_EN) |
                     PHY3450_CTRL_I_LOOPBACK_EN;
        if (diagWr_54612_PhyReg(PHY3450_CTRL_REG, &phyRegData) != DIAGD_RC_OK) {
          DIAGD_DEBUG("diagWr_54612_PhyReg() failed.\n");
          break;
        }
      }
    } /* if ("eth0" net interface) */
    else {
      DIAGD_DEBUG("Unsupported net interface (%s)\n", pNetIf_name);
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

  return(rtn);

} /* end of diag_SetLoopbackMode */


/*
 * Reset Phy
 *
 * Input:
 * pNetIf_name  - Point to network interface for loopback test
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    Failed
 */
static int diag_ResetPhy(char *pNetIf_name)
{
  int       rtn = DIAGD_RC_ERR;
  uint16_t  phyRegData;

  do {
    /* Check which net interface to loopback */
    if (strcmp(pNetIf_name, "eth0") == 0) {
      phyRegData = PHY3450_PHY_RESET;
      if (diagWr_54612_PhyReg(PHY3450_CTRL_REG, &phyRegData) != DIAGD_RC_OK) {
          break;
      }
    } /* if ("eth0" net interface) */
    else {
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

  return(rtn);

} /* end of diag_ResetPhy */


/*
 * Cleanup resources
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
void diag_Loopback_Uninit(char *pNetIf_name)
{
  struct ifreq ifr;

  /*Clean up.......*/

  /* Reset the GENET PHY. */
  diag_ResetPhy(pNetIf_name);

  if (sockfd != -1) {
    if (iff_promisc == false) {
      strncpy(ifr.ifr_name, pNetIf_name, IFNAMSIZ);
      ioctl(sockfd, SIOCGIFFLAGS, &ifr);
      if (ifr.ifr_flags & IFF_PROMISC) {
        /* Restore the SIOCGIFFLAGS */
        printf("clean IFF_PROMISC (ifr_flags=%x)\n", ifr.ifr_flags);
        ifr.ifr_flags &= ~IFF_PROMISC;
        ioctl(sockfd, SIOCSIFFLAGS, &ifr);
      }
    }
    close(sockfd);
    sockfd = 0;
  }

  if (buffer != NULL) {
    free(buffer);
    buffer = NULL;
  }

  /* Close the test reuslt log file */
  diagtCloseTestResultsLogFile();

} /* end of diag_Loopback_Uninit */


/*
 * Loopback test handler
 * TODO 20111104 -
 *    "eth0" GENET - DIAG_LOOPBACK_TYPE_EXTERNAL loopback mode is TBD
 *    "eth1" MoCA  - None
 *    "eth2" WLAN  - TBD...
 *
 * Input:
 * pNetIf_name  - Point to network interface for loopback test
 * loopbackType - DIAG_LOOPBACK_TYPE_INTERNAL or DIAG_LOOPBACK_TYPE_EXTERNAL
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    Failed
 */
int diagd_Loopback_Test(char *pNetIf_name, uint8_t loopbackType)
{
  int   rtn = DIAGD_RC_ERR;
  buffer = (void*)malloc(BUF_SIZE);   /*Buffer for ethernet frame*/
  uint16_t *data = buffer + 14;       /*User data in ethernet frame*/
  struct ethhdr *eh = (struct ethhdr *)buffer; /*Another pointer to ethernet header*/
  unsigned char src_mac[6];           /*our MAC address*/
  /* other host MAC address, hardcoded a fake MAC addr...... */
  unsigned char dest_mac[6] = {0x90, 0x00, 0x75, 0xC8, 0x28, 0xE5};
  struct ifreq ifr;
  struct sockaddr_ll socket_address, from;
  int     ifindex = 0;        /*Ethernet Interface index*/
  int     i, k;
  int     recvLen;            /*length of received packet*/
  int     sendLen;            /*length of sent packet*/
  socklen_t           fromLen = sizeof(from);
  netif_netlink_t     netif_linkstate;
  unsigned long       linkStatus;
  /*stuff for time measuring: */
  struct timeval tv;

  do {
    DIAGD_TRACE("Loopback test, init phase...\n");

    if (diagtOpenTestResultsLogFile() != DIAGD_RC_OK) {
      DIAGD_DEBUG("%s: Failed to open the test results log file.", __func__);
    }
    /* Add a log separator line */
    RESULT_LOG_SEPARATOR();


    /* For loopback test, the bridge interface must be shutdown.
     * TODO -
     * For the time being, shutdown br0 w/o any checking.
     * After loopback test, bruno should be reboot.
     */
    system("ifconfig br0 down");

    /* Set to loopback mode */
    if (diag_SetLoopbackMode(pNetIf_name, loopbackType) != DIAGD_RC_OK) {
      DIAGD_DEBUG("diag_SetLoopbackMode() failed.\n");
      break;
    }

    /* After enable loopback mode, setup for loopback test after link is up. */
    /* Setup the input parameter */
    strcpy(netif_linkstate.netif_name, pNetIf_name);
    netif_linkstate.pData = &linkStatus;          /* temp use */

    /* Wait until link up */
    while (true) {
      diag_Get_Netlink_State(&netif_linkstate);
      if (linkStatus == DIAG_NETLINK_UP)
        break;
    }

    DIAGD_TRACE("Client started, entering initialiation phase...\n");

    /*open socket*/
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd == -1) {
      DIAGD_PERROR("socket():");
      break;
    }
    DIAGD_TRACE("Successfully opened socket: %i\n", sockfd);

    tv.tv_sec = 5;    /* 5 Secs Timeout */
    tv.tv_usec = 0;   /* Not init this field can cause strange errors */
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
               (char *)&tv, sizeof(struct timeval));

    /* Retrieve Ethernet interface index*/
    strncpy(ifr.ifr_name, pNetIf_name, IFNAMSIZ);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
      DIAGD_PERROR("SIOCGIFINDEX: ");
      break;
    }
    ifindex = ifr.ifr_ifindex;
    DIAGD_TRACE("Successfully got interface index: %i\n", ifindex);

    /* Retrieve corresponding MAC */
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
      DIAGD_PERROR("SIOCGIFHWADDR: ");
      break;
    }

    /* Check if we are in promiscuous mode */
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1) {
      DIAGD_PERROR("get SIOCGIFFLAGS: ");
      break;
    }

    if ((ifr.ifr_flags & IFF_PROMISC) == 0) {
      /* No, let's enable promiscuous mode */
      ifr.ifr_flags |= IFF_PROMISC;
      if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) == -1) {
        DIAGD_PERROR("set SIOCSIFFLAGS: ");
        break;
      }
    }
    else {
      /* Don't need to restore later */
      iff_promisc = true;
    }

    for (i = 0; i < 6; i++) {
      src_mac[i] = ifr.ifr_hwaddr.sa_data[i];
    }
    DIAGD_TRACE("Got eth0 MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           src_mac[0],src_mac[1],src_mac[2],src_mac[3],src_mac[4],src_mac[5]);

    /* Prepare sockaddr_ll*/
    socket_address.sll_family   = PF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_IP);
    socket_address.sll_ifindex  = ifindex;
    socket_address.sll_hatype   = 0x00;
    socket_address.sll_pkttype  = PACKET_OTHERHOST;
    socket_address.sll_halen    = ETH_ALEN;
    socket_address.sll_addr[0]  = dest_mac[0];
    socket_address.sll_addr[1]  = dest_mac[1];
    socket_address.sll_addr[2]  = dest_mac[2];
    socket_address.sll_addr[3]  = dest_mac[3];
    socket_address.sll_addr[4]  = dest_mac[4];
    socket_address.sll_addr[5]  = dest_mac[5];
    socket_address.sll_addr[6]  = 0x00;
    socket_address.sll_addr[7]  = 0x00;

    /* Clear memory, we  want to get the sll_ifindex returned value */
    memset(&from, 0, sizeof(from));

    /* Init random number generator*/
    srand(time(NULL));

    DIAGD_TRACE("send packets....\n");

    /* Get the statistic counters prior to start loopback tx/rx */
    diag_Get_Netif_Counters(pNetIf_name, false);

    for (k = 0; k < NUMBER_OF_LOOPBACK_PACKETS; k++) {
      /* Prepare buffer*/
      memcpy((void*)buffer, (void*)dest_mac, ETH_MAC_LEN);
      memcpy((void*)(buffer+ETH_MAC_LEN), (void*)src_mac, ETH_MAC_LEN);
      eh->h_proto = ETH_P_NULL;
      /* Fill it with random data....*/
      for (i = 0; i < (LOOPBACK_PKT_SIZE/sizeof(uint16_t)); i++) {
        data[i] = (uint16_t) rand();
      }

      /* send packet */
      sendLen = sendto(sockfd, buffer, (LOOPBACK_PKT_SIZE+ETH_HEADER_LEN),
                       0, (struct sockaddr*)&socket_address,
                       sizeof(socket_address));
      if (sendLen == -1) {
        DIAGD_PERROR("sendto():");
        break;
      }

      total_sent_packets++;

      /* Wait for incoming packet...*/
      errno = 0;
      from.sll_ifindex = 0xFF;
      recvLen = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr*)&from, &fromLen);
      if (recvLen == -1) {
        DIAGD_TRACE("recvfrom(): loop=%d, recvLen=%d, errno=<%d> %s\n",
                    k, recvLen, errno, strerror(errno));
        if (errno == ETIMEDOUT) {
          DIAGD_PERROR("recvfrom(): ");
        }

        total_missed_packets++;
        if (total_missed_packets >= MAX_NUMBER_OF_MISSING_RX_PKTS) {
          break;
        }
      }
      /* Check if the lengths of tx/rx packets are equal */
      else if (from.sll_ifindex == ifindex) {
        if (sendLen == recvLen) {
          total_recv_packets++;
        }
      }
      else {
        /* TBD - Got a packet from other net interface. Ignore for now. */
      }
    }

    /* Get the statistic counters after loopback complete */
    diag_Get_Netif_Counters(pNetIf_name, false);

    diag_netIf_info_t  *pNetIf;
    uint8_t             bErrorDetected;
    /* Check/log loopback test results */
    diag_Check_Log_netStats_LoopbackTest(
          pNetIf_name, &pNetIf, &bErrorDetected);

    /* Assume that diag_Check_Log_netStats_LoopbackTest()
     * won't return error code
     */
    diag_netif_stats_t *pDelta = &pNetIf->delta_stats;

    /* Log loopback test results */
    if ((bErrorDetected == true) || (total_sent_packets != total_recv_packets)) {
      /* Log the test result - FAIL */
      RESULT_TITLE_LOG("%s %s: FAIL", pNetIf_name, loopbackTestTitle);

      /* Log the test result - FAIL reason */
      if (bErrorDetected == true) {
        RESULT_LOG("Cause - Got transmit or receive errors", total_sent_packets);
      }
      else if (total_missed_packets >= MAX_NUMBER_OF_MISSING_RX_PKTS) {
        RESULT_LOG("Cause - Missed %u packets. Aborted the test",
                   total_missed_packets);
      }
      else {
        RESULT_LOG("Cause - Numbers of transmit and receive packets are not matched");
      }
    }
    else {
      /* Log the test result - PASS */
      RESULT_TITLE_LOG("%s %s PASS", pNetIf_name, loopbackTestTitle);
    }

    RESULT_LOG("Total send: %d packets", total_sent_packets);
    RESULT_LOG("Total recv: %d packets", total_recv_packets);

    RESULT_LOG("delta- rx_bytes:%d rx_packets:%d tx_errors:%d",
               pDelta->tx_bytes, pDelta->tx_packets, pDelta->tx_errors);

    RESULT_LOG("delta- tx_bytes:%d tx_packets:%d rx_errors:%d rx_crc_errors:%d"
               " rx_frame_errors:%d rx_length_errors:%d",
               pDelta->rx_bytes, pDelta->rx_packets, pDelta->rx_errors,
               pDelta->rx_crc_errors, pDelta->rx_frame_errors,
               pDelta->rx_length_errors);

    rtn = DIAGD_RC_OK;

  } while (false);

  if (rtn != DIAGD_RC_OK) {
    /* Log the test result - FAIL TO RUN and its reason */
    RESULT_TITLE_LOG("%s %s FAIL TO RUN", pNetIf_name, loopbackTestTitle);
    RESULT_LOG("Unable to change %s configuration to run the test",
               pNetIf_name);
  }

  diag_Loopback_Uninit(pNetIf_name);

  return(rtn);

} /* end of diagd_Loopback_test */

