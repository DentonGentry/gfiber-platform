/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 * Author: weixiaofeng@google.com (Xiaofeng Wei)
 *
 * This file provides diagnostics MoCA 2.0 API functions
 */

#include "devctl_moca.h"

/*
 */
void *MoCACtl_Open(char *ifname);

CmsRet MoCACtl_Close(void *handle);

CmsRet MoCACtl2_GetInitParms(
  void *ctx,
  PMoCA_INITIALIZATION_PARMS pMoCAInitParms);

CmsRet MoCACtl2_GetCfg(
  void *ctx,
  PMoCA_CONFIG_PARAMS pConfig,
  unsigned long long mask);

CmsRet MoCACtl2_GetStatus(
  void *ctx,
  PMoCA_STATUS pStatus);

CmsRet MoCACtl2_GetNodeStatus(
  void *ctx,
  PMoCA_NODE_STATUS_ENTRY pNodeStatusEntry);

CmsRet MoCACtl2_GetStatistics(
  void *ctx,
  PMoCA_STATISTICS pMoCAStats,
  UINT32 ulReset);

CmsRet MoCACtl2_GetNodeStatistics(
  void *ctx,
  PMoCA_NODE_STATISTICS_ENTRY pNodeStatsEntry,
  UINT32 ulReset);

CmsRet MoCACtl2_GetNodeStatisticsExt(
  void *ctx,
  PMoCA_NODE_STATISTICS_EXT_ENTRY pNodeStatsEntry,
  UINT32 ulReset);

CmsRet MoCACtl2_GetNodeTblStatistics(
  void *ctx,
  PMoCA_NODE_STATISTICS_ENTRY pNodeStatsEntry,
  UINT32 *pulNodeStatsTblSize,
  UINT32 ulReset);

CmsRet MoCACtl2_GetNodeTblStatisticsExt(
  void *ctx,
  PMoCA_NODE_STATISTICS_EXT_ENTRY pNodeStatsEntry,
  UINT32 *pulNodeStatsTblSize,
  UINT32 ulReset);

CmsRet MoCACtl2_GetNodeTblStatus(
  void *ctx,
  PMoCA_NODE_STATUS_ENTRY pNodeStatusEntry,
  PMoCA_NODE_COMMON_STATUS_ENTRY pNodeCommonStatusEntry,
  UINT32 *pulNodeStatusTblSize);

CmsRet MoCACtl2_Fmr(
  void *ctx,
  PMoCA_FMR_PARAMS params);

