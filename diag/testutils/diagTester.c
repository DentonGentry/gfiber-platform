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


#define DIAG_HOSTCMD_PORT   50152   /* the port client will be connecting to */
#define DIAG_BUF_LEN        (1024 * 1)
#define MAXDATASIZE 100 // max number of bytes we can get at once 

#define DIAG_QUIT           0xFFFF  /* Quit from this tester   */
#define DIAG_TRY_AGAIN      0x0000  /* Enter invalid number in diagMenu   */


const char diagdMsgHeaderMarker[] = {"DIag"};
#define DIAG_MSG_MARKER_LEN     sizeof(uint32_t)


void convertUpTime (
  uint32_t timeInSecs, uint32_t *pTimeInHrs,
  uint32_t *pTimeInMin, uint32_t *pTimeInSecs) {
  *pTimeInHrs  = timeInSecs / (NO_OF_SECS_IN_MIN * NO_OF_MINS_IN_HR);
  timeInSecs   = timeInSecs % (NO_OF_SECS_IN_MIN * NO_OF_MINS_IN_HR);
  *pTimeInMin  = timeInSecs / NO_OF_SECS_IN_MIN;
  timeInSecs   = timeInSecs % NO_OF_SECS_IN_MIN;
  *pTimeInSecs = timeInSecs;
  return;
}  /* end of convertUpTime */

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

  scanf("%s", str);

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
void diagTest_Print_InitParms (MoCA_INITIALIZATION_PARMS * pInitParms) {
  int i;
  char buffer[80];
  
  printf ("        MoCA InitTime Configuration          \n");
  printf ("=============================================\n");
  printf ("Operating Version          : %s \n", (pInitParms->operatingVersion == MoCA_VERSION_11) ? "1.1" : "1.0");
  printf ("Network Controller Mode    : %s \n", (pInitParms->ncMode == MoCA_AUTO_NEGOTIATE_FOR_NC) ? "AUTO" : ((pInitParms->ncMode == MoCA_ALWAYS_NC) ? "NC" : "NN"));
  printf ("SingleCh                   : %s \n", (pInitParms->autoNetworkSearchEn == MoCA_AUTO_NW_SCAN_ENABLED) ? "off" :
                                               (pInitParms->autoNetworkSearchEn == MoCA_AUTO_NW_SCAN_DISABLED) ? "on" : "on2");
  printf ("Privacy                    : %s \n", (pInitParms->privacyEn == MoCA_PRIVACY_ENABLED) ? "enabled" : "disabled");
  printf ("Tx Pwr Control             : %s \n", (pInitParms->txPwrControlEn == MoCA_TPC_ENABLED) ? "enabled" : "disabled");

  switch(pInitParms->constTransmitMode)
  {
    case MoCA_NORMAL_OPERATION:
      sprintf(buffer, "%s", "Normal");
      break;
    case MoCA_CONTINUOUS_TX_PROBE_I:
      sprintf(buffer, "%s", "Tx");
      break;
    case MoCA_CONTINUOUS_RX:
    case MoCA_CONTINUOUS_RX_LO_ON:
      sprintf(buffer, "%s", "Rx");
      break;
    case MoCA_EXTERNAL_CONTROL:
      sprintf(buffer, "%s", "External");
      break;
    case MoCA_CONTINUOUS_TX_CW:
      sprintf(buffer, "%s", "Tx - CW");
      break;
    case MoCA_CONTINUOUS_TX_TONE:
      sprintf(buffer, "%s", "Tx - Tone");
      break;
    case MoCA_CONTINUOUS_TX_TONE_SC:
      sprintf(buffer, "%s (0x%x)",
        "Tx - Tone", pInitParms->initOptions.constTxSubCarrier1);
      break;
    case MoCA_CONTINUOUS_TX_DUAL_TONE_SC:
      sprintf(buffer, "%s (0x%x / 0x%x)",
        "Tx - Tone", pInitParms->initOptions.constTxSubCarrier1,
        pInitParms->initOptions.constTxSubCarrier2);
      break;
    case MoCA_CONTINUOUS_TX_BAND:
      sprintf(buffer, "%s (%08x %08x %08x %08x %08x %08x %08x %08x)", 
        "Band", pInitParms->initOptions.constTxNoiseBand[0],
        pInitParms->initOptions.constTxNoiseBand[1],
        pInitParms->initOptions.constTxNoiseBand[2],
        pInitParms->initOptions.constTxNoiseBand[3],
        pInitParms->initOptions.constTxNoiseBand[4],
        pInitParms->initOptions.constTxNoiseBand[5],
        pInitParms->initOptions.constTxNoiseBand[6],
        pInitParms->initOptions.constTxNoiseBand[7]);
      break;
    default:
       sprintf(buffer, "%s", "Unknown");
       break;
  }
  printf ("Const Tx Mode              : %s \n", buffer);
  printf ("Continuous Rx Mode Attn    : %d dBm\n", pInitParms->continuousRxModeAttn);
  printf ("Nv Params - Last Oper Freq : ");
  if (pInitParms->nvParams.lastOperFreq == MoCA_FREQ_UNSET)
    printf("not set\n");
  else
    printf ("%u Mhz\n", pInitParms->nvParams.lastOperFreq);

  printf ("Max Tx Power               : %d dBm\n", pInitParms->maxTxPowerBeacons);
  printf ("PasswordSize               : %u \n", pInitParms->passwordSize);
  printf ("Password                   : ");
  for (i = 0; i < pInitParms->passwordSize; i++) {
    printf ("%c", pInitParms->password [i]);
  }
  printf ("\n");
  printf ("Mcast Mode                 : %s \n", (pInitParms->mcastMode == MoCA_MCAST_BCAST_MODE) ? "Broadcast Mode" : "Normal");
  printf ("Laboratory Mode            : %s \n", (pInitParms->labMode == MoCA_LAB_MODE) ? "LabMode" : "Normal");
  printf ("Taboo Fixed Start Channel  : %d \n", pInitParms->tabooFixedMaskStart);
  printf ("Taboo Fixed Channel Mask   : 0x%08x \n", pInitParms->tabooFixedChannelMask);
  printf ("Taboo Left Mask            : 0x%08x \n", pInitParms->tabooLeftMask);
  printf ("Taboo Right Mask           : 0x%08x \n", pInitParms->tabooRightMask);
  printf ("Pwr Amplifier Driver Power : %d dBm\n", pInitParms->padPower);
  printf ("preferredNC                : %s \n", (pInitParms->preferedNC == MoCA_PREFERED_NC_MODE) ? "preferred" : "non-preferred" );
  printf ("LED Mode                   : %u \n", pInitParms->ledMode);
  printf ("Backoff Mode               : %s \n", pInitParms->boMode == MoCA_BO_MODE_FAST? "fast" : "slow");
  printf ("RF Type                    : %s \n", pInitParms->rfType == MoCA_RF_TYPE_D_BAND? "hi" :
                                               pInitParms->rfType == MoCA_RF_TYPE_E_BAND? "midlo" :
                                               pInitParms->rfType == MoCA_RF_TYPE_F_BAND? "midhi" : "wan");
  printf ("Node Type                  : %s \n", pInitParms->terminalIntermediateType == MoCA_NODE_TYPE_TERMINAL? 
                                                  "terminal":"intermediate");
  printf ("Beacon Channel             : %d \n", pInitParms->beaconChannel);    
  printf ("mrNonDefSeqNum             : %d \n", pInitParms->mrNonDefSeqNum);
  printf ("EGR MC Addr Filter En      : %d \n", pInitParms->egrMcFilterEn);
  printf ("lowPriQNum                 : %d \n", pInitParms->lowPriQNum);      
  printf ("qam256Capability           : %s \n", pInitParms->qam256Capability == MoCA_QAM256_CAPABILITY_DISABLED?
                                                  "off":"on");
  printf ("Freq Mask                  : 0x%08x \n", pInitParms->freqMask);
  printf ("PNS Freq Mask              : 0x%08x \n", pInitParms->pnsFreqMask);   
  printf ("OTF En                     : %d \n", pInitParms->otfEn);   
  printf ("Turbo En                   : %d \n", pInitParms->turboEn);
  printf ("Flow Control En            : %d \n", pInitParms->flowControlEn);
  printf ("Beacon Power Reduction     : %d \n", pInitParms->beaconPwrReduction);
  printf ("Beacon Power Reduction En  : %d \n", pInitParms->beaconPwrReductionEn);
  printf ("Persistent Stop            : %d \n", pInitParms->initOptions.dontStartMoca);   
  printf ("MTM En                     : %d \n", pInitParms->mtmEn);
  printf ("QAM1024 En                 : %d \n", pInitParms->qam1024En);
  printf ("=============================================\n");
}


/** Print MoCA self node status 
 * (From BRCM print msg routine)
 *
 *
 * @param pStatus (IN) pointer to MoCA_STATUS structure
 * @return None.
 */
void diagTest_Print_SelfNodeStatus(MoCA_STATUS *pStatus) {
  uint32_t  coreversionMajor, coreversionMinor, coreversionBuild;  
  uint32_t  timeH, timeM, timeS;  
  int count;  
  uint32_t noOfNodes = 0;
  
      
  coreversionMajor = pStatus->generalStatus.swVersion >> 28;
  coreversionMinor = (pStatus->generalStatus.swVersion << 4) >> 28;
  coreversionBuild = (pStatus->generalStatus.swVersion << 8) >> 8;
  
  printf ("           MoCA Status(General)     \n");
  printf ("==================================  \n");
  printf ("vendorId                  : %3d \t", pStatus->generalStatus.vendorId);
  printf ("  HwVersion                 : 0x%x \n", pStatus->generalStatus.hwVersion);
  printf ("SwVersion                 : %d.%d.%d \t", coreversionMajor, coreversionMinor,
       coreversionBuild);
  printf ("  self MoCA Version         : 0x%x \n", pStatus->generalStatus.selfMoCAVersion);
  printf ("networkVersionNumber      : 0x%x \t", pStatus->generalStatus.networkVersionNumber);
  printf ("  qam256Support             : %s \n", (pStatus->generalStatus.qam256Support == MoCA_QAM_256_SUPPORT_ON) ? "supported" : "unknown" );
  if (pStatus->generalStatus.operStatus == MoCA_OPER_STATUS_ENABLED)
    printf ("operStatus                : Enabled \t");
  else
    printf ("operStatus                : Hw Error \t");
  if (pStatus->generalStatus.linkStatus == MoCA_LINK_UP)
    printf ("  linkStatus                : Up \n");
  else
    printf ("  linkStatus                : Down \n");
  printf ("connectedNodes BitMask    : 0x%x \t", pStatus->generalStatus.connectedNodes);
  if (pStatus->generalStatus.nodeId >= MoCA_MAX_NODES)
    printf ("  nodeId                    : N/A \n");
  else
    printf ("  nodeId                    : %u \n", pStatus->generalStatus.nodeId);
  if (pStatus->generalStatus.ncNodeId >= MoCA_MAX_NODES)
    printf ("ncNodeId                  : N/A \n");
  else
    printf ("ncNodeId                  : %u \t\t", pStatus->generalStatus.ncNodeId);
    convertUpTime (pStatus->miscStatus.MoCAUpTime, &timeH, &timeM, &timeS);
    printf ("  upTime                    : %02uh:%02um:%02us\n", timeH, timeM, timeS);
    convertUpTime (pStatus->miscStatus.linkUpTime, &timeH, &timeM, &timeS);
    printf ("linkUpTime                : %02uh:%02um:%02us", timeH, timeM, timeS);
    if (pStatus->generalStatus.backupNcId >= MoCA_MAX_NODES)
      printf ("  backupNcId                : N/A \n");
    else
      printf ("  backupNcId                : %u \n", pStatus->generalStatus.backupNcId);
    printf ("rfChannel                 : %u Mhz\t", pStatus->generalStatus.rfChannel);
    printf ("  bwStatus                  : 0x%x \n", pStatus->generalStatus.bwStatus);
    printf ("NodesUsableBitMask        : 0x%x \t", pStatus->generalStatus.nodesUsableBitmask);
    printf ("  NetworkTabooMask          : 0x%x \n", pStatus->generalStatus.networkTabooMask);
    printf ("NetworkTabooStart         : %d \t\t", pStatus->generalStatus.networkTabooStart);
    printf ("  txGcdPowerReduction       : %d \n", pStatus->generalStatus.txGcdPowerReduction);
    printf ("pqosEgressNumFlows        : %d \t\t", pStatus->generalStatus.pqosEgressNumFlows);
    /* find the number of connected nodes from the connected nodes bitmask */
    for (count = 0; count < MoCA_MAX_NODES; count++) {
      if (pStatus->generalStatus.connectedNodes & (0x1 << count))
         noOfNodes++;
    }
    printf ("  Num of connectedNodes     : %d \n", noOfNodes);
    printf ("ledStatus                 : %x \n", pStatus->generalStatus.ledStatus);
    printf ("==================================  \n");
    printf ("           MoCA Status(Extended)    \n");
    printf ("==================================  \n");
  
    convertUpTime (pStatus->extendedStatus.lastPmkExchange, &timeH, &timeM, &timeS);
    printf ("lastPmkExchange           : %02uh:%02um:%02us\n", timeH, timeM, timeS );
    printf ("lastPmkInterval           : %d sec\n", pStatus->extendedStatus.lastPmkInterval );
    
    convertUpTime (pStatus->extendedStatus.lastTekExchange, &timeH, &timeM, &timeS);
    printf ("lastTekExchange           : %02uh:%02um:%02us\n", timeH, timeM, timeS );
    printf ("lastTekInterval           : %d sec\n", pStatus->extendedStatus.lastTekInterval );
    
    printf ("PMK Even Key              : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s\n",
         pStatus->extendedStatus.pmkEvenKey[0], pStatus->extendedStatus.pmkEvenKey[1],
         pStatus->extendedStatus.pmkEvenKey[2], pStatus->extendedStatus.pmkEvenKey[3],
         pStatus->extendedStatus.pmkEvenKey[4], pStatus->extendedStatus.pmkEvenKey[5],
         pStatus->extendedStatus.pmkEvenKey[6], pStatus->extendedStatus.pmkEvenKey[7],
         pStatus->extendedStatus.pmkEvenOdd==0?"(ACTIVE)":"");
    printf ("PMK Odd Key               : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s\n",
         pStatus->extendedStatus.pmkOddKey[0], pStatus->extendedStatus.pmkOddKey[1],
         pStatus->extendedStatus.pmkOddKey[2], pStatus->extendedStatus.pmkOddKey[3],
         pStatus->extendedStatus.pmkOddKey[4], pStatus->extendedStatus.pmkOddKey[5],
         pStatus->extendedStatus.pmkOddKey[6], pStatus->extendedStatus.pmkOddKey[7],
         pStatus->extendedStatus.pmkEvenOdd==1?"(ACTIVE)":"");
    printf ("TEK Even Key              : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s\n",
         pStatus->extendedStatus.tekEvenKey[0], pStatus->extendedStatus.tekEvenKey[1],
         pStatus->extendedStatus.tekEvenKey[2], pStatus->extendedStatus.tekEvenKey[3],
         pStatus->extendedStatus.tekEvenKey[4], pStatus->extendedStatus.tekEvenKey[5],
         pStatus->extendedStatus.tekEvenKey[6], pStatus->extendedStatus.tekEvenKey[7],
         pStatus->extendedStatus.tekEvenOdd==0?"(ACTIVE)":"");
    printf ("TEK Odd Key               : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s\n",
         pStatus->extendedStatus.tekOddKey[0], pStatus->extendedStatus.tekOddKey[1],
         pStatus->extendedStatus.tekOddKey[2], pStatus->extendedStatus.tekOddKey[3],
         pStatus->extendedStatus.tekOddKey[4], pStatus->extendedStatus.tekOddKey[5],
         pStatus->extendedStatus.tekOddKey[6], pStatus->extendedStatus.tekOddKey[7],
         pStatus->extendedStatus.tekEvenOdd==1?"(ACTIVE)":"");
    printf ("==================================  \n");
    printf ("           MoCA Status(Misc)    \n");
    printf ("==================================  \n");
    printf ("MAC GUID                  : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
         pStatus->miscStatus.macAddr[0],
         pStatus->miscStatus.macAddr[1],
         pStatus->miscStatus.macAddr[2],
         pStatus->miscStatus.macAddr[3],
         pStatus->miscStatus.macAddr[4],
         pStatus->miscStatus.macAddr[5]);
    printf ("Are we Network Controller : %s \n", (pStatus->miscStatus.isNC == 1) ? "yes" : "no");
    convertUpTime (pStatus->miscStatus.driverUpTime, &timeH, &timeM, &timeS);
    printf ("Driver Up Time            : %02uh:%02um:%02us \n", timeH, timeM, timeS);
    printf ("Link Reset Count          : %u \n", pStatus->miscStatus.linkResetCount);
    printf ("==================================  \n");
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
void diagTest_Print_Config(MoCA_CONFIG_PARAMS *pCfg, uint32_t showAbsSnrTable,
                           uint32_t rftype) {
  int i;

  printf ("        MoCA Configuration          \n");
  printf ("==================================  \n");
  printf ("maxFrameSize                : %u bytes \n", pCfg->maxFrameSize);
  printf ("maxTransmitTime             : %u uSec\n", pCfg->maxTransmitTime);
  printf ("minBwAlarmThreshold         : %u Mbps \n", pCfg->minBwAlarmThreshold);
  printf ("continuousIERRInsert        : %s \n", (pCfg->continuousIERRInsert == MoCA_CONTINUOUS_IE_RR_INSERT_ON) ? "on" : "off" );
  printf ("continuousIEMapInsert       : %s \n", (pCfg->continuousIEMapInsert == MoCA_CONTINUOUS_IE_MAP_INSERT_ON) ? "on" : "off" );
  printf ("maxPktAggr                  : %u pkts\n", pCfg->maxPktAggr);
  printf ("minAggrWaitTime             : %u us\n", pCfg->minAggrWaitTime);
  printf ("maxConstellationInfo (0-7)  : %2u  %2u  %2u  %2u  %2u  %2u  %2u  %2u\n",
     pCfg->constellation[0], pCfg->constellation[1], pCfg->constellation[2], pCfg->constellation[3],
     pCfg->constellation[4], pCfg->constellation[5], pCfg->constellation[6], pCfg->constellation[7] );
  printf ("                    (8-15)  : %2u  %2u  %2u  %2u  %2u  %2u  %2u  %2u\n",
     pCfg->constellation[8], pCfg->constellation[9], pCfg->constellation[10], pCfg->constellation[11],
     pCfg->constellation[12], pCfg->constellation[13], pCfg->constellation[14], pCfg->constellation[15] );
  printf ("pmkExchangeInterval         : %u hrs \n", pCfg->pmkExchangeInterval);
  printf ("tekExchangeInterval         : %u min \n", pCfg->tekExchangeInterval);
  printf ("HighPrioAlloc (Resv:Limit)  : %u:%u \n", pCfg->prioAllocation.resvHigh,
                 pCfg->prioAllocation.limitHigh);
  printf ("medPrioAlloc (Resv:Limit)   : %u:%u \n", pCfg->prioAllocation.resvMed,
                 pCfg->prioAllocation.limitMed);
  printf ("lowPrioAlloc (Resv:Limit)   : %u:%u \n", pCfg->prioAllocation.resvLow,
                 pCfg->prioAllocation.limitLow);
  printf ("snrMargin                   : %1.1fdB \n", (pCfg->snrMargin * 0.5));
   
  printf ("minMapCycle                 : %u micro secs \n", pCfg->minMapCycle);
  printf ("maxMapCycle                 : %u micro secs \n", pCfg->maxMapCycle);
  printf ("rxTxPacketsPerQM            : %u \n", pCfg->rxTxPacketsPerQM);
  printf ("extraRxPacketsPerQM         : %u \n", pCfg->extraRxPacketsPerQM);
  printf ("targetPHYRateQAM128         : %u mbps \n", pCfg->targetPhyRateQAM128);
  printf ("targetPHYRateQAM256         : %u mbps \n", pCfg->targetPhyRateQAM256);
  printf ("targetPHYRateTurbo          : %u mbps \n", pCfg->targetPhyRateTurbo);
  printf ("targetPHYRateTurboPlus      : %u mbps \n", pCfg->targetPhyRateTurboPlus);
  printf ("nbasCappingEn               : %d \n", pCfg->nbasCappingEn);
  printf ("selectiveRR                 : %d \n", pCfg->selectiveRR);
  printf ("Frequency shift mode        : %s \n", pCfg->freqShiftMode == MoCA_FREQ_SHIFT_MODE_MINUS? "minus":
                                                pCfg->freqShiftMode == MoCA_FREQ_SHIFT_MODE_PLUS? "plus":"off");
  printf ("snrMarginOffset             : %1.1f %1.1f %1.1f %1.1f %1.1f %1.1f %1.1f %1.1f %1.1f %1.1f \n",
                                         pCfg->snrMarginOffset[0]*0.5, pCfg->snrMarginOffset[1]*0.5,
                                         pCfg->snrMarginOffset[2]*0.5, pCfg->snrMarginOffset[3]*0.5,
                                         pCfg->snrMarginOffset[4]*0.5, pCfg->snrMarginOffset[5]*0.5,
                                         pCfg->snrMarginOffset[6]*0.5, pCfg->snrMarginOffset[7]*0.5,
                                         pCfg->snrMarginOffset[8]*0.5, pCfg->snrMarginOffset[9]*0.5);

  printf ("SapmTableEn                 : %d\n", pCfg->sapmEn);

  if (((rftype != MoCA_RF_TYPE_D_BAND) && (rftype != MoCA_RF_TYPE_C4_BAND)) ||
      (pCfg->sapmEn))
  {
      printf ("ArplTh                      : %d\n", pCfg->arplTh);
      printf ("SapmTable:");
      for (i=0;i<MoCA_MAX_SAPM_TBL_INDEX * 2;i++)
      {
         if (i%8 == 0)
             printf("\n");

         if (i < MoCA_MAX_SAPM_TBL_INDEX)
             printf ("[%3d=%5.01f] ", i+4, pCfg->sapmTable.sapmTableLo[i]/2.0);
         else
             printf ("[%3d=%5.01f] ", i+141-MoCA_MAX_SAPM_TBL_INDEX,
                     pCfg->sapmTable.sapmTableHi[i-MoCA_MAX_SAPM_TBL_INDEX]/2.0);
      }

      printf("\n");
  }

  printf ("RlapmTableEn                : %s\n", pCfg->rlapmEn==0?"off":pCfg->rlapmEn==1?"on":"phy");

  if (((rftype != MoCA_RF_TYPE_D_BAND) && (rftype != MoCA_RF_TYPE_C4_BAND)) ||
      (pCfg->rlapmEn)) {
    printf ("RlapmTable:");

    if (pCfg->rlapmEn == 1) {
      for (i=0;i<MoCA_MAX_RLAPM_TBL_INDEX;i++) {
        if (i%8 == 0)
          printf("\n");
             
        printf ("[%3d=%4.01f] ", i, pCfg->rlapmTable[i]/2.0);
      }
    }
    else {
      for (i=0;i<MoCA_MAX_RLAPM_TBL_INDEX;i++) {
        if (i%8 == 0)
          printf("\n");
             
          printf ("[%3d=%3d] ", i, (unsigned int)pCfg->rlapmTable[i]+45);
      }
    }

    printf("\n");
    printf ("RlapmCap                    : %1.1fdB \n", (pCfg->rlapmCap * 0.5));
  }

  for (i=0;i<MOCA_MAX_EGR_MC_FILTERS;i++) {
    if (pCfg->mcAddrFilter[i].Valid) {
      MAC_ADDRESS macAddr;

      moca_u32_to_mac(macAddr, pCfg->mcAddrFilter[i].AddrHi, pCfg->mcAddrFilter[i].AddrLo);

      printf ("EgrMcAddrFilter EntryId %02d  : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
          pCfg->mcAddrFilter[i].EntryId, macAddr[0], macAddr[1], macAddr[2],
          macAddr[3], macAddr[4], macAddr[5]);
    }
  }
  printf ("Diplexer                    : %d\n", pCfg->diplexer);
  printf ("Rx Power Tuning             : %d (%d /w diplexer)\n", pCfg->rxPowerTuning, pCfg->rxPowerTuning+pCfg->diplexer);
  
  printf ("EN Capable                  : %d\n", pCfg->enCapable);
  printf ("EN Max Rate in Max BO       : %d\n", pCfg->enMaxRateInMaxBo);
  printf ("loopbackEn                  : %d\n", pCfg->loopbackEn);

  if ( showAbsSnrTable ) {
    float  absSnr;
    UINT32 i;
     
    printf ("snrMarginOffsetAbs          : ");

    for ( i = 0; i < MoCA_MAX_SNR_TBL_INDEX; i++ ) {
       absSnr = (float) pCfg->snrMarginTable.mgnTable[ i ] / 2.0;
       printf ("%2.1f ", absSnr );
    }
    printf ("\n");
  }
  printf ("==================================  \n");
}


/** Print moca bit loading data
 * (BRCM print message routine)
 *
 * Prints MoCA bit loading data stored in pBitLoading and pSecBitLoading
 *
 * @param pBitLoading (IN) pointer to first bit loading data array
 * @param pSecBitLoading (IN) pointer to second bit loading data array
 * @return None.
 */
void diagTest_Print_bitLoading (UINT32 *pBitLoading, UINT32 *pSecBitLoading) {
  UINT32  val, secVal;
  UINT32  subCarrier, secSubCarrier;
  
  secSubCarrier = 0;
  for (subCarrier = 0; subCarrier < (MoCA_MAX_SUB_CARRIERS/8); subCarrier++) {
    val = pBitLoading [subCarrier];
    val = (val<<28) | ((val&0xf0)<<20) | ((val&0xf00)<<12) | 
          ((val&0xf000)<<4) | ((val&0xf0000)>>4) | ((val&0xf00000)>>12) |
          ((val&0xf000000)>>20) | val>>28;
    printf ("%8.8x", val);
    if (((subCarrier+1) % 4) == 0) {
      if (pSecBitLoading == NULL)
        printf ("\n");
      else {
        printf ("\t   ");
        /* Display the second Bit Loading */
        for (;secSubCarrier < (MoCA_MAX_SUB_CARRIERS/8); secSubCarrier++) {
          secVal = pSecBitLoading [secSubCarrier];
          secVal = (secVal<<28) | ((secVal&0xf0)<<20) | ((secVal&0xf00)<<12) | ((secVal&0xf000)<<4)
            | ((secVal&0xf0000)>>4) | ((secVal&0xf00000)>>12) | ((secVal&0xf000000)>>20) | secVal >>28;
          printf ("%8.8x", secVal);
          if (((secSubCarrier+1) % 4) == 0) {
            printf ("\n");
            secSubCarrier++;
            break;
          }
        }  /* for (secSubCarrier) */
      }
    }  /* if (subCarrier) */
    
  }  /* for (subCarrier) */
   
}  /* end of diagTest_Print_bitLoading */



/** Print moca node status
 * (BRCM print message routine)
 *
 * Prints MoCA node status data stored in pNodeStatus and bitLoading
 *
 * @param pNodeStatus (IN) pointer to node status data
 * @param bitLoading (IN) flag indicating whether to print bit loading data 
 * or not
 * @return None.
 */
void diagTest_Print_nodeStatus (PMoCA_NODE_STATUS_ENTRY pNodeStatus,
                                int bitLoading, int header) {
  MAC_ADDRESS macAddr;
  
  moca_u32_to_mac(macAddr, pNodeStatus->eui[0], pNodeStatus->eui[1]);

  printf ("Node                             : %d \n", pNodeStatus->nodeId);
  printf ("=============================================\n");
  printf ("MAC Address                      : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
       macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  printf ("Freq Offset                      : %d KHz \n", pNodeStatus->freqOffset/1000);
  printf ("Protocol Support                 : 0x%X\n", pNodeStatus->protocolSupport);
  printf ("   - Preferred NC                : %d\n", (pNodeStatus->protocolSupport>>6)&1);
  printf ("   - 256 QAM capable             : %d\n", (pNodeStatus->protocolSupport>>4)&1);
  printf ("   - Aggregated PDUs             : %s\n", ((pNodeStatus->protocolSupport>>7)&3) == 0?"6":
                                                    ((pNodeStatus->protocolSupport>>7)&3) == 2?"10":"Not allowed");
  printf ("Other Node UC Pwr Backoff        : %d dB \n", pNodeStatus->otherNodeUcPwrBackOff);
  printf ("Turbo Mode                       : %d\n", pNodeStatus->txUc.turbo);
  printf ("-------------------------------------------------------------------------\n");
  printf ("        Nbas  Preamble    CP    TxPower   RxPower   Rate              SNR\n");
  printf ("=========================================================================\n");
  printf ("TxUc    %4d     %2d      %3d    %3d dBm   N/A       %9u bps   %2.1lf dB\n",
          pNodeStatus->txUc.nBas,
          pNodeStatus->txUc.preambleType, pNodeStatus->txUc.cp,
          pNodeStatus->txUc.txPower, pNodeStatus->maxPhyRates.txUcPhyRate,
          (float)pNodeStatus->txUc.avgSnr/2.0 );
  printf ("RxUc    %4d     %2d      %3d    N/A      %6.2lf dBm %9u bps   %2.1lf dB\n",
          pNodeStatus->rxUc.nBas,
          pNodeStatus->rxUc.preambleType, pNodeStatus->rxUc.cp,
          pNodeStatus->rxUc.rxGain/4.0, pNodeStatus->maxPhyRates.rxUcPhyRate,
          pNodeStatus->rxUc.avgSnr/2.0 );
  printf ("RxBc    %4d     %2d      %3d    N/A      %6.2lf dBm %9u bps   %2.1lf dB\n",
          pNodeStatus->rxBc.nBas,
          pNodeStatus->rxBc.preambleType, pNodeStatus->rxBc.cp,
          pNodeStatus->rxBc.rxGain/4.0, pNodeStatus->maxPhyRates.rxBcPhyRate,
          pNodeStatus->rxBc.avgSnr/2.0 );
  printf ("RxMap   %4d     %2d      %3d    N/A      %6.2lf dBm %9u bps   %2.1lf dB\n",
          pNodeStatus->rxMap.nBas,
          pNodeStatus->rxMap.preambleType, pNodeStatus->rxMap.cp,
          pNodeStatus->rxMap.rxGain/4.0,  pNodeStatus->maxPhyRates.rxMapPhyRate,
          pNodeStatus->rxMap.avgSnr/2.0 );
  printf ("===========================================================\n");
  printf ("\n");
  
  if (bitLoading) {
    printf ("   Tx Unicast Bit Loading Info  \t   Rx Unicast Bit Loading Info \n" );
    printf ("--------------------------------\t   -------------------------------\n");
    diagTest_Print_bitLoading(
        &pNodeStatus->txUc.bitLoading[0], 
        &pNodeStatus->rxUc.bitLoading[0]);
    printf ("--------------------------------\t   -------------------------------\n");
  
    printf ("   Rx Broadcast Bit Loading Info  \t   Rx Map Bit Loading Info \n" );
    printf ("----------------------------------\t   -----------------------------\n");
    diagTest_Print_bitLoading(
      &pNodeStatus->rxBc.bitLoading[0], 
      &pNodeStatus->rxMap.bitLoading[0]);
    printf ("--------------------------------\t   -------------------------------\n");
  }

}  /* end of diagTest_Print_nodeStatus */


/** Print moca node status table of all connected nodes
 * (Brcm print message routine)
 *
 * Prints MoCA in pNodeStatusTbl
 *
 * @param pNodeStatusTbl (IN) pointer to node status data
 * @return None.
 */
void diagTest_Print_nodeStatusTbl(diag_moca_nodestatus_t *pNodeStatusTbl) {
  uint16_t  tblSize, nodes;
  MoCA_NODE_COMMON_STATUS_ENTRY  *pNodeCommonStatus;


  printf("%s: nodeStatusTblSize=%u\n", 
         __func__, pNodeStatusTbl->nodeStatusTblSize);
  
  /* No of Nodes */
  tblSize = pNodeStatusTbl->nodeStatusTblSize/sizeof(MoCA_NODE_STATUS_ENTRY);
  
  printf ("           MoCA Node Status Table            \n");
  printf ("=============================================\n");
  
  for (nodes = 0; nodes < tblSize; nodes++) {
    diagTest_Print_nodeStatus(&pNodeStatusTbl->nodeStatus[nodes], 1, !nodes);
  } /* for (nodes) */

  pNodeCommonStatus = &pNodeStatusTbl->nodeCommonStatus;
  printf ("All Node Information \n");
  printf ("=====================\n");
  printf ("\tNbas  Preamble     CP    TxPower   RxPower  Rate \n");
  printf ("===========================================================\n");
  printf ("TxBc\t%4d      %d        %d    %3d dBm    N/A     %u bps \n",
          pNodeCommonStatus->txBc.nBas,
          pNodeCommonStatus->txBc.preambleType, 
          pNodeCommonStatus->txBc.cp,
          pNodeCommonStatus->txBc.txPower, 
          pNodeCommonStatus->maxCommonPhyRates.txBcPhyRate);
  printf ("TxMap\t%4d      %d        %d    %3d dBm    N/A     %u bps \n",
          pNodeCommonStatus->txMap.nBas,
          pNodeCommonStatus->txMap.preambleType, 
          pNodeCommonStatus->txMap.cp,
          pNodeCommonStatus->txMap.txPower, 
          pNodeCommonStatus->maxCommonPhyRates.txMapPhyRate);
  printf ("===========================================================\n");
  printf ("   Tx Bcast Bit Loading Info    \t      Tx Map Bit Loading Info  \n" );
  printf ("--------------------------------\t    ---------------------------\n");
  diagTest_Print_bitLoading(
      &pNodeCommonStatus->txBc.bitLoading[0], 
      &pNodeCommonStatus->txMap.bitLoading[0]);
  printf ("--------------------------------\t   -------------------------------\n");

}  /* end of diagTest_Print_nodeStatusTbl */


/** Print moca node stats
 * (Brcm print message routine)
 *
 * Prints MoCA node stats data stored in pNodeStats
 *
 * @param pNodeStats (IN) pointer to node stats data
 * @return None.
 */
void diagTest_Print_nodeStats(
    diag_moca_node_stats_entry_t *pNodeStatsEntry) {
  MoCA_NODE_STATISTICS_ENTRY *pNodeStats = &pNodeStatsEntry->nodeStats;

  printf ("Node                             : %d \n", pNodeStatsEntry->nodeId);   
  printf ("MAC Address                      : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
          pNodeStatsEntry->macAddr[0], pNodeStatsEntry->macAddr[1], 
          pNodeStatsEntry->macAddr[2], pNodeStatsEntry->macAddr[3], 
          pNodeStatsEntry->macAddr[4], pNodeStatsEntry->macAddr[5]);   
  printf ("=============================================\n");
  printf ("Unicast Tx Pkts To Node          : %d \n", pNodeStats->txPkts);
  printf ("Unicast Rx Pkts From Node        : %d \n", pNodeStats->rxPkts);
  printf ("Rx CodeWord NoError              : %d \n", pNodeStats->rxCwUnError);
  printf ("Rx CodeWord ErrorAndCorrected    : %d \n", pNodeStats->rxCwCorrected);
  printf ("Rx CodeWord ErrorAndUnCorrected  : %d \n", pNodeStats->rxCwUncorrected);
  printf ("Rx NoSync Errors                 : %d \n", pNodeStats->rxNoSync);
  printf ("Rx No Energy Errors              : %d \n", pNodeStats->rxNoEnergy);
  printf ("=============================================\n");
  printf ("\n");
}  /* end of diagTest_Print_nodeStats */


/** Print extended moca node stats
 * (Brcm print message routine)
 *
 * Prints extended MoCA node stats data stored in pNodeStats
 *
 * @param pNodeStatsEntry (IN) pointer to extended node stats data
 * @return None.
 */
void diagTest_Print_nodeStatsExt(
    diag_moca_node_stats_entry_t *pNodeStatsEntry) {
  MoCA_NODE_STATISTICS_EXT_ENTRY *pNodeStats = &pNodeStatsEntry->nodeStatsExt;
  
  printf ("Node                             : %d \n", pNodeStats->nodeId);
  printf ("=============================================\n");
  
  printf ("NODE_RX_UC_CRC_ERROR                  : %d \n", pNodeStats->rxUcCrcError);
  printf ("NODE_RX_UC_TIMEOUT_ERROR              : %d \n", pNodeStats->rxUcTimeoutError);  
  printf ("NODE_RX_BC_CRC_ERROR                  : %d \n", pNodeStats->rxBcCrcError);
  printf ("NODE_RX_BC_TIMEOUT_ERROR              : %d \n", pNodeStats->rxBcTimeoutError);
  
  printf ("NODE_RX_MAP_CRC_ERROR                 : %d \n", pNodeStats->rxMapCrcError);
  printf ("NODE_RX_MAP_TIMEOUT_ERROR             : %d \n", pNodeStats->rxMapTimeoutError);
  printf ("NODE_RX_BEACON_CRC_ERROR              : %d \n", pNodeStats->rxBeaconCrcError);
  printf ("NODE_RX_BEACON_TIMEOUT_ERROR          : %d \n", pNodeStats->rxBeaconTimeoutError);
  printf ("NODE_RX_RR_CRC_ERROR                  : %d \n", pNodeStats->rxRrCrcError);
  printf ("NODE_RX_RR_TIMEOUT_ERROR              : %d \n", pNodeStats->rxRrTimeoutError);
  
  printf ("NODE_RX_LC_CRC_ERROR                  : %d \n", pNodeStats->rxLcCrcError);
  printf ("NODE_RX_LC_TIMEOUT_ERROR              : %d \n", pNodeStats->rxLcTimeoutError);
  
  printf ("NODE_RX_P1_ERROR                      : %d \n", pNodeStats->rxP2Error);
  printf ("NODE_RX_P2_ERROR                      : %d \n", pNodeStats->rxP2Error);
  printf ("NODE_RX_P3_ERROR                      : %d \n", pNodeStats->rxP3Error);
  printf ("NODE_RX_P1_GCD_ERROR                  : %d \n", pNodeStats->rxP1GcdError);
  
  printf ("=============================================\n");
  printf ("\n");
}


/** Print moca node statistics table of all connected nodes
 * (Brcm print message routine)
 *
 * Prints MoCA in pNodeStatusTbl
 *
 * @param pNodeStatsTbl (IN) pointer to node status data
 * @return None.
 */
void diagTest_Print_nodeStatisticsTbl(
    diag_moca_node_stats_table_t *pNodeStatsTbl) {
  uint16_t  tblSize, i;
  diag_moca_node_stats_entry_t  *pStatsEntry;

  printf("%s: nodeStatsTblSize=%u\n", 
         __func__, pNodeStatsTbl->nodeStatsTblSize);
  
  /* No of Nodes */
  tblSize = pNodeStatsTbl->nodeStatsTblSize/sizeof(diag_moca_node_stats_entry_t);
  pStatsEntry = &pNodeStatsTbl->Stats;
  printf ("        MoCA Node Statistics Table           \n");
  printf ("=============================================\n");
  for (i = 0; i < tblSize; i++, pStatsEntry++) {
    diagTest_Print_nodeStats(pStatsEntry);
  }

  printf ("     MoCA Node Extended Statistics Table     \n");
  printf ("=============================================\n");
  pStatsEntry = &pNodeStatsTbl->Stats;
  for (i = 0; i < tblSize; i++, pStatsEntry++) {
    diagTest_Print_nodeStatsExt(pStatsEntry);
  }

}  /* end of diagTest_Print_nodeStatisticsTbl */

void diagTest_Print_KernMsgsReport(char *pPayload) {
  printf("%s", pPayload);
}

display_LoopbackTestMsg() {
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

    if (cmdIdx == DIAG_TRY_AGAIN) {
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
            MoCA_INITIALIZATION_PARMS  *pNodeInitParms;
  
            /* Point to the payload */
            pNodeInitParms = (MoCA_INITIALIZATION_PARMS *)pPayload;
            diagTest_Print_InitParms(pNodeInitParms);
          }       
          break;

        case DIAGD_REQ_MOCA_GET_STATUS:
          {
            MoCA_STATUS *pStatus;
            pStatus = (MoCA_STATUS *)pPayload;
            diagTest_Print_SelfNodeStatus(pStatus);
          }
          break;
  
        case DIAGD_REQ_MOCA_GET_CONFIG:
          {
            diag_moca_config_t  *pCfg;
            pCfg = (diag_moca_config_t *)pPayload;
            diagTest_Print_Config(&pCfg->Cfg, 0, pCfg->rfType);
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
