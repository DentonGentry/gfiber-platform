/*
 * Copyright 2011 Google Inc. All Rights Reserved.
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
#define DIAG_THLD_LINK_STATE_CNTS_MINS      1
#define DIAG_WAIT_TIME_RUN_CHK_KMSG       \
    (DIAG_THLD_LINK_STATE_CNTS_MINS * DIAG_SECS_PER_MIN)

/* Wait time of monitoring MoCA discard pkts cnts (error counters) */
#define DIAG_MOCA_MON_MON_ERR_CNTS      1
#define DIAG_WAIT_TIME_MOCA_MON_ERR_CNTS  \
    (DIAG_MOCA_MON_MON_ERR_CNTS * DIAG_SECS_PER_MIN)

/* Wait time of monitoring MoCA discard pkts cnts (error counters) */
#define DIAG_MOCA_MON_MON_SERVICE_PERF  1
#define DIAG_WAIT_TIME_MOCA_MON_SERVICE_PERF  \
    (DIAG_MOCA_MON_MON_SERVICE_PERF * DIAG_SECS_PER_MIN)


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

// #define DIAG_THLD_LINK_STATE_CNTS_MIN     20    /* Link stat check per mins */
#define DIAG_THLD_LINK_STATE_CNTS_MIN     5    /* Link stat check per mins */
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



/* MoCA interface statistics counters which query from MoCA core
 * The counter is struct generalStats in MoCA_STATISTICS data structure.
 * Note - Please don't change the following order which must match the
 *        generalStats in MoCA_STATISTICS.
 */
typedef struct _diag_mocaIf_stats_t {
  uint32_t  inUcPkts;         /**< Number of unicast packets sent from this node into the MoCA network */
  uint32_t  inDiscardPktsEcl; /**< Number of packets to be sent into the MoCA network that were dropped at ECL layer */
  uint32_t  inDiscardPktsMac; /**< Number of packets to be sent into the MoCA network that were dropped at MAC layer */
  uint32_t  inUnKnownPkts;    /**< Number of packets sent into the MoCA network destined to an unknown node */
  uint32_t  inMcPkts;         /**< Number of multicast packets sent from this node into the MoCA network */
  uint32_t  inBcPkts;         /**< Number of broadcast packets sent from this node into the MoCA network */
  uint32_t  inOctets_low;     /**< Count of octets sent from this node. Lower 32-bits. Upper 32-bits in inOctets_hi */
  uint32_t  outUcPkts;        /**< Number of unicast packets received by this node out from the MoCA network */
  uint32_t  outDiscardPkts;   /**< Number of packets received by this node out from the MoCA network in error (i.e. CRC) */
  uint32_t  outBcPkts;        /**< Number of broadcast packets received by this node out from the MoCA network */
  uint32_t  outOctets_low;    /**< Count of octets received by this node. Lower 32-bits. Upper 32-bits in outOctets_hi */
  uint32_t  inOctets_hi;      /**< Count of octets sent from this node. Upper 32-bits. */
  uint32_t  outOctets_hi;     /**< Count of octets received by this node. Upper 32-bits. */

  /* The counters in _extendedStats of MoCA_STATISTICS */
  uint32_t  rxMapPkts;        /**< MAP packets received from MoCA network */
  uint32_t  rxRRPkts;         /**< Reservation requests received from MoCA network */
  uint32_t  rxBeacons;        /**< Beacons received from MoCA network */
  uint32_t  rxCtrlPkts;       /**< Link control packets received from MoCA network */

  uint32_t  rxLcAdmReqCrcErr; /**< Number of Admission Requests received with CRC errors. If a node has mis-matched
                                         privacy settings, its admission requests will be counted in this field. */

  /* CRC error counters via MoCACtl2_GetNodeStatisticsExt()*/
  uint32_t  rxMapCrcError;
  uint32_t  rxBeaconCrcError;
  uint32_t  rxRrCrcError;
  uint32_t  rxLcCrcError;
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
 * Declare global data
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
 * Prototypes
 */
bool checkIfTimeout(int diagdApiIdx);
int diag_Get_Netif_Counters(char *pNetif_name, unsigned char bNormalMode);
int Diag_MonNet_GetNetIfStatistics();
void diagd_Rd_Netlink_Msgs();
int Diag_MonMoca_Err_Counts();
int Diag_MonMoca_ServicePerf(void);

#endif /* end of _DIAG_MON_APIS_H_ */
