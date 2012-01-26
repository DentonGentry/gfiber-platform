/*
 * Copyright 2011 Google Inc. All Rights Reserved.
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
 * Global Variables
 *
 *--------------------------------------------------------------------------
 */

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Timestamp of starting time of a hardware monitoring APis
 */
/* Start time of Diag_MonNet_GetNetIfStatistics() */
bool   diag_getStats_firstRun = true;
time_t diagStartTm_getStats = 0;

/* Start time of Diag_Mon_ParseExame_KernMsg() */
bool   diag_chkKernMsg_firstRun = true;
time_t diagStartTm_chkKernMsg = 0;


/* Start time of Diag_MonMoca_Err_Counts() */
bool   diag_moca_monErrCnts_firstRun = true;
time_t diagStartTm_moca_monErrCnts = 0;


/* Start time of Diag_MonMoca_Err_Counts() */
bool   diag_moca_monServicePerf_firstRun = true;
time_t diagStartTm_moca_monServicePerf = 0;


/* -------------------------------------------------------------------------
 *
 * Internal defines, types and variables
 *
 *--------------------------------------------------------------------------
 */


/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */

/*
 * Check if timeout occurred
 *
 * 1. Get current time
 * 2. Find total elapsed time between two different times as obtained from the Unix clock
 *    in a timeval structure.
 * 3. Check if over the max wait time.
 *
 * Input:
 * pTimeoutChecking
 *
 * Output:
 * false   - Not timeout
 * true    - Timed out
 */
bool checkIfTimeout(int diagdApiIdx)
{
  time_t  endTime;
  time_t  maxWaitTime;
  time_t  startTime;
  time_t  timeElapsed;
  bool    ifTimeout = false;       /* default is no timeout */

  do {
    if (diagdApiIdx == DIAG_API_IDX_GET_NET_STATS) {
      startTime   = diagStartTm_getStats;
      maxWaitTime = DIAG_WAIT_TIME_RUN_GET_NET_STATS;
    }
    else if (diagdApiIdx == DIAG_API_IDX_GET_CHK_KERN_KMSG) {
      startTime   = diagStartTm_chkKernMsg;
      maxWaitTime = DIAG_WAIT_TIME_RUN_CHK_KMSG;
    }
    else if (diagdApiIdx == DIAG_API_IDX_MOCA_MON_ERR_CNTS) {
      startTime   = diagStartTm_moca_monErrCnts;
      maxWaitTime = DIAG_WAIT_TIME_MOCA_MON_ERR_CNTS;
    }
    else if (diagdApiIdx == DIAG_API_IDX_MOCA_MON_SERVICE_PERF) {
      startTime   = diagStartTm_moca_monServicePerf;
      maxWaitTime = DIAG_WAIT_TIME_MOCA_MON_SERVICE_PERF;
    }
    else {
      break;    /* Bad API index (Shouldn't get here). Exit */
    }

    /*
     * Note - Let's not to worry about the time_t wraparound issue.
     *        (It is a known issue - the linux's year 2038 problem.).
     */
    time(&endTime);
    timeElapsed = (time_t)difftime(endTime, startTime);

    if (timeElapsed >= maxWaitTime)
    {
      /* Yep, it is timeout. */
      ifTimeout = true;

      /* Let's update the starting time of the APIs now. */
      time(&startTime);

      DIAGD_TRACE("%s: Timeout=%s, timeElapsed=%ld, maxWaitTime=%ld",
                  __func__, (ifTimeout == true)? "true" : "false",
                  timeElapsed, maxWaitTime);
    }

  } while (false);

  return (ifTimeout);

} /* end of checkIfTimeout */


/*
 * =============================================================================
 * Network related subroutines
 * (Netlink related routines - link up/down statistics )
 * =============================================================================
 */

/*
 * Check network netlink statistics and current link state of each network
 * interface.
 *
 * Notes -
 *  1) The link up/down counts are monitoring in the diagd_Rd_Netlink_Msgs().
 *  2) diagd keeps track the link up/down counts since power up.
 *  2) The current netlink up/down counts are based on the setting of
 *     active_stats_idx of diag_netIf_info_t
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK - OK
 */
int diag_Check_NetLinkUpDownCounts()
{
  diag_netIf_info_t *pNetIfs = pDiagInfo->netifs;
  diag_netIf_info_t *pNetIf  = NULL;
  diag_netif_stats_t  *pStatsDelta, *pStats;
  int     i;

  DIAGD_TRACE("%s: enter", __func__);

  do {

    for (i = 0; i < MAX_NETIF_NUM; i++) {

      if (pNetIfs[i].inUse == false) {
        /* The entry is not used. Go to next entry. */
        continue;
      }

      /*
       * Carrier errors (Link up/down counts)- Caused by the NIC card
       *    losing its link connection.
       * Possibilities - Faulty cabling, faulty interfaces on the NIC
       *                 networking equipment or system commands.
       */
      /* Get the starting address of the net interface info. */
      pNetIf = &pNetIfs[i];
      pStats = &pNetIf->statistics[pNetIf->active_stats_idx];
      pStatsDelta = &pNetIf->delta_stats;

      /* Check if the link_down count is over threshold */
      /* TODO 20111027 -
       * 1) If MoCA and GENET interfaces are using same thresholds???
       * 2) Let's use the same threshold for now.
       */
      if (pStatsDelta->link_downs >= DIAG_THLD_LINK_STATE_CNTS) {
        DIAGD_LOG_WARN("%s: Excessive Link State Changed in %u secs.  [linkStat=%s  "
                       "link_ups=%lu  link_downs=%lu  delta_ups=%lu  delta_downs=%lu]",
          pNetIf->name, DIAG_WAIT_TIME_RUN_GET_NET_STATS,
          (pNetIf->netlink_state == DIAG_NETLINK_UP)? "UP":"DOWN",
          pStats->link_ups, pStats->link_downs,
          pStatsDelta->link_ups, pStatsDelta->link_downs);
      }

    } /* end of for */

  } while (false);

  DIAGD_TRACE("%s: exit", __func__);

  return(DIAGD_RC_OK);

} /* end of diag_Check_NetLinkUpDownCounts */

/*
 * This routine reads and processes netlink messages.
 * When detect link status toggled, update the link status counter accordingly
 *
 * Input:
 * sock -
 *
 * Output:
 * None
 */
void diagd_Rd_Netlink_Msgs()
{
  int     len;
  char    buf[4096];
  struct iovec    iov = { buf, sizeof(buf) };
  struct sockaddr_nl  sockaddr;
  struct msghdr   msg = { (void *)&sockaddr, sizeof(sockaddr), &iov, 1, NULL, 0, 0 };
  struct nlmsghdr   *nh;
  struct ifinfomsg  *ifinfo;
  char  ifname[IF_NAMESIZE] = {""};
  char *rc = 0;
  diag_netIf_info_t *pNetIf  = NULL;


  DIAGD_TRACE("****%s_1: pDiagInfo->netlinkSock=%d\n", __func__, pDiagInfo->netlinkSock);

#if 0
  len = recvmsg (pDiagInfo->netlinkSock, &msg, MSG_DONTWAIT);
#else
  len = recvmsg (pDiagInfo->netlinkSock, &msg, 0);
#endif
  if (len == -1) {
    DIAGD_DEBUG("recvmsg failed: %s", strerror(errno));
    return;
  }

  pthread_mutex_lock(&lock);

  for (nh = (struct nlmsghdr *) buf; NLMSG_OK (nh, len); nh = NLMSG_NEXT (nh, len)) {

    /* The end of multipart message. */
    if (nh->nlmsg_type == NLMSG_DONE)
      break;

    /* Message is to be ignored */
    if (nh->nlmsg_type == NLMSG_NOOP)
      continue;

    /*
     * Message signals an error, the payload contains an nlmsgerr structure.
     * TODO - Add error handling later.
     */
    if (nh->nlmsg_type == NLMSG_ERROR) {

      DIAGD_DEBUG("%s:%d Got netlink error.", __FILE__, __LINE__);
      /* Note - The abort() function never returns */
      abort();
    }

    /* Continue with parsing packets. */
    ifinfo = NLMSG_DATA(nh);

    /* Get the net interface name */
    rc = if_indextoname(ifinfo->ifi_index, ifname);

    /* Check if the network interface is detected */
    DIAGD_TRACE("%s - ifi_flags=0x%X", ifname, ifinfo->ifi_flags);

    /* Search the net interface */
    diag_GetStartingAddr_NetIfInfo(ifname, &pNetIf);

    DIAGD_TRACE("%s - pNetIf=0x%X", __func__, (UINT32)pNetIf);

    /*
     * Check if found the net interface.
     * NOTE -
     *  To simplify the implementation, we don't handle a new hotplug
     *  net interface here.
     */
    if (pNetIf == NULL)
      continue;         /* possible a new net interface. Ignore the msg */

    /* Per experiments, when bridge is enabled (eg. issue the "bridge-start"
     * command), kernel sends two identical link change messages.
     * To handle the duplicated messages, we keep track the link state and,
     * 1. If the IFF_RUNNING flag set and link state is non-DIAG_NETLINK_UP,
     *    change link state to DIAG_NETLINK_UP and increase the counter.
     * 2. If IFF_RUNNING set and link state is DIAG_NETLINK_UP, ignore the msg.
     * 3. the Same algorithm to handle when the IFF_RUNNING flag is not set.
     */
    if (ifinfo->ifi_flags & IFF_RUNNING) {

      /* Check if th net interface state is up already */
      if (pNetIf->netlink_state != DIAG_NETLINK_UP) {
        /* The link state changed, let's
         * 1. Change link state
         * 2. Increase the link_ups count.
         */
        pNetIf->netlink_state = DIAG_NETLINK_UP;
        pNetIf->statistics[pNetIf->active_stats_idx].link_ups++;
      }
      DIAGD_TRACE("%s- net interface %d is up (idx=%d, link_up=%ld)",
                  ifname, ifinfo->ifi_index,
                  pNetIf->active_stats_idx,
                  pNetIf->statistics[pNetIf->active_stats_idx].link_ups);
    }
    else {

      if (pNetIf->netlink_state != DIAG_NETLINK_DOWN) {
        /* The link state changed, let's
         * 1. Change link state
         * 2. Increase the link_downs count.
         */
        pNetIf->netlink_state = DIAG_NETLINK_DOWN;
        pNetIf->statistics[pNetIf->active_stats_idx].link_downs++;
      }
      DIAGD_TRACE("%s- net interface %d is down (idx=%d, link_down=%ld)",
                  ifname, ifinfo->ifi_index,
                  pNetIf->active_stats_idx,
                  pNetIf->statistics[pNetIf->active_stats_idx].link_downs);
    }

  } /* end of FOR */

  pthread_mutex_unlock(&lock);

  DIAGD_TRACE("****%s: exit", __func__);

} /* end of diagd_Rd_Netlink_Msgs */




/*
 * =============================================================================
 * Network related APIs
 * =============================================================================
 */

/*
 * Query all known network interfaces on a Linux System.
 * To find every interface, we parse /proc/net/dev.
 *
 * Notes -
 * 1. 'lo' interface won't be included
 * 2. Can't find all network interface via the ioctl method. (The SIOCGIFCONF
 *    is getting current L3 interface)
 *
 * Input:
 * netif_addr_info - (O/P) return the all known network interfaces
 *
 * Output:
 * DIAGD_RC_ERR - Failed to access "/proc/net/dev"
 * DIAGD_RC_OK  - OK
 */
int Diag_MonNet_GetNetworkInterfaces(netIf_t *netif_info)
{
  char  line[512];
  char  *colon;
  char  *name;
  FILE  *fp;
  int rtn = DIAGD_RC_OK;

  do {

    memset(netif_info, 0, sizeof(*netif_info));

    if (0 == (fp = fopen("/proc/net/dev", "r"))) {

      rtn = DIAGD_RC_ERR;
      break;
    }

    while (0 != (name = fgets(line, 512, fp))) {

      while (isspace(name[0])) /* Trim leading whitespace */
        name++;

      colon = strchr (name, ':');
      if (colon) {

        *colon = 0;
        if (strcmp(name, "lo") != 0) {
          DIAGD_TRACE("%s:\n", name);
          strcpy(netif_info->netif_name[netif_info->nInterfaces], name);
          netif_info->nInterfaces++;
        }
      }
    }

    fclose(fp);

  } while (false);

  return (rtn);

} /* end of Diag_GetNetworkInterfaces */


/*
 * Get each network interface statistics
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_ERR - Failed to get the counters
 * DIAGD_RC_OK - OK
 */
int Diag_MonNet_GetNetIfStatistics()
{
  netIf_t   netif_info;
  int   i;
  int   rtn = DIAGD_RC_OK;          /* default is OK */

  DIAGD_ENTRY("%s: enter", __func__);

  do {

    if (diag_getStats_firstRun == false) {
      /* Check if wait time is expired */
      if (checkIfTimeout(DIAG_API_IDX_GET_NET_STATS) == false) {
        break;        /* Wait time is not expired. Exit */
      }
    }
    else {
      /* It is first time running routine after power-up. */
      diag_getStats_firstRun= false;     /* Clear the flag. */
    }

    /* Update the starting time of the api */
    time(&diagStartTm_getStats);

    memset (&netif_info, 0, sizeof(netif_info));

    /*
     * Get the exiting network interfaces every time prior to query the
     * statistics
     */
    if (Diag_MonNet_GetNetworkInterfaces(&netif_info) != DIAGD_RC_OK) {

      rtn = DIAGD_RC_ERR;           /* Failed. Exit */
      break;
    }

    for (i = 0; i < netif_info.nInterfaces; i++) {

      /* Let's get the statistics counters of each interface */
      if (diag_Get_Netif_Counters(netif_info.netif_name[i], true) != DIAGD_RC_OK) {
        break;
      }
    }

    /* Now check the link up/down counts */
    diag_Check_NetLinkUpDownCounts();

  } while (false);

  DIAGD_EXIT("%s: exit", __func__);

  return(rtn);

} /* end of Diag_MonNet_GetNetIfStatistics */



/*
 * Monitor MoCA tx/rx discard packet counters
 * If tx or rx discard packet counts over threshold, log the necessary
 * MoCA statistics counters into moca log file.
 *
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK - OK
 */
int Diag_MonMoca_Err_Counts(void)
{
  int   rtn = DIAGD_RC_OK;          /* default is OK */

  DIAGD_TRACE("%s: enter", __func__);

  do {

    if (diag_moca_monErrCnts_firstRun == false) {
      /* Check if wait time is expired */
      if (checkIfTimeout(DIAG_API_IDX_MOCA_MON_ERR_CNTS) == false) {
        break;        /* Wait time is not expired. Exit */
      }
    }
    else {
      /* It is first time running routine after power-up. */
      diag_moca_monErrCnts_firstRun= false;     /* Clear the flag. */
    }

    /* Update the starting time of the api */
    time(&diagStartTm_moca_monErrCnts);

    /* Monitor MoCA Tx/Rx error counters */
    diagMoca_MonErrorCounts();

  } while (false);

  DIAGD_EXIT("%s: exit", __func__);

  return(rtn);
} /* end of Diag_MonMoca_Err_Counts */


/*
 * Monitor MoCA service performance
 *
 * Check the following info:
 *  1. rxUc phy rate
 *  2. rx power level
 *  3. avarage SNR
 *  4. rxUc bit-loading
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK - OK
 */
int Diag_MonMoca_ServicePerf(void)
{
  int   rtn = DIAGD_RC_OK;          /* default is OK */

  DIAGD_TRACE("%s: enter", __func__);

  do {

    if (diag_moca_monServicePerf_firstRun == false) {
      /* Check if wait time is expired */
      if (checkIfTimeout(DIAG_API_IDX_MOCA_MON_SERVICE_PERF) == false) {
        break;        /* Wait time is not expired. Exit */
      }
    }
    else {
      /* It is first time running routine after power-up. */
      diag_moca_monServicePerf_firstRun= false;     /* Clear the flag. */
    }

    /* Update the starting time of the api */
    time(&diagStartTm_moca_monServicePerf);

    /* Monitor MoCA service performance. */
    diagMoca_MonServicePerf();

  } while (false);

  DIAGD_EXIT("%s: exit", __func__);

  return(rtn);

} /* end of Diag_MonMoca_ServicePerf */

/*
 * Cleanup of diagd
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others       - Initialization failed
 *
 */
int diagd_UnInit()
{
  int   rtn = DIAGD_RC_OK;          /* default is OK */

  /* Close the netlink socket, if it's opened */
  if (pDiagInfo->netlinkSock != DIAG_SOCKET_NOT_OPEN)
    close(pDiagInfo->netlinkSock);

  /* Close the log file if it's opened */
  diagtCloseEventLogFile();

  /* Close the result log file if it's opened */
  diagtCloseTestResultsLogFile();

  return(rtn);

} /* end of diagd_UnInit */


/*
 * Network related APIs (MoCA)
 */


/* ======================================================================= */
#if 0
/* ======================================================================= */


/*
 * Network related APIs (Wireless)
 */
/* Just a thought on this monitoring */
int Diag_MonWifi_CheckAPPower(TBD)
{
} /* end of Diag_MonWifi_CheckAPPower */


/*
 * GPIO related APIs
 */
int Diag_MonIO_GetResetReason(TBD)
{
} /* end of Diag_GetResetReason */


int Diag_MonIO_CheckFanSpeed(TBD)
{
} /* end of Diag_MonIO_CheckFanSpeed */


/* TBD */
int Diag_MonIO_GetCpuTemp(TBD)
{
} /* end of Diag_MonIO_GetCpuTemp */


/*
 * Turn On or off the LED
 */
DiagErrCode Diag_toggleLED(TBD)
{
} /* end of Diag_toggleLED */


/* ======================================================================= */
#endif /* end of if 0 */
/* ======================================================================= */






