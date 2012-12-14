/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagd tester related functions
 *
 */

/*--------------------------------------------------------------------------
 *
 * Includes
 *
 *--------------------------------------------------------------------------
 */
#include <asm/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>

#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "diagdIncludes.h"
#include "diagTestMocaLog.h"


#define DIAG_HOSTCMD_PORT   50152   /* the port client will be connecting to */
#define DIAG_BUF_LEN        (1024 * 1)
#define MAXDATASIZE 100 // max number of bytes we can get at once 

#define DIAG_QUIT           0xFFFF  /* Quit from this tester   */
#define DIAG_TRY_AGAIN      0x0000  /* Enter invalid number in diagMenu   */


const char diagdMsgHeaderMarker[] = {"DIag"};
#define DIAG_MSG_MARKER_LEN     sizeof(uint32_t)

/* TODO 05/29/12: Use an array of string for all commands like
 * {"Get Monitoring Log",
 *  "Get Diag Test Results",
 *  ...
 *  }
 *
 */

uint32_t diagMenu() {
  char str[30];
  int cmdId = DIAG_TRY_AGAIN;
  int len;

  printf("Commands: \n");
  printf(" 1   Get Monitoring Log\n");
  printf(" 2   Get Diag Test Results\n");
  printf(" 3   Run Intrusive Test (Currently only eth0 internal loopback available)\n");
  printf("         Note: The Bruno box will be forced to reboot after this test is finished.\n");
  printf(" 4   Get MoCA Node Connect PHY and CP information\n");
  printf("         Note: This option is currently NOT available!\n");
  printf(" 5   Get MoCA Init Params\n");
  printf(" 6   Get MoCA Self Node status\n");
  printf(" 7   Get MoCA Self Node config\n");
  printf(" 8   Get MoCA Node Status Table\n");
  printf(" 9   Get MoCA Node Statistics Table\n");
  printf("10   Get Summary of Kernel Error & Warning Messages Counters\n");
  printf("11   Get Detail Report of Kernel Error & Warning Messages Counters\n");
  printf("12   Get Network Interface Link Status & Statistics\n");
  printf(" q   Quit \n");

  printf("Enter>> ");

  if (!scanf("%s", str)) {
    printf("no matched!!\n");
    return cmdId;
  }

  /* TODO: Associate each command with a unique cmdId instead
   * of memcmp() to locate cmdId
   */
  len = strlen(str);
  if (len == 1) {
    if (memcmp(str, "1", 1) == 0) {
      cmdId = DIAGD_REQ_GET_MON_LOG;
    }
    else if (memcmp(str, "2", 1) == 0) {
      cmdId = DIAGD_REQ_GET_DIAG_RESULT_LOG;
    }
    else if (memcmp(str, "3", 1) == 0) {
      cmdId = DIAGD_REQ_RUN_TESTS;
    }
    else if (memcmp(str, "4", 1) == 0) {
      cmdId = DIAGD_REQ_MOCA_GET_CONN_INFO;
    }
    else if (memcmp(str, "5", 1) == 0) {
      cmdId = DIAGD_REQ_MOCA_GET_MOCA_INITPARMS;
    }
    else if (memcmp(str, "6", 1) == 0) {
      cmdId = DIAGD_REQ_MOCA_GET_STATUS;
    }
    else if (memcmp(str, "7", 1) == 0) {
      cmdId = DIAGD_REQ_MOCA_GET_CONFIG;
    }
    else if (memcmp(str, "8", 1) == 0) {
      cmdId = DIAGD_REQ_MOCA_GET_NODE_STATUS_TBL;
    }
    else if (memcmp(str, "9", 1) == 0) {
      cmdId = DIAGD_REQ_MOCA_GET_NODE_STATS_TBL;
    }
    else if (memcmp(str, "q", 1) == 0) {
      cmdId = DIAG_QUIT;
    }
  }
  else if (len == 2) {
    if (memcmp(str, "10", 2) == 0) {
      cmdId = DIAGD_REQ_GET_MON_KERN_MSGS_SUM;
    }
    else if (memcmp(str, "11", 2) == 0) {
      cmdId = DIAGD_REQ_GET_MON_KERN_MSGS_DET;
    }
    else if (memcmp(str, "12", 2) == 0) {
      cmdId = DIAGD_REQ_GET_NET_LINK_STATS;
    }
  }

  if (cmdId == DIAG_TRY_AGAIN) {
    printf("%s: Invalid number %s you entered! You need to enter number"
           " 1-12, or 'q' to quit.\n",  __func__, str);
    printf("%s: Try again!\n", __func__);
  }

  return (cmdId);

}  /* end of diagMenu */


void diagTest_printNodeConnInfo(
        diag_moca_node_connect_info_t  *pNodesConnInfo) {
  int   i,j;
  int   nodes = pNodesConnInfo->nodeInfoTblSize/sizeof(diag_moca_node_info_t);
  diag_moca_node_info_t  *pNode = &pNodesConnInfo->nodeInfo[0];
  
  printf("----------------------------\n");
  printf("self Node ID: %d\n", pNodesConnInfo->selfNodeId);
  printf("----------------------------\n");
  printf("rxUcPhyRate/CP\n");
  printf("Tx\\Rx         0              1              2              3              4              5              6              7");
  for (i = 0; i < nodes; i++)
  {
    if (pNode[i].txNodeId != 0xFF)
    {
      printf("\n  %2d", pNode[i].txNodeId );
      for (j = 0; j < 8; j++ )
      {
        printf("  %9d/%-2d/%-1d", 
               pNode[i].rxNodePhyInfo[j].rxUcPhyRate, 
               pNode[i].rxNodePhyInfo[j].cp, 
               pNode[i].rxNodePhyInfo[j].connQuality);
      }
    }
  }

  printf("\n              8              9             10             11             12             13             14             15");
  for (i = 0; i < nodes; i++)
  {
    if (pNode[i].txNodeId != 0xFF)
    {
      printf("\n  %2d", pNode[i].txNodeId );
      for (j = 8; j < 16; j++ )
      {
        printf("  %9d/%-2d/%-1d", 
               pNode[i].rxNodePhyInfo[j].rxUcPhyRate, 
               pNode[i].rxNodePhyInfo[j].cp, 
               pNode[i].rxNodePhyInfo[j].connQuality);
      }
    }
  }
  printf("\n");
           
}  /* end of diagTest_printNodeConnInfo */

/** Print moca initialization parmaters 
 * (From BRCM print msg routine)
 *
 * Prints MoCA initialization parameters stored in pInitParms structure in
 * a clean way.
 *
 * @param pInitParms (IN) pointer to MoCA initialization parameters
 * @return None.
 */
void diagTest_Print_InitParms(diag_moca_init_parms_t * pInitParms) {
  int i;

  printf ("                     MoCA Init Params          \n");
  printf ("==========================================================\n");
  printf ("aes_mm_key               = ");
  for (i = 0; i < 4; i++) {
    printf("0x%x ", pInitParms->aesMmKey.val[i]);
    if (i % 8 == 7) printf("\n        ");
  }
  printf ("\n");

  printf ("aes_pm_key               = ");
  for (i = 0; i < 4; i++) {
    printf("0x%x ", pInitParms->aesPmKey.val[i]);
    if (i % 8 == 7) printf("\n        ");
  }
  printf ("\n");

  printf ("bandwidth                = %u  (0x%X)\n", pInitParms->bandwidth, pInitParms->bandwidth);
  printf ("beacon_channel           = %u  (0x%X)\n", pInitParms->beaconChannel, pInitParms->beaconChannel);
  printf ("beacon_pwr_reduction     = %u  (0x%X)\n", pInitParms->beaconPwrReduction, pInitParms->beaconPwrReduction);
  printf ("beacon_pwr_reduction_en  = %u  (0x%X)\n", pInitParms->beaconPwrReductionEn, pInitParms->beaconPwrReductionEn);
  printf ("bo_mode                  = %u  (0x%X)\n", pInitParms->boMode, pInitParms->boMode);
  printf ("const_rx_submode         = %u  ( 0x%x )\n", pInitParms->constRxSubmode, pInitParms->constRxSubmode);

  printf ("== const_tx_params  =================================== \n");

  printf ("const_tx_submode         = %u  ( 0x%x )\n", pInitParms->constTxParams.const_tx_submode,
      pInitParms->constTxParams.const_tx_submode);
  printf ("const_tx_sc1             = %u  ( 0x%x )\n", pInitParms->constTxParams.const_tx_sc1,
      pInitParms->constTxParams.const_tx_sc1);
  printf ("const_tx_sc2             = %u  ( 0x%x )\n", pInitParms->constTxParams.const_tx_sc2,
      pInitParms->constTxParams.const_tx_sc2);
  printf ("const_tx_band[16]        =\n");
  for (i = 0; i < 16; i++) {
    printf ("%08x ", pInitParms->constTxParams.const_tx_band[i]);
    if (i % 8 == 7) printf("\n");
  }
  printf ("== end const_tx_params  =============================== \n");


  printf ("continuous_power_tx_mode = %u  (0x%X)\n", pInitParms->continuousPwrTxMode, pInitParms->continuousPwrTxMode);
  printf ("continuous_rx_mode_attn  = %u  (0x%X)\n", pInitParms->continuousRxModeAttn, pInitParms->continuousRxModeAttn);
  printf ("device_class             = %u  (0x%X)\n", pInitParms->deviceClass, pInitParms->deviceClass);
  printf ("egr_mc_filter_en         = %u  (0x%X)\n", pInitParms->egrMcFilterEn, pInitParms->egrMcFilterEn);
  printf ("flow_control_en          = %u  (0x%X)\n", pInitParms->flowControlEn, pInitParms->flowControlEn);
  printf ("freq_mask                = %u  (0x%X)\n", pInitParms->freqMask, pInitParms->freqMask);
  printf ("init1                    = %u  (0x%X)\n", pInitParms->init1, pInitParms->init1);
  printf ("init2                    = %u  (0x%X)\n", pInitParms->init2, pInitParms->init2);
  printf ("init3                    = %u  (0x%X)\n", pInitParms->init3, pInitParms->init3);
  printf ("init4                    = %u  (0x%X)\n", pInitParms->init4, pInitParms->init4);
  printf ("init5                    = %u  (0x%X)\n", pInitParms->init5, pInitParms->init5);
  printf ("init6                    = %u  (0x%X)\n", pInitParms->init6, pInitParms->init6);
  printf ("init7                    = %u  (0x%X)\n", pInitParms->init7, pInitParms->init7);
  printf ("init8                    = %u  (0x%X)\n", pInitParms->init8, pInitParms->init8);
  printf ("init9                    = %u  (0x%X)\n", pInitParms->init9, pInitParms->init9);
  printf ("lab_mode                 = %u  (0x%X)\n", pInitParms->labMode, pInitParms->labMode);
  printf ("led_settings             = %u  (0x%X)\n", pInitParms->ledSettings, pInitParms->ledSettings);
  printf ("lof                      = %u  (0x%X)\n", pInitParms->lastOperFreq, pInitParms->lastOperFreq);
  printf ("low_pri_q_num            = %u  (0x%X)\n", pInitParms->lowPriQNum, pInitParms->lowPriQNum);

  diagMoca_log_mac_addr(false, &pInitParms->macAddr);

  printf ("max_tx_power             = %d  (0x%X)\n", pInitParms->maxTxPower, pInitParms->maxTxPower);

  printf ("== mmk_key  =========================================== \n");
  printf ("mmk_key_hi               = %u  (0x%X)\n",
      pInitParms->mmkKey.mmk_key_hi, pInitParms->mmkKey.mmk_key_hi);
  printf ("mmk_key_lo               = %u  (0x%X)\n",
      pInitParms->mmkKey.mmk_key_lo, pInitParms->mmkKey.mmk_key_lo);
  printf ("== end mmk_key  ======================================= \n");

  printf ("mtm_en                   = %u  (0x%X)\n", pInitParms->mtmEn, pInitParms->mtmEn);
  printf ("multicast_mode           = %u  (0x%X)\n", pInitParms->mcastMode, pInitParms->mcastMode);
  printf ("nc mode                  = %u  (0x%X)\n", pInitParms->ncMode, pInitParms->ncMode);
  printf ("ofdma_en                 = %u  (0x%X)\n", pInitParms->ofdmaEn, pInitParms->ofdmaEn);
  printf ("otf_en                   = %u  (0x%X)\n", pInitParms->otfEn, pInitParms->otfEn);

  printf ("== pmk_initial_key  =================================== \n");
  printf ("pmk_initial_key_hi       = %u  ( 0x%x )\n",
      pInitParms->pmkInitKey.pmk_initial_key_hi,
      pInitParms->pmkInitKey.pmk_initial_key_hi);
  printf ("pmk_initial_key_lo       = %u  ( 0x%x )\n",
      pInitParms->pmkInitKey.pmk_initial_key_lo,
      pInitParms->pmkInitKey.pmk_initial_key_lo);
  printf ("== end pmk_initial_key  =============================== \n");

  printf ("pns_freq_mask            = %u  (0x%X)\n", pInitParms->pnsFreqMask, pInitParms->pnsFreqMask);
  printf ("preferred_nc             = %u  (0x%X)\n", pInitParms->preferedNC, pInitParms->preferedNC);
  printf ("primary_ch_offset        = %u  (0x%X)\n", pInitParms->primChOffset, pInitParms->primChOffset);
  printf ("privacy_en               = %u  (0x%X)\n", pInitParms->privacyEn, pInitParms->privacyEn);
  printf ("qam256_capability        = %u  (0x%X)\n", pInitParms->qam256Capability, pInitParms->qam256Capability);
  printf ("rf_band                  = %u  (0x%X)\n", pInitParms->rfBand, pInitParms->rfBand);
  printf ("single_channel_operation = %u  (0x%X)\n", pInitParms->singleChOp, pInitParms->singleChOp);

  printf ("== taboo_channels  ==================================== \n");
  printf ("taboo_fixed_mask_start   = %u  (0x%X)\n", pInitParms->tabooFixedMaskStart, pInitParms->tabooFixedMaskStart);
  printf ("taboo_fixed_channel_mask = %u  (0x%X)\n", pInitParms->tabooFixedChannelMask, pInitParms->tabooFixedChannelMask);
  printf ("taboo_left_mask          = %u  (0x%X)\n", pInitParms->tabooLeftMask, pInitParms->tabooLeftMask);
  printf ("taboo_right_mask         = %u  (0x%X)\n", pInitParms->tabooRightMask, pInitParms->tabooRightMask);
  printf ("tpc_en                   = %u  (0x%X)\n", pInitParms->txPwrControlEn, pInitParms->txPwrControlEn);
  printf ("turbo_en                 = %u  (0x%X)\n", pInitParms->turboEn, pInitParms->turboEn);
  printf ("== end taboo_channels  ================================ \n");

  printf ("==========================================================\n");

}


/** Print MoCA self node status 
 * (From BRCM print msg routine)
 *
 *
 * @param pStatus (IN) pointer to MoCA_STATUS structure
 * @return None.
 */
void diagTest_Print_SelfNodeStatus(diag_moca_status_t *pStatus) {

  diagMocaMyStatusLog(false, pStatus);
  
}  /* end of diagTest_Print_SelfNodeStatus */


/** Print moca configuration
 * (Brcm print message routine)
 *
 * Prints MoCA configuration data stored in pCfg
 *
 * @param pCfg (IN) pointer to config data
 * @param showAbsSnrTable (IN) flag indicating whether or not to print
 *                             absolute value SNR table
 * @return None.
 */
void diagTest_Print_Config(diag_moca_config_parms_t *pCfg, uint32_t showAbsSnrTable,
                           uint32_t rftype) {
  printf("                 MoCA Configuration Parameters\n");
  printf("==================================================================\n");
  printf("arpl_th_50                 = %d\n", pCfg->arplTh50);
  printf("arpl_th_100                = %d\n", pCfg->arplTh100);
  printf("assertText                 = %u (0x%X)\n", pCfg->assertText, pCfg->assertText);
  printf("assert_restart             = %u (0x%X)\n", pCfg->assertRestart,
      pCfg->assertRestart);
  printf("cir_prints                 = %u (0x%X)\n", pCfg->cirPrints, pCfg->cirPrints);
  printf("continuous_ie_map_insert   = %u (0x%X)\n", pCfg->continuousIeMapInsert,
      pCfg->continuousIeMapInsert);
  printf("continuous_ie_rr_insert    = %u (0x%X)\n", pCfg->continuousIeRrInsert,
      pCfg->continuousIeRrInsert);
  printf("dont_start_moca            = %u (0x%X)\n", pCfg->dontStartMoca,
      pCfg->dontStartMoca);
  printf("en_capable                 = %u (0x%X)\n", pCfg->enCapable,
      pCfg->enCapable);
  printf("extra_rx_packets_per_qm    = %u (0x%X)\n", pCfg->extraRxPktsPerQm,
      pCfg->extraRxPktsPerQm);
  printf("fragmentation              = %u (0x%X)\n", pCfg->fragmentation,
      pCfg->fragmentation);
  printf("freq_shift                 = %u (0x%X)\n", pCfg->freqShift,
      pCfg->freqShift);
  printf("lab_snr_graph_set          = %u (0x%X)\n", pCfg->labSnrGraphSet,
      pCfg->labSnrGraphSet);
  printf("lof_update                 = %u (0x%X)\n", pCfg->lofUpdate,
      pCfg->lofUpdate);
  printf("loopback_en                = %u (0x%X)\n", pCfg->loopbackEn,
      pCfg->loopbackEn);
  printf("m1_tx_power_variation      = %u (0x%X)\n", pCfg->m1TxPwrVariation,
      pCfg->m1TxPwrVariation);
  printf("max_frame_size             = %u (0x%X)\n", pCfg->maxFrameSize,
      pCfg->maxFrameSize);
  printf("max_map_cycle              = %u (0x%X)\n", pCfg->maxMapCycle,
      pCfg->maxMapCycle);
  printf("max_pkt_aggr               = %u (0x%X)\n", pCfg->maxPktAggr,
      pCfg->maxPktAggr);
  printf("max_transmit_time          = %u (0x%X)\n", pCfg->maxTxTime,
      pCfg->maxTxTime);
  printf("min_bw_alarm_threshold     = %u (0x%X)\n", pCfg->minBwAlarmThreshold,
      pCfg->minBwAlarmThreshold);
  printf("min_map_cycle              = %u (0x%X)\n", pCfg->minMapCycle,
      pCfg->minMapCycle);
  printf("moca_core_trace_enable     = %u (0x%X)\n", pCfg->coreTraceEn,
      pCfg->coreTraceEn);
  printf("nbas_capping_en            = %u (0x%X)\n", pCfg->nbasCappingEn,
      pCfg->nbasCappingEn);
  printf("ooo_lmo_threshold          = %u (0x%X)\n", pCfg->oooLmoThreshold,
      pCfg->oooLmoThreshold);
  printf("orr_en                     = %u (0x%X)\n", pCfg->orrEn,
      pCfg->orrEn);
  printf("password                   = %s\n",  pCfg->pwd.password);
  printf("per_mode                   = %u (0x%X)\n",  pCfg->perMode,
      pCfg->perMode);
  printf("pmk_exchange_interval      = %u (0x%X)\n",  pCfg->pmkExchInterval,
      pCfg->pmkExchInterval);
  printf("power_state                = %u (0x%X)\n",  pCfg->pwrState,
      pCfg->pwrState);
  diagMoca_log_priority_allocations(false, &pCfg->priAlloc);
  printf("pss_en                     = %u (0x%X)\n",  pCfg->pssEn,
      pCfg->pssEn);
  printf("res1                       = %u (0x%X)\n",  pCfg->res1,
      pCfg->res1);
  printf("res2                       = %u (0x%X)\n",  pCfg->res2,
      pCfg->res2);
  printf("res3                       = %u (0x%X)\n",  pCfg->res3,
      pCfg->res3);
  printf("res4                       = %u (0x%X)\n",  pCfg->res4,
      pCfg->res4);
  printf("res5                       = %u (0x%X)\n",  pCfg->res5,
      pCfg->res5);
  printf("res6                       = %u (0x%X)\n",  pCfg->res6,
      pCfg->res6);
  printf("res7                       = %u (0x%X)\n",  pCfg->res7,
      pCfg->res7);
  printf("res8                       = %u (0x%X)\n",  pCfg->res8,
      pCfg->res8);
  printf("res9                       = %u (0x%X)\n",  pCfg->res9,
      pCfg->res9);

  diagMoca_log_rlapm_table_100(false, &pCfg->rlampTbl100);
  diagMoca_log_rlapm_table_50(false, &pCfg->rlampTbl50);

  printf("rx_power_tuning            = %d (0x%X)\n",  pCfg->rxPwrTuning,
      pCfg->rxPwrTuning);
  printf("rx_tx_packets_per_qm       = %u (0x%X)\n",  pCfg->rxTxPktsPerQm,
      pCfg->rxTxPktsPerQm);
  printf("sapm_en                    = %u (0x%X)\n",  pCfg->sapmEn,
      pCfg->sapmEn);

  diagMoca_log_sapm_table_100(false, &pCfg->sapmTbl100);
  diagMoca_log_sapm_table_50(false, &pCfg->sapmTbl50);
  diagMoca_log_snr_margin_ldpc(false, &pCfg->snrMarginLdpc);
  diagMoca_log_snr_margin_ldpc_pre5(false, &pCfg->snrMarginLdpcPre5);
  diagMoca_log_snr_margin_ofdma(false, &pCfg->snrMarginOfdma);
  diagMoca_log_snr_margin_rs(false, &pCfg->snrMarginRs);
  diagMoca_log_snr_margin_table_ldpc(false, &pCfg->snrMarginTblLdpc);
  diagMoca_log_snr_margin_table_ldpc_pre5(false, &pCfg->snrMarginTblLdpcPre5);
  diagMoca_log_snr_margin_table_ofdma(false, &pCfg->snrMarginTblOfdma);
  diagMoca_log_snr_margin_table_rs(false, &pCfg->snrMarginTblRs);
  diagMoca_log_start_ulmo(false, &pCfg->startUlmo);

  printf("snr_prints                 = %u (0x%X)\n",  pCfg->snrPrints,
      pCfg->snrPrints);
  printf("target_phy_rate_20         = %u (0x%X)\n",  pCfg->targetPhyRate20,
      pCfg->targetPhyRate20);
  printf("target_phy_rate_20_turbo   = %u (0x%X)\n",  pCfg->targetPhyRate20Turbo,
      pCfg->targetPhyRate20Turbo);
  printf("target_phy_rate_qam_128    = %u (0x%X)\n",  pCfg->targetPhyRateQam128,
      pCfg->targetPhyRateQam128);
  printf("target_phy_rate_qam_256    = %u (0x%X)\n",  pCfg->targetPhyRateQam256,
      pCfg->targetPhyRateQam256);
  printf("tek_exchange_interval      = %u (0x%X)\n",  pCfg->tekExchInterval,
      pCfg->tekExchInterval);
  printf("verbose                    = %u (0x%X)\n",  pCfg->verbose,
      pCfg->verbose);
  printf("wdog_enable                = %u (0x%X)\n",  pCfg->wdogEn,
      pCfg->wdogEn);
  printf("==================================================================\n");

}


/** Print moca node status table of all connected nodes
 * (Brcm print message routine)
 *
 * Prints MoCA in pNodeStatusTbl
 *
 * @param pNodeStatusTbl (IN) pointer to node status data
 * @return None.
 */
void diagTest_Print_nodeStatusTbl(diag_moca_nodestatus_t *pNodeStatusTbl) {
  uint32_t  tblSize;
  uint32_t count;
  diag_moca_nodestatus_entry_t *pNodeStatus = NULL;

  tblSize = pNodeStatusTbl->nodeStatusTblSize;
  pNodeStatus = pNodeStatusTbl->nodeStatus;

      for (count = 0;
              count < tblSize/sizeof(diag_moca_nodestatus_entry_t);
                  count++, pNodeStatus++) {
          diagMocaNodeStatusLog(false, pNodeStatus);
      }

}  /* end of diagTest_Print_nodeStatusTbl */


/** Print moca node statistics table of all connected nodes
 * (Brcm print message routine)
 *
 * Prints MoCA in pNodeStatusTbl
 *
 * @param pNodeStatsTbl (IN) pointer to node status data
 * @return None.
 */
void diagTest_Print_nodeStatisticsTbl(diag_moca_node_stats_table_t *pNodeStatsTbl) {
  diagMocaNodeStatsLog(false, pNodeStatsTbl);
}  /* end of diagTest_Print_nodeStatisticsTbl */

void diagTest_Print_KernMsgsReport(char *pPayload) {
  printf("%s", pPayload);
}

void display_LoopbackTestMsg() {
  printf("The thin Bruno will be rebooted once the loopback test is done!\n");
  printf("To check the loopback test result. You need to wait until\n");
  printf("the bruno box is up and running. Then select option 2.\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  struct sockaddr_in my_addr;
  char *pBuffer = NULL;
  int   bytecount;
  int   buffer_len=0;
  int   hsock = -1;
  int   err = 1;
  int   cmdIdx;
  int   total_recv_bytecount = 0;
  char *pPayload = NULL;
  char *pTmpPayload;
  uint32_t  msgLen;

  diag_msg_header_t  *pMsgHdr = NULL;


  if (argc != 2) {
    fprintf(stderr, "Usage: diagTester <server_ip>\n");
    exit (0);
  }

  do {

    /* Dispaly command menu */
    cmdIdx = diagMenu();

    if (cmdIdx == DIAG_QUIT) {
      break;
    }

    if (cmdIdx == DIAG_TRY_AGAIN || cmdIdx == DIAGD_REQ_MOCA_GET_CONN_INFO) {
      continue;
    }

    /* close the socket if it was used */
    if (hsock != -1) {
       close(hsock);
    }

    hsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(hsock == -1){
      fprintf(stderr, "Error initializing socket %d\n",errno);
      break;
    }

    memset(&my_addr, 0, sizeof(my_addr));

    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr(argv[1]);
    my_addr.sin_port = htons(DIAG_HOSTCMD_PORT);

    if ( connect( hsock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1 ) {
      if ((err = errno) != EINPROGRESS) {
        fprintf(stderr, "Error connecting socket (errno:%s)\n", strerror(errno));
        break;
      }
    }

    /* Now lets do the client related stuff */
    /* free pBuffer if it was used */
    if (pBuffer != NULL) {
       free(pBuffer);
       pBuffer = NULL;
    }

    buffer_len = DIAG_BUF_LEN;
    pBuffer = malloc(buffer_len);
    memset(pBuffer, '\0', buffer_len);

    /* Compose the reqeust to diagd */
    pMsgHdr = (diag_msg_header_t *)pBuffer;
    memset(pMsgHdr, 0, sizeof(diag_msg_header_t));
    bcopy((void *)diagdMsgHeaderMarker, 
          (void *)&pMsgHdr->headerMarker,
          DIAG_MSG_MARKER_LEN);
    pMsgHdr->len = 0;
    
    /* Set the msgType */
    pMsgHdr->msgType = cmdIdx;
   
    if ((bytecount=send(hsock, pBuffer, sizeof(diag_msg_header_t), 0))== -1) {
      err = errno;
      fprintf(stderr, "Error sending data %s\n", strerror(errno));
      break;
    }
    printf("Sent bytes %d\n", bytecount);

    err = 0;      /* Default no error */

    total_recv_bytecount = 0;
    
    /* Read rsp header first to get the length*/
    if ((bytecount = recv(hsock, pBuffer, sizeof(diag_msg_header_t), 0))== -1) {
      err = errno;
      fprintf(stderr, "Error receiving data %s\n", strerror(errno));
      break;
    }

    DIAGD_TRACE("%s: RspHdr  bytecount=%u, headerMarker=0x%x, len=%u, msgType=0x%x", 
                __func__, bytecount, pMsgHdr->headerMarker, 
                pMsgHdr->len, pMsgHdr->msgType);


    /* Check request to get a log file */
    if ((cmdIdx == DIAGD_REQ_GET_MON_LOG) ||(cmdIdx == DIAGD_REQ_GET_DIAG_RESULT_LOG)) {

      do {
    
        /* Yes, download a log file */
        if ((bytecount = recv(hsock, pBuffer, buffer_len, 0))== -1) {
          err = errno;
          fprintf(stderr, "Error receiving data %s\n", strerror(errno));
          break;
        }
        else if (bytecount == 0) {
          /* command completed */
          printf("\nCommand Completed: total_recv_bytecount=%d.\n\n", 
                 total_recv_bytecount);
          break;
        } 

        if ((cmdIdx == DIAGD_REQ_GET_MON_LOG) ||(cmdIdx == DIAGD_REQ_GET_DIAG_RESULT_LOG)) {
          pBuffer[bytecount] = '\0';   /* Set NULL to indicate the end of string */
          printf("%s", pBuffer);
        }
        else {
          /* ignore for now */
        }
      } while (true);
    }  /* if (cmdIdx) */
    else {
      
      pTmpPayload = pPayload;
      msgLen = pMsgHdr->len;
      
      /* Other requests */
      /* free pPayload if it was used */
      if (pPayload != NULL) {
         free(pPayload);
         pPayload = NULL;
      }

      pPayload = malloc(msgLen);
      memset(pPayload, 0, msgLen);

      pTmpPayload = pPayload;
      
      do {
        /* Receive payload of response msg from server */
        if ((bytecount = recv(hsock, pBuffer, buffer_len, 0))== -1) {
          err = errno;
          fprintf(stderr, "Error receiving data %s\n", strerror(errno));
          break;
        }
        else if (bytecount == 0) {
          /* command completed */
          printf("\nCommand Completed: total_recv_bytecount=%d.\n\n", 
                 total_recv_bytecount);
          err = 0;
          break;
        } 

        printf("Recv payload: bytecount=%d.\n", bytecount); 

        if (msgLen < (total_recv_bytecount + bytecount)) {
          fprintf(stderr, "Recved too many data(expected=%u, actual=%u)\n",
                  msgLen, (total_recv_bytecount + bytecount));
          break;
          err = 1;
        }

        memcpy(pTmpPayload, pBuffer, bytecount);
        pTmpPayload =  pTmpPayload + bytecount;
        
        total_recv_bytecount += bytecount;

      } while (true);

      if (cmdIdx == DIAGD_REQ_RUN_TESTS) {
        display_LoopbackTestMsg();
        continue;
      }

      if (!total_recv_bytecount || err) {
        printf("No available information is received from the thin Bruno!\n\n");
        continue;
      }
      /* Parse the following responses */
      switch (cmdIdx) {
        case DIAGD_REQ_MOCA_GET_CONN_INFO:
          {
            diag_moca_node_connect_info_t  *pNodeInfo;
            /* Point to the payload */
            pNodeInfo = (diag_moca_node_connect_info_t *)pPayload;
            diagTest_printNodeConnInfo(pNodeInfo);        
          }
          break;
  
  
        case DIAGD_REQ_MOCA_GET_MOCA_INITPARMS:
          {
            diag_moca_init_parms_t *pNodeInitParms;
  
            /* Point to the payload */
            pNodeInitParms = (diag_moca_init_parms_t *)pPayload;
            diagTest_Print_InitParms(pNodeInitParms);
          }       
          break;

        case DIAGD_REQ_MOCA_GET_STATUS:
          {
            diag_moca_status_t *pStatus;
            pStatus = (diag_moca_status_t *)pPayload;
            diagTest_Print_SelfNodeStatus(pStatus);
          }
          break;
  
        case DIAGD_REQ_MOCA_GET_CONFIG:
          {
            diag_moca_config_t  *pCfg;
            pCfg = (diag_moca_config_t *)pPayload;
            diagTest_Print_Config(&pCfg->Cfg, 0, pCfg->rfBand);
          }
          break;
  
        case DIAGD_REQ_MOCA_GET_NODE_STATUS_TBL:
          {
            diag_moca_nodestatus_t *pNodeStatusTbl;
            pNodeStatusTbl = (diag_moca_nodestatus_t *)pPayload;          
            diagTest_Print_nodeStatusTbl(pNodeStatusTbl);
          }
          break;
  
        case DIAGD_REQ_MOCA_GET_NODE_STATS_TBL:
          {
            diag_moca_node_stats_table_t *pNodeStatsTbl;
            pNodeStatsTbl = (diag_moca_node_stats_table_t *)pPayload;
            diagTest_Print_nodeStatisticsTbl(pNodeStatsTbl);
          }
          break;
        case DIAGD_REQ_GET_MON_KERN_MSGS_SUM:
        case DIAGD_REQ_GET_MON_KERN_MSGS_DET:
        case DIAGD_REQ_GET_NET_LINK_STATS:
          {
            diagTest_Print_KernMsgsReport(pPayload);
          }
          break;
      }  /* end of switch */

    }  /* else (cmdIdx) */

  } while (true);

  
  if (pBuffer != NULL) {
    free(pBuffer);
  }

  if (pPayload != NULL) {
     free(pPayload);
  }

  if (hsock != -1) {
    close(hsock);
  }

  return(err);
}
