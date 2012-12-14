/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides MoCA diagnostics related data structures and definitions.
 */

#ifndef _DIAG_MOCA_H_
#define _DIAG_MOCA_H_

#define NO_OF_SECS_IN_MIN        60
#define NO_OF_MINS_IN_HR         60


/* */
#define DIAG_MOCA_NODE_NOT_ACTIVE       0
#define DIAG_MOCA_NODE_ACTIVE           1

#define DIAG_MOCA_NODE_SELF_NODE        (1 << 15)     /* indicate self node */


#define GET_LOCAL_TIME(_ptm) \
{  \
  time_t  currtime;   \
  time(&currtime);    \
  _ptm = localtime(&currtime); \
}


/* MoCA active node ID table */
typedef struct __attribute__((packed,aligned(2))) _diag_moca_node_mac_t {

  uint16_t    active;       /* 1: active; 0: inactive */

  macaddr_t   macAddr;

} diag_moca_node_mac_t;


/* MoCA active node ID table */
typedef struct _diag_moca_node_mac_table_t {

  uint16_t              selfNodeId;       /* Self Node ID */
  uint16_t              connected_nodes;
  diag_moca_node_mac_t  nodemacs[MOCA_MAX_NODES];

} diag_moca_node_mac_table_t;


/* Performance levels which are used in diagMoca_GetConnInfo() */
typedef enum _diag_moca_conn_quality_index {
  DIAG_MOCA_CONN_QLTY_EXC      = 0,
  DIAG_MOCA_CONN_QLTY_GOOD     = 1,
  DIAG_MOCA_CONN_QLTY_IMPAIRED = 2,
  DIAG_MOCA_CONN_QLTY_UNUSABLE = 3,
  DIAG_MOCA_CONN_QLTY_NOT_CONN = 4, /* not in MoCA network */
  DIAG_MOCA_CONN_QLTY_MAX
} diag_moca_conn_quality_index;


/* Moca connection quality reference table
 * The index is number of (connected nodes - 1).
 */
typedef struct _diag_moca_connt_qlty_ref_t {
  uint32_t    refPhyRate[MOCA_MAX_NODES];
} diag_moca_connt_qlty_ref_t;


/* Performance levels which are used in diagMoca_MonServicePerf() */
typedef enum _diag_moca_ref_index {
  DIAG_MOCA_PERF_LVL_GOOD     = 0,
  DIAG_MOCA_PERF_LVL_POOR     = 1,
  DIAG_MOCA_PERF_LVL_MAX
} diag_moca_ref_index;

#define DIAG_MOCA_PERF_LVL_UNUSABLE   DIAG_MOCA_PERF_LVL_MAX


#define BIT_LOADING_LEN  ((MoCA_MAX_SUB_CARRIERS * MAX_BITS_PER_SUB_CARRIER)/(BITS_PER_BYTE*BYTES_PER_WORD))

/*
 * The structure is for moca performance reference table per
 * node connection status. Based on
 * 1) rxUcPhyRate
 * 2) rxUcAvgSnr - average SNR
 * 3) rxUcPower  - Rx Power level
 * 4) rxUcBitLoading
 */
typedef struct _diag_moca_ref_tbl_t {
  /* rxuc phy rate */
  /* Per HW engineer, checking rxUc PHY rate is sufficient */
  uint32_t        rxUcPhyRate_11;  /* for moca 1.1 */
  uint32_t        rxUcPhyRate_20;  /* for moca 2.0 */
} diag_moca_ref_tbl_t;



/*
 * The structure contains the service performance monitoring results
 * Refer to diag_moca_ref_index for the performance results.
 */
typedef struct _diag_moca_perf_status_entry_t {
  uint8_t   valid;
  uint8_t   nodeId;
  uint8_t   rxUcPhyRate;
  uint8_t   rxUcPower;
  uint8_t   rxUcAvgSnr;
  uint8_t   rxUcBitLoading;
  uint8_t   rsv[2];         /* make 8-bytes alignment */
} diag_moca_perf_status_entry_t;

typedef struct  __attribute__((packed,aligned(4))) _diag_moca_config_parms_t {
  int32_t   arplTh50;
  int32_t   arplTh100;
  uint32_t  assertText;
  uint32_t  assertRestart;
  uint32_t  cirPrints;
  uint32_t  continuousIeMapInsert;
  uint32_t  continuousIeRrInsert;
  uint32_t  dontStartMoca;
  uint32_t  enCapable;
  uint32_t  extraRxPktsPerQm;
  uint32_t  fragmentation;
  uint32_t  freqShift;
  uint32_t  labSnrGraphSet;
  uint32_t  lofUpdate;
  uint32_t  loopbackEn;
  uint32_t  m1TxPwrVariation;
  uint32_t  maxFrameSize;
  uint32_t  maxMapCycle;
  uint32_t  maxPktAggr;
  uint32_t  maxTxTime;
  uint32_t  minBwAlarmThreshold;
  uint32_t  minMapCycle;
  uint32_t  coreTraceEn;
  uint32_t  nbasCappingEn;
  uint32_t  oooLmoThreshold;
  uint32_t  orrEn;
  uint32_t  perMode;
  uint32_t  pmkExchInterval;
  uint32_t  pwrState;
  uint32_t  pssEn;
  uint32_t  res1;
  uint32_t  res2;
  uint32_t  res3;
  uint32_t  res4;
  uint32_t  res5;
  uint32_t  res6;
  uint32_t  res7;
  uint32_t  res8;
  uint32_t  res9;
  int32_t   rxPwrTuning;
  uint32_t  rxTxPktsPerQm;
  uint32_t  sapmEn;
  uint32_t  snrPrints;
  uint32_t  targetPhyRate20;
  uint32_t  targetPhyRate20Turbo;
  uint32_t  targetPhyRateQam128;
  uint32_t  targetPhyRateQam256;
  uint32_t  tekExchInterval;
  uint32_t  verbose;
  uint32_t  wdogEn;
  struct moca_password pwd;
  struct moca_priority_allocations priAlloc;
  struct moca_rlapm_table_100 rlampTbl100;
  struct moca_rlapm_table_50 rlampTbl50;
  struct moca_sapm_table_100 sapmTbl100;
  struct moca_sapm_table_50 sapmTbl50;
  struct moca_snr_margin_ldpc snrMarginLdpc;
  struct moca_snr_margin_ldpc_pre5 snrMarginLdpcPre5;
  struct moca_snr_margin_ofdma snrMarginOfdma;
  struct moca_snr_margin_rs snrMarginRs;
  struct moca_snr_margin_table_ldpc snrMarginTblLdpc;
  struct moca_snr_margin_table_ldpc_pre5 snrMarginTblLdpcPre5;
  struct moca_snr_margin_table_ofdma snrMarginTblOfdma;
  struct moca_snr_margin_table_rs snrMarginTblRs;
  struct moca_start_ulmo startUlmo;

} diag_moca_config_parms_t;

typedef struct _diag_moca_config_t {
  uint32_t           rfBand;
  diag_moca_config_parms_t Cfg;
} diag_moca_config_t;


typedef struct __attribute__((packed,aligned(4))) _diag_moca_status_t {
  uint32_t nodeId;
  uint32_t singleChOp;  /* single channel operation indication */
  uint32_t txGcdPowerReduction; /*tx_gcd_power_reduction */
  uint32_t ledStatus;
  uint32_t pqosEgressNumFlows;
  struct   moca_node_status ns;
  struct   moca_fw_version fw;
  struct   moca_interface_status intf;
  struct   moca_network_status net;
  struct   moca_current_keys key;
  struct   moca_key_times keyTimes;
  struct   moca_mac_addr macAddr;
  struct   moca_drv_info drv;
} diag_moca_status_t;

/* 10/12/12 Note:
 *       Expand diag_moca_nodeprofile_t struture to add more profiles if
 *       HW engineer requests for them.
 */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_nodeprofile_t {
  uint32_t                        type;  /* profile type */
  struct moca_gen_node_ext_status rxUc;  /* RX_UC_NPER for MoCA20 */
} diag_moca_nodeprofile_t;

typedef struct __attribute__((packed,aligned(4))) _diag_moca_nodestatus_entry_t {

  uint32_t                          nodeId;
  struct moca_gen_node_status       gns;
  diag_moca_nodeprofile_t           profile;
} diag_moca_nodestatus_entry_t;

typedef struct __attribute__((packed,aligned(4))) _diag_moca_nodestatus_t {

  uint32_t                          nodeStatusTblSize;
  diag_moca_nodestatus_entry_t      nodeStatus[MOCA_MAX_NODES];
} diag_moca_nodestatus_t;

typedef struct __attribute__((packed,aligned(4))) _diag_moca_stats_t {
  struct moca_gen_stats                 genStats;
  struct moca_ext_octet_count           extOctCnt;
  struct moca_error_stats               totalExtStats;
} diag_moca_stats_t;



#define MAC_ADDR_LEN  6
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_phy_info_t {
  uint32_t              rxUcPhyRate;      /* bps */
  uint16_t              cp;               /* Cyclic prefix length */
  uint16_t              connQuality;      /* TBD- Per rxUcPhyRate, rank it's connection quality */
} diag_moca_node_phy_info_t;


/* TODO 10/31/2012: diag_moca_node_info_t originally in MoCA1.1 was used when diagMoca_FmrInitCb()
 * is called in diagMoc_getConnInfo(). diagMoca_getConnInfo() is currently emptied out.
 * We can re-visit this one when rewrite diagMoca_GetConnInfo().
 */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_info_t {
  uint32_t        txNodeId;   /* Tx node ID */
  macaddr_t       macAddr;    /* Tx node's MAC address */
  diag_moca_node_phy_info_t   rxNodePhyInfo[MOCA_MAX_NODES];
} diag_moca_node_info_t;



#define DIAG_MOCA_INVALID_NODE_ID   0xFF
/* MoCA node statistics information table - as mocactl showtbl --nodestats */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_stats_entry_t {

  uint32_t      nodeId;
  macaddr_t     macAddr;        /* Node's MAC address */
  struct moca_node_stats      nodeStats;
  struct moca_node_stats_ext  nodeStatsExt;

} diag_moca_node_stats_entry_t;



/*
 * MoCA API related data structures
 */

/* MoCA node connection information */
/* NOTE - Per BRCM MoCA data structure, BRCM only monitor upto 9
 * responsded (tx) nodes in the current moca code
 */
#define MAX_RSP_NODES       16
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_connect_info_t {

  /* Self node ID */
  uint32_t              selfNodeId;

  /* Total size of diag_moca_node_info_t table
   * The value will be a whole multiple of sizeof(diag_moca_node_info_t)
   */
  uint32_t              nodeInfoTblSize;
  diag_moca_node_info_t nodeInfo[MAX_RSP_NODES];

} diag_moca_node_connect_info_t;


/* MoCA node statistics information table - as mocactl showtbl --nodestats */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_statistics_table_t {

  /* Total size of diag_moca_node_stats_entry_t table
   * The value will be a whole multiple of sizeof(diag_moca_node_stats_entry_t)
   */
  uint32_t                      nodeStatsTblSize;
  diag_moca_node_stats_entry_t  Stats;

} diag_moca_node_stats_table_t;



/* The logged message types which are located in diag_moca_log_msg_hdr_t */
typedef enum _diag_moca_log_msgs {

  DIAG_MOCA_LOG_NONE = 0x0,
  DIAG_MOCA_LOG_EXCESSIVE_TX_DISCARD_PKTS = 0x1,
  DIAG_MOCA_LOG_EXCESSIVE_RX_DISCARD_PKTS = 0x2,
  DIAG_MOCA_LOG_EXCESSIVE_TX_RX_DISCARD_PKTS = 0x3,
  DIAG_MOCA_LOG_POOR_PHY_RATE = 0x10,

} diag_moca_log_msgs;


/* Header of MoCA log message in DIAGD_MOCA_LOG_FILE
 * - DIAG_MOCA_LOG_EXCESSIVE_TX_DISCARD_PKTS
 * - DIAG_MOCA_LOG_EXCESSIVE_RX_DISCARD_PKTS
 * - DIAG_MOCA_LOG_EXCESSIVE_TX_RX_DISCARD_PKTS
 */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_log_msg_hdr_t {

  uint16_t   msgType;    /* refer to diag_moca_log_msgs */

  uint16_t   msgLen;     /* Total msg size exclude header */

  struct tm  *currTime;  /* Time stamps (local time) */

} diag_moca_log_msg_hdr_t;


/* Header of MoCA log message in DIAGD_MOCA_LOG_FILE */
typedef struct __attribute__((packed,aligned(4))) _diag_mocalog_discardpkts_exceed_t {

  diag_moca_log_msg_hdr_t   msgHdr;

  diag_mocaIf_stats_t   prevStats;
  diag_mocaIf_stats_t   currStats;
  diag_moca_node_stats_table_t  nodeStats;

} diag_mocalog_discardpkts_exceed_t;

#define DIAG_MOCA_LOG_MAX_SIZE_DISCARDPKTS_INFO   \
    (sizeof(diag_mocalog_discardpkts_exceed_t) +  \
    (sizeof(diag_moca_node_stats_entry_t) * 15))

/*  size of nodeStatsTblSize + 15 * size of diag_moca_node_stats_entry_t */
#define DIAG_MOCA_MAX_NODE_STATS_SIZE \
    (sizeof(uint32_t ) +  \
    (sizeof(diag_moca_node_stats_entry_t) * 15))


/* Header of MoCA log message in DIAGD_MOCA_LOG_FILE
 * - DIAG_MOCA_LOG_POOR_PHY_RATE
 */
typedef struct _diag_moca_perf_status_t {

  diag_moca_log_msg_hdr_t       msgHdr;

  uint8_t                       noConnectedNodes;
  diag_moca_perf_status_entry_t  perfResult[MOCA_MAX_NODES];
  diag_moca_nodestatus_t        nodeStatus;
} diag_moca_perf_status_t;

/* TODO 10/31/2012: Put init paramters in MoCA 1.1 here  but
 * later on need to revisit init paramters for MoCA 2.0
 */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_init_parms_t {
  uint32_t         bandwidth;
  uint32_t         beaconChannel;
  uint32_t         beaconPwrReduction;
  uint32_t         beaconPwrReductionEn;
  uint32_t         boMode;
  uint32_t         constRxSubmode;
  uint32_t         continuousPwrTxMode;
  int32_t          continuousRxModeAttn;
  uint32_t          deviceClass;
  uint32_t         egrMcFilterEn;
  uint32_t         flowControlEn;
  uint32_t         freqMask;
  uint32_t         init1;
  uint32_t         init2;
  uint32_t         init3;
  uint32_t         init4;
  uint32_t         init5;
  uint32_t         init6;
  uint32_t         init7;
  uint32_t         init8;
  uint32_t         init9;
  uint32_t         labMode;
  uint32_t         ledSettings;
  uint32_t         lastOperFreq;
  uint32_t         lowPriQNum;
  int32_t          maxTxPower;
  uint32_t         mtmEn;
  uint32_t         mcastMode;
  uint32_t         ncMode;
  uint32_t         ofdmaEn;
  uint32_t         otfEn;
  uint32_t         pnsFreqMask;
  uint32_t         preferedNC;
  int32_t          primChOffset; /* primary_ch_offset */
  uint32_t         privacyEn;
  uint32_t         qam256Capability;
  uint32_t         tabooFixedMaskStart;
  uint32_t         tabooFixedChannelMask;
  uint32_t         tabooLeftMask;
  uint32_t         tabooRightMask;
  uint32_t         txPwrControlEn;
  uint32_t         turboEn;
  uint32_t         rfBand;
  uint32_t         singleChOp; /* single_channel_operation */

  struct moca_aes_mm_key aesMmKey;
  struct moca_aes_pm_key aesPmKey;
  struct moca_const_tx_params constTxParams;
  struct moca_mac_addr macAddr;
  struct moca_mmk_key mmkKey;
  struct moca_pmk_initial_key pmkInitKey;

} diag_moca_init_parms_t;

/*
 * Declare diagMoca related global variables
 */

/* Reference PHY rates of connection quality per number of connected nodes */
extern diag_moca_connt_qlty_ref_t diagMoca_connQltyTbl;

/*
 * A reference table of MoCA node service performance.
 * 1) The current data in the table is temp data.
 * 2) HW engineer provides measure data which is read from "diag_ref_data.txt"
 *    in during init time 
 */
extern diag_moca_ref_tbl_t diagMocaPerfReferenceTable[DIAG_MOCA_PERF_LVL_MAX];


/*
 * Prototypes
 */
int diagMoca_MonErrorCounts(void);
int diagMoca_MonServicePerf(void);
int diagMoca_GetInitParms(diag_moca_init_parms_t *pInitParms);
int diagMoca_GetNodeStatus(
    diag_moca_nodestatus_t *pNodeStatus, uint32_t *pBufLen);
int diagMoca_GetNodeStatistics(
      diag_moca_node_stats_table_t *pNodeStats,
      uint16_t *pSize);
int diagMoca_GetConnInfo(diag_moca_node_connect_info_t *pConnInfo);
int diagd_MoCA_Init();
void diagMoca_ConvertUpTime(uint32_t timeInSecs, uint32_t *pTimeInHrs,
                            uint32_t *pTimeInMin, uint32_t *pTimeInSecs);
int diagMoca_GetStatus(diag_moca_status_t *pStatus);
int diagMoca_GetStatistics(diag_moca_stats_t *pMocaStats);
void diagMoca_CopyStats(diag_mocaIf_stats_t *mocaIf, diag_moca_stats_t *stats);

#endif /* end of _DIAG_MOCA_H_ */
