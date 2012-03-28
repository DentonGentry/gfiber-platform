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


/* -------------------------------------------------------------------------
 *
 * Internal defines, types and variables
 *
 *--------------------------------------------------------------------------
 */

/* Reference PHY rates of connection quality per number of connected nodes */
diag_moca_connt_qlty_ref_t diagMoca_connQltyTbl = {
  {100000000,        /* connected nodes - 1 */
   100000000,        /* connected nodes - 2 */
   100000000,        /* connected nodes - 3 */
   100000000,        /* connected nodes - 4 */
   100000000,        /* connected nodes - 5 */
   100000000,        /* connected nodes - 6 */
   100000000,        /* connected nodes - 7 */
   100000000,        /* connected nodes - 8 */
   100000000,        /* connected nodes - 9 */
   100000000,        /* connected nodes - 10*/
   100000000,        /* connected nodes - 11 */
   100000000,        /* connected nodes - 12 */
   100000000,        /* connected nodes - 13 */
   100000000,        /* connected nodes - 14 */
   100000000,        /* connected nodes - 15 */
   100000000},       /* connected nodes - 16 */
};



/*
 * A reference table of MoCA node service performance.
 * TODOs 2011/11/30
 * 1) The current data in the table is temp data.
 * 2) Later, we need HW engineer to provide measure data.
 */
diag_moca_ref_tbl_t diagMocaPerfReferenceTable[DIAG_MOCA_PERF_LVL_MAX] = {

  /* Reference node data of DIAG_MOCA_PERF_LVL_GOOD */
  {180000000,                 /* rxUcPhyRate */
   -50,                       /* rxUcGain   = -50 dBm */
   35.0,                      /* rxUcAvgSnr = 35.0 dB */
   /* rxUcBitLoading */
   {0x00006666, 0x66666666, 0x66666666, 0x66666666,
    0x66666666, 0x66666666, 0x66666666, 0x66666666,
    0x66666666, 0x66666666, 0x66666666, 0x66666666,
    0x66666666, 0x66666666, 0x66660000, 0x00000000,
    0x00000000, 0x00000666, 0x66666666, 0x66666666,
    0x66666666, 0x66666666, 0x66666666, 0x66666666,
    0x66666666, 0x66666666, 0x66666666, 0x66666666,
    0x66666666, 0x66666666, 0x66666666, 0x66666000},
  },

  /* Reference node data of DIAG_MOCA_PERF_LVL_POOR */
  {120000000,                 /* rxUcPhyRate */
   -60,                       /* rxUcGain   = -60 dBm */
   30.0,                      /* rxUcAvgSnr = 30.0 dB */
   /* rxUcBitLoading */
   {0x00004444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44440000, 0x00000000,
    0x00000000, 0x00000444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444000,},
  },
};


/* file descriptor for accessing mocad */
static void  *g_mocaHandle = NULL;
static pthread_mutex_t diagMoca_Mutex;
static pthread_cond_t diagMoca_cond;

/* NOTE -
 * For FMR feature, get fmr data in the fmr callback routine, so
 * 1) Allocate Memory in diagMoca_FmrResponseCb()
 * 2) Free the allocated memory in diagMoca_GetConnInfo()
 */
static diag_moca_node_connect_info_t *pNodeConnInfo = NULL;
/* bConnInfoValid = false, if If the diagMoca_FmrResponseCb failed */
static bool   bConnInfoValid = false;



/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */

/*
 * Convert from seconds to Hr:Mins:Secs
 *
 * Input:
 * timeInSecs   - (IN) Point to starting address of performance status
 * pTimeInHrs   - (OUT) Number of hours
 * pTimeInMin   - (OUT) Number of mins
 * pTimeInSecs  - (OUT) Number of secs
 *
 * Output:
 * none
 */
static void diagMoca_ConvertUpTime(uint32_t timeInSecs, uint32_t *pTimeInHrs,
                          uint32_t *pTimeInMin, uint32_t *pTimeInSecs)
{
  *pTimeInHrs  = timeInSecs / (NO_OF_SECS_IN_MIN * NO_OF_MINS_IN_HR) ;
  timeInSecs   = timeInSecs % (NO_OF_SECS_IN_MIN * NO_OF_MINS_IN_HR) ;
  *pTimeInMin  = timeInSecs / NO_OF_SECS_IN_MIN ;
  timeInSecs   = timeInSecs % NO_OF_SECS_IN_MIN ;
  *pTimeInSecs = timeInSecs ;
} /* end of diagMoca_ConvertUpTime */


/*
 * Build message header to the messages of MoCA log
 *
 * Input:
 * pHdr  - Point to header
 * msgType
 * msgLen
 *
 * Output:
 * none
 */
static void diagMoca_buildHdrMocaLogMsg(
  diag_moca_log_msg_hdr_t *pHdr,
  uint16_t  msgType, uint16_t msgLen)
{
  time_t  currtime;

  time(&currtime);
  pHdr->msgType = msgType;
  pHdr->currTime = localtime(&currtime);
  pHdr->msgLen = msgLen;

  DIAGD_TRACE("%s: msgHdr   msgType=0x%x, msgLen=%u",
              __func__, pHdr->msgType, pHdr->msgLen);

} /* end of diagMoca_buildHdrMocaLogMsg */



/*
 * Processes nodestats command.
 *
 * Input:
 * pNodeBL    - (IN) Point to starting address of node bit-loading data
 * pPerfLevel - (OUT)
 *
 * Output:
 * none
 */
static void diagMoca_CompareBitLoading(uint32_t *pNodeBL, uint8_t *pPerfLevel)
{
  int       rtn = DIAGD_RC_OK;
  int       i;
  uint8_t   perfLevel;
  uint32_t *pRefBL;
  uint32_t  refBlData, nodeBlData;

  DIAGD_ENTRY("%s: BIT_LOADING_LEN: %d", __func__, BIT_LOADING_LEN);

  for (perfLevel = 0; perfLevel < DIAG_MOCA_PERF_LVL_MAX; perfLevel++) {

    rtn = DIAGD_RC_OK;    /* Reset to DIAGD_RC_OK before loop started */

    pRefBL = (uint32_t *)&diagMocaPerfReferenceTable[perfLevel].rxUcBitLoading[0];

    for (i = 0; i < BIT_LOADING_LEN; i++) {
      /* Compare node's bit-loading data to bit-loading data in ref table */
      /* TODO/TBD 20111017 -
       *  How many times the Node bit-loading are wore than ref data to be
       *  returned error?????
       */
      refBlData = pRefBL[i];

// =========> TODO 2011/11/30 Let's consider to compare per sub-carrier

      nodeBlData = pNodeBL[i];
      nodeBlData = (nodeBlData<<28) | ((nodeBlData&0xf0)<<20)
                   | ((nodeBlData&0xf00)<<12) | ((nodeBlData&0xf000)<<4)
                   | ((nodeBlData&0xf0000)>>4) | ((nodeBlData&0xf00000)>>12)
                   | ((nodeBlData&0xf000000)>>20) | nodeBlData >>28 ;

      DIAGD_TRACE("%s: idx: %u, nodeBlData: %8.8x, refBlData: %8.8x",
                 __func__, i, nodeBlData, refBlData);

      if (nodeBlData < refBlData) {
        /* node BL data is worse then data in ref table. Exit. */
        rtn = DIAGD_RC_ERR;
        break;
      }
    } /* for BIT_LOADING_LEN */

    /* Check the compare result */
    if (rtn == DIAGD_RC_OK) {
      DIAGD_TRACE("%s: perfLevel: %d", __func__, perfLevel);
      break;
    }

  } /* for DIAG_MOCA_PERF_LVL_MAX */

  /* Return the performance level result */
  *pPerfLevel = perfLevel;

  DIAGD_EXIT("%s: Bit-Loading Result: %s", __func__,
             (perfLevel == DIAG_MOCA_PERF_LVL_GOOD)? "Good" :
             (perfLevel == DIAG_MOCA_PERF_LVL_POOR)? "Poor" : "Impaired");

} /* end of diagMoca_CompareBitLoading */


/*
 * Call back return.
 * It is originally from pqos_callback_return() of mocactl.c
 *
 * Input:
 * ctx        - (IN) MoCA handle.
 *
 * Output:
 * None
 */
static void diagMoca_callback_return(void * ctx)
{

  moca_cancel_event_loop(ctx);
  pthread_mutex_lock(&diagMoca_Mutex);
  pthread_cond_signal(&diagMoca_cond);
  pthread_mutex_unlock(&diagMoca_Mutex);

} /* end of diagMoca_callback_return */


/*
 * Start event loop
 * It is originally from moca_start_event_loop() of mocactl.c
 *
 * Input:
 * ctx        - (IN) MoCA handle.
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * DIAGD_RC_ERR - Failed
 */
static int diagMoca_startEventLoop(void *ctx, pthread_t *thread)
{
  int rtn = DIAGD_RC_OK;
  int ret;

  do {

    pthread_mutex_init(&diagMoca_Mutex, NULL);
    pthread_cond_init(&diagMoca_cond, NULL);

    ret = pthread_create(thread, NULL, (void *)moca_event_loop, ctx);
    if (ret != 0) {
      DIAGD_DEBUG("%s: pthread_create() failed (error=%d)",
                  __func__, ret);
      rtn = DIAGD_RC_ERR;
      break;
    }

    /* Give the thread a chance to run */
    usleep(1000);

  } while (false);

  return(rtn);
} /* end of diagMoca_startEventLoop */



/*
 * Wait for event.
 * It is originally from moca_wait_for_event() of mocactl.c
 *
 * Input:
 * ctx        - (IN) MoCA handle.
 *
 * Output:
 * DIAGD_RC_OK  - OK
 *
 */
static int diagMoca_WaitForEvent(void *ctx, uint32_t timeout_s)
{
  struct timeval now;
  struct timespec timeout;
  int ret;

  gettimeofday(&now, NULL);
  timeout.tv_sec = now.tv_sec + timeout_s; /* wait 5 seconds for the response */
  timeout.tv_nsec = now.tv_usec * 1000;
  pthread_mutex_lock(&diagMoca_Mutex);
  ret = pthread_cond_timedwait(&diagMoca_cond, &diagMoca_Mutex, &timeout);
  pthread_mutex_unlock(&diagMoca_Mutex);

  if (ret == ETIMEDOUT) {
    ret = DIAGD_RC_PTHREAD_WAIT_TIMEOUT;     /* Change to diagd error code */
    DIAGD_DEBUG("%s: pthread_cond_timedwait: timed-out.", __func__);
  }

  pthread_cond_destroy(&diagMoca_cond);
  pthread_mutex_destroy(&diagMoca_Mutex);
  return(ret);

} /* end of diagMoca_WaitForEvent */


/*
 * Get mac addresses of active nodes .
 * It is originally from fmr_response_cb() of mocactl.c
 *
 * Input:
 * ctx              - (IN) MoCA handle.
 * pNodeMacAddrTbl  - (OUT) point to the starting address of
 *                          the node mac address table.
 *                    The memory must be allocated by the caller.
 *
 * Output:
 * DIAGD_RC_OK  - OK
 */
static int diagMoca_getActiveNodes(
  void *ctx,
  diag_moca_node_mac_table_t *pNodeMacAddrTbl)
{
  int   rtn = DIAGD_RC_OK;
  int   i;
  struct moca_gen_status       gs;
  struct moca_gen_node_status  gsn;
  struct moca_init_time        init;
  diag_moca_node_mac_t        *pNode = &pNodeMacAddrTbl->nodemacs[0];

  DIAGD_ENTRY("%s: ", __func__);
  memset(pNodeMacAddrTbl, 0, sizeof(*pNodeMacAddrTbl));
  memset(&gs, 0, sizeof(gs));

  /* get active node bitmask */
  moca_get_gen_status(ctx, &gs);

  /* get status entry for each node */
  for(i = 0; i < MOCA_MAX_NODES; i++) {

    if((gs.connected_nodes & (1 << i)) == 0)
       continue;      /* Not active. Next one */

    pNodeMacAddrTbl->connected_nodes++;

    pNode[i].active = DIAG_MOCA_NODE_ACTIVE;
    /* Check if it is self-node */
    if(gs.node_id == i)
    {
      /* It's a self node */
      pNodeMacAddrTbl->selfNodeId = i;
      moca_get_init_time(ctx, &init);
      moca_u32_to_mac(pNode[i].macAddr, init.mac_addr_hi, init.mac_addr_lo);
    }
    else
    {
      moca_get_gen_node_status(ctx, i, &gsn);
      moca_u32_to_mac(pNode[i].macAddr, gsn.eui_hi, gsn.eui_lo);
    }
  }

  for(i = 0; i < MOCA_MAX_NODES; i++) {
    DIAGD_TRACE ("%2i (active=%u)   %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
            i, pNode[i].active,
            pNode[i].macAddr[0], pNode[i].macAddr[1], pNode[i].macAddr[2],
            pNode[i].macAddr[3], pNode[i].macAddr[4], pNode[i].macAddr[5]);
  }

  DIAGD_EXIT("%s: ", __func__);

  return (rtn);

} /* diagMoca_getActiveNodes */


/*
 * The FMR calls The FMR trap with the FMR information.
 * It is originally from fmr_response_cb() of mocactl.c
 *
 * Input:
 * arg      - (IN) Instance of access mocad
 * in       - (IN) FMR information
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
static void diagMoca_FmrResponseCb(void *ctx, struct moca_fmr_response *in)
{
  int       rtn = DIAGD_RC_ERR;
  int       i, j, node;
  uint32_t *pRespondedNode;
  uint32_t  refPhyRate;
  uint16_t *pFmrinfoNode;
  diag_moca_node_info_t          *pNodeInfo;
  diag_moca_node_phy_info_t      *pRxNodePhyInfo;
  diag_moca_node_mac_table_t      nodeMacAddrTbl;
  diag_moca_node_mac_t           *pNodeMac = &nodeMacAddrTbl.nodemacs[0];


  DIAGD_ENTRY("%s: ", __func__);

  do {

    if (pNodeConnInfo == NULL) {
      DIAGD_DEBUG("%s: malloc failed", __func__);
      break;
    }

    memset(pNodeConnInfo, 0, sizeof(diag_moca_node_connect_info_t));

    /* Get node IDs */
    diagMoca_getActiveNodes(ctx, &nodeMacAddrTbl);

    for (node = 0, i = 0; i < MAX_RSP_NODES; i++) {
      /* Get the starting address of the responded node */
      pRespondedNode = (void *)&in->responded_node_0 +
          ((sizeof(in->responded_node_0)*i) + (sizeof(in->fmrinfo_node_0)*i));

      /* Point the FMR information nodes */
      pFmrinfoNode = (uint16_t *)(pRespondedNode + 1);

      /* Check if it's a valid node id */
      if (*pRespondedNode == DIAG_MOCA_INVALID_NODE_ID) {
        continue;       /* An invalid ID. Go next one */
      }

      /* Get the starting address of nodeInfo[] */
      pNodeInfo = &pNodeConnInfo->nodeInfo[node];

      /* Point to the starting address of nodePhyInfo[] */
      pRxNodePhyInfo = &pNodeInfo->rxNodePhyInfo[0];

      /* Get the TX Node ID and it's MAC address */
      pNodeInfo->txNodeId = *pRespondedNode;
      DIAGD_TRACE("%s: txNodeId=%d", __func__, pNodeInfo->txNodeId);

      memcpy(&pNodeInfo->macAddr[0],
             &nodeMacAddrTbl.nodemacs[pNodeInfo->txNodeId].macAddr[0],
             MAC_ADDR_LEN);

      for (j = 0; j < MoCA_MAX_NODES; j++, pRxNodePhyInfo++) {
        /* Get nBas */
        pRxNodePhyInfo->rxUcPhyRate = pFmrinfoNode[j] & 0x7FF;
        /* Get GAP */
        pRxNodePhyInfo->cp = pFmrinfoNode[j] >> 11;
        /* Get CP if GAP is non-zero */
        if (pRxNodePhyInfo->cp > 0) {
          /* CP = (GAP * 2) + 10 */
          pRxNodePhyInfo->cp = (pRxNodePhyInfo->cp * 2) + 10;
        }
        /* Get rxUcPhyRate - For Bruno, we don't use turbo mode */
        pRxNodePhyInfo->rxUcPhyRate = moca_phy_rate(
                                      pRxNodePhyInfo->rxUcPhyRate,
                                      (unsigned long)pRxNodePhyInfo->cp,
                                      (unsigned long)0);
      } /* end of for (MoCA_MAX_NODES) */

      node++;

    } /* end of for (MAX_RSP_NODES) */

    /* Set self node ID */
    pNodeConnInfo->selfNodeId = (uint32_t)nodeMacAddrTbl.selfNodeId;

    /* Based on the connected nodes to rate the connection quality */
    pNodeConnInfo->nodeInfoTblSize = 0;
    if (node > 0) {
      pNodeConnInfo->nodeInfoTblSize += (sizeof(diag_moca_node_info_t) * node);

      /* Check the connection quality of nodes */
      /* Get the reference of phy rate based on the connected nodes
       * The reference PHY rate is located at (node - 1) index.
       */
      refPhyRate = diagMoca_connQltyTbl.refPhyRate[nodeMacAddrTbl.connected_nodes - 1];
      for (i = 0; i < node; i++) {
        /* Get the starting address of nodeInfo[] */
        pNodeInfo = &pNodeConnInfo->nodeInfo[i];

        for (j = 0; j < MoCA_MAX_NODES; j++)
        {
          /* Check if the node is in the MoCA network */
          if (pNodeMac[j].active == DIAG_MOCA_NODE_ACTIVE) {

            /* Compare the node PHY rate to the reference PHY rate */
            if (pNodeInfo->rxNodePhyInfo[j].rxUcPhyRate > refPhyRate) {
              pNodeInfo->rxNodePhyInfo[j].connQuality = DIAG_MOCA_CONN_QLTY_GOOD;
            }
            else {
              pNodeInfo->rxNodePhyInfo[j].connQuality = DIAG_MOCA_CONN_QLTY_IMPAIRED;
            }
          }
          else {
            /* The node is not in the MoCA network */
            pNodeInfo->rxNodePhyInfo[j].connQuality = DIAG_MOCA_CONN_QLTY_NOT_CONN;
          }
          DIAGD_TRACE("%s: txNode=%d, rxNode=%d, rxUcPhyRate=%u, cp=%u, Qlty=%u",
                      __func__, pNodeInfo->txNodeId, j,
                      pNodeInfo->rxNodePhyInfo[j].rxUcPhyRate,
                      pNodeInfo->rxNodePhyInfo[j].cp,
                      pNodeInfo->rxNodePhyInfo[j].connQuality);
        } /* end of for (MoCA_MAX_NODES)*/

      } /* for (nodes)*/

    } /* if (node > 0) */

    rtn = DIAGD_RC_OK;

  } while (false);


  if (rtn == DIAGD_RC_OK) {
    /* Indicate the data in pNodeConnInfo memory is valid */
    bConnInfoValid = true;
  }

  /* Return the control the event */
  diagMoca_callback_return(g_mocaHandle);

  DIAGD_EXIT("%s: ", __func__);

} /* end of diagMoca_FmrResponseCb */




/*
 * Retrieve node statistics
 * Caller should allocation the memory of diag_moca_config_t structure.
 *
 * Input:
 * pStats   - (OUT) Point to diag_moca_stats_t.
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagMoca_GetStats(diag_moca_stats_t *pStats)
{
  int   rtn = DIAGD_RC_ERR;
  int   i;
  CmsRet nRet = CMSRET_SUCCESS;
  int count;
  PMoCA_NODE_STATISTICS_EXT_ENTRY pTotalExtStats;
  MoCA_NODE_STATISTICS_EXT_ENTRY  nodeStats;

  memset(pStats, 0, sizeof(diag_moca_stats_t));
  pTotalExtStats = &pStats->totalExtStats;

  /* Retrieve current statistic information of MoCA interface */
  nRet = MoCACtl2_GetStatistics(g_mocaHandle, &pStats->stats, 0);

  /* Accumulate node extended statistics for all nodes*/
  for (count = 0; count<MoCA_MAX_NODES; count++)
  {
    unsigned int *dest = (unsigned int *) pTotalExtStats;
    unsigned int *src = (unsigned int *) &nodeStats;

    nodeStats.nodeId = count ;
    nRet = MoCACtl2_GetNodeStatisticsExt( g_mocaHandle, &nodeStats, 0) ;

    if (nRet == CMSRET_SUCCESS)
    {
      for (i = 0; i<sizeof(nodeStats)/sizeof(unsigned int); i++)
          *(dest++) += *(src++);
    }
  }

  /* Convert the returned code to diagd's return code. */
  if (nRet == CMSRET_SUCCESS)
    rtn = DIAGD_RC_OK;

  return(rtn);
} /* end of diagMoca_GetStats */



/*
 * Processes get config command.
 * Caller should allocation the memory of diag_moca_config_t structure.
 *
 * Input:
 * pCfg   - (OUT) Point to diag_moca_config_t.
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagMoca_GetConfig(diag_moca_config_t *pCfg)
{
  int   rtn = DIAGD_RC_ERR;
  CmsRet nRet = CMSRET_SUCCESS;
  MoCA_INITIALIZATION_PARMS mocaInit;

  do {

    nRet = MoCACtl2_GetInitParms(g_mocaHandle, &mocaInit);

    if (nRet != CMSRET_SUCCESS) {

      break;
    }

    memset(pCfg, 0x00, sizeof(diag_moca_config_t));
    pCfg->rfType = mocaInit.rfType;

    /* Read the current global configuration. */
    nRet = MoCACtl2_GetCfg( g_mocaHandle, &pCfg->Cfg, MoCA_CFG_PARAM_ALL_MASK);

    /* Convert the returned code to diagd's return code. */
    if (nRet == CMSRET_SUCCESS)
      rtn = DIAGD_RC_OK;

  } while (false);

  return(rtn);

} /* end of diagMoca_GetConfig */


/*
 * Processes get initparms command.
 * Caller should allocation the memory of MoCA_INITIALIZATION_PARMS structure.
 *
 * Input:
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others -       refer to enum CmsRet in BRCM cms.h
 */
int diagMoca_GetInitParms(PMoCA_INITIALIZATION_PARMS pInitParms)
{
  int   rtn = DIAGD_RC_ERR;

  /* Read the current global configuration. */
  rtn = (int)MoCACtl2_GetInitParms(g_mocaHandle, pInitParms) ;

  return(rtn);
} /* end of diagMoca_GetInitParms */


/*
 * Retrieve current status information of the self-node
 * Caller should allocation the memory of MoCA_STATUS structure.
 *
 * Input:
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * others -       refer to enum CmsRet in BRCM cms.h
 */
int diagMoca_GetStatus(PMoCA_STATUS pStatus)
{
  int   rtn = DIAGD_RC_ERR;

  memset(pStatus, 0x00, sizeof(MoCA_STATUS));

  /* Retrieve the current status information of the self node */
  rtn = (int)MoCACtl2_GetStatus(g_mocaHandle, pStatus) ;

  return(rtn);

} /* end of diagMoca_GetStatus */


/*
 * Processes getting nodestatus command.
 * Caller should allocation the memory of diag_moca_node_stats_table_t
 * structure.
 *
 * Input:
 * pNodeStats   - Pointer of the node statistics table
 * pSize        - (IN/OUT)Actual table size
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others -       Refer to enum CmsRet in BRCM cms.h
 */
int diagMoca_GetNodeStatistics(
      diag_moca_node_stats_table_t *pNodeStats,
      uint16_t *pSize)
{
  int       rtn = DIAGD_RC_ERR;
  uint32_t  statsTblSize, extStatsTblSize;
  int       node;
  uint32_t  prevConnectedNodes;
  int       idx = 0, j;
  diag_moca_node_mac_table_t      nodeMacTbl;
  diag_moca_node_mac_t           *pMacAddr = NULL;
  diag_moca_node_stats_entry_t   *pNodeStatsEntry = NULL;
  struct moca_gen_status          gs;
  MoCA_NODE_STATISTICS_ENTRY      nodeStats[MoCA_MAX_NODES];
  MoCA_NODE_STATISTICS_EXT_ENTRY  nodeStatsExt[MoCA_MAX_NODES];


  DIAGD_ENTRY("%s", __func__);

  do {

    memset(pNodeStats, 0x00, *pSize);

    /* get active node bitmask */
    moca_get_gen_status(g_mocaHandle, &gs);
    prevConnectedNodes = gs.connected_nodes;

    /* Get node statistics w/o reset */
    rtn = (int)MoCACtl2_GetNodeTblStatistics(
              g_mocaHandle, &nodeStats[0], &statsTblSize, (uint32_t)0);
    if (rtn != DIAGD_RC_OK) {
      break;
    }

    /* Get node extended statistics w/o reset */
    rtn = (int)MoCACtl2_GetNodeTblStatisticsExt(
              g_mocaHandle, &nodeStatsExt[0], &extStatsTblSize, (uint32_t)0);
    if (rtn != DIAGD_RC_OK) {
      DIAGD_DEBUG("%s: MoCACtl2_GetNodeTblStatisticsExt() failed (rtn=0x%x)",
                  __func__, rtn);
      break;
    }

    /* Get Mac Address of active nodes */
    diagMoca_getActiveNodes(g_mocaHandle, &nodeMacTbl);

    /* Get current active node bitmask again to check if topology changed. */
    moca_get_gen_status(g_mocaHandle, &gs);
    if (prevConnectedNodes != gs.connected_nodes) {
      if (idx < 2) {
        DIAGD_DEBUG("%s: Topology Changed (connectedNode-Prev=0x%08X, curr=0x%08X.",
                    __func__, prevConnectedNodes, gs.connected_nodes);
        break;
      }
      /* */
      idx++;
      continue;
    }

    /* Copy the statistics counters into the table */
    pMacAddr    = &nodeMacTbl.nodemacs[0];
    pNodeStatsEntry = &pNodeStats->Stats;
    for(node = 0, idx = 0; idx < nodeMacTbl.connected_nodes; idx++, pMacAddr++) {

      if (pMacAddr->active != DIAG_MOCA_NODE_ACTIVE) {
        continue;
      }

      if (nodeMacTbl.selfNodeId == idx) {
        continue;
      }

      /* Copy the node ID and MAC address. */
      pNodeStatsEntry[node].nodeId = idx;

      DIAGD_TRACE("%s: nodeID=%2i   %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
              __func__, idx,
              pMacAddr->macAddr[0], pMacAddr->macAddr[1], pMacAddr->macAddr[2],
              pMacAddr->macAddr[3], pMacAddr->macAddr[4], pMacAddr->macAddr[5]);

      memcpy(&pNodeStatsEntry[node].macAddr[0], &pMacAddr->macAddr[0], MAC_ADDR_LEN);

      /* Get the entry of the node statistics */
      for (j = 0; j < (statsTblSize/sizeof(MoCA_NODE_STATISTICS_ENTRY)); j++) {
        if (nodeStats[j].nodeId != idx)
          continue;
        /* Copy the statistics to the database */
        memcpy(&pNodeStatsEntry[node].nodeStats,
               &nodeStats[j],
               sizeof(MoCA_NODE_STATISTICS_ENTRY));
        break;    /* Done. Exit from loop */
      } /* end of for */

      /* Get the entry of the extended node statistics */
      for (j = 0; j < (statsTblSize/sizeof(MoCA_NODE_STATISTICS_ENTRY)); j++) {
        if (pNodeStatsEntry[j].nodeId != idx)
          continue;
        /* Copy the extended statistics to the database */
        memcpy(&pNodeStatsEntry[node].nodeStatsExt,
               &nodeStatsExt[j],
               sizeof(MoCA_NODE_STATISTICS_EXT_ENTRY));
        break;    /* Done. Exit from loop */
      } /* end of for */

      node++;      /* Increase the node index */
    } /* end of for */

    /* Get the total table size */
    pNodeStats->nodeStatsTblSize = 0;
    if (node != 0) {
      pNodeStats->nodeStatsTblSize += (sizeof(diag_moca_node_stats_entry_t) * node);
    }

    *pSize = sizeof(uint32_t);    /* default size is nodeStatsTblSize */
    /* Actual size = node table size + sizeof (nodeStatsTblSize) */
    *pSize += pNodeStats->nodeStatsTblSize;

    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s: rtn=0x%x (*pSize=%u)", __func__, rtn, *pSize);

  return(rtn);

} /* diagMoca_GetNodeStatistics */


/*
 * Retrieve current node status table
 * Caller should allocation the memory of diag_moca_nodestatus_t structure.
 *
 * Input:
 * pNodeStatus -  Point to the location of node status table.
 * pBufLen     -  (IN/OUT) input buffer length, output the actual data length
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others       - refer to enum CmsRet in BRCM cms.h
 */
int diagMoca_GetNodeStatus(
    diag_moca_nodestatus_t *pNodeStatus, uint32_t *pBufLen)
{
  int     rtn = DIAGD_RC_ERR;
  CmsRet  nRet = CMSRET_SUCCESS;

  memset(pNodeStatus, 0x00, *pBufLen);

  nRet = MoCACtl2_GetNodeTblStatus(
                g_mocaHandle,
                &pNodeStatus->nodeStatus[0],
                &pNodeStatus->nodeCommonStatus,
                &pNodeStatus->nodeStatusTblSize);

  /* Convert the mocad's return code to diagd's return code. */
  if (nRet == CMSRET_SUCCESS) {
      rtn = DIAGD_RC_OK;

    /* Calculate the actual table length */
    *pBufLen = offsetof(diag_moca_nodestatus_t, nodeStatus) +
               pNodeStatus->nodeStatusTblSize;
  }

  DIAGD_EXIT("%s: rtn=0x%x (nodeStatusTblSize=%u, *pBufLen=%u)",
             __func__, rtn, pNodeStatus->nodeStatusTblSize, *pBufLen);

  return(rtn);

} /* diagMoca_GetNodeStatus */


/*
 * Get node connection information via FMR process
 * It is originally from FmrHandler() of mocactl.c
 *
 * NOTE -
 *
 *
 * Input:
 * pConnInfo - (OUT) Point to database to save the FMR information
 *                   The memory must be allocated by the caller.
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * DIAGD_RC_ERR - Failed
 * others       - refer to enum CmsRet in BRCM cms.h
 */
int diagMoca_GetConnInfo(diag_moca_node_connect_info_t *pConnInfo)
{
  int             rtn = DIAGD_RC_OK;
  MoCA_FMR_PARAMS fmrParams ;
  CmsRet          nRet = CMSRET_SUCCESS ;
  pthread_t       event_thread;

  memset (&fmrParams, 0x00, sizeof(fmrParams)) ;

  /* Use broadcast address to query node connection information
   * of all active nodes
   */
  fmrParams.address[0] = 0xFFFFFFFF;
  fmrParams.address[1] = 0xFFFFFFFF;


  /* By default, set bConnInfoValid flag to indicate the data is
   * invalid in pNodeConnInfo
   */
  pNodeConnInfo = pConnInfo;
  bConnInfoValid = false;

  rtn = diagMoca_startEventLoop(g_mocaHandle, &event_thread);
  if (rtn == DIAGD_RC_OK) {
    moca_register_fmr_response_cb(
            g_mocaHandle,
            &diagMoca_FmrResponseCb,
            g_mocaHandle);
    nRet = MoCACtl2_Fmr(g_mocaHandle, &fmrParams);
    if (nRet == CMSRET_SUCCESS) {
      rtn = diagMoca_WaitForEvent(g_mocaHandle, 5);
    }
    else {
      rtn = nRet;
      DIAGD_DEBUG("%s: MoCACtl2_Fmr() failed (error=%d)", __func__, nRet);
    }
  }

  if ((rtn == DIAGD_RC_OK) && (bConnInfoValid != true)) {
    rtn = DIAGD_RC_ERR;
  }

  /* Restore to defaults */
  pNodeConnInfo = NULL;
  bConnInfoValid = false;

  return (rtn);

} /* diagMoca_GetConnInfo */


/*
 * Initailization of diagd
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others       - Initialization failed

 */
int diagd_MoCA_Init()
{
  int   rtn = DIAGD_RC_OK;          /* default is OK */


  DIAGD_ENTRY("%s", __func__);

  do {

    /* Open mocad */
    g_mocaHandle = MoCACtl_Open( NULL );
    if (g_mocaHandle == NULL) {
      /* Failed to open mocad */
      DIAGD_DEBUG("%s: MoCACtl_Open failed", __func__);

      rtn = DIAGD_RC_FAILED_OPEN_MOCAD;
      break;
    }

    rtn = diagtOpenMocaLogFile();

  } while (false);

  DIAGD_EXIT("%s - rtn=0x%X", __func__, rtn);

  return(rtn);

} /* end of diagd_MoCA_Init */


void diagd_MoCA_UnInit()
{
  /* Close the netlink socket, if it's opened */
  if (g_mocaHandle !=  NULL) {
    MoCACtl_Close(g_mocaHandle);
    g_mocaHandle = NULL;
  }

  /* Close the MoCA log file if it is opened */
  diagtCloseMocaLogFile();

} /* end of diagd_MoCA_UnInit */


/*
 * Monitor MoCA Error counters
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * DIAGD_RC_ERR - Failed
 */
int diagMoca_MonErrorCounts(void)
{
  int       rtn = DIAGD_RC_ERR;
  CmsRet    nRet = CMSRET_SUCCESS;
  uint32_t  totalPkts, discardPkts;
  bool      err;
  uint16_t  txDiscardTooManyMsg, rxDiscardTooManyMsg;
  MoCA_STATISTICS       mocaStats;
  diag_mocaIf_info     *pMocaIf = &pDiagInfo->mocaIf;
  diag_mocaIf_stats_t  *pCurr;
  diag_mocaIf_stats_t  *pPrev;
  diag_mocaIf_stats_t  *pDelta = &pMocaIf->delta_stats;
  diag_mocalog_discardpkts_exceed_t  *pMsg = NULL;
  MoCA_NODE_STATISTICS_EXT_ENTRY cummulativeExtStats;
  diag_moca_node_stats_entry_t  *pStats;

  DIAGD_ENTRY("%s", __func__);

  do {
    /* Point to the previous MoCA counters */
    pPrev = &pMocaIf->statistics[pMocaIf->active_stats_idx];

    /* Change active index */
    pMocaIf->active_stats_idx = (pMocaIf->active_stats_idx == 0)? 1 : 0;

    /* Point to the current MoCA counters */
    pCurr = &pMocaIf->statistics[pMocaIf->active_stats_idx];

    /* Get Moca Stats w/0 reset */
    nRet = MoCACtl2_GetStatistics(g_mocaHandle, &mocaStats, 0);
    if (nRet != CMSRET_SUCCESS) {
      break;          /* Fail to get the data */
    }

    DIAGD_TRACE("%s: pMocaIf->active_stats_idx :%d",
                __func__, pMocaIf->active_stats_idx);


    /* Copy the statistics to diag database */
    memcpy(pCurr, &mocaStats.generalStats, offsetof(diag_mocaIf_stats_t, inOctets_hi));
    pCurr->inOctets_hi = mocaStats.BitStats64.inOctets_hi;
    pCurr->outOctets_hi = mocaStats.BitStats64.outOctets_hi;

    /* Get the extended counters
     * TBD 2011/12/05 - Need to decide if need to check timeout errors also
     */
    pCurr->rxMapPkts  = mocaStats.extendedStats.rxBeacons;
    pCurr->rxRRPkts   = mocaStats.extendedStats.rxRRPkts;
    pCurr->rxBeacons  = mocaStats.extendedStats.rxBeacons;
    pCurr->rxCtrlPkts = mocaStats.extendedStats.rxCtrlPkts;

    pCurr->rxLcAdmReqCrcErr = mocaStats.extendedStats.rxLcAdmReqCrcErr;

    /* Allocate the max size of data base
     * Get max size of data structure
     */
    /* Get the node statistics table including the error counters */
    txDiscardTooManyMsg = DIAG_MOCA_LOG_MAX_SIZE_DISCARDPKTS_INFO;
    pMsg = malloc(txDiscardTooManyMsg);
    if (pMsg == NULL) {
      /* Fail to allocate memory for diag_mocalog_discardpkts_exceed_t. Exit. */
      DIAGD_DEBUG("%s: MoCACtl2_Fmr() failed (error=%d)", __func__, nRet);
      break;
    }


    /* Per definitions of tx/rx discard packet counters, error causes
     * couldn't be pin-down. So we log more statistics counters.
     */
    /* Update the nodeStats element and ignore the return status */
    diagMoca_GetNodeStatistics(&pMsg->nodeStats, &txDiscardTooManyMsg);

    memset(&cummulativeExtStats, 0, sizeof(MoCA_NODE_STATISTICS_EXT_ENTRY));
    if (pMsg->nodeStats.nodeStatsTblSize > 0) {
      /* Calculate the total errors */
      discardPkts = pMsg->nodeStats.nodeStatsTblSize / sizeof(diag_moca_node_stats_entry_t);
      pStats = &pMsg->nodeStats.Stats;
      for (totalPkts=0;totalPkts < discardPkts; totalPkts++, pStats++)
      {
        unsigned int *dest = (unsigned int *)&cummulativeExtStats;
        unsigned int *src;
        uint16_t      i;

        src = (unsigned int *)&pStats->nodeStatsExt;
        for(i=0; i < sizeof(cummulativeExtStats)/sizeof(unsigned int); i++)
            *(dest++) += *(src++);
      }
    }

    /* Only copy the CRC error counters to database
     * TBD 2011/12/05 - Need to decide if need to check timeout errors also
     */
    pCurr->rxMapCrcError = cummulativeExtStats.rxMapCrcError;
    pCurr->rxBeaconCrcError = cummulativeExtStats.rxBeaconCrcError;
    pCurr->rxRrCrcError  = cummulativeExtStats.rxRrCrcError;
    pCurr->rxLcCrcError  = cummulativeExtStats.rxMapCrcError;


    /* Get delta of the Tx packets */
    DIAG_GET_UINT32_DELTA(pCurr->inUcPkts, pPrev->inUcPkts, pDelta->inUcPkts);
    DIAG_GET_UINT32_DELTA(pCurr->inDiscardPktsEcl, pPrev->inDiscardPktsEcl, pDelta->inDiscardPktsEcl);
    DIAG_GET_UINT32_DELTA(pCurr->inDiscardPktsMac, pPrev->inDiscardPktsMac, pDelta->inDiscardPktsMac);
    DIAG_GET_UINT32_DELTA(pCurr->inUnKnownPkts, pPrev->inUnKnownPkts, pDelta->inUnKnownPkts);
    DIAG_GET_UINT32_DELTA(pCurr->inMcPkts, pPrev->inMcPkts, pDelta->inMcPkts);
    DIAG_GET_UINT32_DELTA(pCurr->inBcPkts, pPrev->inBcPkts, pDelta->inBcPkts);


    DIAGD_TRACE("%s: curr inUcPkts:%u, inMcPkts:%u, inBcPkts:%u, inUnKnownPkts:%u",
                __func__, pCurr->inUcPkts, pCurr->inMcPkts, pCurr->inBcPkts, pCurr->inUnKnownPkts);
    DIAGD_TRACE("%s: prev inUcPkts:%u, inMcPkts:%u, inBcPkts:%u, inUnKnownPkts:%u",
                __func__, pPrev->inUcPkts, pPrev->inMcPkts, pPrev->inBcPkts, pPrev->inUnKnownPkts);

    DIAGD_TRACE("%s: curr inDiscardPktsEcl:%u, inDiscardPktsMac:%u,",
                __func__, pCurr->inDiscardPktsEcl, pCurr->inDiscardPktsMac);
    DIAGD_TRACE("%s: prev inDiscardPktsEcl:%u, inDiscardPktsMac:%u,",
                __func__, pPrev->inDiscardPktsEcl, pPrev->inDiscardPktsMac);



    /* Get delta of the Rx packets */
    DIAG_GET_UINT32_DELTA(pCurr->outUcPkts, pPrev->outUcPkts, pDelta->outUcPkts);
    DIAG_GET_UINT32_DELTA(pCurr->outDiscardPkts, pPrev->outDiscardPkts, pDelta->outDiscardPkts);
    DIAG_GET_UINT32_DELTA(pCurr->outBcPkts, pPrev->outBcPkts, pDelta->outBcPkts);

    DIAGD_TRACE("%s: curr outUcPkts:%u, outBcPkts:%u, outDiscardPkts:%u",
                __func__, pCurr->outUcPkts, pCurr->outBcPkts, pCurr->outDiscardPkts);
    DIAGD_TRACE("%s: prev outUcPkts:%u, outBcPkts:%u, outDiscardPkts:%u",
                __func__, pPrev->outUcPkts, pPrev->outBcPkts, pPrev->outDiscardPkts);

    DIAG_GET_UINT32_DELTA(pCurr->rxMapPkts, pPrev->rxMapPkts, pDelta->rxMapPkts);
    DIAG_GET_UINT32_DELTA(pCurr->rxRRPkts, pPrev->rxRRPkts, pDelta->rxRRPkts);
    DIAG_GET_UINT32_DELTA(pCurr->rxBeacons, pPrev->rxBeacons, pDelta->rxBeacons);
    DIAG_GET_UINT32_DELTA(pCurr->rxCtrlPkts, pPrev->rxCtrlPkts, pDelta->rxCtrlPkts);

    DIAGD_TRACE("%s: curr rxMapPkts:%u, rxRRPkts:%u, rxBeacons:%u, rxCtrlPkts:%u",
                __func__, pCurr->rxMapPkts, pCurr->rxRRPkts,
                pCurr->rxBeacons, pCurr->rxCtrlPkts);
    DIAGD_TRACE("%s: prev rxMapPkts:%u, rxRRPkts:%u, rxBeacons:%u, rxCtrlPkts:%u",
                __func__, pPrev->rxMapPkts, pPrev->rxRRPkts,
                pPrev->rxBeacons, pPrev->rxCtrlPkts);

    DIAG_GET_UINT32_DELTA(pCurr->rxLcAdmReqCrcErr, pPrev->rxLcAdmReqCrcErr, pDelta->rxLcAdmReqCrcErr);
    DIAG_GET_UINT32_DELTA(pCurr->rxMapCrcError, pPrev->rxMapCrcError, pDelta->rxMapCrcError);
    DIAG_GET_UINT32_DELTA(pCurr->rxBeaconCrcError, pPrev->rxBeaconCrcError, pDelta->rxBeaconCrcError);
    DIAG_GET_UINT32_DELTA(pCurr->rxRrCrcError, pPrev->rxRrCrcError, pDelta->rxRrCrcError);
    DIAG_GET_UINT32_DELTA(pCurr->rxLcCrcError, pPrev->rxLcCrcError, pDelta->rxLcCrcError);

    DIAGD_TRACE("%s: curr rxLcAdmReqCrcErr:%u, rxMapCrcError:%u, rxBeaconCrcError:%u, "
                "rxRrCrcError:%u, rxLcCrcError:%u",
                __func__, pCurr->rxLcAdmReqCrcErr, pCurr->rxMapCrcError,
                pCurr->rxBeaconCrcError, pCurr->rxRrCrcError, pCurr->rxLcCrcError);
    DIAGD_TRACE("%s: prev rxLcAdmReqCrcErr:%u, rxMapCrcError:%u, rxBeaconCrcError:%u, "
                "rxRrCrcError:%u, rxLcCrcError:%u",
                __func__, pPrev->rxLcAdmReqCrcErr, pPrev->rxMapCrcError,
                pPrev->rxBeaconCrcError, pPrev->rxRrCrcError, pPrev->rxLcCrcError);

    /* Note - We don't calculate the delta of in and out octets */

    /* Start checking if discard packets over their thresholds */
    txDiscardTooManyMsg = DIAG_MOCA_LOG_NONE;
    rxDiscardTooManyMsg = DIAG_MOCA_LOG_NONE;

    /* Get the total Tx packets and the discarded Tx packets in delta time */
    totalPkts = pDelta->inBcPkts + pDelta->inMcPkts + pDelta->inUcPkts;
    /* Per definitions,
     * 1) inDiscardPktsEcl - the link down, the SW internal queues are full,
     *      the packet rate exceed its limitation, unknown
     *      packets rate exceed its limitation 15 pps, or CRC error.
     * 2) inDiscardPktsMac - Number of Ingress Packets discarded in the MoCA
     *      core MAC, if the link of the destined node becomes unusable
     * 3) inUnKnownPkts - Number of Ingress Unknown UC/MC Packets, including
     *      packets with CRC, since MoCA doesn't check for CRC errors
     */
    discardPkts = pDelta->inDiscardPktsEcl + pDelta->inDiscardPktsMac +
                  pDelta->inUnKnownPkts;
    DIAG_CHK_ERR_THLD(totalPkts,
                      discardPkts,
                      diagMocaThld_pctTxDiscardPkts,
                      err);
    DIAGD_TRACE("%s: Total Tx Pkts=%u  Discard Tx Pkts=%u",
                __func__, totalPkts, discardPkts);
    if (err == true) {
      /* The discard Tx packets exceeds the threshold. Log the information */
      DIAGD_LOG_WARN("MoCA: Excessive Tx discard packets in %d secs  "
                     "[Total Tx Pkts=%u  Discard Tx Pkts=%u]",
                     diagWaitTime_MocaChkErrs, totalPkts, discardPkts);
      /* indciate to log */
      txDiscardTooManyMsg = DIAG_MOCA_LOG_EXCESSIVE_TX_DISCARD_PKTS;
    }

    /* Get total Rx packets and Rx discarded packets in delta time */
    totalPkts = pDelta->outBcPkts + pDelta->outUcPkts + pDelta->rxMapPkts +
                pDelta->rxRRPkts  + pDelta->rxBeacons + pDelta->rxCtrlPkts;

    /* Per definitions,
     * 1) outDiscardPkts - CRC error or Timeout on preamble. CRC packets may be
     *      either egress packets with MoCA CRC error or packet received with
     *      CRC originally from the Ethernet on the other side of the MoCA
     *      network
     */
    discardPkts = pDelta->outDiscardPkts + pDelta->rxLcAdmReqCrcErr +
                  pDelta->rxMapCrcError  + pDelta->rxBeaconCrcError +
                  pDelta->rxRrCrcError   + pDelta->rxLcCrcError;

    DIAG_CHK_ERR_THLD(totalPkts,
                      discardPkts,
                      diagMocaThld_pctRxDiscardPkts,
                      err);
    DIAGD_TRACE("%s: Total Rx Pkts=%u  Discard Rx Pkts=%u",
                __func__, totalPkts, discardPkts);

    if (err == true) {
      /* The discard Tx packets exceeds the threshold. Log the information */
      DIAGD_LOG_WARN("MoCA: Excessive Rx discard packets in %d secs  "
                     "[Total Rx Pkts=%u  Discard Rx Pkts=%u]",
                     diagWaitTime_MocaChkErrs, totalPkts, discardPkts);
      /* indicate to log */
      rxDiscardTooManyMsg = DIAG_MOCA_LOG_EXCESSIVE_RX_DISCARD_PKTS;
    }

    /* Check if need to log to MoCA log files */
    if ((txDiscardTooManyMsg == DIAG_MOCA_LOG_NONE) &&
        (rxDiscardTooManyMsg == DIAG_MOCA_LOG_NONE)) {
      DIAGD_TRACE("%s: Error counts are not over thresholds", __func__);
      break;        /* Don't need to log. Exit. */
    }

    /* Get the msg type per txDiscardTooManyMsg and rxDiscardTooManyMsg setting */
    if (rxDiscardTooManyMsg == DIAG_MOCA_LOG_NONE) {
      /* Got too many Tx discard pkts */
      rxDiscardTooManyMsg = DIAG_MOCA_LOG_EXCESSIVE_TX_DISCARD_PKTS;
    }
    else if (txDiscardTooManyMsg == DIAG_MOCA_LOG_NONE) {
      /* Got too many Rx discard pkts */
      rxDiscardTooManyMsg = DIAG_MOCA_LOG_EXCESSIVE_RX_DISCARD_PKTS;
    }
    else {
      /* Too many Tx/Rx discard packets */
      rxDiscardTooManyMsg = DIAG_MOCA_LOG_EXCESSIVE_TX_RX_DISCARD_PKTS;
    }

    /* Per definitions of tx/rx discard packet counters, error causes
     * couldn't be pin-down. So we log more statistics counters.
     */
    /* Calculate the table size excluding msgHdr */
    txDiscardTooManyMsg =
          sizeof(uint32_t) + pMsg->nodeStats.nodeStatsTblSize +
          (sizeof(diag_mocaIf_stats_t) * 2);

    /* Copy the current stats */
    memcpy(&pMsg->currStats, pCurr, sizeof(diag_mocaIf_stats_t));
    /* Copy the previous stats */
    memcpy(&pMsg->prevStats, pPrev, sizeof(diag_mocaIf_stats_t));
    diagMoca_buildHdrMocaLogMsg(
        &pMsg->msgHdr,
        rxDiscardTooManyMsg,
        txDiscardTooManyMsg);

    /* Write to Moca Log file */
    diagMocaLog((char *)pMsg);

    /* Write to diagd log file in string format */
// =======> TODO

    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s - rtn=0x%X", __func__, rtn);

  if (pMsg != NULL) {
    /* Free allocated memory */
    free(pMsg);
  }

  return (rtn);

} /* diagMoca_MonErrorCounts */



/*
 * Monitor performance of the connected nodes in MoCA network
 * Check the following status of each connected node against reference table
 * 1) rxUc phy rate
 * 2) rxUc rx power level
 * 3) average SNR
 * 4) rxUc bit-loading
 * TODO 2011/11/30 - Update data in reference table (diagMocaPerfReferenceTable[])
 *
 * There are three levels defined -
 * 1) If node status of a active node is equal or better than data in
 *    DIAG_MOCA_PERF_LVL_GOOD, its performance is "good"
 * 2) If node status of a active node is equal of better than data in
 *    DIAG_MOCA_PERF_LVL_POOR, its performance is "poor"
 * 3) If node status of a active node is worse than data in
 *    DIAG_MOCA_PERF_LVL_POOR, its performance is "impaired"
 *
 * Input:
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 * DIAGD_RC_OUT_OF_MEM - failed to allocate memory
 */
int diagMoca_MonServicePerf()
{
  int       rtn = DIAGD_RC_ERR;
  int       count, i;
  uint16_t  msgType, tmp;
  uint32_t  bufLen;
  PMoCA_STATUS              pStatus = NULL;
  diag_moca_nodestatus_t   *pNodeStatus = NULL;
  diag_moca_perf_status_t  *pPerfStatus = NULL;
  PMoCA_NODE_STATUS_ENTRY   pNode_Status;


  do {
    /* Allocate the memory for reading MoCA_STATUS data */
    pStatus = (PMoCA_STATUS)malloc(sizeof(MoCA_STATUS));
    if (pStatus == NULL) {
      rtn = DIAGD_RC_OUT_OF_MEM;
      break;
    }

    /* Retrieve the current status information of the self node */
    rtn = diagMoca_GetStatus(pStatus);
    if (rtn == DIAGD_RC_ERR) {
      break;
    }

    /* Check the link status of self node */
    if (pStatus->generalStatus.linkStatus == MoCA_LINK_DOWN) {
/* TODO 20111018 --> printout the nodestatus information in a subroutine. */
      DIAGD_TRACE("%s: linkstatus = DOWN", __func__);
      break;
    }


    /* Allocate the memory for performance checking results */
    pPerfStatus = (diag_moca_perf_status_t *)malloc(sizeof(diag_moca_perf_status_t));
    if (pPerfStatus == NULL) {
      rtn = DIAGD_RC_OUT_OF_MEM;
      break;
    }

    /* Clear the memory */
    memset(pPerfStatus, 0x00, sizeof(diag_moca_perf_status_t));
    for (i = 0; i < MoCA_MAX_NODES; i++) {
      pPerfStatus->perfResult[i].valid = false;
    }

    pNodeStatus = &pPerfStatus->nodeStatus;
    /* Retrieve current node status table */
    bufLen = sizeof(diag_moca_nodestatus_t);
    rtn = diagMoca_GetNodeStatus(pNodeStatus, &bufLen);
    if (rtn == DIAGD_RC_ERR) {
      break;
    }

    /* Get the connected nodes */
    for (i = 0; i < MoCA_MAX_NODES ; i++) {
      if (pStatus->generalStatus.connectedNodes & (0x1 << i))
        pPerfStatus->noConnectedNodes++ ;
    }

    /* Due to the connected nodes are all active in MoCA network, the total
     * nodes should be 2 or more in order to check service performance.
     */
    if (pPerfStatus->noConnectedNodes < 2) {
/* TODO 20111018 --> printout the nodestatus information in a subroutine. */
      DIAGD_TRACE("%s: no of connected nodes = %d",
                  __func__, pPerfStatus->noConnectedNodes);
      break;
    }

    /* Start checking MoCA Service Performance to the connected nodes */
    pNode_Status = &pNodeStatus->nodeStatus[0];
    msgType = DIAG_MOCA_LOG_NONE;
    DIAGD_TRACE("%s: Loop through pNodeStatus (nodeStatusTblSize: %u)\n",
                 __func__, pNodeStatus->nodeStatusTblSize);

    for (count = 0;
         count < pNodeStatus->nodeStatusTblSize/sizeof(MoCA_NODE_STATUS_ENTRY);
         count++, pNode_Status++) {
      int32_t   s_nodeData;
      uint32_t  u_nodeData;
      diag_moca_ref_status_entry_t  *pPerfStatusEntry = NULL;

      pPerfStatusEntry = &pPerfStatus->perfResult[pNode_Status->nodeId];
      pPerfStatusEntry->nodeId = pNode_Status->nodeId;  /* the connected Id */
      pPerfStatusEntry->valid = true;

      /* Check RxUc PhyRate */
      DIAGD_TRACE("%s: Check RxUc Phy Rate...", __func__);
      u_nodeData = pNode_Status->maxPhyRates.rxUcPhyRate;
      for (i = 0; i < DIAG_MOCA_PERF_LVL_MAX; i++) {
        DIAGD_TRACE("%s: Node PhyRate=%9u,  refPhyRate = %9u",
                  __func__, u_nodeData, diagMocaPerfReferenceTable[i].rxUcPhyRate);
        if (u_nodeData >= diagMocaPerfReferenceTable[i].rxUcPhyRate) {
          break;    /* found it. */
        }
      } /* for DIAG_MOCA_PERF_LVL_MAX */
      DIAGD_TRACE("%s: RxUc Phy Rate Result: %s", __func__,
                 (i == DIAG_MOCA_PERF_LVL_GOOD)? "Good" :
                 (i == DIAG_MOCA_PERF_LVL_POOR)? "Poor" : "Impaired");

      /* Save the perf result */
      pPerfStatusEntry->rxUcPhyRate = i;
      /* Check if the rxUC phy rate of the connected node is good. */
      if (i == DIAG_MOCA_PERF_LVL_GOOD) {
        /* Yes, the rxUC phy rate is good.
         * Skip checking the SNR, rx pwr level and bit-loading of the node.
         */
        pPerfStatusEntry->rxUcGain = i;
        pPerfStatusEntry->rxUcAvgSnr = i;
        pPerfStatusEntry->rxUcBitLoading = i;
        continue;     /* Check next connected node */
      }

      /* Indicate to log the information into MoCA log file later. */
      msgType = DIAG_MOCA_LOG_POOR_PHY_RATE;

      /* Check RxUC Power level */
      DIAGD_TRACE("%s: Check RxUC Power...", __func__);
      s_nodeData = pNode_Status->rxUc.rxGain;
      for (i = 0; i < DIAG_MOCA_PERF_LVL_MAX; i++) {
        DIAGD_TRACE("%s: Node rxUcPwr=%6.2lf,  ref rxUcPwr = %6.2lf",
                    __func__, s_nodeData/4.0, diagMocaPerfReferenceTable[i].rxUcGain);
        if (s_nodeData >= (int32_t)(diagMocaPerfReferenceTable[i].rxUcGain * 4)) {
          break;    /* found it. */
        }
      } /* for DIAG_MOCA_PERF_LVL_MAX */
      DIAGD_TRACE("%s: RxUC Power Result: %s", __func__,
                 (i == DIAG_MOCA_PERF_LVL_GOOD)? "Good" :
                 (i == DIAG_MOCA_PERF_LVL_POOR)? "Poor" : "Impaired");

      /* Save the perf result */
      pPerfStatusEntry->rxUcGain = i;

      /* Check Rx Avg SNR */
      DIAGD_TRACE("%s: Check Rx Avg SNR...", __func__);
      s_nodeData = pNode_Status->rxUc.avgSnr;
      for (i = 0; i < DIAG_MOCA_PERF_LVL_MAX; i++) {
        DIAGD_TRACE("%s: Node avgSnr=%3.lf,  ref avgSnr = %3.1lf",
                    __func__, s_nodeData/2.0, diagMocaPerfReferenceTable[i].rxUcAvgSnr);
        if (s_nodeData >= (int32_t)(diagMocaPerfReferenceTable[i].rxUcAvgSnr * 2)) {
          break;    /* found it. */
        }
      } /* for DIAG_MOCA_PERF_LVL_MAX */
      DIAGD_TRACE("%s: Rx Avg SNR Result: %s", __func__,
                 (i == DIAG_MOCA_PERF_LVL_GOOD)? "Good" :
                 (i == DIAG_MOCA_PERF_LVL_POOR)? "Poor" : "Impaired");

      /* Save the perf result */
      pPerfStatusEntry->rxUcAvgSnr = i;

      /* Compare RxUC Bit Loading to the reference bit-loading data */
      DIAGD_TRACE("%s: Check RxUC Bit Loading...", __func__);
      diagMoca_CompareBitLoading(
          &pNode_Status->rxUc.bitLoading[0],
          (uint8_t *)&tmp);

      /* Save the perf result */
      pPerfStatusEntry->rxUcBitLoading = tmp;

    } /* for (nodes) */


    /* Save to to moca log file? */
    if (msgType != DIAG_MOCA_LOG_NONE) {
      /* Detect poor rxUc phy rate. Save all of the node status to moca log
       * 1. Calculate msg length
       * 2. build msg header.
       * 3. Save the msg to moca log file.
       */
      /* Calculate the table size excluding msgHdr */
      tmp = offsetof(diag_moca_perf_status_t, nodeStatus) -
            sizeof(diag_moca_log_msg_hdr_t) + bufLen;

      /* build message header */
      diagMoca_buildHdrMocaLogMsg(&pPerfStatus->msgHdr, msgType, tmp);

      /* Write the message to Moca Log file */
      diagMocaLog((char *)pPerfStatus);

    /* Write to diagd log file in text format */
// =======> TODO

    }

  } while (false);

  /* free all resources */
  if (pStatus != NULL) {
    free(pStatus);
  }

  if (pPerfStatus != NULL) {
    free(pPerfStatus);
  }

  DIAGD_TRACE("%s: exit (rtn=0x%x)\n", __func__, rtn);

  return(rtn);
} /* diagMoca_MonServicePerf */
