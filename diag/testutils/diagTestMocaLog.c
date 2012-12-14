/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Logging routines
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
static const char *diagMoca_prfDesc[MOCA_EXT_STATUS_PROFILE_TX_OFDMA+1] = {
  /* MOCA_EXT_STATUS_PROFILE_RX_UCAST */
  "RX Unicast",
  /* MOCA_EXT_STATUS_PROFILE_RX_BCAST */
  "RX Broadcast",
  /* MOCA_EXT_STATUS_PROFILE_RX_MAP */
  "RX Map",
  /* MOCA_EXT_STATUS_PROFILE_TX_UCAST */
  "TX Unicast",
  /* MOCA_EXT_STATUS_PROFILE_TX_BCAST */
  "TX Broadcast",
  /* MOCA_EXT_STATUS_PROFILE_TX_MAP */
  "TX Map",
  /* MOCA_EXT_STATUS_PROFILE_RX_UC_VLPER */
  "RX Unicast VLPER",
  /* MOCA_EXT_STATUS_PROFILE_RX_UC_NPER */
  "RX Unicast NPER",
  /* MOCA_EXT_STATUS_PROFILE_RX_BC_VLPER */
  "RX Broadcast VLPER",
  /* MOCA_EXT_STATUS_PROFILE_RX_BC_NPER */
  "RX Broadcast NPER",
  /* MOCA_EXT_STATUS_PROFILE_RX_MAP_20 */
  "RX Map 2.0",
  /* MOCA_EXT_STATUS_PROFILE_RX_OFDMA */
  "RX OFDMA",
  /* MOCA_EXT_STATUS_PROFILE_TX_UC_VLPER */
  "TX Unicast VLPER",
  /* MOCA_EXT_STATUS_PROFILE_TX_UC_NPER */
  "TX Unicast NPER",
  /* MOCA_EXT_STATUS_PROFILE_TX_BC_VLPER */
  "TX Broadcast VLPER",
  /* MOCA_EXT_STATUS_PROFILE_TX_BC_NPER */
  "TX Broadcast NPER",
  /* MOCA_EXT_STATUS_PROFILE_TX_MAP_20 */
  "TX Map 2.0",
  /* MOCA_EXT_STATUS_PROFILE_TX_OFDMA */
  "TX OFDMA"};

#define DIAGD_LOG_W_TS(format, args...) \
      diagLog(true, true, NULL, format,  ## args)
#define DIAGD_LOG_WO_TS(logging, format, args...) \
      diagLog(logging, false, NULL, format,  ## args)

/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */

uint32_t moca_count_bits(uint32_t val)
{
  uint32_t count = 0;

  while (val)
  {
    if (val & 0x1)
      count++;

    val >>= 1;
  }

  return(count);
}

void diagLog(bool logging, bool timestamp, const char *msgLvl, const char *format_str, ...)
{
  va_list     argList;
  time_t      currtime;
  char        dtstr[50];
  struct tm  *ptm;


  if (logging == true) {
    printf("Error: No Logging in diagTester program\n");
  }
  else {  /* write to stderr */
      // if print the time stamp
      if (timestamp == true) {
        time(&currtime);
        ptm = localtime(&currtime);
        strftime(dtstr, sizeof(dtstr), "%b %d %Y %T", ptm);
        dtstr[strlen(dtstr)] = '\0';
        fprintf(stderr, "%s ", dtstr);
      }

      if (msgLvl != NULL) {
        // logging message level
        fprintf(stderr, "%s ", msgLvl);
      }

      // Now print the caller's message
      va_start(argList, format_str);
      vfprintf(stderr, format_str, argList);
      fprintf(stderr, "\n");
      va_end(argList);
  }
}

void convertUpTime(
  uint32_t timeInSecs, uint32_t *pTimeInHrs,
  uint32_t *pTimeInMin, uint32_t *pTimeInSecs) {
  *pTimeInHrs  = timeInSecs / (NO_OF_SECS_IN_MIN * NO_OF_MINS_IN_HR);
  timeInSecs   = timeInSecs % (NO_OF_SECS_IN_MIN * NO_OF_MINS_IN_HR);
  *pTimeInMin  = timeInSecs / NO_OF_SECS_IN_MIN;
  timeInSecs   = timeInSecs % NO_OF_SECS_IN_MIN;
  *pTimeInSecs = timeInSecs;
  return;
}  /* end of convertUpTime */

/*
 * Write MoCA interface statistics to diagd log file with timestamp
 *
 * Input:
 * logging  -  Flag to write to diag log file
 * pStats   -  Point to Diag MoCA interface statistics
 *
 * Output:
 * None
 */
void diagMocaStatsLog(bool logging, diag_mocaIf_stats_t *pStats)
{
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_total_pkts=%u", pStats->ecl_tx_total_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_ucast_pkts=%u", pStats->ecl_tx_ucast_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_bcast_pkts=%u", pStats->ecl_tx_bcast_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_mcast_pkts=%u", pStats->ecl_tx_mcast_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_ucast_unknown=%u", pStats->ecl_tx_ucast_unknown);
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_mcast_unknown=%u", pStats->ecl_tx_mcast_unknown);
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_ucast_drops=%u", pStats->ecl_tx_ucast_drops);
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_mcast_drops=%u", pStats->ecl_tx_mcast_drops);
  DIAGD_LOG_WO_TS(logging, "    ecl_tx_buff_drop_pkts=%u", pStats->ecl_tx_buff_drop_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_rx_total_pkts=%u", pStats->ecl_rx_total_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_rx_ucast_pkts=%u", pStats->ecl_rx_ucast_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_rx_bcast_pkts=%u", pStats->ecl_rx_bcast_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_rx_mcast_pkts=%u", pStats->ecl_rx_mcast_pkts);
  DIAGD_LOG_WO_TS(logging, "    ecl_rx_ucast_drops=%u", pStats->ecl_rx_ucast_drops);
  DIAGD_LOG_WO_TS(logging, "    mac_tx_low_drop_pkts=%u", pStats->mac_tx_low_drop_pkts);
  DIAGD_LOG_WO_TS(logging, "    mac_rx_buff_drop_pkts=%u", pStats->mac_rx_buff_drop_pkts);
  DIAGD_LOG_WO_TS(logging, "    rx_beacons=%u", pStats->rx_beacons);
  DIAGD_LOG_WO_TS(logging, "    rx_map_packets=%u", pStats->rx_map_packets);
  DIAGD_LOG_WO_TS(logging, "    rx_rr_packets=%u", pStats->rx_rr_packets);
  DIAGD_LOG_WO_TS(logging, "    rx_control_uc_packets=%u", pStats->rx_control_uc_packets);
  DIAGD_LOG_WO_TS(logging, "    rx_control_bc_packets=%u", pStats->rx_control_bc_packets);

  DIAGD_LOG_WO_TS(logging, "    in_octets_hi=%u", pStats->in_octets_hi);
  DIAGD_LOG_WO_TS(logging, "    in_octets_lo=%u", pStats->in_octets_lo);
  DIAGD_LOG_WO_TS(logging, "    out_octets_hi=%u", pStats->out_octets_hi);
  DIAGD_LOG_WO_TS(logging, "    out_octets_lo=%u", pStats->out_octets_lo);

  DIAGD_LOG_WO_TS(logging, "    rx_uc_crc_error=%u", pStats->rx_uc_crc_error);
  DIAGD_LOG_WO_TS(logging, "    rx_bc_crc_error=%u", pStats->rx_bc_crc_error);
  DIAGD_LOG_WO_TS(logging, "    rx_map_crc_error=%u", pStats->rx_map_crc_error);
  DIAGD_LOG_WO_TS(logging, "    rx_beacon_crc_error=%u", pStats->rx_beacon_crc_error);
  DIAGD_LOG_WO_TS(logging, "    rx_rr_crc_error=%u", pStats->rx_rr_crc_error);
  DIAGD_LOG_WO_TS(logging, "    rx_lc_uc_crc_error=%u", pStats->rx_lc_uc_crc_error);
  DIAGD_LOG_WO_TS(logging, "    rx_lc_bc_crc_error=%u", pStats->rx_lc_bc_crc_error);
}

/*
 * Write Diag MoCA service performance monitoring results to
 * diagd log file with timestamp.
 *
 * Input:
 * logging  -  Flag to write to diag log file
 * pStats   -  Point to Diag MoCA service performance monitoring results
 *
 * Output:
 * None
 */
void diagMocaPerfStatusLog(bool logging, diag_moca_perf_status_entry_t *pPerfStatus)
{
  DIAGD_LOG_WO_TS(logging, "============ Performace Status ===================");
  DIAGD_LOG_WO_TS(logging, "       valid=%s", pPerfStatus->valid == true? "true":"false");
  DIAGD_LOG_WO_TS(logging, "       nodeId=%d", pPerfStatus->nodeId);
  DIAGD_LOG_WO_TS(logging, "       rxUcPhyRate=%d", pPerfStatus->rxUcPhyRate);
  DIAGD_LOG_WO_TS(logging, "       rxUcPower=%d", pPerfStatus->rxUcPower);
  DIAGD_LOG_WO_TS(logging, "       rxUcAvgSnr=%d", pPerfStatus->rxUcAvgSnr);
  DIAGD_LOG_WO_TS(logging, "       rxUcBitLoading=%d", pPerfStatus->rxUcBitLoading);
  DIAGD_LOG_WO_TS(logging, "========= end Performace Status ===================");
}

/*
 * Write MoCA node status to diagd log file with timestamp
 *
 * Input:
 * logging     -  Flag to write to diag log file
 * pNodeStatus -  Point to MoCA node status entry
 *
 * Output:
 * None
 */
void diagMocaNodeStatusLog(bool logging, diag_moca_nodestatus_entry_t *pNodeStatus)
{
  DIAGD_LOG_WO_TS(logging, "Node                             : %d ", pNodeStatus->nodeId);
  DIAGD_LOG_WO_TS(logging, "=============================================");
  diagMoca_log_gen_node_status(logging, &pNodeStatus->gns);
  diagMoca_log_gen_node_ext_status(logging, pNodeStatus->profile.type, &pNodeStatus->profile.rxUc);
}

/*
 * Write MoCA node statistics table to diagd log file with timestamp
 *
 * Input:
 * logging  -  Flag to write to diag log file
 * pStats   -  Point to Diag node statistics table
 *
 * Output:
 * None
 */
void diagMocaNodeStatsLog(bool logging, diag_moca_node_stats_table_t  *pNodeStats)
{
  int idx=0;
  int nodes=0;
  diag_moca_node_stats_entry_t *pNodeStatsEntry = &pNodeStats->Stats;

  nodes =  pNodeStats->nodeStatsTblSize/sizeof(diag_moca_node_stats_entry_t);

  for (idx=0; idx < nodes; idx ++) {
    DIAGD_LOG_WO_TS(logging, "=============================================");
    DIAGD_LOG_WO_TS(logging, "Node                             : %d ", pNodeStatsEntry->nodeId);
    DIAGD_LOG_WO_TS(logging, "MAC Address                      : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
        pNodeStatsEntry->macAddr.addr[0],
        pNodeStatsEntry->macAddr.addr[1],
        pNodeStatsEntry->macAddr.addr[2],
        pNodeStatsEntry->macAddr.addr[3],
        pNodeStatsEntry->macAddr.addr[4],
        pNodeStatsEntry->macAddr.addr[5]);
    DIAGD_LOG_WO_TS(logging, "=============================================");
    DIAGD_LOG_WO_TS(logging, "Unicast Tx Pkts To Node          : %d ", pNodeStatsEntry->nodeStats.tx_packets);
    DIAGD_LOG_WO_TS(logging, "Unicast Rx Pkts From Node        : %d ", pNodeStatsEntry->nodeStats.rx_packets);
    DIAGD_LOG_WO_TS(logging, "Rx CodeWord NoError              : %d ", pNodeStatsEntry->nodeStats.rx_cw_unerror);
    DIAGD_LOG_WO_TS(logging, "Rx CodeWord ErrorAndCorrected    : %d ", pNodeStatsEntry->nodeStats.rx_cw_corrected);
    DIAGD_LOG_WO_TS(logging, "Rx CodeWord ErrorAndUnCorrected  : %d ", pNodeStatsEntry->nodeStats.rx_cw_uncorrected);
    DIAGD_LOG_WO_TS(logging, "Rx NoSync Errors                 : %d ", pNodeStatsEntry->nodeStats.rx_no_sync);
    DIAGD_LOG_WO_TS(logging, "=============================================");
    DIAGD_LOG_WO_TS(logging, "        MoCA Extended Node Statistics Data");
    DIAGD_LOG_WO_TS(logging, "=============================================");
    DIAGD_LOG_WO_TS(logging, "NODE_RX_UC_CRC_ERROR                  : %d ", pNodeStatsEntry->nodeStatsExt.rx_uc_crc_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_UC_TIMEOUT_ERROR              : %d ", pNodeStatsEntry->nodeStatsExt.rx_uc_timeout_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_BC_CRC_ERROR                  : %d ", pNodeStatsEntry->nodeStatsExt.rx_bc_crc_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_BC_TIMEOUT_ERROR              : %d ", pNodeStatsEntry->nodeStatsExt.rx_bc_timeout_error);

    DIAGD_LOG_WO_TS(logging, "NODE_RX_MAP_CRC_ERROR                 : %d ", pNodeStatsEntry->nodeStatsExt.rx_map_crc_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_MAP_TIMEOUT_ERROR             : %d ", pNodeStatsEntry->nodeStatsExt.rx_map_timeout_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_BEACON_CRC_ERROR              : %d ", pNodeStatsEntry->nodeStatsExt.rx_beacon_crc_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_BEACON_TIMEOUT_ERROR          : %d ", pNodeStatsEntry->nodeStatsExt.rx_beacon_timeout_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_RR_CRC_ERROR                  : %d ", pNodeStatsEntry->nodeStatsExt.rx_rr_crc_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_RR_TIMEOUT_ERROR              : %d ", pNodeStatsEntry->nodeStatsExt.rx_rr_timeout_error);

    DIAGD_LOG_WO_TS(logging, "NODE_RX_LC_UC_CRC_ERROR               : %d ", pNodeStatsEntry->nodeStatsExt.rx_lc_uc_crc_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_LC_BC_CRC_ERROR               : %d ", pNodeStatsEntry->nodeStatsExt.rx_lc_bc_crc_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_LC_UC_TIMEOUT_ERROR           : %d ", pNodeStatsEntry->nodeStatsExt.rx_lc_uc_timeout_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_LC_BC_TIMEOUT_ERROR           : %d ", pNodeStatsEntry->nodeStatsExt.rx_lc_bc_timeout_error);

    DIAGD_LOG_WO_TS(logging, "NODE_RX_P1_ERROR                      : %d ", pNodeStatsEntry->nodeStatsExt.rx_probe1_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_P2_ERROR                      : %d ", pNodeStatsEntry->nodeStatsExt.rx_probe2_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_P3_ERROR                      : %d ", pNodeStatsEntry->nodeStatsExt.rx_probe3_error);
    DIAGD_LOG_WO_TS(logging, "NODE_RX_P1_GCD_ERROR                  : %d ", pNodeStatsEntry->nodeStatsExt.rx_probe1_gcd_error);
    DIAGD_LOG_WO_TS(logging, "=============================================");

    pNodeStatsEntry++;
  }
}

/*
 * Write MoCA self node status to diagd log file with timestamp
 *
 * Input:
 * logging  -  Flag to write to diag log file
 * pStatus  -  Point to MoCA_STATUS
 *
 * Output:
 * None
 */
void diagMocaMyStatusLog(bool logging, diag_moca_status_t *pStatus)
{
  DIAGD_LOG_WO_TS(logging, "            MoCA Status               ");
  DIAGD_LOG_WO_TS(logging, "======================================");
  DIAGD_LOG_WO_TS(logging, "Self Node Id = %u", pStatus->nodeId);
  diagMoca_log_node_status(logging, &pStatus->ns);
  diagMoca_log_interface_status(logging, &pStatus->intf);
  diagMoca_log_network_status(logging, &pStatus->net);
  diagMoca_log_fw_version(logging, &pStatus->fw);
  diagMoca_log_current_keys(logging, &pStatus->key);
  diagMoca_log_key_times(logging, &pStatus->keyTimes);
  diagMoca_log_mac_addr(logging, &pStatus->macAddr);
  diagMoca_log_drv_info(logging, &pStatus->drv);
  diagMoca_log_single_channel_operation(logging, &pStatus->singleChOp);
  diagMoca_log_tx_gcd_power_reduction(logging, &pStatus->txGcdPowerReduction);
  diagMoca_log_led_status(logging, &pStatus->ledStatus);
  DIAGD_LOG_WO_TS(logging, "========== end MoCA Status ===========");
}


void diagMoca_log_pqos_egress_numflows(bool logging, uint32_t * in)
{
  DIAGD_LOG_WO_TS(logging,"pqos_egress_numflows: %u  ( 0x%x )", *in, *in);
}

void diagMoca_log_led_status(bool logging, uint32_t * in)
{
  DIAGD_LOG_WO_TS(logging,"led_status: %u  ( 0x%x )", *in, *in);
}

void diagMoca_log_preferred_nc(bool logging, uint32_t * in)
{
  DIAGD_LOG_WO_TS(logging,"preferred_nc: %u  ( 0x%x )", *in, *in);
}

void diagMoca_log_single_channel_operation(bool logging, uint32_t * in)
{
  DIAGD_LOG_WO_TS(logging,"single_channel_operation: %u  ( 0x%x )", *in, *in);
}

void diagMoca_log_mac_addr(bool logging, struct moca_mac_addr * in)
{
  DIAGD_LOG_WO_TS(logging,"== mac_addr  ========================================== ");
  DIAGD_LOG_WO_TS(logging,"val: %02x:%02x:%02x:%02x:%02x:%02x ", MOCA_DISPLAY_MAC(in->val));
  DIAGD_LOG_WO_TS(logging,"== end mac_addr  ====================================== ");
}

void diagMoca_log_node_status(bool logging, struct moca_node_status * in)
{
  DIAGD_LOG_WO_TS(logging, "== node_status  ======================================= ");
  DIAGD_LOG_WO_TS(logging, "vendor_id            : %u  ( 0x%x )", in->vendor_id, in->vendor_id);
  DIAGD_LOG_WO_TS(logging, "moca_hw_version      : %u  ( 0x%x )", in->moca_hw_version, in->moca_hw_version);
  DIAGD_LOG_WO_TS(logging, "moca_sw_version_major: %u  ( 0x%x )", in->moca_sw_version_major, in->moca_sw_version_major);
  DIAGD_LOG_WO_TS(logging, "moca_sw_version_minor: %u  ( 0x%x )", in->moca_sw_version_minor, in->moca_sw_version_minor);
  DIAGD_LOG_WO_TS(logging, "moca_sw_version_rev  : %u  ( 0x%x )", in->moca_sw_version_rev, in->moca_sw_version_rev);
  DIAGD_LOG_WO_TS(logging, "self_moca_version    : %u  ( 0x%x )", in->self_moca_version, in->self_moca_version);
  DIAGD_LOG_WO_TS(logging, "qam_256_support      : %u  ( 0x%x )", in->qam_256_support, in->qam_256_support);
  DIAGD_LOG_WO_TS(logging, "== end node_status  =================================== ");
}

void diagMoca_log_fw_version(bool logging, struct moca_fw_version * in)
{
  DIAGD_LOG_WO_TS(logging, "== fw_version  ======================================== ");
  DIAGD_LOG_WO_TS(logging, "version_moca : %u  ( 0x%x )", in->version_moca, in->version_moca);
  DIAGD_LOG_WO_TS(logging, "version_major: %u  ( 0x%x )", in->version_major, in->version_major);
  DIAGD_LOG_WO_TS(logging, "version_minor: %u  ( 0x%x )", in->version_minor, in->version_minor);
  DIAGD_LOG_WO_TS(logging, "version_patch: %u  ( 0x%x )", in->version_patch, in->version_patch);
  DIAGD_LOG_WO_TS(logging, "== end fw_version  ==================================== ");
}

void diagMoca_log_drv_info(bool logging, struct moca_drv_info * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== drv_info  ========================================== ");
  DIAGD_LOG_WO_TS(logging, "version     : %u  ( 0x%x )", in->version, in->version);
  DIAGD_LOG_WO_TS(logging, "build_number: %u  ( 0x%x )", in->build_number, in->build_number);
  DIAGD_LOG_WO_TS(logging, "hw_rev      : %u  ( 0x%x )", in->hw_rev, in->hw_rev);
  DIAGD_LOG_WO_TS(logging, "uptime      : %02uh:%02um:%02us ", (in->uptime / 3600), ((in->uptime % 3600) / 60), (in->uptime % 60));
  DIAGD_LOG_WO_TS(logging, "link_uptime : %02uh:%02um:%02us ", (in->link_uptime / 3600), ((in->link_uptime % 3600) / 60), (in->link_uptime % 60));
  DIAGD_LOG_WO_TS(logging, "core_uptime : %02uh:%02um:%02us ", (in->core_uptime / 3600), ((in->core_uptime % 3600) / 60), (in->core_uptime % 60));
  sprintf(outBuf, "%s", "ifname[16]  : ");
  for (i = 0; i < 16; i++) {
    sprintf(inBuf, "%c", in->ifname[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging,outBuf);
  sprintf(outBuf, "%s", "devname[64] : ");
  for (i = 0; i < 64; i++) {
    sprintf(inBuf, "%c", in->devname[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging,outBuf);
  DIAGD_LOG_WO_TS(logging, "rf_band     : %u  ( 0x%x )", in->rf_band, in->rf_band);
  DIAGD_LOG_WO_TS(logging, "chip_id     : %u  ( 0x%x )", in->chip_id, in->chip_id);
  DIAGD_LOG_WO_TS(logging, "== end drv_info  ====================================== ");
}

void diagMoca_log_current_keys(bool logging, struct moca_current_keys * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== current_keys  ====================================== ");
  sprintf(outBuf, "%s", "pmk_even_key[2]    : ");
  for (i = 0; i < 2; i++) {
    sprintf(inBuf, "%08X ", in->pmk_even_key[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging, outBuf);
  sprintf(outBuf, "%s", "pmk_odd_key[2]     : ");
  for (i = 0; i < 2; i++) {
    sprintf(inBuf, "%08X ", in->pmk_odd_key[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging,outBuf);
  sprintf(outBuf, "%s", "tek_even_key[2]    : ");
  for (i = 0; i < 2; i++) {
    sprintf(inBuf, "%08X ", in->tek_even_key[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging, outBuf);
  sprintf(outBuf, "%s", "tek_odd_key[2]     : ");
  for (i = 0; i < 2; i++) {
    sprintf(inBuf, "%08X ", in->tek_odd_key[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging, outBuf);
  sprintf(outBuf, "%s", "aes_pmk_even_key[4]: ");
  for (i = 0; i < 4; i++) {
    sprintf(inBuf, "%08X ", in->aes_pmk_even_key[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging, outBuf);
  sprintf(outBuf, "%s", "aes_pmk_odd_key[4] : ");
  for (i = 0; i < 4; i++) {
    sprintf(inBuf, "%08X ", in->aes_pmk_odd_key[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging,outBuf);
  sprintf(outBuf, "%s", "aes_tek_even_key[4]: ");
  for (i = 0; i < 4; i++) {
    sprintf(inBuf, "%08X ", in->aes_tek_even_key[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging, outBuf);
  sprintf(outBuf, "%s", "aes_tek_odd_key[4] : ");
  for (i = 0; i < 4; i++) {
    sprintf(inBuf, "%08X ", in->aes_tek_odd_key[i]);
    strcat(outBuf, inBuf);
  }
  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end current_keys  ================================== ");
}

void diagMoca_log_key_times(bool logging, struct moca_key_times *in)
{
  /* Not sure if this is the right format */
  DIAGD_LOG_WO_TS(logging, "== key times  ==================================== ");
  DIAGD_LOG_WO_TS(logging, "tek_time          : %u", in->tek_time);
  DIAGD_LOG_WO_TS(logging, "tek_last_interval          : %u", in->tek_last_interval);
  DIAGD_LOG_WO_TS(logging, "tek_even_odd          : %u", in->tek_even_odd);
  DIAGD_LOG_WO_TS(logging, "pmk_time          : %u", in->pmk_time);
  DIAGD_LOG_WO_TS(logging, "pmk_last_interval          : %u", in->pmk_last_interval);
  DIAGD_LOG_WO_TS(logging, "pmk_even_odd          : %u", in->pmk_even_odd);
  DIAGD_LOG_WO_TS(logging, "== end key times ================================== ");
}

void diagMoca_log_network_status(bool logging, struct moca_network_status * in)
{
  DIAGD_LOG_WO_TS(logging, "== network_status  ==================================== ");
  DIAGD_LOG_WO_TS(logging, "network_moca_version: %u  ( 0x%x )", in->network_moca_version, in->network_moca_version);
  DIAGD_LOG_WO_TS(logging, "connected_nodes     : %d  (bitmask 0x%x)", moca_count_bits(in->connected_nodes), in->connected_nodes);
  DIAGD_LOG_WO_TS(logging, "node_id             : %u  ( 0x%x )", in->node_id, in->node_id);
  DIAGD_LOG_WO_TS(logging, "nc_node_id          : %u  ( 0x%x )", in->nc_node_id, in->nc_node_id);
  DIAGD_LOG_WO_TS(logging, "backup_nc_id        : %u  ( 0x%x )", in->backup_nc_id, in->backup_nc_id);
  DIAGD_LOG_WO_TS(logging, "bw_status           : %u  ( 0x%x )", in->bw_status, in->bw_status);
  DIAGD_LOG_WO_TS(logging, "nodes_usable_bitmask: %u  ( 0x%x )", in->nodes_usable_bitmask, in->nodes_usable_bitmask);
  DIAGD_LOG_WO_TS(logging, "network_taboo_mask  : %u  ( 0x%x )", in->network_taboo_mask, in->network_taboo_mask);
  DIAGD_LOG_WO_TS(logging, "network_taboo_start : %u  ( 0x%x )", in->network_taboo_start, in->network_taboo_start);
  DIAGD_LOG_WO_TS(logging, "== end network_status  ================================ ");
}

void diagMoca_log_interface_status(bool logging, struct moca_interface_status * in)
{
  DIAGD_LOG_WO_TS(logging, "== interface_status  ================================== ");
  DIAGD_LOG_WO_TS(logging, "link_status      : %s (%d)", (in->link_status ? "Up" : "Down"), in->link_status);
  DIAGD_LOG_WO_TS(logging, "rf_channel       : %2d - %d MHz", in->rf_channel, in->rf_channel * 25);
  DIAGD_LOG_WO_TS(logging, "primary_channel  : %2d - %d MHz", in->primary_channel, in->primary_channel * 25);
  DIAGD_LOG_WO_TS(logging, "secondary_channel: %2d - %d MHz", in->secondary_channel, in->secondary_channel * 25);
  DIAGD_LOG_WO_TS(logging, "== end interface_status  ============================== ");
}

void diagMoca_log_tx_gcd_power_reduction(bool logging,  uint32_t *in)
{
  DIAGD_LOG_WO_TS(logging, "tx_gcd_power_reduction : %u", *in);
}

uint8_t diagMoca_get_subcarrier(uint32_t * p_bit_loading, uint32_t sub_carrier)
{
  uint8_t value = 0;

  value = (uint8_t)(p_bit_loading[sub_carrier/8] >> (28 - ((sub_carrier % 8) * 4))) & 0xF;

  return(value);
}

void diagMoca_log_subcarriers(
    bool logging,
    int32_t start,
    int32_t end,
    uint32_t * p_bit_loading1,
    uint32_t * p_bit_loading2)
{
  int32_t dir;
  int32_t subCarrier1;
  int32_t subCarrier2;
  uint32_t print_nums = 1;
  char inBuf[128];
  char outBuf[128];

  if (start < end)
    dir = 1;
  else
    dir = -1;

  subCarrier2 = start;
  for (subCarrier1 = start;
      start < end ? subCarrier1 <= end : subCarrier1 >= end;
      subCarrier1 += dir)
  {
    if (print_nums)
    {
      sprintf(outBuf, "%3.3d - %3.3d:  ", subCarrier1, subCarrier1 + (32 * dir) - dir );
      print_nums = 0;
    }
    sprintf (inBuf, "%x", diagMoca_get_subcarrier(p_bit_loading1, subCarrier1)) ;
    strcat(outBuf, inBuf);
    if (((subCarrier1 + (dir > 0 ? 1 : 0)) % 32) == 0) {
      if (p_bit_loading2 != NULL)
      {
        strcat(outBuf, "   ") ;
        /* Display the second Bit Loading */
        for (; subCarrier2 >= 0; subCarrier2 += dir) {
          sprintf (inBuf, "%x", diagMoca_get_subcarrier(p_bit_loading2, subCarrier2)) ;
          strcat(outBuf, inBuf);
          if (((subCarrier2 + (dir > 0 ? 1 : 0)) % 32) == 0) {
            subCarrier2 += dir;
            break ;
          }
        } /* for (secSubCarrier) */
      }
      DIAGD_LOG_WO_TS(logging, outBuf) ;
      print_nums = 1;
    } /* if (subCarrier) */
  } /* for (subCarrier) */

}

void diagMoca_log_bit_loading(
    bool logging,
    uint32_t * p_bit_loading1,
    uint32_t * p_bit_loading2,
    uint32_t num_carriers)
{
  /* If the number of carriers is 256, it's 1.1 otherwise it's 2.0.
   * We want to display the sub-carriers in order of increasing
   * frequency. The array holds the sub-carriers starting with SC
   * index 0.
   * For 1.1, the SCs should be displayed as: 127-0,255-128
   * For 2.0, the SCs should be displayed as : 256-511, 0-255
   *
   * There are 8 sub-carriers per u32. */

  if (num_carriers == MOCA_MAX_SUB_CARRIERS_1_1)
  {
    diagMoca_log_subcarriers(logging, 127, 0, p_bit_loading1, p_bit_loading2);
    diagMoca_log_subcarriers(logging, 255, 128, p_bit_loading1, p_bit_loading2);
  }
  else if (num_carriers == MOCA_MAX_SUB_CARRIERS)
  {
    diagMoca_log_subcarriers(logging, 256, 511, p_bit_loading1, p_bit_loading2);
    diagMoca_log_subcarriers(logging, 0, 255, p_bit_loading1, p_bit_loading2);
  }
  else
  {
    DIAGD_TRACE("%s Unsupported number of sub-carriers %d\n", __func__, num_carriers);
  }

}

void diagMoca_log_gen_node_status(bool logging, struct moca_gen_node_status * in)
{
  DIAGD_LOG_WO_TS(logging, "== gen_node_status  ===================================");
  DIAGD_LOG_WO_TS(logging, "eui             : %02x:%02x:%02x:%02x:%02x:%02x ", MOCA_DISPLAY_MAC(in->eui));
  DIAGD_LOG_WO_TS(logging, "zero            : %u  ( 0x%04x )", in->zero, in->zero);
  DIAGD_LOG_WO_TS(logging, "freq_offset     : 0x%x  ( %d )", in->freq_offset, in->freq_offset);
  DIAGD_LOG_WO_TS(logging, "node_tx_backoff : %u  ( 0x%x )", in->node_tx_backoff, in->node_tx_backoff);
  DIAGD_LOG_WO_TS(logging, "protocol_support: %u  ( 0x%x )", in->protocol_support, in->protocol_support);
  DIAGD_LOG_WO_TS(logging, "== end gen_node_status  ===============================");
}

void diagMoca_log_gen_node_ext_status(
    bool logging,
    uint32_t profile_type,
    struct moca_gen_node_ext_status * in)
{
  DIAGD_LOG_WO_TS(logging, "== gen_node_ext_status  ===============================");
  DIAGD_LOG_WO_TS(logging, "profile_type   : %s", diagMoca_prfDesc[profile_type]);
  DIAGD_LOG_WO_TS(logging, "nbas           : %u  ( 0x%x )", in->nbas, in->nbas);
  DIAGD_LOG_WO_TS(logging, "preamble_type  : %u  ( 0x%x )",
      (in->preamble_type == 0 ? 1 : (in->preamble_type == 1 ?
          1 : (in->preamble_type == 2 ? 2 : (in->preamble_type == 12 ?
              0 : (in->preamble_type - 1))))),
      (in->preamble_type == 0 ? 1 : (in->preamble_type == 1 ?
          1 : (in->preamble_type == 2 ? 2 : (in->preamble_type == 12 ?
              0 : (in->preamble_type - 1))))));
  DIAGD_LOG_WO_TS(logging, "cp             : %u  ( 0x%x )", in->cp, in->cp);
  DIAGD_LOG_WO_TS(logging, "tx_power       : %d dBm", in->tx_power);
  DIAGD_LOG_WO_TS(logging, "rx_power       : %1.3f dBm", (float)(in->rx_power / 4.0));
  DIAGD_LOG_WO_TS(logging, "bit_loading[64]: ");
  diagMoca_log_bit_loading(logging, &in->bit_loading[0], NULL,
      (profile_type > MOCA_EXT_STATUS_PROFILE_TX_MAP ?
       MOCA_MAX_SUB_CARRIERS : MOCA_MAX_SUB_CARRIERS_1_1));

  DIAGD_LOG_WO_TS(logging, "avg_snr        : %1.3f ", (float)(in->avg_snr / 256.0));
  DIAGD_LOG_WO_TS(logging, "phy_rate       : %d Mbps", in->phy_rate);
  DIAGD_LOG_WO_TS(logging, "turbo_status   : %u  ( 0x%x )", in->turbo_status, in->turbo_status);
  DIAGD_LOG_WO_TS(logging, "== end gen_node_ext_status  ===========================");
}

void diagMoca_log_priority_allocations(bool logging, struct moca_priority_allocations * in)
{
  DIAGD_LOG_WO_TS(logging, "== priority_allocations  ==============================");
  DIAGD_LOG_WO_TS(logging, "reservation_pqos: %u  ( 0x%x )", in->reservation_pqos, in->reservation_pqos);
  DIAGD_LOG_WO_TS(logging, "reservation_high: %u  ( 0x%x )", in->reservation_high, in->reservation_high);
  DIAGD_LOG_WO_TS(logging, "reservation_med : %u  ( 0x%x )", in->reservation_med, in->reservation_med);
  DIAGD_LOG_WO_TS(logging, "reservation_low : %u  ( 0x%x )", in->reservation_low, in->reservation_low);
  DIAGD_LOG_WO_TS(logging, "limitation_pqos : %u  ( 0x%x )", in->limitation_pqos, in->limitation_pqos);
  DIAGD_LOG_WO_TS(logging, "limitation_high : %u  ( 0x%x )", in->limitation_high, in->limitation_high);
  DIAGD_LOG_WO_TS(logging, "limitation_med  : %u  ( 0x%x )", in->limitation_med, in->limitation_med);
  DIAGD_LOG_WO_TS(logging, "limitation_low  : %u  ( 0x%x )", in->limitation_low, in->limitation_low);
  DIAGD_LOG_WO_TS(logging, "== end priority_allocations ==========================");
}

void diagMoca_log_rlapm_table_100(bool logging, struct moca_rlapm_table_100 * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== rlapm_table_100  ===================================");

  sprintf(outBuf, "rlapmtable[66]: ");
  for (i = 0; i < 66; i++) {
    sprintf(inBuf, "%02x ", in->rlapmtable[i]);
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "                ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end rlapm_table_100  ===============================");
}

void diagMoca_log_rlapm_table_50(bool logging, struct moca_rlapm_table_50 * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== rlapm_table_50  ====================================");

  sprintf(outBuf, "rlapmtable[66]: ");
  for (i = 0; i < 66; i++) {
    sprintf(inBuf, "%02x ", in->rlapmtable[i]);
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "                ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end rlapm_table_50  ================================");
}

void diagMoca_log_sapm_table_50(bool logging, struct moca_sapm_table_50 * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== sapm_table_50  ====================================");
  sprintf(outBuf, "  ");
  for (i = 0; i < 256; i++) {
    sprintf(inBuf, "%02x ", in->val[i]);
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "  ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end sapm_table_50  ================================");
}

void diagMoca_log_sapm_table_100(bool logging, struct moca_sapm_table_100 * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== sapm_table_100  ====================================");
  sprintf(outBuf, "  ");
  for (i = 0; i < 512; i++) {
    sprintf(inBuf, "%02x ", in->val[i]);
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "  ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end sapm_table_100  ================================");
}

void diagMoca_log_snr_margin_rs(bool logging, struct moca_snr_margin_rs * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== snr_margin_rs  =====================================");
  DIAGD_LOG_WO_TS(logging, "base_margin: %1.3f ", (float)(in->base_margin / 256.0));

  sprintf(outBuf, "offsets[10]: ");
  for (i = 0; i < 10; i++) {
    sprintf(inBuf, "%1.3f ", (float)(in->offsets[i] / 256.0));
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "             ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end snr_margin_rs  =================================");
}

void diagMoca_log_snr_margin_ldpc(bool logging, struct moca_snr_margin_ldpc * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== snr_margin_ldpc  ===================================");
  DIAGD_LOG_WO_TS(logging, "base_margin: %1.3f ", (float)(in->base_margin / 256.0));
  sprintf(outBuf, "offsets[10]: ");
  for (i = 0; i < 10; i++) {
    sprintf(inBuf, "%1.3f ", (float)(in->offsets[i] / 256.0));
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "             ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end snr_margin_ldpc  ===============================");
}

void diagMoca_log_snr_margin_ldpc_pre5(bool logging, struct moca_snr_margin_ldpc_pre5 * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== snr_margin_ldpc_pre5  ==============================");
  DIAGD_LOG_WO_TS(logging, "base_margin: %1.3f ", (float)(in->base_margin / 256.0));
  sprintf(outBuf, "offsets[10]: ");
  for (i = 0; i < 10; i++) {
    sprintf(inBuf, "%1.3f ", (float)(in->offsets[i] / 256.0));
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "             ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end snr_margin_ldpc_pre5  ==========================");
}


void diagMoca_log_snr_margin_ofdma(bool logging, struct moca_snr_margin_ofdma * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== snr_margin_ofdma  ==================================");
  DIAGD_LOG_WO_TS(logging, "base_margin: %1.3f ", (float)(in->base_margin / 256.0));
  sprintf(outBuf, "offsets[10]: ");
  for (i = 0; i < 10; i++) {
    sprintf(inBuf, "%1.3f ", (float)(in->offsets[i] / 256.0));
    strcat(outBuf, inBuf);
    if (i % 8 == 7){
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "             ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end snr_margin_ofdma  ==============================");
}

void diagMoca_log_snr_margin_table_ldpc(
    bool logging,
    struct moca_snr_margin_table_ldpc * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== snr_margin_table_ldpc  ===================================");
  sprintf(outBuf, "  ");
  for (i = 0; i < 22; i++) {
    sprintf(inBuf, "%1.3f ", (float)(in->mgntable[i] / 256.0));
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "  ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end snr_margin_table_ldpc  ===================================");
}

void diagMoca_log_snr_margin_table_ldpc_pre5(
    bool logging,
    struct moca_snr_margin_table_ldpc_pre5 * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== snr_margin_table_ldpc_pre5  ===================================");
  sprintf(outBuf, "  ");
  for (i = 0; i < 22; i++) {
    sprintf(inBuf, "%1.3f ", (float)(in->mgntable[i] / 256.0));
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "  ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end snr_margin_table_ldpc_pre5  ===================================");
}

void diagMoca_log_snr_margin_table_ofdma(
    bool logging,
    struct moca_snr_margin_table_ofdma * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== snr_margin_table_ofdma  ==================================");
  sprintf(outBuf, "  ");
  for (i = 0; i < 22; i++) {
    sprintf(inBuf, "%1.3f ", (float)(in->mgntable[i] / 256.0));
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "  ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end snr_margin_table_ofdma  ==================================");
}

void diagMoca_log_snr_margin_table_rs(
    bool logging,
    struct moca_snr_margin_table_rs * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== snr_margin_table_rs  ==================================");
  sprintf(outBuf, "  ");
  for (i = 0; i < 22; i++) {
    sprintf(inBuf, "%1.3f ", (float)(in->mgntable[i] / 256.0));
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "  ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end snr_margin_table_rs  ==================================");
}

void diagMoca_log_start_ulmo(bool logging, struct moca_start_ulmo * in)
{
  uint32_t i;
  char inBuf[128];
  char outBuf[128];

  DIAGD_LOG_WO_TS(logging, "== start_ulmo  ========================================");
  DIAGD_LOG_WO_TS(logging, "node_id       : %u  ( 0x%x )", in->node_id, in->node_id);

  sprintf(outBuf, "subcarrier[16]: ");
  for (i = 0; i < 16; i++) {
    sprintf(inBuf, "%08x ", in->subcarrier[i]);
    strcat(outBuf, inBuf);
    if (i % 8 == 7) {
      DIAGD_LOG_WO_TS(logging, outBuf);
      sprintf(outBuf, "                ");
    }
  }

  DIAGD_LOG_WO_TS(logging, outBuf);
  DIAGD_LOG_WO_TS(logging, "== end start_ulmo  ====================================");
}
