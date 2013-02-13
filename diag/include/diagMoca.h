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
  MAC_ADDRESS macAddr;

} diag_moca_node_mac_t;

/* MoCA active node ID table */
typedef struct _diag_moca_node_mac_table_t {

  uint16_t              selfNodeId;       /* Self Node ID */
  uint16_t              connected_nodes;
  diag_moca_node_mac_t  nodemacs[MoCA_MAX_NODES];

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
  uint32_t    refPhyRate[MoCA_MAX_NODES];
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
 * 3) rxUcGain - Rx Power level
 * 4) rxUcBitLoading
 */
typedef struct _diag_moca_ref_tbl_t {

  /* rxuc phy rate
   * - Compare to pNodeStatus->maxPhyRates.rxUcPhyRate
   */
  UINT32        rxUcPhyRate;

  /* rxuc rx power level
   * - Compare to pNodeStatus->rxUc.rxGain/4.0
   */
  float         rxUcGain;

  /* rxuc average SNR
   * - Compare to pNodeStatus->rxBc.avgSnr/2.0
   */
  float         rxUcAvgSnr;

  /* rxuc bit-loading
   * - Compare to pNodeStatus->rxUc.bitLoading
   */
  UINT32        rxUcBitLoading[BIT_LOADING_LEN];

} diag_moca_ref_tbl_t;



/*
 * The structure contains the service performance monitoring results
 * Refer to diag_moca_ref_index for the performance results.
 */
typedef struct _diag_moca_perf_status_entry_t {
  uint8_t   valid;
  uint8_t   nodeId;
  uint8_t   rxUcPhyRate;
  uint8_t   rxUcGain;
  uint8_t   rxUcAvgSnr;
  uint8_t   rxUcBitLoading;
  uint8_t   rsv[2];         /* make 8-bytes alignment */
} diag_moca_ref_status_entry_t;


typedef struct _diag_moca_config_t {
  uint32_t           rfType;
  MoCA_CONFIG_PARAMS Cfg;
} diag_moca_config_t;


/* */
typedef struct _diag_moca_nodestatus_t {

  MoCA_NODE_COMMON_STATUS_ENTRY nodeCommonStatus ;
   /* The value will be a a whole multiple of sizeof(MoCA_NODE_STATUS_ENTRY) */
  uint32_t                nodeStatusTblSize ;
  MoCA_NODE_STATUS_ENTRY  nodeStatus[MoCA_MAX_NODES];

} diag_moca_nodestatus_t;


typedef struct _diag_moca_stats_t {
  MoCA_STATISTICS   stats;
  MoCA_NODE_STATISTICS_EXT_ENTRY totalExtStats;
} diag_moca_stats_t;


#define MAC_ADDR_LEN  6
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_phy_info_t {
  uint32_t              rxUcPhyRate;      /* bps */
  uint16_t              cp;               /* Cyclic prefix length */
  uint16_t              connQuality;      /* TBD- Per rxUcPhyRate, rank it's connection quality */
} diag_moca_node_phy_info_t;


/*
 * txNodeId -         The tx node ID sends to rxNodePhyInfo[] nodes
 *                    up to MoCA_MAX_NODES
 * rxNodePhyInfo[] -  The information in each entry is
 *                    PHY rate and CP of a corresponding rx node ID.
 * For Example -
 * 1) txNodeId = 1     - Tx node ID is 1
 * 2) rxNodePhyInfo[3] - Rx Node ID is 3
 *      rxUcPhyRate    - Rx PHY rate of node 3 from node 1
 *      cp             - Rx CP of node 3 and node 1
 */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_info_t {
  uint32_t        txNodeId;   /* Tx node ID */
  MAC_ADDRESS     macAddr;    /* Tx node's MAC address */
  diag_moca_node_phy_info_t   rxNodePhyInfo[MoCA_MAX_NODES];
} diag_moca_node_info_t;


#define DIAG_MOCA_INVALID_NODE_ID   0xFF
/* MoCA node statistics information table - as mocactl showtbl --nodestats */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_stats_entry_t {

  /* Node ID is also located in MoCA_NODE_STATISTICS_ENTRY */
  uint32_t      nodeId;
  MAC_ADDRESS   macAddr;        /* Node's MAC address */
  MoCA_NODE_STATISTICS_ENTRY      nodeStats;
  MoCA_NODE_STATISTICS_EXT_ENTRY  nodeStatsExt;

} diag_moca_node_stats_entry_t;


/*
 * MoCA API related data structures
 */

/* MoCA node connection information */
/* NOTE - Per BRCM MoCA data structure, BRCM only monitor upto 9
 * responsded (tx) nodes in the current moca code
 */
#define MAX_RSP_NODES       9
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


/* MoCA node status table */
typedef struct __attribute__((packed,aligned(4))) _diag_moca_node_status_table_t {

   MoCA_NODE_COMMON_STATUS_ENTRY commonStatusEntry;
   /* Total size of statusEntry table.
    * The value will be a whole multiple of sizeof(MoCA_NODE_STATUS_ENTRY)
    */
   uint32_t                      nodeStatusTblSize;
   /* NodeStatusEntry table */
   MoCA_NODE_STATUS_ENTRY        statusEntry;

} diag_moca_node_status_table_t;


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
  diag_moca_ref_status_entry_t  perfResult[MoCA_MAX_NODES];
  diag_moca_nodestatus_t        nodeStatus;
} diag_moca_perf_status_t;



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
int diagMoca_GetInitParms(PMoCA_INITIALIZATION_PARMS pInitParms);
int diagMoca_GetStatus(PMoCA_STATUS pStatus);
int diagMoca_GetNodeStatus(
    diag_moca_nodestatus_t *pNodeStatus, uint32_t *pBufLen);
int diagMoca_GetNodeStatistics(
      diag_moca_node_stats_table_t *pNodeStats,
      uint16_t *pSize);
int diagMoca_GetConnInfo(diag_moca_node_connect_info_t *pConnInfo);
int diagd_MoCA_Init();
void diagMoca_ConvertUpTime(uint32_t timeInSecs, uint32_t *pTimeInHrs,
                            uint32_t *pTimeInMin, uint32_t *pTimeInSecs);

#endif /* end of _DIAG_MOCA_H_ */
