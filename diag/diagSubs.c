/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics monitoring related functions
 *
 */

/*--------------------------------------------------------------------------
 *
 * Includes
 *
 *--------------------------------------------------------------------------
 */

#include "diagdIncludes.h"


/*--------------------------------------------------------------------------
 *
 * Global variables
 *
 *--------------------------------------------------------------------------
 */

diag_info_t   diag_info;
diag_info_t  *pDiagInfo = &diag_info;


/* -------------------------------------------------------------------------
 *
 * Internal defines, types and variables
 *
 *--------------------------------------------------------------------------
 */

/*
 * Network statistics counter names.
 * The names in the array are used to query counter under "/sys/class/net/'netif_name'/statistics/'counter_name'".
 * NOTES -
 *  1. The sequence of names must be sync with the sequence of structure diag_netif_stats_t
 *      2. If not sync, either failed to access the counter or get a incorrect counter value.
 */

static const char diag_netif_stats_cnt_names[DIAG_NET_CNTS][NETIF_STATS_NAME_MAX_LEN] =
{
  "rx_bytes\0",
  "rx_packets\0",
  "rx_errors\0",
  "rx_crc_errors\0",
  "rx_frame_errors\0",
  "rx_length_errors\0",
  "tx_bytes\0",
  "tx_packets\0",
  "tx_errors\0",
};

/* Diag LED control table.
 * The order of the entries should be same
 * as the ones within enum diag_led_indicator defined
 * in include/diagSubs.h
 */

diag_led_table_t diagLedTbl[DIAG_LED_IND_MAX] = {
  {"SOLIDRED", SOLID_RED},
  {"SOLIDBLUE", SOLID_BLUE},
  {"BLINKRED", BLINK_RED},
  {"BLINKBLUE", BLINK_BLUE},
  {"FLASHRED", FLASH_RED},
  {"FLASHBLUE", FLASH_BLUE},
  {"FASTFLASHRED", FAST_FLASH_RED},
  {"FASTFLASHBLUE", FAST_FLASH_BLUE}
};

/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */

/* -------------------------------------------------------------------------
 * Host command handling related routines
 *--------------------------------------------------------------------------
 */

/*
 * Close the specified socket
 *
 * Input:
 * pSock -  Pointer of socket to be closed
 *
 * Output:
 * None
 */
void diag_CloseSocket(int *pSock)
{
  if (*pSock != DIAG_SOCKET_NOT_OPEN) {
    close(*pSock);
    *pSock = DIAG_SOCKET_NOT_OPEN;
  }
} /* end of diag_CloseSocket */


/*
 * Close the specified file descriptor
 *
 * Input:
 * pSock -  Pointer of socket to be closed
 *
 * Output:
 * None
 */
void diag_CloseFileDesc(int *pFd)
{
  if (*pFd != DIAG_FD_NOT_OPEN) {
    close(*pFd);
    *pFd = DIAG_FD_NOT_OPEN;
  }

} /* end of diag_CloseFileDesc */

/*
 * Initialization of host command handler
 * - Open socket for host command handling
 * - Allocate host request data buffer
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others       - Initialization failed
 *
 */
int diag_CmdHandler_Init(void)
{
  int   rtn = DIAGD_RC_ERR;   /* Default is OK */
  int   rc;                   /* Return code of system calls */
  struct sockaddr_in addr;    /* Socket parameters for bind */


  DIAGD_ENTRY("%s: ", __func__);

  do {

    /* Allocate memory for host command request buffer */
    pDiagInfo->hostReqData = (uint8_t *)malloc(DIAG_HOSTREQ_BUF_LEN);
    if (pDiagInfo->hostReqData == NULL) {
      DIAGD_DEBUG("%s: Unable to create socket: %s\n", __func__, strerror(errno));
      break;
    }

    /* create Internet domain socket */
    pDiagInfo->hostCmdSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (pDiagInfo->hostCmdSock == DIAG_SOCKET_NOT_OPEN) {
      DIAGD_DEBUG("%s: malloc failed: %s", __func__, strerror(errno));
      break;
    }
    /* Fill in socket structure */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DIAG_HOSTCMD_PORT);

    /* Bind socket to the port */
    rc =  bind(pDiagInfo->hostCmdSock, (struct sockaddr *)&addr, sizeof(addr));
    if (rc == -1) {
      DIAGD_DEBUG("%s: bind failed: %s", __func__, strerror(errno));
      break;
    }

    /* listen for clients on the socket */
    rc = listen(pDiagInfo->hostCmdSock, 1);
    if (rc == -1) {
      DIAGD_DEBUG("%s: listen failed: %s", __func__, strerror(errno));
      break;
    }
    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s: (rtn=%d)", __func__, rtn);

  return (rtn);
} /* end of diag_CmdHandler_Init */


/*
 * Cleanup of host command handling
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others       - Initialization failed
 *
 */
void diag_CmdHandler_Uninit(void)
{

  diag_CloseSocket(&pDiagInfo->hostCmdSock);

  diag_CloseFileDesc(&pDiagInfo->hostCmdDesc);

  if (pDiagInfo->hostReqData != NULL) {
    free(pDiagInfo->hostReqData);
    pDiagInfo->hostReqData = NULL;
  }

} /* end of diag_CmdHandler_Uninit */



/* -------------------------------------------------------------------------
 * memory and register related routines
 *--------------------------------------------------------------------------
 */

/*
 * Access 32-bit register
 *
 * Input:
 * regAddr  - (IN) Starting physical address
 * pRegData - (IN/OUT) data of the register
 * wr       - true:  write operation
 *            false: read operation
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagAccessReg(off_t regAddr, uint32_t *pRegData, bool wr)
{
  int rtn = DIAGD_RC_OK;
  int pageSize = getpagesize();
  int fd = 0;
  void *mapBase = NULL;
  void *virtAddr;

  do {

    fd = open("/dev/mem", (wr == true)? (O_RDWR | O_SYNC) : (O_RDONLY | O_SYNC));
    mapBase = mmap(
                  NULL,
                  pageSize * 2 /* in case value spans page */,
                  (wr == true)? (PROT_READ | PROT_WRITE) : PROT_READ,
                  MAP_SHARED,
                  fd,
                  regAddr & ~(off_t)(pageSize - 1));

    /* TODO - How to handle if mmap failed */
    if (mapBase == MAP_FAILED) {
      DIAGD_PERROR("mmap");
      rtn = DIAGD_RC_ERR;
      break;
    }

    virtAddr = (char*)mapBase + (regAddr & (pageSize - 1));

    if (wr == true) {
        *(volatile uint32_t *)virtAddr = *pRegData;
    }
    else {
      *pRegData = *(volatile uint32_t *)virtAddr;
    }

  } while(false);

  /* Cleanup */
  if ((mapBase != NULL) && (mapBase != MAP_FAILED)) {
    /* ignore the returned result */
    if (munmap(mapBase, pageSize * 2) == -1) {
      DIAGD_PERROR("munmap: ");
    }
  }

  if (fd) {
    close(fd);
  }

  return(rtn);
} /* end of diagReadReg */


/*
 * Read Phy BCRM 54612 Register via MDIO register
 *
 * Input:
 * phyRegAddr   - (IN) Starting physical address
 * pRegData     - (OUT) data of the register
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagRd_54612_PhyReg(uint8_t regAddr, uint16_t *pRegData)
{
  uint32_t  regData;
  int       rtn = DIAGD_RC_ERR;

  do {
    /* Set the register to read the phy reg */
    regData = MDIO_START_BUSY | MDIO_RD |
              MDIO_PHY_REG_ADDR((uint32_t)regAddr);
    if (diagAccessReg(GENET_0_UMAC_MDIO_CMD, &regData, true) != DIAGD_RC_OK) {
      DIAGD_DEBUG("%s: failed to wr MDIO reg at line %d", __func__, __LINE__);
      break;
    }

    /* Read the phy reg back*/
    diagAccessReg(GENET_0_UMAC_MDIO_CMD, &regData, false);
    if (regData & MDIO_READ_FAIL) {
      DIAGD_DEBUG("%s: failed to rd phy reg (0x%8X)", __func__, regData);
      *pRegData = 0;
    }
    else {
      *pRegData = (uint16_t)(regData & MDIO_REG_DATA_MASK);
      rtn = DIAGD_RC_OK;
    }
  } while (false);

  return (rtn);
} /* end of diagRd_54612_PhyReg */


/*
 * Write Phy BCRM 54612 Register
 *
 * Input:
 * phyRegAddr   - (IN) Starting physical address
 * pRegData     - (IN) data of the register
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagWr_54612_PhyReg(uint8_t regAddr, uint16_t *pRegData)
{
  uint32_t  regData;
  int       rtn = DIAGD_RC_ERR;

  do {
    /* Set the register to read the phy reg */
    regData = MDIO_START_BUSY | MDIO_WR |
              (MDIO_PHY_REG_ADDR((uint32_t)regAddr) |
              ((uint32_t)*pRegData & MDIO_REG_DATA_MASK));
    DIAGD_TRACE("%s: Wr regData=0x%08X", __func__, regData);
    if (diagAccessReg(GENET_0_UMAC_MDIO_CMD, &regData, true) != DIAGD_RC_OK) {
      DIAGD_DEBUG("%s: failed to wr MDIO reg at line %d",
                  __func__, __LINE__);
      break;
    }

    /* Check until the busy is completed */
    while (true) {

      /* Wait until the busy bit cleared */
      diagAccessReg(GENET_0_UMAC_MDIO_CMD, &regData, false);
      DIAGD_TRACE("%s: RD regData=0x%08X", __func__, regData);
      if ((regData & MDIO_START_BUSY) == 0) {
        break;      /* wr to phy complete */
      }
    }
    rtn = DIAGD_RC_OK;

  } while (false);

  return (rtn);
} /* end of diagWr_54612_PhyReg */


/* -------------------------------------------------------------------------
 * Network related routines
 *--------------------------------------------------------------------------
 */

/*
 * Net interface up/down of the specified net interface
 *
 * Input:
 * phyRegAddr   - (IN) Starting physical address
 * pRegData     - (IN) data of the register
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diag_netIf_UpDown(char *netIf, bool netIfUp)
{
  int     rtn = DIAGD_RC_ERR;
  struct ifreq ifr;
  int     sockfd = 0;             /*Socketdescriptor*/
  int     ifindex = 0;    /*Ethernet Interface index*/


  do {
    /* open socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
      DIAGD_PERROR("socket():");
      break;;
    }

    /* retrieve ethernet interface index */
    strncpy(ifr.ifr_name, netIf, IFNAMSIZ);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
      DIAGD_PERROR("SIOCGIFINDEX");
      break;
    }

    ifindex = ifr.ifr_ifindex;
    /* Get current IFF flags */
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1) {
      DIAGD_PERROR("get SIOCGIFFLAGS: ");
      break;
    }

    /* Set the IFF_UP flag accordingly */
    if (netIfUp == true) {
      ifr.ifr_flags |= IFF_UP;
    }
    else {
      ifr.ifr_flags &= ~IFF_UP;
    }

    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) != -1) {
      DIAGD_PERROR("set SIOCSIFFLAGS: ");
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

  return (rtn);
} /* end of diag_netIf_UpDown */


/* ================================================================= */
#ifdef BRCM_7425_CPU_REG_ENABLE
/* ================================================================= */
/*
 * This routine returns the CPU temperature in centigrade.
 *
 * TBD 20110928 -
 * Per BRCM email on Sep 16, 2011,
 * - Only know if data_valid is 1, the measured data (bit 9:0), output_code,
 *   from PVT is valid.
 * - Not sure how to use the "done" bit.
 * - Equation - T (deg. C) = 418 – (0.556 x Output_code)
 *
 * Input -
 *  pTemperature - Return the CPU temperature.
 *                 When rtn == DIAGD_RC_OK, the returned data of
 *                 pTemperature is valid
 *  pRegData     - return the read-back data of the
 *                 AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS reg.
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diag_Read_CPU_Temperature(double *pTemperature, uint32_t *pRegData)
{
  uint32_t  value;
  int       rtn = DIAGD_RC_OK;

  do {
#ifdef SIMULATION_TEMPERATURE_MON_REG
    DIAGD_TRACE("Simulation - reg data (in hex): ");
    scanf("%x", &value);
#else
    rtn = diagAccessReg(
              AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS,
              &value,
              false);
    if (rtn != DIAGD_RC_OK) {
      DIAGD_DEBUG("%s at line %d: Failed to rd CPU tem reg 0x%08X",
                  __func__, __LINE__,
                  AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS);
    }

#endif /* end of SIMULATION_TEMPERATURE_MON_REG */

    DIAGD_TRACE("%s: data=0x%x\n", __func__, value);

    *pRegData = value;

    /* Check if the valid_data bit is set */
    if (value & AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS_VALID_DATA_MASK) {
      /* Yes, data is valid. Let's get the measured data*/
      value &= AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS_DATA_MASK;

      /*
       * Let's convert to centigrade - Per BRCM email, the equation is
       *    T (deg. C) = 418 – (0.556 x Output_code)
       */
      *pTemperature = 418 - ((double)value * 0.556);

    }
    else {
      /* The measured data is not available. */
      rtn = DIAGD_RC_ERR;
    }

  } while (false);

  if (rtn == DIAGD_RC_OK) {
    DIAGD_TRACE("%s: regData=0x%x,  CPU Temperature(centigrade): %4.2f\n",
           __FILE__, *pRegData, *pTemperature);
  }
  else {
    DIAGD_TRACE("%s: Failed.\n", __func__);
  }

  return(rtn);

} /* end of diag_Read_CPU_Temperature */

/* ================================================================= */
#endif /* end of BRCM_7425_CPU_REG_ENABLE */
/* ================================================================= */


/*
 * return the starting address of the specified network interface's database
 *
 * Input:
 * netIf_name - (IN)  network interface's name (eth0, ....).
 * pNetIf     - (OUT) Starting address of diag_netIf_info_t
 *                    NULL  - can't find the net interface in database.
 *                    Others - Point to the database of the net interface
 *
 * Output:
 * None
 */
void diag_GetStartingAddr_NetIfInfo(char *pNetif_name, diag_netIf_info_t **pNetIf)
{
  diag_netIf_info_t  *pNetIfs = pDiagInfo->netifs;
  int                 i;

  DIAGD_ENTRY("%s: ", __func__);

  *pNetIf = NULL;

  /* Get the starting address of the specified network interface. */
  for (i = 0; i < MAX_NETIF_NUM; i++) {

    if (pNetIfs[i].inUse == true) {

      DIAGD_TRACE("%s: pNetIfs[%d].name=%s, pNetif_name=%s",
                  __func__, i, pNetIfs[i].name, pNetif_name);

      /* Compare the network interface name */
      if (strcmp(pNetIfs[i].name, pNetif_name) == 0) {
        *pNetIf = &pNetIfs[i];
        break;
      }
    }
  } /* end of for */

} /* end of diag_GetStartingAddr_NetIfInfo */


/*
 * Get the network interface link state via "/sys/class/net/'netif_name'/carrier"
 *
 * Input:
 * netif_linkstate - (In/Output) return content of carrier.
 *                   DIAG_NETLINK_UP or DIAG_NETLINK_DOWN
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diag_Get_Netlink_State(netif_netlink_t *netif_linkstate)
{
  FILE *pf;
  char command[COMMAND_LEN];
  char data[DATA_SIZE];
  int rtn = DIAGD_RC_OK;

  do {
    /* Execute a process listing */
    sprintf(command, "cat /sys/class/net/%s/carrier", netif_linkstate->netif_name);

    /* Setup our pipe for reading and execute our command. */
    pf = popen(command, "r");

    if(!pf) {
      DIAGD_DEBUG("Could not open pipe for output.");
      rtn = DIAGD_RC_ERR;
      break;
    }

    /* Grab data from process execution */
    fgets(data, DATA_SIZE , pf);
    *(netif_linkstate->pData) = strtoul(data, NULL, 0);

    /* Check if the net interface carrier is up */
    if (*(netif_linkstate->pData) == 1)
      *(netif_linkstate->pData) = DIAG_NETLINK_UP;    /* link is up */
    else
      *(netif_linkstate->pData) = DIAG_NETLINK_DOWN;  /* Link is down */

    if (pclose(pf) != 0)
      DIAGD_DEBUG("Error: Failed to close command stream");

  } while (false);

  return(rtn);

} /* end of diag_Get_Netlink_State */


/*
 * Open a netlink socket for monitoring link up/down states
 *
 * Network interface Linkup/linkdown related subroutine
 *
 * Input:
 * None
 *
 * Output:
 * -1:          Failed to open a netlink socket.
 * otherwise:   OK
 */
static int diag_netlink_socket(void)
{
  int   sock = -1;    /* Set to unable to open netlink socket */
  struct sockaddr_nl sockaddr;


  do {

    /* Open a netlink socket */
    sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0) {
      DIAGD_DEBUG("%s: Failed to open netlink socket: %s", __func__, strerror(errno));
      break;
    }

    /* To creates a NETLINK_ROUTE netlink socket which will listen to the
     * RTMGRP_LINK (create/delete/up/down events of each network interface)
     */
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.nl_family = AF_NETLINK;
    sockaddr.nl_groups = RTMGRP_LINK;

    if (bind(sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == -1) {

      DIAGD_DEBUG("Failed to bind netlink socket: %s", strerror(errno));
      close(sock);
      sock = -1;
    }

  } while (0);

  DIAGD_TRACE("%s: sock=%d", __func__, sock);

  return (sock);

} /* end of diag_netlink_socket */


/*
 * Calculate the delta of statistics of statistics[] field.
 * 1) When interval timeout occurs, or
 * 2) The loopback test
 *
 * Input:
 * pNetIf - Starting address of the network interface's database.
 *
 * Output:
 * None
 */
void diag_Update_Statistics_Delta(diag_netIf_info_t *pNetIf)
{
  diag_netif_stats_t  *pCurr, *pPrev, *pDelta;
  uint8_t  idx;

  do {

    /* Get the starting address of the net interface info. */
    pCurr  = &pNetIf->statistics[pNetIf->active_stats_idx];
    /* Get the starting addresses of previous/delta of statistics databases */
    idx    = (pNetIf->active_stats_idx == 0)? 1 : 0;
    pPrev  = &pNetIf->statistics[idx];
    pDelta = &pNetIf->delta_stats;

    /* Update the delta of statistics */
    DIAG_GET_UINT32_DELTA(pCurr->rx_bytes, pPrev->rx_bytes, pDelta->rx_bytes);
    DIAG_GET_UINT32_DELTA(pCurr->rx_packets, pPrev->rx_packets, pDelta->rx_packets);
    DIAG_GET_UINT32_DELTA(pCurr->rx_errors, pPrev->rx_errors, pDelta->rx_errors);
    DIAG_GET_UINT32_DELTA(pCurr->rx_crc_errors,
                          pPrev->rx_crc_errors, pDelta->rx_crc_errors);
    DIAG_GET_UINT32_DELTA(pCurr->rx_frame_errors,
                          pPrev->rx_frame_errors, pDelta->rx_frame_errors);
    DIAG_GET_UINT32_DELTA(pCurr->rx_length_errors,
                          pPrev->rx_length_errors, pDelta->rx_length_errors);
    DIAG_GET_UINT32_DELTA(pCurr->tx_bytes, pPrev->tx_bytes, pDelta->tx_bytes);
    DIAG_GET_UINT32_DELTA(pCurr->tx_packets,
                          pPrev->tx_packets, pDelta->tx_packets);
    DIAG_GET_UINT32_DELTA(pCurr->tx_errors, pPrev->tx_errors, pDelta->tx_errors);
    DIAG_GET_UINT32_DELTA(pCurr->link_ups, pPrev->link_ups, pDelta->link_ups);
    DIAG_GET_UINT32_DELTA(pCurr->link_downs, pPrev->link_downs, pDelta->link_downs);

  } while (false);

} /* end of diag_Update_Statistics_Delta */


/*
 * Get the specified counter under "/sys/class/net/'netif_name'/statistics/'counter_name'"
 *
 * Input:
 * netif_counter - (In/Output) return content of the specified counter.
 *
 * Output:
 * 1 - Failed to access "/sys/class/net/'netif_name'/statistics/'counter_name'"
 * 0 - OK
 */
int diag_Get_Netif_One_Counter(netIf_counter_t *netif_counter)
{
  FILE *pf;
  char command[COMMAND_LEN];
  char data[DATA_SIZE];
  int rtn = 0;

  do {
    /* Execute a process listing */
    sprintf(command, "cat /sys/class/net/%s/statistics/%s",
            netif_counter->netif_name, netif_counter->counter_name);

    /* Setup our pipe for reading and execute our command. */
    pf = popen(command, "r");

    if(!pf) {
      DIAGD_DEBUG("Could not open pipe for output.");
      rtn = 1;
      break;
    }

    /* Grab data from process execution */
    fgets(data, DATA_SIZE , pf);
    *(netif_counter->pData) = strtoul(data, NULL, 0);

    if (pclose(pf) != 0)
      DIAGD_DEBUG("Error: Failed to close command stream");

  } while (0);

  return(rtn);

} /* end of diag_Get_Netif_One_Counter */


/*
 * Check if network error statistics (exclude link up/down cnts) of the
 * specified network interface exceed their error thresholds.
 *
 * Input:
 * pNetIf - Starting address of the network interface's database.
 *
 * Output:
 * None
 */
void diag_Check_netStatistics(diag_netIf_info_t *pNetIf)
{
  diag_netif_stats_t  *pCurrStats;
  diag_netif_stats_t  *pDelta = &pNetIf->delta_stats;
  bool                err;
  bool                logStats = false;


  DIAGD_ENTRY("%s: ", __func__);

  do {

    /* We check the following counters -
     * 1) CRC Errors - Frames were sent but were corrupted in transit.
     *      The presence of CRC errors, but not many collisions
     *      usually is an indication of electrical noise.
     *      Possibilities of causing CRC error -
     *      a) Bad connector -  Check the connector that attaches the network
     *         cable to the workstation's adapter card.
     *      b) Bad port - If the device is connected to a hub or a switch,
     *         the port on that device might be causing the problem. Also,
     *         be sure to check the connector on that end of the cable segment.
     *      c) Bad cable - There's always the chance that a cable has been damaged
     *         or disconnected.
     *      d) Malfunctioning network component
     * 2) Frame Errors: An incorrect CRC and a non-integer number of bytes are
     *      received. This is usually the result of collisions or a
     *      bad Ethernet device.
     * 3) Length Errors: The received frame length was less than or exceeded
     *      the Ethernet standard. This is most frequently due to
     *      incompatible duplex settings.
     *      The root cause of the errors could be a defective network interface,
     *      transceiver, or a corrupt network interface driver
     */
    /* Get the starting address of the net interface info. */
    pCurrStats = &pNetIf->statistics[pNetIf->active_stats_idx];

    /* CRC Errors -
     * 1. Get the number of CRC errors during elapsed time.
     * 2. Calculate and check if the errors exceeds the CRC error threshold
     */
    DIAG_CHK_ERR_THLD(pDelta->rx_packets,
                      pDelta->rx_crc_errors,
                      diagNetThld_pctRxCrcErrs,
                      err);
    if (err == true) {
      /* The crc errors exceeds the error threshold */
      DIAGD_LOG_WARN("%s: Excessive CRC Errors in %d secs  "
                     "[RxPkts=%lu  CRC Errs=%lu]",
                     pNetIf->name, DIAG_WAIT_TIME_RUN_GET_NET_STATS,
                     pDelta->rx_packets, pDelta->rx_crc_errors);
      logStats = true;        /* indicate to print net statistics */
    }

    /* Frame Errors -
     * 1. Get the number of frame errors during elapsed time.
     * 2. Calculate and check if the errors exceeds the frame error threshold
     */
    DIAG_CHK_ERR_THLD(pDelta->rx_packets,
                      pDelta->rx_frame_errors,
                      diagNetThld_pctRxFrameErrs,
                      err);
    if (err == true) {
      /* The frame errors exceeds the error threshold */
      DIAGD_LOG_WARN("%s: Excessive Frame Errors in %d secs  "
                     "[RxPkts=%lu  Frame Errs=%lu]",
                     pNetIf->name, DIAG_WAIT_TIME_RUN_GET_NET_STATS,
                     pDelta->rx_packets, pDelta->rx_frame_errors);
      logStats = true;        /* indicate to print net statistics */
    }

    /* Length Errors -
     * 1. Get the number of frame errors during elapsed time.
     * 2. Calculate and check if the errors exceeds the length error threshold
     */
    DIAG_CHK_ERR_THLD(pDelta->rx_packets,
                      pDelta->rx_length_errors,
                      diagNetThld_pctRxLenErrs,
                      err);
    if (err == true) {
      /* The frame errors exceeds the error threshold */
      DIAGD_LOG_WARN("%s: Excessive Length Errors in %d secs  "
                     "[RxPkts=%lu  Len Errs=%lu]",
                     pNetIf->name, DIAG_WAIT_TIME_RUN_GET_NET_STATS,
                     pDelta->rx_packets, pDelta->rx_length_errors);
      logStats = true;        /* indicate to print net statistics */
    }

    /* Check if log the net interface's statistics counters */
    if (logStats == true) {
      DIAGD_LOG_INFO("%s: rx_bytes=%lu  rx_packets=%lu  rx_errors=%lu  "
                     "rx_crc_errors=%lu  rx_frame_errors=%lu  rx_length_errors=%lu  "
                     "tx_bytes=%lu  tx_packets=%lu  tx_errors=%lu",
        pNetIf->name,
        pCurrStats->rx_bytes, pCurrStats->rx_packets, pCurrStats->rx_errors,
        pCurrStats->rx_crc_errors, pCurrStats->rx_frame_errors,
        pCurrStats->rx_length_errors, pCurrStats->tx_bytes,
        pCurrStats->tx_packets, pCurrStats->tx_errors);
    }

  } while (false);

  DIAGD_EXIT("%s: exit", __func__);

} /* end of diag_Check_netStatistics */


/*
 * Get network counters of a specified network interface.
 * Also -
 * 1. If a new net interface detected, get the current link state.
 *
 * Input:
 * netif_name   - Network interface name ('eth0', 'eth1'...)
 * normalMode   - true: normal mode, false:  loopback test
 *
 * Output:
 * DIAGD_RC_OK - OK
 */
int diag_Get_Netif_Counters(char *pNetif_name, unsigned char bNormalMode)
{
  int i;
  unsigned long     *pCounter = NULL;
  diag_netIf_info_t *pNetIfs = pDiagInfo->netifs;
  diag_netIf_info_t *pNetIf  = NULL;
  netIf_counter_t   netif_counter;
  unsigned long     linkup;
  unsigned long     linkdown;
  unsigned char     stats_idx;


  DIAGD_ENTRY("%s", __func__);

  /* Get the starting address of the specified network interface. */
  diag_GetStartingAddr_NetIfInfo(pNetif_name, &pNetIf);

  DIAGD_TRACE("%s: pNetIf=%lu", __func__, (unsigned long)pNetIf);

  /* Check if found the database of the net interface */
  if (pNetIf == NULL) {

    /*
     * No found. Let's find available entry for the new network interface.
     */
    for (i = 0; i < MAX_NETIF_NUM; i++) {

      if (pNetIfs[i].inUse == false) {

        /* Add a new entry -
         * 1. Get the starting address of the entry.
         * 2. Copy the interface name into the entry.
         * 3. Set the flag to indicate the interface is in use
         * 4. Increase the number of network interface in use.
         * 5. Get the current link status.
         */
        pNetIf = &pNetIfs[i];
        strcpy(pNetIf->name, pNetif_name);
        pNetIf->inUse = true;
        pDiagInfo->nNetIfs++;

        /* Setup the input parameter of diag_Get_Netif_One_Counter(): netif name */
        strcpy(netif_counter.netif_name, pNetif_name);
        netif_counter.pData = &linkup;          /* temp use */
        diag_Get_Netlink_State((netif_netlink_t *)&netif_counter);
        /* Save the link status to data base */
        pNetIfs[i].netlink_state = (unsigned char)linkup;

        DIAGD_TRACE("%s: nNetIfs=%d pNetIfs[%d].name=%s, pNetif_name=%s link=%s",
            __func__, pDiagInfo->nNetIfs, i, pNetIfs[i].name, pNetif_name,
            (pNetIfs[i].netlink_state == DIAG_NETLINK_UP)? "UP":"DOWN");

        break;
      }
    }
  }

  if (pNetIf == NULL) {

    /* No available entry to the network interface. */
    /* TODO - 20110919
     * - print to diag log file???
     */
    DIAGD_DEBUG("%s: 2-- pNetIf=%lu", __func__, (unsigned long)pNetIf);

    return(DIAGD_RC_NO_NETIF_ENTRY_AVAIL);    /* Failed. Exit */
  }

  /*
   * Double-buffering is used for the statistics counter.
   * 1. Keep the previous statistics counters.
   * 2. Let's change the active stats index to another one.
   * 3. NOTE -
   *    Due to diagd monitors the linkup and linkdown counts and
   *    the link counts are not part of network counters, so When switch
   *    active_stats_idx, copy the linkup/linkdown counts.
   */
  linkup = pNetIf->statistics[pNetIf->active_stats_idx].link_ups;
  linkdown = pNetIf->statistics[pNetIf->active_stats_idx].link_downs;

  pNetIf->active_stats_idx = (pNetIf->active_stats_idx == 0)? 1 : 0;
  stats_idx = pNetIf->active_stats_idx;
  pCounter = (unsigned long *)&pNetIf->statistics[stats_idx];

  /* Let's copy the linkup and linkdown counts to the active buffer */
  pNetIf->statistics[pNetIf->active_stats_idx].link_ups   = linkup;
  pNetIf->statistics[pNetIf->active_stats_idx].link_downs = linkdown;

  /* Setup the input parameter of diag_Get_Netif_One_Counter(): netif name */
  strcpy(netif_counter.netif_name, pNetif_name);

  /* Get statistic counters of the net interface. */
  for (i = 0; i < DIAG_NET_CNTS; i++, pCounter++) {

    /*
     * Setup the input parameters to query specified counter:
     * 1. Copy the statistics counter name in string.
     * 2. Set the starting address of the query counter.
     */
    strcpy(netif_counter.counter_name, diag_netif_stats_cnt_names[i]);
    netif_counter.pData = pCounter;
    diag_Get_Netif_One_Counter(&netif_counter);
  }

  diag_netif_stats_t  *pCurr = &pNetIf->statistics[stats_idx];
  DIAGD_TRACE("%s: active_stats_idx:%d", __func__, pNetIf->active_stats_idx);

  DIAGD_TRACE("rx_bytes:%lu rx_packets:%lu tx_errors:%lu",
              pCurr->tx_bytes, pCurr->tx_packets, pCurr->tx_errors);

  DIAGD_TRACE("tx_bytes:%lu tx_packets:%lu rx_errors:%lu rx_crc_errors:%lu"
              "rx_frame_errors:%lu rx_length_errors:%lu",
              pCurr->rx_bytes, pCurr->rx_packets, pCurr->rx_errors,
              pCurr->rx_crc_errors, pCurr->rx_frame_errors,
              pCurr->rx_length_errors);

  /* Get delta of statistics */
  diag_Update_Statistics_Delta(pNetIf);

  /* Check if we are in normal mode */
  if (bNormalMode == true) {
    /* In normal mode, let's check if the network statistics error counters
     * (exclude link up/down counts) exceed their error thresholds.
     */
    diag_Check_netStatistics(pNetIf);
  }
  if (bNormalMode == false) {
    /* Do nothgin here .... */
  }

  DIAGD_EXIT("Exit %s", __func__);

  return(DIAGD_RC_OK);

} /* end of diag_Get_Netif_Counters */



/*
 * Initailization of diagd
 *
 * Input:
 *  refFile - reference data filename
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others       - Initialization failed
 *
 */
int diagd_Init(char *refFile)
{
  int   rtn = DIAGD_RC_OK;          /* default is OK */
  int   i;


  DIAGD_ENTRY("%s", __func__);

  do {

    if (diagtOpenEventLogFile() != DIAGD_RC_OK) {

      DIAGD_DEBUG("%s: Failed to open diag log file", __func__);
      break;
    }

    /* Upload diag log file once diagd starts running */
    diagUploadLogFile();

    /* Let's zero the database of diagd */
    memset(pDiagInfo, 0, sizeof(diag_info));
    pDiagInfo->hostCmdSock = DIAG_SOCKET_NOT_OPEN;
    pDiagInfo->hostCmdDesc = DIAG_FD_NOT_OPEN;
    pDiagInfo->netlinkSock = DIAG_SOCKET_NOT_OPEN;
    for (i = 0; i < MAX_NETIF_NUM; i++) {
      pDiagInfo->netifs[i].inUse = false;
    }

    /* For monitoring network link state, let's open netlink socket */
    pDiagInfo->netlinkSock = diag_netlink_socket();
    if (pDiagInfo->netlinkSock == DIAG_SOCKET_NOT_OPEN) {
      /*
       * TODO - 2011/09/19
       *  1) HOWTO - If failed to open the netlink socket
       *  2) For now, we treat it as a fatal error and abort the diagd.
       */
      rtn = DIAGD_RC_FAILED_OPEN_NETLINK_SOCKET;
      DIAGD_DEBUG("%s: failed to open netlink socket", __func__);
      break;
    }
    else
      DIAGD_TRACE("%s: pDiagInfo->netlinkSock=%d", __func__, pDiagInfo->netlinkSock);

    rtn = diagd_MoCA_Init();
    if (rtn != DIAGD_RC_OK) {
      DIAGD_DEBUG("%s: failed to init diagMoCA module....", __func__);
      break;
    }

    /* Parse the reference data */
    if (refFile != NULL) {
      diagReadDiagDataFile(refFile);
    }
    else {
      diagReadDiagDataFile(DIAGD_REF_DATA_FILE);
    }

    DIAGD_TRACE("%s - rtn=0x%X", __func__, rtn);

  } while (false);

  return(rtn);

} /* end of diagd_Init */

/* Routine that turns on LED color
 * (red or blue) by writing content
 * to Bruno LED control file
 *
 * Input:
 * ledInd: LED indicator
 *
 * Output:
 * NONE
 */
static void diag_set_LED(diag_led_indicator ledInd)
{
  char filename[64];
  char *ptr = NULL;
  int  fd;


  if (ledInd >= DIAG_LED_IND_MAX) {
    DIAGD_DEBUG("%s: ledInd is invalid = %d", __func__, ledInd);
    return;
  }

  snprintf(filename, sizeof(filename), "%s.diagd_tmp", BRUNO_LED_CTRL_FNAME);
  fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);

  if (fd >= 0) {
    ptr = diagLedTbl[ledInd].num_seq;
    write(fd, ptr, strlen(ptr));
    close(fd);
    rename(filename, BRUNO_LED_CTRL_FNAME);
  }
}

/* Routine that send alarm
 * by turning on LED solid red
 *
 * Input:
 * NONE
 *
 * Output:
 * NONE
 */
void diagSendAlarm()
{
    diag_set_LED(DIAG_LED_SOLID_RED);
}
