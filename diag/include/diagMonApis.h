/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics monitor related data structures and definitions.
 */

#ifndef _DIAG_MON_APIS_H_
#define _DIAG_MON_APIS_H_


#define COMMAND_LEN 80
#define DATA_SIZE   30

/*
 * Elapse time of running each Monitoring API
 */
#define DIAG_SECS_PER_MIN         60
/* Wait time of each loop
 * RULE - The minimum time unit is a minute
 */
/* Note -
 *  Need to make sure the DIAG_WAIT_TIME_PER_LOOP is smaller
 *  number than the shortest elapsed time of APIs
 */
#define DIAG_WAIT_TIME_PER_LOOP   5      /* sleep 5 seconds per loop */
/* Wait time of running get network statistics and link status counts */
#define DIAG_WAIT_TIME_GET_NET_STATS_MINS   1
#define DIAG_WAIT_TIME_RUN_GET_NET_STATS  \
    (DIAG_WAIT_TIME_GET_NET_STATS_MINS * DIAG_SECS_PER_MIN)
/* Wait time of running parsing kernel messages (printk) */
#define DIAG_WAIT_TIME_CHECK_KERN_MSGS_MINS 1
#define DIAG_WAIT_TIME_RUN_CHK_KMSG       \
    (DIAG_WAIT_TIME_CHECK_KERN_MSGS_MINS * DIAG_SECS_PER_MIN) 
/* Wait time of monitoring MoCA discard pkts cnts (error counters) */
#define DIAG_MOCA_MON_ERR_CNTS      1
#define DIAG_WAIT_TIME_MOCA_MON_ERR_CNTS  \
    (DIAG_MOCA_MON_ERR_CNTS * DIAG_SECS_PER_MIN)

/* Wait time of monitoring MoCA discard pkts cnts (error counters) */
#define DIAG_MOCA_MON_SERVICE_PERF  1
#define DIAG_WAIT_TIME_MOCA_MON_SERVICE_PERF  \
    (DIAG_MOCA_MON_SERVICE_PERF * DIAG_SECS_PER_MIN)

/* Wait time of monitoring log rotation */
#define DIAG_LOG_MON_ROTATION  15
#define DIAG_WAIT_TIME_LOG_MON_ROTATION  \
    (DIAG_LOG_MON_ROTATION * DIAG_SECS_PER_MIN)


/*
 * Definitions of the threshold of error counters occurred during elapsed time.
 * - If a counter reach/over the threshold, it is possible caused by the
 *   faulty hardware (faulty cable, equipments, or component on bruno)
 *
 * TODO 20111026 - Need to fine tune the thresholds later
 */
/* The threshold of rx CRC error in percentage */
#define DIAG_NET_THLD_PCT_RX_CRC_ERRS     3
/* The threshold of rx Frame error in percentage */
#define DIAG_NET_THLD_PCT_RX_FRAME_ERRS   3
/* The threshold of rx length error in percentage */
#define DIAG_NET_THLD_PCT_RX_LEN_ERRS     3

#define DIAG_THLD_LINK_STATE_CNTS_MIN     5     /* Link stat check per mins */
#define DIAG_THLD_LINK_STATE_CNTS   \
    (DIAG_THLD_LINK_STATE_CNTS_MIN * DIAG_WAIT_TIME_GET_NET_STATS_MINS)

/* Define the error threshold of MoCA interface
 * Tx discard packet threshold = discard Tx pkts / total Tx pkts (UC/MC/BC)
 * Rx discard packet threshold = discard Rx pkts / total Rx pkts (UC/MC/BC)
 */
#define DIAG_THLD_PCT_MOCA_TX_DISCARD_PKTS   3
#define DIAG_THLD_PCT_MOCA_RX_DISCARD_PKTS   3


/* DIAG_CHK_ERR_THLD compares the error counts to the error threshold.
 * The error threshold is (Rx pkts * percentage of the Rx pkts)
 * Cases of errors over threshold after elapsed time -
 * 1) _rxPkt == 0 and _errCnts > 0 occurred
 * 2) _errCnts >= calculated err_thld (including calculated err_thld == 0)
 *
 * _err: true   - If the err count exceeds the threshold
 *       false  - otherwise
 */
#define DIAG_CHK_ERR_THLD(_rxPkt, _errCnts, _pct, _err) \
{ \
  unsigned long err_thld;       \
  _err = false;                 \
  if (_errCnts > 0) {           \
    if (_rxPkt == 0)            \
      _err = true;              \
    else {                      \
      err_thld = (unsigned long)((_rxPkt * _pct)/100);  \
      if (_errCnts >= err_thld) \
        _err = true;            \
    }                           \
  }                             \
}


/* DIAG_GET_UINT32_DELTA - Calculate delta of _curr and _prev */
#define MAX_VALUE_UINT32    0x0FFFFFFFF
#define DIAG_GET_UINT32_DELTA(_curr, _prev, _delta) \
{ \
  if (_curr >= _prev)                               \
    _delta = _curr - _prev;                         \
  else                                              \
    _delta = _curr + (MAX_VALUE_UINT32 - _prev);    \
}


/* Index of hardware monitor APIs - Use in checkIfTimeout() */
typedef enum {
  DIAG_API_IDX_GET_NET_STATS = 0,           /* Diag_MonNet_GetNetIfStatistics() */
  DIAG_API_IDX_GET_CHK_KERN_KMSG = 1,       /* Diag_Mon_ParseExamine_KernMsg()  */
  DIAG_API_IDX_MOCA_MON_ERR_CNTS = 2,       /* Diag_MonMoca_Err_Counts()        */
  DIAG_API_IDX_MOCA_MON_SERVICE_PERF = 3,   /* Diag_MonMoca_ServicePerf()       */
  DIAG_API_IDX_LOG_MON_ROTATION = 4,        /* Diag_MonLog_Rotation()           */
  DIAG_APIS_MAX_VALUE
} diag_api_indexes;



#define MAX_NETIF_NUM   10  /* The maximum support network interfaces */


typedef struct _diag_netif {
  unsigned char nInterfaces;
  char    netif_name[MAX_NETIF_NUM][IF_NAMESIZE];
} netIf_t;


#define NETIF_STATS_NAME_MAX_LEN  30
typedef struct _diag_stats_parms {
  char    netif_name[IF_NAMESIZE];
  char    counter_name[NETIF_STATS_NAME_MAX_LEN];
  unsigned long *pData;
} netIf_counter_t, netif_netlink_t;


/* Network interface statistics counters */
typedef struct _diag_netif_netlink_stats_t {
  /* Rx/Tx statistics counters */
  unsigned long rx_bytes;
  unsigned long rx_packets;
  unsigned long rx_errors;
  unsigned long rx_crc_errors;
  unsigned long rx_frame_errors;
  unsigned long rx_length_errors;
  unsigned long tx_bytes;
  unsigned long tx_packets;
  unsigned long tx_errors;

  /* Link up/down counters */
  unsigned long link_ups;
  unsigned long link_downs;
} diag_netif_stats_t;


/* Network interface link status */
typedef enum {
  DIAG_NETLINK_NONE = 0,        /* not use                    */
  DIAG_NETLINK_DOWN,           /* network interface is down  */
  DIAG_NETLINK_UP,             /* network interface is up    */
  DIAG_NEXLINK_MAX_VALUE
} diag_netlink_state;


/* The number of network statistics EXCLUDES link_up and link_down counter. */
 #define DIAG_NET_CNTS  ((sizeof(diag_netif_stats_t)/sizeof(unsigned long)) - 2)



/* MoCA interface statistics counters which query from
 * moca_get_gen_stats(), moca_get_ext_octet_count()
 * and  moca_get_error_stats()
 */
typedef struct _diag_mocaIf_stats_t {
  /* extract from struct moca_gen_stats */
  uint32_t                ecl_tx_total_pkts;
  uint32_t                ecl_tx_ucast_pkts;
  uint32_t                ecl_tx_bcast_pkts;
  uint32_t                ecl_tx_mcast_pkts;
  uint32_t                ecl_tx_ucast_unknown;
  uint32_t                ecl_tx_mcast_unknown;
  uint32_t                ecl_tx_ucast_drops;
  uint32_t                ecl_tx_mcast_drops;
  uint32_t                ecl_tx_buff_drop_pkts;
  uint32_t                ecl_rx_total_pkts;
  uint32_t                ecl_rx_ucast_pkts;
  uint32_t                ecl_rx_bcast_pkts;
  uint32_t                ecl_rx_mcast_pkts;
  uint32_t                ecl_rx_ucast_drops;
  uint32_t                mac_tx_low_drop_pkts;
  uint32_t                mac_rx_buff_drop_pkts;
  uint32_t                rx_beacons;
  uint32_t                rx_map_packets;
  uint32_t                rx_rr_packets;
  uint32_t                rx_control_uc_packets;
  uint32_t                rx_control_bc_packets;

  /* extract from struct moca_ext_octet_count */
  uint32_t                in_octets_hi;
  uint32_t                in_octets_lo;
  uint32_t                out_octets_hi;
  uint32_t                out_octets_lo;

  /* extract from struct moca_error_stats */
  uint32_t                rx_uc_crc_error;
  uint32_t                rx_bc_crc_error;
  uint32_t                rx_map_crc_error;
  uint32_t                rx_beacon_crc_error;
  uint32_t                rx_rr_crc_error;
  uint32_t                rx_lc_uc_crc_error;
  uint32_t                rx_lc_bc_crc_error;
} diag_mocaIf_stats_t;



/* Network interface related data structure */
typedef struct _diag_netif_info {
  unsigned char   inUse;                  /* 1 - data is valid in the database. */
  char            name[IF_NAMESIZE];
  unsigned char   active_stats_idx;
  /* Double-buffer of net interface statistics for comparing with prev counters */
  diag_netif_stats_t    statistics[2];

  /* Update their delta of statistics of statistics[] when interval timed out. */
  diag_netif_stats_t    delta_stats;

  /* netlink relate varliable */
  unsigned char   netlink_state;          /* refer to diag_netlink_state */

  bool            check_crc_errs;         /* true - check CRC counts */
} diag_netIf_info_t;


/* MoCa interface related data structure */
typedef struct _diag_mocaIf_info {

  unsigned char         active_stats_idx;

  /* Double-buffer of MoCA interface statistics for comparing with prev counters */
  diag_mocaIf_stats_t   statistics[2];

  /* Update their delta of statistics of statistics[] when interval timed out. */
  diag_mocaIf_stats_t   delta_stats;

} diag_mocaIf_info;


/*
 * Main diagnostics database.
 * NOTE - 20110914
 *  Expand this database during development.
 */
typedef struct _diag_info_blk {
  /* socket related fields used in diagd_Cmd_Handler
   * DIAG_SOCKET_NOT_OPEN: socket is not created (or fail to create),
   *                       otherwise: OK
   */
  int             hostCmdSock;        /* socket descriptor */
  int             hostCmdDesc;        /* file descriptor for socket */
  uint8_t        *hostReqData;        /* Point of host request data buffer */

  /* Socket to get link status
   * DIAG_SOCKET_NOT_OPEN: socket is not created (or fail to create),
   *                       otherwise: OK
   */
  int             netlinkSock;

  /* number of network interface detected */
  unsigned char   nNetIfs;
  /* network interface statistics and link states */
  diag_netIf_info_t netifs[MAX_NETIF_NUM];

  diag_mocaIf_info  mocaIf;

} diag_info_t;


/*
 * Declare diagMonApis related global variables
 */

/*
 * Timestamp of starting time of a hardware monitoring APis
 */
/* Start time of Diag_MonNet_GetNetIfStatistics() */
extern bool   diag_getStats_firstRun;
extern time_t diagStartTm_getStats;

/* Start time of Diag_Mon_ParseExamine_KernMsg() */
extern bool   diag_chkKernMsg_firstRun;
extern time_t diagStartTm_chkKernMsg;

extern pthread_mutex_t lock;

/*
 * Monitoring thresholds
 */
/* net statistics releated thresholds */
extern uint32_t  diagNetThld_pctRxCrcErrs;
extern uint32_t  diagNetThld_pctRxFrameErrs;
extern uint32_t  diagNetThld_pctRxLenErrs;

/* MoCA related thresholds */
extern uint32_t  diagMocaThld_pctTxDiscardPkts;
extern uint32_t  diagMocaThld_pctRxDiscardPkts;

/* Net interface link up/down cnts */
extern uint32_t  diagNetlinkThld_linkCnts;

/*
 * Monitoring intervals (wait times in seconds)
 * */
/*  Wait time of getting net statistices (include net link counts) */
extern time_t  diagWaitTime_getNetStats;
/* Wait time of checking kernel messages */
extern time_t  diagWaitTime_chkKernMsgs;
/* Wait time of monitoring MoCA error counts via mocad */
extern time_t  diagWaitTime_MocaChkErrs;
/* Wait time of monitoring MoCA performance */
extern time_t  diagWaitTime_MocaMonPerf;
/* Wait time of monitoring Diag log rotation */
extern time_t  diagWaitTime_LogMonRotate;


/*
 * Prototypes
 */
bool checkIfTimeout(int diagdApiIdx);
int Diag_MonNet_GetNetIfStatistics();
void diagd_Rd_Netlink_Msgs();
int Diag_MonMoca_Err_Counts();
int Diag_MonMoca_ServicePerf(void);
int Diag_MonLog_Rotate(void);

#endif /* end of _DIAG_MON_APIS_H_ */
