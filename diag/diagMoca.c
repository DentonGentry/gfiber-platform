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
 */
diag_moca_ref_tbl_t diagMocaPerfReferenceTable[DIAG_MOCA_PERF_LVL_MAX] = {

  /* Reference node data of DIAG_MOCA_PERF_LVL_GOOD */
  {180,  /* MoCA 1.1 rxUcPhyRate in Mbps when rxUcPower = -50 dBm */
   440   /* MoCA 2.0 rxUcPhyRate in Mpbs when rxUcPower = -50 dBm*/
  },

  /* Reference node data of DIAG_MOCA_PERF_LVL_POOR */
  {120,  /* MoCA 1.1 rxUcPhyRate in Mbps when rxUcPower = -60 dBm */
   220   /* MoCA 2.0 rxUcPhyRate in Mbps when rxUcPower = -60 dBm */
  }
};


/* file descriptor for accessing mocad */
static void  *g_mocaHandle = NULL;
static pthread_mutex_t diagMoca_Mutex;
static pthread_cond_t diagMoca_cond;

/* NOTE -
 * For FMR feature, get fmr data in the fmr callback routine, so
 * 1) Allocate Memory in diagMoca_FmrInitCb()
 * 2) Free the allocated memory in diagMoca_GetConnInfo()
 */
static diag_moca_node_connect_info_t *pNodeConnInfo = NULL;
/* bConnInfoValid = false, if If the diagMoca_FmrInitCb failed */
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
void diagMoca_ConvertUpTime(uint32_t timeInSecs, uint32_t *pTimeInHrs,
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
  int   rtn = DIAGD_RC_ERR;
  int   i, ret;
  struct moca_network_status  ns;
  struct moca_gen_node_status gsn;
  struct moca_mac_addr        mac_addr;
  diag_moca_node_mac_t        *pNode = &pNodeMacAddrTbl->nodemacs[0];

  DIAGD_ENTRY("%s: ", __func__);

  do {
    memset(pNodeMacAddrTbl, 0, sizeof(*pNodeMacAddrTbl));
    memset(&ns, 0, sizeof(ns));

    /* get active node bitmask */
    ret = moca_get_network_status(ctx, &ns);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_network_status() failed! ret = %d", __func__, ret);
      break;
    }

    /* get status entry for each node */
    for(i = 0; i < MOCA_MAX_NODES; i++) {

      if((ns.connected_nodes & (1 << i)) == 0)
        continue;      /* Not active. Next one */

      pNodeMacAddrTbl->connected_nodes++;

      pNode[i].active = DIAG_MOCA_NODE_ACTIVE;
      /* Check if it is self-node */
      if(ns.node_id == i)
      {
        /* It's a self node */
        pNodeMacAddrTbl->selfNodeId = i;
        ret = moca_get_mac_addr(ctx, &mac_addr);
        if (ret != MOCA_API_SUCCESS) {
          DIAGD_TRACE("%s moca_get_mac_addr() failed! ret = %d", __func__, ret);
          break;
        }
        memcpy(&pNode[i].macAddr, &mac_addr.val.addr[0], sizeof(macaddr_t));
      }
      else
      {
        ret = moca_get_gen_node_status(ctx, i, &gsn);
        if (ret != MOCA_API_SUCCESS) {
          DIAGD_TRACE("%s moca_get_gen_node_status() failed! ret = %d", __func__, ret);
          break;
          break;
        }
        memcpy(&pNode[i].macAddr, &gsn.eui.addr[0], sizeof(macaddr_t));
      }
    }

    for(i = 0; i < MOCA_MAX_NODES; i++) {
      DIAGD_TRACE ("%2i (active=%u)   %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
          i, pNode[i].active,
          pNode[i].macAddr.addr[0], pNode[i].macAddr.addr[1], pNode[i].macAddr.addr[2],
          pNode[i].macAddr.addr[3], pNode[i].macAddr.addr[4], pNode[i].macAddr.addr[5]);
    }

    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s: rtn = 0x%X", __func__, rtn);

  return(rtn);

} /* diagMoca_getActiveNodes */


/*
 * The FMR calls The FMR trap with the FMR information.
 * It is originally from fmr_response_cb() of mocactl.c
 * It changes to moca_register_fmr_init_cb on MoCA 2.0
 *
 * Input:
 * arg      - (IN) Instance of access mocad
 * in       - (IN) FMR information
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
static void diagMoca_FmrInitCb(void *ctx, struct moca_fmr_init_out *in)
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

      memcpy(&pNodeInfo->macAddr,
             &nodeMacAddrTbl.nodemacs[pNodeInfo->txNodeId].macAddr,
             MAC_ADDR_LEN);

      for (j = 0; j < MOCA_MAX_NODES; j++, pRxNodePhyInfo++) {
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
                                      (unsigned long)0,
                                      MoCA_VERSION_2_0);
      } /* end of for (MOCA_MAX_NODES) */

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

        for (j = 0; j < MOCA_MAX_NODES; j++)
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
        } /* end of for (MOCA_MAX_NODES)*/

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

} /* end of diagMoca_FmrInitCb */


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

  rtn = diagMoca_GetStatistics(pStats);
  if (rtn != DIAGD_RC_OK) {
    DIAGD_TRACE("%s fail to get moca self node statistics", __func__);
  }

  return rtn;
} /* end of diagMoca_GetStats */



/*
 * Processes mocap get --config command.
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
  int ret;
  int rtn = DIAGD_RC_ERR;
  diag_moca_config_parms_t *pCfgParms = &pCfg->Cfg;

  memset(pCfg, 0, sizeof(diag_moca_config_t));

  do {
    ret = moca_get_rf_band(g_mocaHandle, &pCfg->rfBand);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_rf_band() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_arpl_th_50(g_mocaHandle, &pCfgParms->arplTh50);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_arpl_th_50() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_arpl_th_100(g_mocaHandle, &pCfgParms->arplTh100);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_arpl_th_100() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_assertText(g_mocaHandle, &pCfgParms->assertText);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_assertText() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_assert_restart(g_mocaHandle, &pCfgParms->assertRestart);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_assert_restart() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_cir_prints(g_mocaHandle, &pCfgParms->cirPrints);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_cir_prints() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_continuous_ie_map_insert(g_mocaHandle, &pCfgParms->continuousIeMapInsert);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_continuous_ie_map_insert() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_continuous_ie_rr_insert(g_mocaHandle, &pCfgParms->continuousIeRrInsert);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_continuous_ie_rr_insert() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_dont_start_moca(g_mocaHandle, &pCfgParms->dontStartMoca);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_dont_start_moca() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_en_capable(g_mocaHandle, &pCfgParms->enCapable);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_en_capable() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_extra_rx_packets_per_qm(g_mocaHandle, &pCfgParms->extraRxPktsPerQm);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_extra_rx_packets_per_qm() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_fragmentation(g_mocaHandle, &pCfgParms->fragmentation);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_fragmentation() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_freq_shift(g_mocaHandle, &pCfgParms->freqShift);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_freq_shift() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_lab_snr_graph_set(g_mocaHandle, &pCfgParms->labSnrGraphSet);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_lab_snr_graph_set() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_lof_update(g_mocaHandle, &pCfgParms->lofUpdate);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_lof_update() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_loopback_en(g_mocaHandle, &pCfgParms->loopbackEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_loopback_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_m1_tx_power_variation(g_mocaHandle, &pCfgParms->m1TxPwrVariation);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_m1_tx_power_variation() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_max_frame_size(g_mocaHandle, &pCfgParms->maxFrameSize);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_max_frame_size() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_max_map_cycle(g_mocaHandle, &pCfgParms->maxMapCycle);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_max_map_cycle() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_max_pkt_aggr(g_mocaHandle, &pCfgParms->maxPktAggr);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_max_pkt_aggr() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_max_transmit_time(g_mocaHandle, &pCfgParms->maxTxTime);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_max_transmit_time() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_min_bw_alarm_threshold(g_mocaHandle, &pCfgParms->minBwAlarmThreshold);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_min_bw_alarm_threshold() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_min_map_cycle(g_mocaHandle, &pCfgParms->minMapCycle);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_min_map_cycle() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_moca_core_trace_enable(g_mocaHandle, &pCfgParms->coreTraceEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_moca_core_trace_enable() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_nbas_capping_en(g_mocaHandle, &pCfgParms->nbasCappingEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_nbas_capping_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_ooo_lmo_threshold(g_mocaHandle, &pCfgParms->oooLmoThreshold);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_ooo_lmo_threshold() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_orr_en(g_mocaHandle, &pCfgParms->orrEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_orr_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_password(g_mocaHandle, &pCfgParms->pwd);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_password() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_per_mode(g_mocaHandle, &pCfgParms->perMode);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_per_mode() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_pmk_exchange_interval(g_mocaHandle, &pCfgParms->pmkExchInterval);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_pmk_exchange_interval() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_power_state(g_mocaHandle, &pCfgParms->pwrState);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_power_state() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_priority_allocations(g_mocaHandle, &pCfgParms->priAlloc);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_priority_allocations() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_pss_en(g_mocaHandle, &pCfgParms->pssEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_pss_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res1(g_mocaHandle, &pCfgParms->res1);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res1() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res2(g_mocaHandle, &pCfgParms->res2);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res2() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res3(g_mocaHandle, &pCfgParms->res3);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res3() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res4(g_mocaHandle, &pCfgParms->res4);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res4() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res5(g_mocaHandle, &pCfgParms->res5);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res5() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res6(g_mocaHandle, &pCfgParms->res6);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res6() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res7(g_mocaHandle, &pCfgParms->res7);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res7() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res8(g_mocaHandle, &pCfgParms->res8);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res8() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_res9(g_mocaHandle, &pCfgParms->res9);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_res9() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_rlapm_table_100(g_mocaHandle, &pCfgParms->rlampTbl100);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_rlapm_table_100() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_rlapm_table_50(g_mocaHandle, &pCfgParms->rlampTbl50);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_rlapm_table_50() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_rx_power_tuning(g_mocaHandle, &pCfgParms->rxPwrTuning);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_rx_power_tuning() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_rx_tx_packets_per_qm(g_mocaHandle, &pCfgParms->rxTxPktsPerQm);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_rx_tx_packets_per_qm() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_sapm_en(g_mocaHandle, &pCfgParms->sapmEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_sapm_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_sapm_table_100(g_mocaHandle, &pCfgParms->sapmTbl100);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_sapm_table_100() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_sapm_table_50(g_mocaHandle, &pCfgParms->sapmTbl50);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_sapm_table_50() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_snr_margin_ldpc(g_mocaHandle, &pCfgParms->snrMarginLdpc);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_snr_margin_ldpc() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_snr_margin_ldpc_pre5(g_mocaHandle, &pCfgParms->snrMarginLdpcPre5);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_snr_margin_ldpc_pre5() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_snr_margin_ofdma(g_mocaHandle, &pCfgParms->snrMarginOfdma);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_snr_margin_ofdma() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_snr_margin_rs(g_mocaHandle, &pCfgParms->snrMarginRs);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_snr_margin_rs() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_snr_margin_table_ldpc(g_mocaHandle, &pCfgParms->snrMarginTblLdpc);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_snr_margin_table_ldpc() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_snr_margin_table_ldpc_pre5(g_mocaHandle, &pCfgParms->snrMarginTblLdpcPre5);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_snr_margin_table_ldpc_pre5() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_snr_margin_table_ofdma(g_mocaHandle, &pCfgParms->snrMarginTblOfdma);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_snr_margin_table_ofdma() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_snr_margin_table_rs(g_mocaHandle, &pCfgParms->snrMarginTblRs);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_snr_margin_table_rs() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_snr_prints(g_mocaHandle, &pCfgParms->snrPrints);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_snr_prints() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_start_ulmo(g_mocaHandle, &pCfgParms->startUlmo);
    /* TODO: 10/30/2012 Not to check ret for now.
     * need to investigate why ret is none 0 or
     * moca_get_start_ulmo() call is not successful
     */
//    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_start_ulmo() failed! ret = %d", __func__, ret);
//      break;
//    }

    ret = moca_get_target_phy_rate_20(g_mocaHandle, &pCfgParms->targetPhyRate20);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_target_phy_rate_20() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_target_phy_rate_20_turbo(g_mocaHandle, &pCfgParms->targetPhyRate20Turbo);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_target_phy_rate_20_turbo() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_target_phy_rate_qam128(g_mocaHandle, &pCfgParms->targetPhyRateQam128);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_target_phy_rate_qam128() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_target_phy_rate_qam256(g_mocaHandle, &pCfgParms->targetPhyRateQam256);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_target_phy_rate_qam256() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_tek_exchange_interval(g_mocaHandle, &pCfgParms->tekExchInterval);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_tek_exchange_interval() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_verbose(g_mocaHandle, &pCfgParms->verbose);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_verbose() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_wdog_enable(g_mocaHandle, &pCfgParms->wdogEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_wdog_enable() failed! ret = %d", __func__, ret);
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

  return(rtn);

} /* end of diagMoca_GetConfig */


/*
 * Processes get initparms command.
 *  TODO: 10/31/2012 need to revisit for MoCA2.0
 *        mocap get --init command
 * Caller should allocation the memory of diag_moca_init_parms_t structure.
 *
 * Input:
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * others -       DIAGD_RC_ERR
 */
int diagMoca_GetInitParms(diag_moca_init_parms_t *pMocaInitParms)
{
  int ret;
  int rtn = DIAGD_RC_ERR;
  struct moca_taboo_channels tc;


  memset(pMocaInitParms, 0, sizeof(*pMocaInitParms));

  do {
    ret = moca_get_aes_mm_key(g_mocaHandle, &pMocaInitParms->aesMmKey);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_aes_mm_key() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_aes_pm_key(g_mocaHandle, &pMocaInitParms->aesPmKey);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_aes_pm_key() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_bandwidth(g_mocaHandle, &pMocaInitParms->bandwidth);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_bandwidth() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_beacon_channel(g_mocaHandle, &pMocaInitParms->beaconChannel);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_beacon_channel() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_beacon_pwr_reduction(g_mocaHandle, &pMocaInitParms->beaconPwrReduction);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_beacon_pwr_reduction() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_beacon_pwr_reduction_en(g_mocaHandle, &pMocaInitParms->beaconPwrReductionEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_beacon_pwr_reduction_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_bo_mode(g_mocaHandle, &pMocaInitParms->boMode);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_bo_mode() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_const_rx_submode(g_mocaHandle, &pMocaInitParms->constRxSubmode);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_const_rx_submode() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_const_tx_params(g_mocaHandle, &pMocaInitParms->constTxParams);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_const_tx_params() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_continuous_power_tx_mode(g_mocaHandle, &pMocaInitParms->continuousPwrTxMode);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_continuous_power_tx_mode() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_continuous_rx_mode_attn(g_mocaHandle, &pMocaInitParms->continuousRxModeAttn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_continuous_rx_mode_attn() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_device_class(g_mocaHandle, &pMocaInitParms->deviceClass);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_device_class() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_egr_mc_filter_en(g_mocaHandle, &pMocaInitParms->egrMcFilterEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_egr_mc_filter() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_flow_control_en(g_mocaHandle, &pMocaInitParms->flowControlEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_flow_control_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_freq_mask(g_mocaHandle, &pMocaInitParms->freqMask);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_freq_mask() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init1(g_mocaHandle, &pMocaInitParms->init1);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init1() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init2(g_mocaHandle, &pMocaInitParms->init2);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init2() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init3(g_mocaHandle, &pMocaInitParms->init3);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init3() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init4(g_mocaHandle, &pMocaInitParms->init4);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init4() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init5(g_mocaHandle, &pMocaInitParms->init5);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init5() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init6(g_mocaHandle, &pMocaInitParms->init6);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init6() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init7(g_mocaHandle, &pMocaInitParms->init7);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init7() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init8(g_mocaHandle, &pMocaInitParms->init8);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init8() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_init9(g_mocaHandle, &pMocaInitParms->init9);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_init9() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_lab_mode(g_mocaHandle, &pMocaInitParms->labMode);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_lab_mode() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_led_settings(g_mocaHandle, &pMocaInitParms->ledSettings);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_led_settings() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_lof(g_mocaHandle, &pMocaInitParms->lastOperFreq);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_lof() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_low_pri_q_num(g_mocaHandle, &pMocaInitParms->lowPriQNum);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s __moca_get_low_pri_q_num() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_mac_addr(g_mocaHandle, &pMocaInitParms->macAddr);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_mac_addr() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_max_tx_power(g_mocaHandle, &pMocaInitParms->maxTxPower);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_max_tx_power() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_mmk_key(g_mocaHandle, &pMocaInitParms->mmkKey);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_mmk_key() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_mtm_en(g_mocaHandle, &pMocaInitParms->mtmEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_mtm_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_multicast_mode(g_mocaHandle, &pMocaInitParms->mcastMode);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_multicast_mode() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_nc_mode(g_mocaHandle, &pMocaInitParms->ncMode);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_nc_mode() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_ofdma_en(g_mocaHandle, &pMocaInitParms->ofdmaEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_ofdma_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_otf_en(g_mocaHandle, &pMocaInitParms->otfEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_otf_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_pmk_initial_key(g_mocaHandle, &pMocaInitParms->pmkInitKey);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_pmk_initial_key() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_pns_freq_mask(g_mocaHandle, &pMocaInitParms->pnsFreqMask);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_pns_freq_mask() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_preferred_nc(g_mocaHandle, &pMocaInitParms->preferedNC);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_preferred_nc() failed! ret = %d", __func__, ret);
      break;
    }

    ret = __moca_get_primary_ch_offset(g_mocaHandle, &pMocaInitParms->primChOffset);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_primary_ch_offset() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_privacy_en(g_mocaHandle, &pMocaInitParms->privacyEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_privacy_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_qam256_capability(g_mocaHandle, &pMocaInitParms->qam256Capability);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_qam256_capability() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_taboo_channels(g_mocaHandle, &tc);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_taboo_channels() failed! ret = %d", __func__, ret);
      break;
    }

    pMocaInitParms->tabooFixedMaskStart = tc.taboo_fixed_mask_start;
    pMocaInitParms->tabooFixedChannelMask = tc.taboo_fixed_channel_mask;
    pMocaInitParms->tabooLeftMask = tc.taboo_left_mask;
    pMocaInitParms->tabooRightMask = tc.taboo_right_mask;

    ret = moca_get_tpc_en(g_mocaHandle, &pMocaInitParms->txPwrControlEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_tpc_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_turbo_en(g_mocaHandle, &pMocaInitParms->turboEn);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_turbo_en() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_rf_band(g_mocaHandle, &pMocaInitParms->rfBand);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_rf_band() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_single_channel_operation(g_mocaHandle, &pMocaInitParms->singleChOp);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_single_channel_operation() failed! ret = %d", __func__, ret);
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);


  return(rtn);

} /* end of diagMoca_GetInitParms */


/*
 * Retrieve current status information of the self-node
 * Caller should allocate the memory of diag_moca_status_t structure.
 *
 * Input:
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * others -       DIAGD_RC_ERR
 */
int diagMoca_GetStatus(diag_moca_status_t *pStatus)
{
  int ret;
  int rtn = DIAGD_RC_ERR;

  DIAGD_ENTRY("%s", __func__);

  do {
    ret = moca_get_node_status(g_mocaHandle, &pStatus->ns);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_node_status() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_single_channel_operation(g_mocaHandle, &pStatus->singleChOp);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_single_channel_operation() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_phy_status(g_mocaHandle, &pStatus->txGcdPowerReduction);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_phy_status() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_led_status(g_mocaHandle, &pStatus->ledStatus);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_led_status() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_pqos_egress_numflows(g_mocaHandle, &pStatus->pqosEgressNumFlows);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_pqos_egress_numflows() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_interface_status(g_mocaHandle, &pStatus->intf);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_interface_status() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_network_status(g_mocaHandle, &pStatus->net);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_network_status() failed! ret = %d", __func__, ret);
      break;
    }
    pStatus->nodeId = pStatus->net.node_id;

    ret = moca_get_drv_info(g_mocaHandle, &pStatus->drv);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_drv_info() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_fw_version(g_mocaHandle, &pStatus->fw);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_fw_version() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_current_keys(g_mocaHandle, &pStatus->key);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_current_keys() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_key_times(g_mocaHandle, &pStatus->keyTimes);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_key_times() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_mac_addr(g_mocaHandle, &pStatus->macAddr);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_mac_addr() failed! ret = %d", __func__, ret);
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s rtn = 0x%X", __func__, rtn);
  return(rtn);
}


/*
 * Check if MoCA link status is UP
 *
 * Input:
 * pLinkup -  Pointer to linkup boolean variable.
 *
 * Output:
 * DIAGD_RC_OK  - OK
 * Others       - FAILED
 *
 */
int diagMoca_IsLinkUp(bool *pLinkup)
{
  struct moca_interface_status ifstatus;
  int ret;
  int rtn = DIAGD_RC_ERR;

  DIAGD_ENTRY("%s", __func__);

  do {
    ret = moca_get_interface_status(g_mocaHandle, &ifstatus);
    if (ret != MOCA_API_SUCCESS)
    {
      DIAGD_TRACE("%s moca_get_interface_status() fails! ret = %d", __func__, ret);
      break;
    }

    DIAGD_TRACE("%s: MoCA interface link=%s", __func__,
        (ifstatus.link_status == MOCA_LINK_UP)? "UP":"DOWN");
    if (ifstatus.link_status == MOCA_LINK_UP)
    {
      *pLinkup = true;
    }
    else {
      *pLinkup = false;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s: rtn = 0x%X linkup =%s", __func__, rtn, (*pLinkup)? "true":"false");
  return(rtn);
}


/*
 * Getting slef-node statistics information.
 * Caller should allocation the memory of diag_moca_stats_t
 * structure.
 *
 * Input:
 * pMocaStats   - Pointer of the diag moca statistics
 *
 * Output:
 * DIAGD_RC_OK  - OK
 *
 */
int diagMoca_GetStatistics(diag_moca_stats_t *pMocaStats)
{
  int ret;
  int rtn = DIAGD_RC_ERR;

  DIAGD_ENTRY("%s", __func__);

  do {
    ret = moca_get_gen_stats(g_mocaHandle, &pMocaStats->genStats);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_gen_stats() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_ext_octet_count(g_mocaHandle, &pMocaStats->extOctCnt);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_ext_octet_count() failed! ret = %d", __func__, ret);
      break;
    }

    ret = moca_get_error_stats(g_mocaHandle, &pMocaStats->totalExtStats);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_error_stats() failed! ret = %d", __func__, ret);
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s rtn = 0x%X", __func__, rtn);
  return(rtn);
}


int  diagMoca_GetNodeStatsTbl(
         diag_moca_node_stats_entry_t *pNodeStatsEntry,
         uint32_t *statsTblSize,
         uint32_t ulReset)
{
  int i, num_nodes = 0;
  struct moca_network_status ns;
  int ret;
  int rtn = DIAGD_RC_ERR;

  DIAGD_ENTRY("%s", __func__);

  do {
    /* get node bitmask */
    ret = moca_get_network_status(g_mocaHandle, &ns);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_network_status() failed! ret = %d", __func__, ret);
      break;
    }

    /* get stats entry for each node */
    for (i = 0; i < MOCA_MAX_NODES; i++) {
      if (!(ns.connected_nodes & (1 << i)))
        continue;
      if (ns.node_id == i)
        continue;

      pNodeStatsEntry->nodeId = i;
      /* get node_stats, destination nodeId=i */
      memset(&pNodeStatsEntry->nodeStats, 0, sizeof(pNodeStatsEntry->nodeStats));
      ret = moca_get_node_stats(g_mocaHandle, i, &pNodeStatsEntry->nodeStats);
      if (ret != MOCA_API_SUCCESS) {
        DIAGD_TRACE("%s moca_get_node_stats() failed! ret = %d", __func__, ret);
        break;
      }

      /* get node_stats_ext, destination nodeId=i */
      memset(&pNodeStatsEntry->nodeStatsExt, 0, sizeof(pNodeStatsEntry->nodeStatsExt));
      ret = moca_get_node_stats_ext(g_mocaHandle, i, &pNodeStatsEntry->nodeStatsExt);
      if (ret != MOCA_API_SUCCESS) {
        DIAGD_TRACE("%s moca_get_node_stats_ext() failed! ret = %d", __func__, ret);
        break;
      }

      pNodeStatsEntry++;
      num_nodes++;
    }

    /* fill in *pulNodeStatusTblSize with number of nodes * entry size */
    *statsTblSize = num_nodes * sizeof(*pNodeStatsEntry);

    if(ulReset)
      moca_set_reset_stats(g_mocaHandle);

    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s rtn = 0x%X", __func__, rtn);
  return(rtn);
}


/*
 * Processes getting node statistics command.
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
 *
 */
int diagMoca_GetNodeStatistics(
      diag_moca_node_stats_table_t *pNodeStats,
      uint16_t *pSize)
{
  int       rtn = DIAGD_RC_ERR;
  int       ret;
  int       node;
  uint32_t  prevConnectedNodes;
  diag_moca_node_mac_table_t      nodeMacTbl;
  diag_moca_node_mac_t           *pMacAddr = NULL;
  diag_moca_node_stats_entry_t   *pNodeStatsEntry = NULL;
  struct moca_network_status      ns;
  uint32_t  statsTblSize;
  int       idx = 0, j;
  diag_moca_node_stats_entry_t    nodeStatsTbl[MOCA_MAX_NODES];
  bool      linkup = false;


  DIAGD_ENTRY("%s", __func__);

  do {
    /* Check if MoCA link is up to
     * prevent invalid node statistics information
     * If it is down, return DIAGD_RC_ERR
     */
    rtn = diagMoca_IsLinkUp(&linkup);

    DIAGD_DEBUG("%s: rtn = 0x%X linkup = %s", __func__, rtn, linkup? "true":"false");

    if (rtn != DIAGD_RC_OK || linkup == false) {
      break;
    }

    memset(pNodeStats, 0x00, *pSize);

    /* get active node bitmask */
    ret = moca_get_network_status(g_mocaHandle, &ns);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_network_status() failed! ret = %d", __func__, ret);
      break;
    }

    prevConnectedNodes = ns.connected_nodes;

    /* Get node statistics w/o reset */
    diagMoca_GetNodeStatsTbl(&nodeStatsTbl[0], &statsTblSize, 0);

    /* Get Mac Address of active nodes */
    rtn = diagMoca_getActiveNodes(g_mocaHandle, &nodeMacTbl);
    if(rtn != DIAGD_RC_OK) {
      DIAGD_TRACE("%s diagMoca_getActiveNodes() fails!", __func__);
      break;
    }

    /* Get current active node bitmask again to check if topology changed. */
    ret = moca_get_network_status(g_mocaHandle, &ns);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_network_status() failed! ret = %d", __func__, ret);
      break;
    }

    if (prevConnectedNodes != ns.connected_nodes) {
      if (idx < 2) {
        DIAGD_DEBUG("%s: Topology Changed (connectedNode-Prev=0x%08X, curr=0x%08X.",
            __func__, prevConnectedNodes, ns.connected_nodes);
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
          pMacAddr->macAddr.addr[0], pMacAddr->macAddr.addr[1], pMacAddr->macAddr.addr[2],
          pMacAddr->macAddr.addr[3], pMacAddr->macAddr.addr[4], pMacAddr->macAddr.addr[5]);

      memcpy(&pNodeStatsEntry[node].macAddr, &pMacAddr->macAddr, sizeof(macaddr_t));

      /* Get the entry of the node statistics */
      for (j = 0; j < (statsTblSize/sizeof(diag_moca_node_stats_entry_t)); j++) {
        if (nodeStatsTbl[j].nodeId != idx)
          continue;
        /* Copy the statistics to the database */
        memcpy(&pNodeStatsEntry[node].nodeStats,
            &nodeStatsTbl[j].nodeStats,
            sizeof(struct moca_node_stats));
        break;    /* Done. Exit from loop */
      } /* end of for */

      /* Get the entry of the extended node statistics */
      for (j = 0; j < (statsTblSize/sizeof(diag_moca_node_stats_entry_t)); j++) {
        if (pNodeStatsEntry[j].nodeId != idx)
          continue;
        /* Copy the extended statistics to the database */
        memcpy(&pNodeStatsEntry[node].nodeStatsExt,
            &nodeStatsTbl[j].nodeStatsExt,
            sizeof(struct moca_node_stats_ext));
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


int diagMoca_GetNodeTblStatus(diag_moca_nodestatus_t *pNodeStatus)
{
  int i, ret, num_nodes = 0;
  struct moca_network_status ns;
  struct moca_gen_node_ext_status_in gnes;
  int myNodeId = 0;
  diag_moca_nodestatus_entry_t *pNodeStatusEntry = pNodeStatus->nodeStatus;
  int rtn = DIAGD_RC_ERR;


  DIAGD_ENTRY("%s", __func__);

  do {
    /* get node bitmask */
    ret = moca_get_network_status(g_mocaHandle, &ns);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_network_status() failed! ret = %d", __func__, ret);
      break;
    }

    myNodeId = ns.node_id;
    memset(pNodeStatus, 0, sizeof(diag_moca_nodestatus_t));

    /* get status entry for each node */
    for (i = 0; i < MOCA_MAX_NODES; i++) {
      if (!(ns.connected_nodes & (1 << i)))
        continue;
      if(myNodeId == i)
        continue;

      pNodeStatusEntry->nodeId = i;
      /* get gen_node_status */
      ret = moca_get_gen_node_status(g_mocaHandle, i, &pNodeStatusEntry->gns);
      if (ret != MOCA_API_SUCCESS) {
        DIAGD_TRACE("%s moca_get_gen_node_status() failed! ret = %d",
            __func__, ret);
        break;
      }

      gnes.index = i;
      gnes.profile_type = MOCA_EXT_STATUS_PROFILE_RX_UC_NPER;
      pNodeStatusEntry->profile.type = MOCA_EXT_STATUS_PROFILE_RX_UC_NPER;

      ret = moca_get_gen_node_ext_status(
          g_mocaHandle,
          &gnes,
          &pNodeStatusEntry->profile.rxUc);
      if (ret != 0) {
        DIAGD_DEBUG("%s: Error to get gen_node_ext_status! destination nodeId=%u",
            __func__, i);
      }
      else {
        /* check if the destination node is MoCA1.1 */
        if (pNodeStatusEntry->profile.rxUc.nbas == 0) {
          gnes.index = i;
          gnes.profile_type = MOCA_EXT_STATUS_PROFILE_RX_UCAST;
          pNodeStatusEntry->profile.type = MOCA_EXT_STATUS_PROFILE_RX_UCAST;

          ret = moca_get_gen_node_ext_status(
              g_mocaHandle,
              &gnes,
              &pNodeStatusEntry->profile.rxUc);
          if (ret != MOCA_API_SUCCESS) {
            DIAGD_TRACE("%s moca_get_gen_node_ext_status() failed! ret = %d",
                __func__, ret);
            break;
          }
        }

        pNodeStatusEntry++;
        num_nodes++;
      }
    }

    /* fill in *pulNodeStatusTblSize with number of nodes * entry size */
    pNodeStatus->nodeStatusTblSize = num_nodes * sizeof(*pNodeStatusEntry);

    rtn = DIAGD_RC_OK;

  } while (false);

  DIAGD_EXIT("%s: rtn=0x%x num_nodes = %u, nodeStatusTblSize = %u", __func__,
      rtn, num_nodes, pNodeStatus->nodeStatusTblSize);
  return(rtn);

}


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
  bool    linkup = false;

  DIAGD_ENTRY("%s", __func__);

  /* Check if MoCA link is up to
   * prevent invalid or all zero node status information
   * If it is down, return DIAGD_RC_ERR
   */
  rtn = diagMoca_IsLinkUp(&linkup);

  DIAGD_DEBUG("%s: rtn = 0x%X linkup = %s", __func__, rtn, linkup? "true":"false");

  if (rtn != DIAGD_RC_OK || linkup == false) {
    return DIAGD_RC_ERR;
  }

  memset(pNodeStatus, 0x00, *pBufLen);

  rtn = diagMoca_GetNodeTblStatus(pNodeStatus);

  if (rtn == DIAGD_RC_OK) {
    /* Calculate the actual table length */
    *pBufLen = offsetof(diag_moca_nodestatus_t, nodeStatus) +
      pNodeStatus->nodeStatusTblSize;
  }

  DIAGD_EXIT("%s: rtn=0x%x (nodeStatusTblSize=%u, *pBufLen=%u)",
      __func__, rtn, pNodeStatus->nodeStatusTblSize, *pBufLen);

  return (rtn);

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

/* Comment out for now since this function is called
 * only when process DIAGD_REQ_MOCA_GET_CONN_INFO request
 * from the remote diagTester client program.
 * TODO: 10/18/2012 Need to rewrite this function based on MoCA 2.0 APIs
 */
#if 0
  int             rtn = DIAGD_RC_OK;
  MoCA_FMR_PARAMS fmrParams ;
  CmsRet          nRet = CMSRET_SUCCESS ;
  pthread_t       event_thread;
  bool            linkup = false;

  /* Add a check if MoCA link is up to
   * prevent invalid connection information
   * or crash.
   * If it is down, return DIAGD_RC_ERR
   */
  rtn = diagMoca_IsLinkUp(&linkup);

  if (rtn != DIAGD_RC_OK || linkup == false) {
    /* Restore to defaults */
    pNodeConnInfo = NULL;
    bConnInfoValid = false;
    return (DIAGD_RC_ERR);
  }

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
    moca_register_fmr_init_cb(
            g_mocaHandle,
            &diagMoca_FmrInitCb,
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
#endif

  return (DIAGD_RC_OK);

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
    g_mocaHandle = moca_open( NULL );
    if (g_mocaHandle == NULL) {
      /* Failed to open mocad */
      DIAGD_DEBUG("%s: MoCACtl_Open failed", __func__);

      rtn = DIAGD_RC_FAILED_OPEN_MOCAD;
      break;
    }

  } while (false);

  DIAGD_EXIT("%s - rtn=0x%X", __func__, rtn);

  return(rtn);

} /* end of diagd_MoCA_Init */


void diagd_MoCA_UnInit()
{
  /* Close the netlink socket, if it's opened */
  if (g_mocaHandle !=  NULL) {
    moca_close(g_mocaHandle);
    g_mocaHandle = NULL;
  }
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
  uint32_t  totalPkts, discardPkts;
  bool      err;
  uint16_t  txDiscardTooManyMsg, rxDiscardTooManyMsg;
  uint16_t  nodeStatsSize, msgLen;
  diag_moca_stats_t    mocaStats;
  diag_mocaIf_info     *pMocaIf = &pDiagInfo->mocaIf;
  diag_mocaIf_stats_t  *pCurr;
  diag_mocaIf_stats_t  *pPrev;
  diag_mocaIf_stats_t  *pDelta = &pMocaIf->delta_stats;
  diag_mocalog_discardpkts_exceed_t  *pMsg = NULL;
  diag_moca_status_t  *pStatus = NULL;

  DIAGD_ENTRY("%s", __func__);

  do {
    /* Point to the previous MoCA counters */
    pPrev = &pMocaIf->statistics[pMocaIf->active_stats_idx];

    /* Change active index */
    pMocaIf->active_stats_idx = (pMocaIf->active_stats_idx == 0)? 1 : 0;

    /* Point to the current MoCA counters */
    pCurr = &pMocaIf->statistics[pMocaIf->active_stats_idx];

    /* Get Moca Stats w/0 reset */
    rtn = diagMoca_GetStatistics(&mocaStats);
    if (rtn != DIAGD_RC_OK) {
      break;          /* Fail to get the data */
    }

    DIAGD_TRACE("%s: pMocaIf->active_stats_idx :%d",
        __func__, pMocaIf->active_stats_idx);


    /* Copy the statistics to diag database */
    memset(pCurr, 0, sizeof(diag_mocaIf_stats_t));
    diagMoca_CopyStats(pCurr, &mocaStats);

    /* Allocate the max size of data base
     * Get max size of data structure
     */
    /* Get the node statistics table including the error counters */
    pMsg = malloc(DIAG_MOCA_LOG_MAX_SIZE_DISCARDPKTS_INFO);
    if (pMsg == NULL) {
      /* Fail to allocate memory for diag_mocalog_discardpkts_exceed_t. Exit. */
      DIAGD_DEBUG("%s: mcalloc failed (error=%d)", __func__, errno);
      break;
    }

    nodeStatsSize = DIAG_MOCA_MAX_NODE_STATS_SIZE;


    /* Per definitions of tx/rx discard packet counters, error causes
     * couldn't be pin-down. So we log more statistics counters.
     */
    rtn = diagMoca_GetNodeStatistics(&pMsg->nodeStats, &nodeStatsSize);
    if (rtn != DIAGD_RC_OK) {
      DIAGD_DEBUG("%s: diagMoca_GetNodeStatistics() failed rtn = 0x%X", __func__, rtn);
      break;
    }

    /* In MoCA 2.0, accummulative extStats is stored in struct moca_error_stats.
     * It is already retrieved via moca_get_error_stats() in diagMoca_GetStatistics()
     * and is stored to diag statistics database(pCurr) in diagMoca_CopyStats().
     * No need to accumulate extStats from each node
     */

    /* Get delta of the Tx packets */
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_total_pkts, pPrev->ecl_tx_total_pkts, pDelta->ecl_tx_total_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_ucast_pkts, pPrev->ecl_tx_ucast_pkts, pDelta->ecl_tx_ucast_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_bcast_pkts, pPrev->ecl_tx_bcast_pkts, pDelta->ecl_tx_bcast_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_mcast_pkts, pPrev->ecl_tx_mcast_pkts, pDelta->ecl_tx_mcast_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_ucast_unknown, pPrev->ecl_tx_ucast_unknown, pDelta->ecl_tx_ucast_unknown);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_mcast_unknown, pPrev->ecl_tx_mcast_unknown, pDelta->ecl_tx_mcast_unknown);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_ucast_drops, pPrev->ecl_tx_ucast_drops, pDelta->ecl_tx_ucast_drops);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_mcast_drops, pPrev->ecl_tx_mcast_drops, pDelta->ecl_tx_mcast_drops);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_tx_buff_drop_pkts, pPrev->ecl_tx_buff_drop_pkts, pDelta->ecl_tx_buff_drop_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->mac_tx_low_drop_pkts, pPrev->mac_tx_low_drop_pkts, pDelta->mac_tx_low_drop_pkts);


    DIAGD_TRACE("%s: curr ecl_tx_total_pkts:%u, ecl_tx_ucast_pkts:%u, ecl_tx_bcast_pkts:%u, ecl_tx_mcast_pkts:%u",
        __func__, pCurr->ecl_tx_total_pkts, pCurr->ecl_tx_ucast_pkts, pCurr->ecl_tx_bcast_pkts, pCurr->ecl_tx_mcast_pkts);
    DIAGD_TRACE("%s: prev ecl_tx_total_pkts:%u, ecl_tx_ucast_pkts:%u, ecl_tx_bcast_pkts:%u, ecl_tx_mcast_pkts:%u",
        __func__, pPrev->ecl_tx_total_pkts, pPrev->ecl_tx_ucast_pkts, pPrev->ecl_tx_bcast_pkts,pPrev->ecl_tx_mcast_pkts);

    DIAGD_TRACE("%s: curr ecl_tx_ucast_unknown:%u, ecl_tx_mcast_unknown:%u, ecl_tx_ucast_drops:%u, ecl_tx_mcast_drops:%u",
        __func__, pCurr->ecl_tx_ucast_unknown, pCurr->ecl_tx_mcast_unknown, pCurr->ecl_tx_ucast_drops, pCurr->ecl_tx_mcast_drops);
    DIAGD_TRACE("%s: prev ecl_tx_ucast_unknown:%u, ecl_tx_mcast_unknown:%u, ecl_tx_ucast_drops:%u, ecl_tx_mcast_drops:%u",
        __func__, pPrev->ecl_tx_ucast_unknown, pPrev->ecl_tx_mcast_unknown, pPrev->ecl_tx_ucast_drops, pPrev->ecl_tx_mcast_drops);

    DIAGD_TRACE("%s: curr ecl_tx_buff_drop_pkts:%u, mac_tx_low_drop_pkts:%u,",
        __func__, pCurr->ecl_tx_buff_drop_pkts, pCurr->mac_tx_low_drop_pkts);
    DIAGD_TRACE("%s: prev ecl_tx_buff_drop_pkts:%u, mac_tx_low_drop_pkts:%u,",
        __func__, pPrev->ecl_tx_buff_drop_pkts, pPrev->mac_tx_low_drop_pkts);



    /* Get delta of the Rx packets */
    DIAG_GET_UINT32_DELTA(pCurr->ecl_rx_total_pkts, pPrev->ecl_rx_total_pkts, pDelta->ecl_rx_total_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_rx_ucast_pkts, pPrev->ecl_rx_ucast_pkts, pDelta->ecl_rx_ucast_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_rx_bcast_pkts, pPrev->ecl_rx_bcast_pkts, pDelta->ecl_rx_bcast_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_rx_mcast_pkts, pPrev->ecl_rx_mcast_pkts, pDelta->ecl_rx_mcast_pkts);
    DIAG_GET_UINT32_DELTA(pCurr->ecl_rx_ucast_drops, pPrev->ecl_rx_ucast_drops, pDelta->ecl_rx_ucast_drops);
    DIAG_GET_UINT32_DELTA(pCurr->mac_rx_buff_drop_pkts, pPrev->mac_rx_buff_drop_pkts, pDelta->mac_rx_buff_drop_pkts);

    DIAGD_TRACE("%s: curr ecl_rx_total_pkts:%u, ecl_rx_ucast_pkts:%u, ecl_rx_bcast_pkts:%u, ecl_rx_mcast_pkts:%u",
        __func__, pCurr->ecl_rx_total_pkts, pCurr->ecl_rx_ucast_pkts, pCurr->ecl_rx_bcast_pkts, pCurr->ecl_rx_mcast_pkts);
    DIAGD_TRACE("%s: prev ecl_rx_total_pkts:%u, ecl_rx_ucast_pkts:%u, ecl_rx_bcast_pkts:%u, ecl_rx_mcast_pkts:%u",
        __func__, pPrev->ecl_rx_total_pkts, pPrev->ecl_rx_ucast_pkts, pPrev->ecl_rx_bcast_pkts,pPrev->ecl_rx_mcast_pkts);

    DIAGD_TRACE("%s: curr ecl_rx_ucast_drops:%u, mac_rx_buff_drop_pkts:%u",
        __func__,  pCurr->ecl_rx_ucast_drops, pCurr->mac_rx_buff_drop_pkts);
    DIAGD_TRACE("%s: prev ecl_rx_ucast_drops:%u, mac_rx_buff_drop_pkts:%u",
        __func__,  pPrev->ecl_rx_ucast_drops, pPrev->mac_rx_buff_drop_pkts);

    DIAGD_TRACE("%s: curr rx_beacons:%u, rx_map_packets:%u, rx_rr_packets:%u, rx_control_uc_packets:%u,",
        __func__, pCurr->rx_beacons, pCurr->rx_map_packets, pCurr->rx_rr_packets, pCurr->rx_control_uc_packets);
    DIAGD_TRACE("%s: prev rx_beacons:%u, rx_map_packets:%u, rx_rr_packets:%u, rx_control_uc_packets:%u,",
        __func__, pPrev->rx_beacons, pPrev->rx_map_packets, pPrev->rx_rr_packets, pPrev->rx_control_uc_packets);

    DIAG_GET_UINT32_DELTA(pCurr->rx_uc_crc_error, pPrev->rx_uc_crc_error, pDelta->rx_uc_crc_error);
    DIAG_GET_UINT32_DELTA(pCurr->rx_bc_crc_error, pPrev->rx_bc_crc_error, pDelta->rx_bc_crc_error);
    DIAG_GET_UINT32_DELTA(pCurr->rx_map_crc_error, pPrev->rx_map_crc_error, pDelta->rx_map_crc_error);
    DIAG_GET_UINT32_DELTA(pCurr->rx_beacon_crc_error, pPrev->rx_beacon_crc_error, pDelta->rx_beacon_crc_error);
    DIAG_GET_UINT32_DELTA(pCurr->rx_rr_crc_error, pPrev->rx_rr_crc_error, pDelta->rx_rr_crc_error);
    DIAG_GET_UINT32_DELTA(pCurr->rx_lc_uc_crc_error, pPrev->rx_lc_uc_crc_error, pDelta->rx_lc_uc_crc_error);
    DIAG_GET_UINT32_DELTA(pCurr->rx_lc_bc_crc_error, pPrev->rx_lc_bc_crc_error, pDelta->rx_lc_bc_crc_error);

    DIAGD_TRACE("%s: curr rx_uc_crc_error:%u, rx_bc_crc_error:%u, rx_map_crc_error:%u, rx_beacon_crc_error:%u",
        __func__, pCurr->rx_uc_crc_error, pCurr->rx_bc_crc_error, pCurr->rx_map_crc_error, pCurr->rx_beacon_crc_error);
    DIAGD_TRACE("%s: prev rx_uc_crc_error:%u, rx_bc_crc_error:%u, rx_map_crc_error:%u, rx_beacon_crc_error:%u",
        __func__, pPrev->rx_uc_crc_error, pPrev->rx_bc_crc_error, pPrev->rx_map_crc_error, pPrev->rx_beacon_crc_error);
    DIAGD_TRACE("%s: curr rx_rr_crc_error:%u, rx_lc_uc_crc_error:%u, rx_lc_bc_crc_error:%u",
        __func__, pCurr->rx_rr_crc_error, pCurr->rx_lc_uc_crc_error, pCurr->rx_lc_bc_crc_error);
    DIAGD_TRACE("%s: prev rx_rr_crc_error:%u, rx_lc_uc_crc_error:%u, rx_lc_bc_crc_error:%u",
        __func__, pPrev->rx_rr_crc_error, pPrev->rx_lc_uc_crc_error, pPrev->rx_lc_bc_crc_error);


    /* Note - We don't calculate the delta of in and out octets */

    /* Start checking if discard packets over their thresholds */
    txDiscardTooManyMsg = DIAG_MOCA_LOG_NONE;
    rxDiscardTooManyMsg = DIAG_MOCA_LOG_NONE;

    /* Get the total Tx packets and the discarded Tx packets in delta time */
    totalPkts = pDelta->ecl_tx_total_pkts;

    discardPkts = pDelta->ecl_tx_ucast_unknown +
      pDelta->ecl_tx_mcast_unknown +
      pDelta->ecl_tx_ucast_drops +
      pDelta->ecl_tx_mcast_drops +
      pDelta->ecl_tx_buff_drop_pkts +
      pDelta->mac_tx_low_drop_pkts;

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
    /* Per definitions,
     * 1) discardPkts - CRC error or Timeout on preamble. CRC packets may be
     *      either egress packets with MoCA CRC error or packet received with
     *      CRC originally from the Ethernet on the other side of the MoCA
     *      network
     */
    totalPkts = pDelta->ecl_rx_total_pkts;
    discardPkts = pDelta->mac_rx_buff_drop_pkts +
      pDelta->rx_uc_crc_error +
      pDelta->rx_bc_crc_error +
      pDelta->rx_map_crc_error +
      pDelta->rx_beacon_crc_error +
      pDelta->rx_rr_crc_error +
      pDelta->rx_lc_uc_crc_error +
      pDelta->rx_lc_bc_crc_error;

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
    msgLen =
      sizeof(uint32_t) + pMsg->nodeStats.nodeStatsTblSize +
      (sizeof(diag_mocaIf_stats_t) * 2);

    /* Copy the current stats */
    memcpy(&pMsg->currStats, pCurr, sizeof(diag_mocaIf_stats_t));
    /* Copy the previous stats */
    memcpy(&pMsg->prevStats, pPrev, sizeof(diag_mocaIf_stats_t));
    diagMoca_buildHdrMocaLogMsg(
        &pMsg->msgHdr,
        rxDiscardTooManyMsg,
        msgLen);

    /* Allocate the memory for reading MoCA_STATUS data */
    pStatus = (diag_moca_status_t *)malloc(sizeof(diag_moca_stats_t));
    if (pStatus == NULL) {
      rtn = DIAGD_RC_OUT_OF_MEM;
      break;
    }

    /* Retrieve the current status information of the self node */
    rtn = diagMoca_GetStatus(pStatus);
    if (rtn == DIAGD_RC_ERR) {
      break;
    }

    /* Write to diagd log file in string format */
    diagMocaStrLog((char *)pMsg, pStatus);

    rtn = DIAGD_RC_OK;

  } while (false);


  /* free all allocated memory */
  if (pStatus != NULL) {
    free(pStatus);
  }

  if (pMsg != NULL) {
    free(pMsg);
  }

  DIAGD_EXIT("%s - rtn=0x%X", __func__, rtn);
  return (rtn);
} /* diagMoca_MonErrorCounts */



/*
 * Monitor performance of the connected nodes in MoCA network
 * Check the following status of each connected node against reference table
 * 1) rxUc phy rate
 * 2) rxUc rx power level
 * 3) average SNR
 * 4) rxUc bit-loading
 *
 * Note:2012/10/11 - Per HW engineeer, check 1) rxUc phy rate is sufficient.
 * If rxUc phy rate is poor, then log:
 *     * 2) rxUc rx power level
 *     * 3) average SNR
 *     * 4) rxUc bit-loading
 *
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
  diag_moca_status_t            *pStatus = NULL;
  diag_moca_nodestatus_entry_t  *pNode_Status;
  diag_moca_nodestatus_t   *pNodeStatus = NULL;
  diag_moca_perf_status_t  *pPerfStatus = NULL;
  bool linkup = false;
  struct moca_network_status ns;
  int ret;


  DIAGD_ENTRY("%s", __func__);

  do {
    /* Allocate the memory for reading MoCA_STATUS data */
    pStatus = (diag_moca_status_t *)malloc(sizeof(diag_moca_stats_t));
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
    rtn = diagMoca_IsLinkUp(&linkup);
    if (rtn != DIAGD_RC_OK || !linkup) {
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
    for (i = 0; i < MOCA_MAX_NODES; i++) {
      pPerfStatus->perfResult[i].valid = false;
    }

    pNodeStatus = &pPerfStatus->nodeStatus;
    /* Retrieve current node status table */
    bufLen = sizeof(diag_moca_nodestatus_t);

    /* rewrtie diagMoca_GetNodeStatus() */
    rtn = diagMoca_GetNodeStatus(pNodeStatus, &bufLen);
    if (rtn != DIAGD_RC_OK) {
      break;
    }

    /* Get the connected nodes */
    ret = moca_get_network_status(g_mocaHandle, &ns);
    if (ret != MOCA_API_SUCCESS) {
      DIAGD_TRACE("%s moca_get_network_status() failed! ret = %d", __func__, ret);
      break;
    }

    for (i = 0; i < MOCA_MAX_NODES ; i++) {
      if (ns.connected_nodes & (0x1 << i))
        pPerfStatus->noConnectedNodes++ ;
    }


    /* Due to the connected nodes are all active in MoCA network, the total
     * nodes should be 2 or more in order to check service performance.
     */
    if (pPerfStatus->noConnectedNodes < 2) {
      DIAGD_TRACE("%s: no of connected nodes = %d",
          __func__, pPerfStatus->noConnectedNodes);
      break;
    }

    /* Start checking MoCA Service Performance to the connected nodes */
    pNode_Status = &pNodeStatus->nodeStatus[0];
    msgType = DIAG_MOCA_LOG_NONE;
    DIAGD_TRACE("%s: Loop through pNodeStatus (nodeStatusTblSize: %d)\n",
        __func__, pNodeStatus->nodeStatusTblSize);

    for (count = 0;
        count < pNodeStatus->nodeStatusTblSize/sizeof(diag_moca_nodestatus_entry_t);
        count++, pNode_Status++) {
      uint32_t u_nodeData = 0;
      uint32_t profileType = 0;
      diag_moca_perf_status_entry_t  *pPerfStatusEntry = NULL;

      pPerfStatusEntry = &pPerfStatus->perfResult[pNode_Status->nodeId];
      pPerfStatusEntry->nodeId = pNode_Status->nodeId;  /* the connected Id */
      pPerfStatusEntry->valid = true;

      /* Check RxUc PhyRate */
      DIAGD_TRACE("%s: Check RxUc Phy Rate...", __func__);
      /* For MoCA2.0 phy_rate contains the value in Mbps */
      profileType = pNode_Status->profile.type;
      u_nodeData = pNode_Status->profile.rxUc.phy_rate;

      for (i = 0; i < DIAG_MOCA_PERF_LVL_MAX; i++) {
        DIAGD_TRACE("%s: Node profileTyp = %u, PhyRate=%9u,  refPhyRate = %9u",
            __func__, profileType, u_nodeData,
            (profileType == MOCA_EXT_STATUS_PROFILE_RX_UC_NPER)?
            diagMocaPerfReferenceTable[i].rxUcPhyRate_20:
            diagMocaPerfReferenceTable[i].rxUcPhyRate_11);

        if (u_nodeData >= ((profileType == MOCA_EXT_STATUS_PROFILE_RX_UC_NPER)?
              diagMocaPerfReferenceTable[i].rxUcPhyRate_20:
              diagMocaPerfReferenceTable[i].rxUcPhyRate_11)) {
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
        pPerfStatusEntry->rxUcPower = i;
        pPerfStatusEntry->rxUcAvgSnr = i;
        pPerfStatusEntry->rxUcBitLoading = i;
        continue;     /* Check next connected node */
      }

      /* Indicate to log the information into MoCA log file later. */
      msgType = DIAG_MOCA_LOG_POOR_PHY_RATE;

      DIAGD_TRACE("%s: Check RxUC Power...", __func__);
      /* 10/02/2012 per HW engineer Roy Chen, checking RxUc PHY Rate
       * is sufficient.
       * If rxUcPhyRate is poor, also set the performance level of
       * rx_power, avg_srn and bit-loading of the node
       * to the same as rxUcPhyRate's.
       *
       * RxUc phy_rate, rx_power, avg_snr and all the rxUc bit loading
       * data will be logged in diagMocaStrLog()
       */
      pPerfStatusEntry->rxUcPower = i;
      pPerfStatusEntry->rxUcAvgSnr = i;
      pPerfStatusEntry->rxUcBitLoading = i;

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

      /* Write to diagd log file in text format */
      diagMocaStrLog((char *)pPerfStatus, pStatus);

    }

    rtn = DIAGD_RC_OK;

  } while (false);

  /* free all resources */
  if (pStatus != NULL) {
    free(pStatus);
  }

  if (pPerfStatus != NULL) {
    free(pPerfStatus);
  }

  DIAGD_EXIT("%s - rtn=0x%X", __func__, rtn);
  return(rtn);
} /* diagMoca_MonServicePerf */

/*
 * Copy data from MoCA statistics
 *
 */
void diagMoca_CopyStats(diag_mocaIf_stats_t *mocaIf, diag_moca_stats_t *stats)
{
  /* copy from moca_gen_stats */
  mocaIf->ecl_tx_total_pkts     = stats->genStats.ecl_tx_total_pkts;
  mocaIf->ecl_tx_ucast_pkts     = stats->genStats.ecl_tx_ucast_pkts;
  mocaIf->ecl_tx_bcast_pkts     = stats->genStats.ecl_tx_bcast_pkts;
  mocaIf->ecl_tx_mcast_pkts     = stats->genStats.ecl_tx_mcast_pkts;
  mocaIf->ecl_tx_ucast_unknown  = stats->genStats.ecl_tx_ucast_unknown;
  mocaIf->ecl_tx_mcast_unknown  = stats->genStats.ecl_tx_mcast_unknown;
  mocaIf->ecl_tx_ucast_drops    = stats->genStats.ecl_tx_ucast_drops;
  mocaIf->ecl_tx_mcast_drops    = stats->genStats.ecl_tx_mcast_drops;
  mocaIf->ecl_tx_buff_drop_pkts = stats->genStats.ecl_tx_buff_drop_pkts;
  mocaIf->ecl_rx_total_pkts     = stats->genStats.ecl_rx_total_pkts;
  mocaIf->ecl_rx_ucast_pkts     = stats->genStats.ecl_rx_ucast_pkts;
  mocaIf->ecl_rx_bcast_pkts     = stats->genStats.ecl_rx_bcast_pkts;
  mocaIf->ecl_rx_mcast_pkts     = stats->genStats.ecl_rx_mcast_pkts;
  mocaIf->ecl_rx_ucast_drops    = stats->genStats.ecl_rx_ucast_drops;
  mocaIf->mac_tx_low_drop_pkts  = stats->genStats.mac_tx_low_drop_pkts;
  mocaIf->mac_rx_buff_drop_pkts = stats->genStats.mac_rx_buff_drop_pkts;
  mocaIf->rx_beacons            = stats->genStats.rx_beacons;
  mocaIf->rx_map_packets        = stats->genStats.rx_map_packets;
  mocaIf->rx_rr_packets         = stats->genStats.rx_rr_packets;
  mocaIf->rx_control_uc_packets	= stats->genStats.rx_control_uc_packets;
  mocaIf->rx_control_bc_packets	= stats->genStats.rx_control_bc_packets;

  /* copy from moca_ext_octet_count */
  mocaIf->in_octets_hi          = stats->extOctCnt.in_octets_hi;
  mocaIf->in_octets_lo          = stats->extOctCnt.in_octets_lo;
  mocaIf->out_octets_hi         = stats->extOctCnt.out_octets_hi;
  mocaIf->out_octets_lo         = stats->extOctCnt.out_octets_lo;

  /* copy from moca_error_stats */
  mocaIf->rx_uc_crc_error       = stats->totalExtStats.rx_uc_crc_error;
  mocaIf->rx_bc_crc_error       = stats->totalExtStats.rx_bc_crc_error;
  mocaIf->rx_map_crc_error      = stats->totalExtStats.rx_map_crc_error;
  mocaIf->rx_beacon_crc_error   = stats->totalExtStats.rx_beacon_crc_error;
  mocaIf->rx_rr_crc_error       = stats->totalExtStats.rx_rr_crc_error;
  mocaIf->rx_lc_uc_crc_error    = stats->totalExtStats.rx_lc_uc_crc_error;
  mocaIf->rx_lc_bc_crc_error    = stats->totalExtStats.rx_lc_bc_crc_error;

}
