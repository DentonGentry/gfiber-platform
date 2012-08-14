/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 * Author: weixiaofeng@google.com (Xiaofeng Wei)
 *
 * This file provides diagnostics MoCA 2.0 API functions
 *
 */

/*--------------------------------------------------------------------------
 *
 * Includes
 *
 *--------------------------------------------------------------------------
 */

#include "diagdIncludes.h"

static sNodeMacAddr gBcastMacAddr = {{0xFFFFFFFF, 0xFFFFFFFF}};

/*
 * MoCACtl_Open
 * This function opens handle to MoCA driver
 */
void *MoCACtl_Open(char *ifname)
{
  return moca_open(ifname);
}

/*
 * MoCACtl_Close
 * This function closes handle of MoCA driver
 */
CmsRet MoCACtl_Close(void *handle)
{
  moca_close(handle);
  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetInitParms
 * This function is called to retrieve current initialization parameters
 */
CmsRet MoCACtl2_GetInitParms(
  void *ctx,
  PMoCA_INITIALIZATION_PARMS pMoCAInitParms)
{
  int i;
  struct moca_password pw;
  struct moca_node_status ns;
  struct moca_taboo_channels tc;
  struct moca_const_tx_params tp;

  moca_get_password(ctx, &pw);
  moca_get_node_status(ctx, &ns);
  moca_get_taboo_channels(ctx, &tc);

  memset(pMoCAInitParms, 0, sizeof(*pMoCAInitParms));

  moca_get_nc_mode(ctx, &pMoCAInitParms->ncMode);

  moca_get_privacy_en(ctx, &pMoCAInitParms->privacyEn);
  moca_get_tpc_en(ctx, &pMoCAInitParms->txPwrControlEn);
  moca_get_continuous_power_tx_mode(ctx, &pMoCAInitParms->constTransmitMode);
  moca_get_lof(ctx, &pMoCAInitParms->nvParams.lastOperFreq);
  moca_get_max_tx_power(ctx, &pMoCAInitParms->maxTxPowerBeacons);
  moca_get_bo_mode(ctx, &pMoCAInitParms->boMode);
  moca_get_rf_band(ctx, &pMoCAInitParms->rfType);
  moca_get_led_settings(ctx, &pMoCAInitParms->ledMode);
  moca_get_freq_mask(ctx, &pMoCAInitParms->freqMask);
  moca_get_pns_freq_mask(ctx, &pMoCAInitParms->pnsFreqMask);
  moca_get_otf_en(ctx, &pMoCAInitParms->otfEn);
  moca_get_flow_control_en(ctx, &pMoCAInitParms->flowControlEn);
  moca_get_mtm_en(ctx, &pMoCAInitParms->mtmEn);
  moca_get_qam1024_en(ctx, &pMoCAInitParms->qam1024En);
  moca_get_turbo_en(ctx, &pMoCAInitParms->turboEn);
  moca_get_multicast_mode(ctx, &pMoCAInitParms->mcastMode);
  moca_get_lab_mode(ctx, &pMoCAInitParms->labMode);

  pMoCAInitParms->tabooFixedMaskStart = tc.taboo_fixed_mask_start;
  pMoCAInitParms->tabooFixedChannelMask = tc.taboo_fixed_channel_mask;
  pMoCAInitParms->tabooLeftMask = tc.taboo_left_mask;
  pMoCAInitParms->tabooRightMask = tc.taboo_right_mask;

  moca_get_preferred_nc(ctx, &pMoCAInitParms->preferedNC);
  moca_get_beacon_pwr_reduction_en(ctx, &pMoCAInitParms->beaconPwrReductionEn);
  moca_get_beacon_pwr_reduction(ctx, &pMoCAInitParms->beaconPwrReduction);

  __moca_get_low_pri_q_num(ctx, &pMoCAInitParms->lowPriQNum);

  moca_get_beacon_channel(ctx, &pMoCAInitParms->beaconChannel);
  moca_get_qam256_capability(ctx, &pMoCAInitParms->qam256Capability);
  moca_get_continuous_rx_mode_attn(ctx, &pMoCAInitParms->continuousRxModeAttn);
  moca_get_egr_mc_filter_en(ctx, &pMoCAInitParms->egrMcFilterEn);

  pMoCAInitParms->operatingVersion = ns.self_moca_version;
  strcpy((char *)pMoCAInitParms->password, (const char *)pw.password);
  pMoCAInitParms->passwordSize = (UINT32)strlen((const char *)pw.password);

  __moca_get_const_tx_params(ctx, &tp);
  pMoCAInitParms->initOptions.constTxSubCarrier1 = tp.const_tx_sc1;
  pMoCAInitParms->initOptions.constTxSubCarrier2 = tp.const_tx_sc2;
  for (i = 0; i < MoCA_CONTINUOUS_TX_BAND_ARRAY_SIZE; i++)
    pMoCAInitParms->initOptions.constTxNoiseBand[i] = tp.const_tx_band[i];

  moca_get_dont_start_moca(ctx, &pMoCAInitParms->initOptions.dontStartMoca);

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetCfg
 * This function retrieves current configuration parameters
 */
CmsRet MoCACtl2_GetCfg(
  void *ctx,
  PMoCA_CONFIG_PARAMS pConfig,
  unsigned long long mask)
{
  int i, ret;
  uint32_t moca_reg;
  struct moca_snr_margin_rs rs;
  struct moca_snr_margin_ldpc ldpc;
  struct moca_snr_margin_ldpc_pre5 pre5;
  struct moca_snr_margin_ofdma ofdma;

  moca_reg = pConfig->RegMem.input;
  memset(pConfig, 0, sizeof(*pConfig));
  pConfig->RegMem.input = moca_reg;

  moca_get_max_frame_size(ctx, &pConfig->maxFrameSize);
  moca_get_max_transmit_time(ctx, &pConfig->maxTransmitTime);
  moca_get_min_bw_alarm_threshold(ctx, &pConfig->minBwAlarmThreshold);
  moca_get_continuous_ie_rr_insert(ctx, &pConfig->continuousIERRInsert);
  moca_get_continuous_ie_map_insert(ctx, &pConfig->continuousIEMapInsert);
  moca_get_max_pkt_aggr(ctx, &pConfig->maxPktAggr);

  for (i = 0; i < MOCA_MAX_NODES; i++) {
    ret = moca_get_max_constellation(ctx, i, &pConfig->constellation[i]);
    if (ret != MOCA_API_SUCCESS)
      return(CMSRET_INTERNAL_ERROR);
  }

  moca_get_freq_shift(ctx, &pConfig->freqShiftMode);
  moca_get_pmk_exchange_interval(ctx, &pConfig->pmkExchangeInterval);
  pConfig->pmkExchangeInterval /= 3600 * 1000;

  moca_get_tek_exchange_interval(ctx, &pConfig->tekExchangeInterval);
  pConfig->tekExchangeInterval /= 60 * 1000;

  __moca_get_priority_allocations(
    ctx,
    (struct moca_priority_allocations *)&pConfig->prioAllocation);

  moca_get_arpl_th_50(ctx, &pConfig->arplTh50);
  moca_get_arpl_th_100(ctx, &pConfig->arplTh100);

  moca_get_sapm_en(ctx, &pConfig->sapmEn);
  moca_get_sapm_table_50(ctx, (struct moca_sapm_table_50 *)&pConfig->sapmTable50);
  moca_get_sapm_table_100(ctx, (struct moca_sapm_table_100 *)&pConfig->sapmTable100);

  moca_get_rlapm_en(ctx, &pConfig->rlapmEn);
  moca_get_rlapm_table_50(ctx, (struct moca_rlapm_table_50 *)pConfig->rlapmTable50);
  moca_get_rlapm_table_100(ctx, (struct moca_rlapm_table_50 *)pConfig->rlapmTable100);

  moca_set_rlapm_cap_50(ctx, &pConfig->rlapmCap50);
  moca_get_rlapm_cap_100(ctx, &pConfig->rlapmCap100);

  for (i = 0; i < MOCA_MAX_EGR_MC_FILTERS; i++) {
    ret = moca_get_egr_mc_addr_filter(
            ctx,
            i,
            (struct moca_egr_mc_addr_filter_get *)&pConfig->mcAddrFilter[i]);
    if (ret != MOCA_API_SUCCESS)
      return(CMSRET_INTERNAL_ERROR);
  }

  moca_get_rx_power_tuning(ctx, &pConfig->rxPowerTuning);
  moca_get_en_capable(ctx, &pConfig->enCapable);
  moca_get_min_map_cycle(ctx, &pConfig->minMapCycle);
  moca_get_max_map_cycle(ctx, &pConfig->maxMapCycle);
  moca_get_extra_rx_packets_per_qm(ctx, &pConfig->extraRxPacketsPerQM);

  moca_get_rx_tx_packets_per_qm(ctx, &pConfig->rxTxPacketsPerQM);
  moca_get_target_phy_rate_20(ctx, &pConfig->targetPhyRate20);
  moca_get_target_phy_rate_20_turbo(ctx, &pConfig->targetPhyRate20Turbo);
  moca_get_target_phy_rate_qam128(ctx, &pConfig->targetPhyRateQAM128);
  moca_get_target_phy_rate_qam256(ctx, &pConfig->targetPhyRateQAM256);
  moca_get_nbas_capping_en(ctx, &pConfig->nbasCappingEn);
  moca_get_loopback_en(ctx, &pConfig->loopbackEn);
  moca_get_selective_rr(ctx, &pConfig->selectiveRR);
  moca_get_pss_en(ctx, &pConfig->pssEn);
  moca_get_min_aggr_waiting_time(ctx, &pConfig->minAggrWaitTime);
  moca_get_diplexer(ctx, &pConfig->diplexer);
  moca_get_en_max_rate_in_max_bo(ctx, &pConfig->enMaxRateInMaxBo);
  moca_get_lab_register(ctx, pConfig->RegMem.input, pConfig->RegMem.value);

  moca_get_snr_margin_rs(ctx, &rs);
  moca_get_snr_margin_ldpc(ctx, &ldpc);
  moca_get_snr_margin_ldpc_pre5(ctx, &pre5);
  moca_get_snr_margin_ofdma(ctx, &ofdma);

  pConfig->snrMarginRs = rs.base_margin;
  pConfig->snrMarginLdpc = ldpc.base_margin;
  pConfig->snrMarginLdpcPre5 = pre5.base_margin;
  pConfig->snrMarginOfdma = ofdma.base_margin;
  for (i = 0; i < MoCA_MAX_SNR_TBL_INDEX; i++) {
    pConfig->snrMarginRsOffset[i] = rs.offsets[i];
    pConfig->snrMarginLdpcOffset[i] = ldpc.offsets[i];
    pConfig->snrMarginLdpcPre5Offset[i] = pre5.offsets[i];
    pConfig->snrMarginOfdmaOffset[i] = ofdma.offsets[i];
  }

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetStatus
 * This function retrieves status information of MoCA interface
 */
CmsRet MoCACtl2_GetStatus(
  void *ctx,
  PMoCA_STATUS pStatus)
{
  struct moca_interface_status is;
  struct moca_network_status net;
  struct moca_node_status ns;
  struct moca_drv_info info;
  struct moca_current_keys key;
  struct moca_key_times key_times;
  struct moca_mac_addr mac;

  memset(pStatus, 0, sizeof(*pStatus));

  moca_get_interface_status(ctx, &is);
  moca_get_network_status(ctx, &net);
  moca_get_node_status(ctx, &ns);
  moca_get_drv_info(ctx, &info);

  pStatus->generalStatus.vendorId = ns.vendor_id;
  pStatus->generalStatus.vendorId = ns.vendor_id;
  pStatus->generalStatus.swVersion = ns.moca_sw_version_rev;
  pStatus->generalStatus.selfMoCAVersion = ns.self_moca_version;
  pStatus->generalStatus.qam256Support = ns.qam_256_support;

  pStatus->generalStatus.networkVersionNumber = net.network_moca_version;
  pStatus->generalStatus.connectedNodes = net.connected_nodes;
  pStatus->generalStatus.nodeId = net.node_id;
  pStatus->generalStatus.ncNodeId = net.nc_node_id;
  pStatus->generalStatus.backupNcId = net.backup_nc_id;

  pStatus->generalStatus.linkStatus = is.link_status;
  pStatus->generalStatus.rfChannel = is.rf_channel * 25;

  pStatus->generalStatus.bwStatus = net.bw_status;
  pStatus->generalStatus.nodesUsableBitmask = net.nodes_usable_bitmask;
  pStatus->generalStatus.networkTabooMask = net.network_taboo_mask;
  pStatus->generalStatus.networkTabooStart = net.network_taboo_start;

  moca_get_single_channel_operation(ctx, &pStatus->generalStatus.operStatus);
  moca_get_phy_status(ctx, &pStatus->generalStatus.txGcdPowerReduction);
  moca_get_led_status(ctx, &pStatus->generalStatus.ledStatus);
  moca_get_pqos_egress_numflows(ctx, &pStatus->generalStatus.pqosEgressNumFlows);

  moca_get_mac_addr(ctx, &mac);
  memcpy((uint8_t *)&pStatus->miscStatus.macAddr, mac.val.addr, MAC_ADDR_LEN);

  pStatus->miscStatus.isNC = (pStatus->generalStatus.nodeId == pStatus->generalStatus.ncNodeId) ? 1 : 0;
  pStatus->miscStatus.driverUpTime = info.uptime;

  pStatus->generalStatus.hwVersion = info.chip_id;
  pStatus->miscStatus.MoCAUpTime = info.core_uptime;
  pStatus->miscStatus.linkUpTime = info.link_uptime;
  pStatus->generalStatus.MoCARev = info.hw_rev;

  moca_get_current_keys(ctx, &key);
  moca_get_key_times(ctx, &key_times);

  moca_u32_to_mac(pStatus->extendedStatus.pmkEvenKey, key.pmk_even_key_hi, key.pmk_even_key_lo);
  moca_u32_to_mac(pStatus->extendedStatus.pmkOddKey, key.pmk_odd_key_hi, key.pmk_odd_key_lo);
  moca_u32_to_mac(pStatus->extendedStatus.tekEvenKey, key.tek_even_key_hi, key.tek_even_key_lo);
  moca_u32_to_mac(pStatus->extendedStatus.tekOddKey, key.tek_odd_key_hi, key.tek_odd_key_lo);

  pStatus->extendedStatus.lastTekExchange = key_times.tek_time;
  pStatus->extendedStatus.lastTekInterval = key_times.tek_last_interval;
  pStatus->extendedStatus.tekEvenOdd = key_times.tek_even_odd;
  pStatus->extendedStatus.lastPmkExchange = key_times.pmk_time;
  pStatus->extendedStatus.lastPmkInterval = key_times.pmk_last_interval;
  pStatus->extendedStatus.pmkEvenOdd = key_times.pmk_even_odd;

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetNodeStatus
 * This function retrieves current node status for a particular node
 */
CmsRet MoCACtl2_GetNodeStatus(
  void * ctx,
  PMoCA_NODE_STATUS_ENTRY pNodeStatusEntry)
{
  struct moca_gen_node_status gs;
  struct moca_gen_node_ext_status_in gns;
  int ret;

  memset(&pNodeStatusEntry->eui, 0, sizeof(pNodeStatusEntry->eui));
  memset(&pNodeStatusEntry->txUc, 0, sizeof(pNodeStatusEntry->txUc));
  memset(&pNodeStatusEntry->rxUc, 0, sizeof(pNodeStatusEntry->rxUc));
  memset(&pNodeStatusEntry->rxBc, 0, sizeof(pNodeStatusEntry->rxBc));
  memset(&pNodeStatusEntry->rxMap, 0, sizeof(pNodeStatusEntry->rxMap));

  gns.index = pNodeStatusEntry->nodeId;

  moca_get_gen_node_status(ctx, gns.index, &gs);
  moca_mac_to_u32(
    &(pNodeStatusEntry->eui[0]),
    &(pNodeStatusEntry->eui[1]),
    gs.eui.addr);

  gns.profile_type = MOCA_EXT_STATUS_PROFILE_TX_UC_NPER;
  ret = moca_get_gen_node_ext_status(
    ctx,
    &gns,
    (struct moca_gen_node_ext_status *)&pNodeStatusEntry->txUc);
  if (ret != 0) {
    gns.profile_type = MOCA_EXT_STATUS_PROFILE_TX_UCAST;
    moca_get_gen_node_ext_status(
      ctx,
      &gns,
      (struct moca_gen_node_ext_status *)&pNodeStatusEntry->txUc);
  }

  gns.profile_type = MOCA_EXT_STATUS_PROFILE_RX_UC_NPER;
  ret = moca_get_gen_node_ext_status(
    ctx,
    &gns,
    (struct moca_gen_node_ext_status *)&pNodeStatusEntry->rxUc);
  if (ret != 0) {
    gns.profile_type = MOCA_EXT_STATUS_PROFILE_RX_UCAST;
    moca_get_gen_node_ext_status(
      ctx,
      &gns,
      (struct moca_gen_node_ext_status *)&pNodeStatusEntry->rxUc);
  }

  gns.profile_type = MOCA_EXT_STATUS_PROFILE_RX_BC_NPER;
  ret = moca_get_gen_node_ext_status(
    ctx,
    &gns,
    (struct moca_gen_node_ext_status *)&pNodeStatusEntry->rxBc);
  if (ret != 0) {
    gns.profile_type = MOCA_EXT_STATUS_PROFILE_RX_BCAST;
    moca_get_gen_node_ext_status(
      ctx,
      &gns,
      (struct moca_gen_node_ext_status *)&pNodeStatusEntry->rxBc);
  }

  gns.profile_type = MOCA_EXT_STATUS_PROFILE_RX_MAP_20;
  ret = moca_get_gen_node_ext_status(
    ctx,
    &gns,
    (struct moca_gen_node_ext_status *)&pNodeStatusEntry->rxMap);
  if (ret != 0) {
    gns.profile_type = MOCA_EXT_STATUS_PROFILE_RX_MAP;
    moca_get_gen_node_ext_status(
      ctx,
      &gns,
      (struct moca_gen_node_ext_status *)&pNodeStatusEntry->rxMap);
  }

  pNodeStatusEntry->maxPhyRates.txUcPhyRate =
    moca_phy_rate(
      pNodeStatusEntry->txUc.nBas,
      pNodeStatusEntry->txUc.cp,
      pNodeStatusEntry->txUc.turbo,
      MoCA_VERSION_2_0);

  pNodeStatusEntry->maxPhyRates.rxUcPhyRate =
    moca_phy_rate(
      pNodeStatusEntry->rxUc.nBas,
      pNodeStatusEntry->rxUc.cp,
      pNodeStatusEntry->rxUc.turbo,
      MoCA_VERSION_2_0);

  pNodeStatusEntry->maxPhyRates.rxBcPhyRate =
    moca_phy_rate(
      pNodeStatusEntry->rxBc.nBas,
      pNodeStatusEntry->rxBc.cp,
      0,
      MoCA_VERSION_2_0);

  pNodeStatusEntry->maxPhyRates.rxMapPhyRate =
    moca_phy_rate(
      pNodeStatusEntry->rxMap.nBas,
      pNodeStatusEntry->rxMap.cp,
      0,
      MoCA_VERSION_2_0);

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetStatistics
 * This function retrieves current statistic information of MoCA interface
 */
CmsRet MoCACtl2_GetStatistics(
  void *ctx,
  PMoCA_STATISTICS pMoCAStats,
  UINT32 ulReset)
{
  int i;
  struct moca_gen_stats gs;
  struct moca_ext_octet_count eo;

  memset(pMoCAStats, 0, sizeof(*pMoCAStats));

  moca_get_gen_stats(ctx, &gs);

  pMoCAStats->generalStats.inTotalPkts = gs.ecl_tx_total_pkts;
  pMoCAStats->generalStats.inTotalBytes = gs.ecl_tx_total_bytes;
  pMoCAStats->generalStats.inUcPkts = gs.ecl_tx_ucast_pkts;
  pMoCAStats->generalStats.inBcPkts = gs.ecl_tx_bcast_pkts;
  pMoCAStats->generalStats.inMcPkts = gs.ecl_tx_mcast_pkts;
  pMoCAStats->generalStats.inUcUnKnownPkts = gs.ecl_tx_ucast_unknown;
  pMoCAStats->generalStats.inMcUnKnownPkts = gs.ecl_tx_mcast_unknown;
  pMoCAStats->generalStats.inUcDiscardPkts = gs.ecl_tx_ucast_drops;
  pMoCAStats->generalStats.inMcDiscardPkts = gs.ecl_tx_mcast_drops;
  pMoCAStats->generalStats.inDiscardBufPkts = gs.ecl_tx_buff_drop_pkts;

  pMoCAStats->generalStats.outTotalPkts = gs.ecl_rx_total_pkts;
  pMoCAStats->generalStats.outTotalBytes = gs.ecl_rx_total_bytes;
  pMoCAStats->generalStats.outUcPkts = gs.ecl_rx_ucast_pkts;
  pMoCAStats->generalStats.outBcPkts = gs.ecl_rx_bcast_pkts;
  pMoCAStats->generalStats.outMcPkts = gs.ecl_rx_mcast_pkts;
  pMoCAStats->generalStats.outUcUnKnownPkts = gs.ecl_rx_ucast_unknown;
  pMoCAStats->generalStats.outMcUnKnownPkts = gs.ecl_rx_mcast_unknown;
  pMoCAStats->generalStats.outUcDiscardPkts = gs.ecl_rx_ucast_drops;
  pMoCAStats->generalStats.outMcDiscardPkts = gs.ecl_rx_mcast_drops;
  pMoCAStats->generalStats.outDiscardBufPkts = gs.mac_rx_buff_drop_pkts;

  moca_get_ext_octet_count(ctx, &eo);

  pMoCAStats->BitStats64.inOctets_hi = eo.in_octets_hi;
  pMoCAStats->generalStats.inOctets_low = eo.in_octets_lo;
  pMoCAStats->BitStats64.outOctets_hi = eo.out_octets_hi;
  pMoCAStats->generalStats.outOctets_low = eo.out_octets_lo;

  pMoCAStats->generalStats.ncHandOffs = gs.nc_handoff_counter;
  pMoCAStats->generalStats.ncBackups = gs.nc_backup_counter;

  for (i = 0; i < MoCA_NUM_AGGR_PKT_COUNTS; i++)
    pMoCAStats->generalStats.aggrPktStatsTx[i] = gs.aggr_pkt_stats_tx[i];

  pMoCAStats->generalStats.aggrPktStatsRxMax = gs.aggr_pkt_stats_rx_max;
  pMoCAStats->generalStats.aggrPktStatsRxCount = gs.aggr_pkt_stats_rx_count;

  pMoCAStats->generalStats.receivedDataFiltered = gs.ecl_rx_mcast_filter_pkts;
  pMoCAStats->generalStats.lowDropData = gs.mac_tx_low_drop_pkts;

  pMoCAStats->extendedStats.rxMapPkts = gs.rx_map_packets;
  pMoCAStats->extendedStats.rxRRPkts = gs.rx_rr_packets;
  pMoCAStats->extendedStats.rxBeacons = gs.rx_beacons;
  pMoCAStats->extendedStats.rxCtrlPkts = gs.rx_control_packets;
  pMoCAStats->extendedStats.txBeacons = gs.tx_beacons;

  pMoCAStats->extendedStats.txMaps = gs.tx_map_packets;
  pMoCAStats->extendedStats.txLinkCtrlPkts = gs.tx_control_packets;
  pMoCAStats->extendedStats.txRRs = gs.tx_rr_packets;

  pMoCAStats->extendedStats.resyncAttempts = gs.resync_attempts_to_network;

  pMoCAStats->extendedStats.fcCounter[0] = gs.ecl_fc_bg;
  pMoCAStats->extendedStats.fcCounter[1] = gs.ecl_fc_low;
  pMoCAStats->extendedStats.fcCounter[2] = gs.ecl_fc_medium;
  pMoCAStats->extendedStats.fcCounter[3] = gs.ecl_fc_high;
  pMoCAStats->extendedStats.fcCounter[4] = gs.ecl_fc_pqos;
  pMoCAStats->extendedStats.fcCounter[5] = gs.ecl_fc_bp_all;

  pMoCAStats->extendedStats.txProtocolIe = gs.tx_protocol_ie;
  pMoCAStats->extendedStats.rxProtocolIe = gs.rx_protocol_ie;

  //pMoCAStats->extendedStats.gMiiTxBufFull
  //pMoCAStats->extendedStats.MoCARxBufFull

  //pMoCAStats->extendedStats.thisHandOffs
  //pMoCAStats->extendedStats.thisBackups

  //pMoCAStats->extendedStats.txTimeIe
  //pMoCAStats->extendedStats.rxTimeIe

  //pMoCAStats->extendedStats.rxLcAdmReqCrcErr
  //pMoCAStats->extendedStats.rxDataCrc

  if (ulReset)
    moca_set_reset_stats(ctx);

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetNodeStatistics
 * This function is called to retrieve current node statistics for a node
 */
CmsRet MoCACtl2_GetNodeStatistics(
  void *ctx,
  PMoCA_NODE_STATISTICS_ENTRY pNodeStatsEntry,
  UINT32 ulReset)
{
  moca_get_node_stats(
    ctx,
    pNodeStatsEntry->nodeId,
    (struct moca_node_stats *)&pNodeStatsEntry->txPkts);

  if (ulReset)
    moca_set_reset_stats(ctx);

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetNodeStatisticsExt
 * This function retrieves current node extended statistics for a node
 */
CmsRet MoCACtl2_GetNodeStatisticsExt(
  void *ctx,
  PMoCA_NODE_STATISTICS_EXT_ENTRY pNodeStatsEntry,
  UINT32 ulReset)
{
  moca_get_node_stats_ext_acc(
    ctx,
    pNodeStatsEntry->nodeId,
    (struct moca_node_stats_ext_acc *)&pNodeStatsEntry->rxUcCrcError);

  if (ulReset)
    moca_set_reset_stats(ctx);

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetNodeTblStatistics
 * This function retrieves current node statistics table
 */
CmsRet MoCACtl2_GetNodeTblStatistics(
  void *ctx,
  PMoCA_NODE_STATISTICS_ENTRY pNodeStatsEntry,
  UINT32 *pulNodeStatsTblSize,
  UINT32 ulReset)
{
  int i, num_nodes = 0;
  struct moca_network_status ns;

  /* get node bitmask */
  moca_get_network_status(ctx, &ns);

  /* get stats entry for each node */
  for (i = 0; i < MOCA_MAX_NODES; i++) {
    if (!(ns.connected_nodes & (1 << i)))
      continue;
    if (ns.node_id == i)
      continue;

    pNodeStatsEntry->nodeId = i;
    MoCACtl2_GetNodeStatistics(ctx, pNodeStatsEntry, 0);
    pNodeStatsEntry++;
    num_nodes++;
  }

  /* fill in *pulNodeStatusTblSize with number of nodes * entry size */
  *pulNodeStatsTblSize = num_nodes * sizeof(*pNodeStatsEntry);

  if(ulReset)
    moca_set_reset_stats(ctx);

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetNodeTblStatisticsExt
 * This function retrieves current node extended statistics table
 */
CmsRet MoCACtl2_GetNodeTblStatisticsExt(
  void *ctx,
  PMoCA_NODE_STATISTICS_EXT_ENTRY pNodeStatsEntry,
  UINT32 *pulNodeStatsTblSize,
  UINT32 ulReset)
{
  int i, num_nodes = 0;
  struct moca_network_status ns;

  /* get node bitmask */
  moca_get_network_status(ctx, &ns);

  /* get stats entry for each node */
  for (i = 0; i < MOCA_MAX_NODES; i++) {
    if (!(ns.connected_nodes & (1 << i)))
      continue;
    if (ns.node_id == i)
      continue;

    pNodeStatsEntry->nodeId = i;
    MoCACtl2_GetNodeStatisticsExt(ctx, pNodeStatsEntry, 0);
    pNodeStatsEntry++;
    num_nodes++;
  }

  /* fill in *pulNodeStatusTblSize with number of nodes * entry size */
  *pulNodeStatsTblSize = num_nodes * sizeof(*pNodeStatsEntry);

  if (ulReset)
    moca_set_reset_stats(ctx);

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_GetNodeTblStatus
 * This function retrieves current node status table
 */
CmsRet MoCACtl2_GetNodeTblStatus(
  void *ctx,
  PMoCA_NODE_STATUS_ENTRY pNodeStatusEntry,
  PMoCA_NODE_COMMON_STATUS_ENTRY pNodeCommonStatusEntry,
  UINT32 *pulNodeStatusTblSize)
{
  int i, ret, num_nodes = 0;
  struct moca_network_status ns;
  struct moca_gen_node_ext_status_in gns;

  memset(pNodeStatusEntry, 0, sizeof(*pNodeStatusEntry));
  memset(pNodeCommonStatusEntry, 0, sizeof(*pNodeCommonStatusEntry));

  /* get common status */
  gns.index = pNodeStatusEntry->nodeId;

  gns.profile_type = MOCA_EXT_STATUS_PROFILE_TX_BC_NPER;
  ret = moca_get_gen_node_ext_status(
    ctx,
    &gns,
    (struct moca_gen_node_ext_status *)&pNodeCommonStatusEntry->txBc);
  if (ret != 0) {
    gns.profile_type = MOCA_EXT_STATUS_PROFILE_TX_BCAST;
    moca_get_gen_node_ext_status(
      ctx,
      &gns,
      (struct moca_gen_node_ext_status *)&pNodeCommonStatusEntry->txBc);
  }

  gns.profile_type = MOCA_EXT_STATUS_PROFILE_TX_MAP_20;
  ret = moca_get_gen_node_ext_status(
    ctx,
    &gns,
    (struct moca_gen_node_ext_status *)&pNodeCommonStatusEntry->txMap);
  if (ret != 0) {
    gns.profile_type = MOCA_EXT_STATUS_PROFILE_TX_MAP;
    moca_get_gen_node_ext_status(
      ctx,
      &gns,
      (struct moca_gen_node_ext_status *)&pNodeCommonStatusEntry->txMap);
  }

  pNodeCommonStatusEntry->maxCommonPhyRates.txBcPhyRate =
    moca_phy_rate(
      pNodeCommonStatusEntry->txBc.nBas,
      pNodeCommonStatusEntry->txBc.cp,
      0,
      MoCA_VERSION_2_0);

  pNodeCommonStatusEntry->maxCommonPhyRates.txMapPhyRate =
    moca_phy_rate(
      pNodeCommonStatusEntry->txMap.nBas,
      pNodeCommonStatusEntry->txMap.cp,
      0,
      MoCA_VERSION_2_0);

  /* get node bitmask */
  moca_get_network_status(ctx, &ns);

  /* get status entry for each node */
  for (i = 0; i < MOCA_MAX_NODES; i++) {
    if (!(ns.connected_nodes & (1 << i)))
      continue;
    if(ns.node_id == i)
      continue;

    pNodeStatusEntry->nodeId = i;
    MoCACtl2_GetNodeStatus(ctx, pNodeStatusEntry);
    pNodeStatusEntry++;
    num_nodes++;
  }

  /* fill in *pulNodeStatusTblSize with number of nodes * entry size */
  *pulNodeStatusTblSize = num_nodes * sizeof(*pNodeStatusEntry);

  return(CMSRET_SUCCESS);
}

/*
 * MoCACtl2_Fmr
 * This function is called to initiate an FMR request with the MOCA driver.
 */
CmsRet MoCACtl2_Fmr(
  void *ctx,
  PMoCA_FMR_PARAMS params)
{
  int i, ret;
  struct moca_fmr_request req;
  struct moca_network_status ns;
  struct moca_gen_node_status gns;
  UINT32 addr[2];

  memset(&req, 0, sizeof(req));

  /* get node bitmask */
  moca_get_network_status(ctx, &ns);

  if (memcmp(params->address, gBcastMacAddr.macAddr, sizeof(MAC_ADDRESS))) {
    /* get status entry for each node */
    for (i = 0; i < MOCA_MAX_NODES; i++) {
      if (!(ns.connected_nodes & (1 << i)))
        continue;

      ret = moca_get_gen_node_status(ctx, i, (struct moca_gen_node_status *)&gns);
      moca_mac_to_u32(&addr[0], &addr[1], gns.eui.addr);
      if ((ret == 0) && ((gns.protocol_support >> 24) == MoCA_VERSION_11)) {
        if ((addr[0] == params->address[0]) && (addr[1] == params->address[1])) {
          req.wave0Nodemask = (1 << i);
          break;
        }
      }
    }
  }
  else {  /* broadcast */
    req.wave0Nodemask = ns.connected_nodes;
  }

  if (req.wave0Nodemask != 0)
    ret = __moca_set_fmr_request(ctx, &req);
  else
    return(CMSRET_INVALID_ARGUMENTS);

  if (ret != 0)
    return (CMSRET_INTERNAL_ERROR);
  return(CMSRET_SUCCESS);
}

