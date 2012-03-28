/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics related host command definitions and data structures.
 */

#ifndef _DIAG_HOST_CMD_H_
#define _DIAG_HOST_CMD_H_


// diagd Packet Group Types
#define DIAGD_PKT_CMD    0x80000000   /* diagd command packet (to diagd) */
#define DIAGD_PKT_RSP    0x40000000   /* diagd response packet(from diagd) */

// diagd Packet sub-command types
typedef enum
{
  /* Get log files */
  /* Get monitoring log file */
  DIAGD_SUB_CMD_GET_MON_LOG           = 0x100,

  /* Get diagnostics test result log file */
  DIAGD_SUB_CMD_GET_DIAG_RESULT_LOG   = 0x101,

  /* Run Loopback test */
  DIAGD_SUB_CMD_RUN_TESTS             = 0x201,

  /* Query MoCA PHY Rate of connected nodes */
  DIAGD_SUB_CMD_MOCA_CONNECT_INFO     = 0x300,

  /* Query MoCA log file                    */
  DIAGD_SUB_CMD_GET_MOCA_LOG          = 0x301,

  /* Query self node initial parameters
   * Refer to mocactl show --initparms
   */
  DIAGD_SUB_CMD_MOCA_INITPARMS        = 0x310,

  /* Query node status of self node
   * Equal to  show --status
   */
  DIAGD_SUB_CMD_MOCA_STATUS           = 0x311,

  /* Query node configuration of self node
   * Equal to  show --config
   */
  DIAGD_SUB_CMD_MOCA_CONFIG           = 0x312,

  /* Query node status of connected nodes
   * Equal to mocactl showtbl --nodestatus
   */
  DIAGD_SUB_CMD_MOCA_NODE_STATUS_TBL  = 0x320,

  /* Query node statistics of connected nodes
   * Equal to mocactl showtbl --nodestats
   */
  DIAGD_SUB_CMD_MOCA_NODE_STATS_TBL   = 0x321,

} DIAG_sub_cmd_types_t;

/* = Types ================================================================= */

typedef enum {
  /*
   * The following packets are from remote hosts
   */
  /* Get monitoring log file command */
  DIAGD_REQ_GET_MON_LOG           = DIAGD_PKT_CMD | DIAGD_SUB_CMD_GET_MON_LOG,
  /* Get diagnostic test result log file command */
  DIAGD_REQ_GET_DIAG_RESULT_LOG   = DIAGD_PKT_CMD | DIAGD_SUB_CMD_GET_DIAG_RESULT_LOG,
  /* Run diagnostics/tests */
  DIAGD_REQ_RUN_TESTS             = DIAGD_PKT_CMD | DIAGD_SUB_CMD_RUN_TESTS,

  /* MoCA related requests */
  /* Query MoCA connection information */
  DIAGD_REQ_MOCA_GET_CONN_INFO    = DIAGD_PKT_CMD | DIAGD_SUB_CMD_MOCA_CONNECT_INFO,
  /* Query MoCA log file */
  DIAGD_REQ_MOCA_GET_MOCA_LOG     = DIAGD_PKT_CMD | DIAGD_SUB_CMD_GET_MOCA_LOG,
  /* Query MoCA initial parameters */
  DIAGD_REQ_MOCA_GET_MOCA_INITPARMS   = DIAGD_PKT_CMD | DIAGD_SUB_CMD_MOCA_INITPARMS,
  /* Query status of the MoCA interface (self) */
  DIAGD_REQ_MOCA_GET_STATUS           = DIAGD_PKT_CMD | DIAGD_SUB_CMD_MOCA_STATUS,
  /* Query configuration of the MoCA interface (self) */
  DIAGD_REQ_MOCA_GET_CONFIG           = DIAGD_PKT_CMD | DIAGD_SUB_CMD_MOCA_CONFIG,
  /* Query MoCA node status of connected nodes */
  DIAGD_REQ_MOCA_GET_NODE_STATUS_TBL  = DIAGD_PKT_CMD | DIAGD_SUB_CMD_MOCA_NODE_STATUS_TBL,
  /* Query MoCA node statistics of connected nodes */
  DIAGD_REQ_MOCA_GET_NODE_STATS_TBL  = DIAGD_PKT_CMD | DIAGD_SUB_CMD_MOCA_NODE_STATS_TBL,

} diagd_host_req_types_t;

typedef enum {
  /*
   * The following packets are response to remote hosts
   */
  /* Response of getting monitoring log file */
  DIAGD_RSP_GET_MON_LOG           = DIAGD_PKT_RSP | DIAGD_SUB_CMD_GET_MON_LOG,
  /* Response of getting diagnostic test result log file */
  DIAGD_RSP_GET_DIAG_RESULT_LOG   = DIAGD_PKT_RSP | DIAGD_SUB_CMD_GET_DIAG_RESULT_LOG,
  /* Response of run diagnostics  */
  DIAGD_RSP_RUN_TESTS             = DIAGD_PKT_RSP | DIAGD_SUB_CMD_RUN_TESTS,

  /* MoCA related requests */
  /* Response of MoCA connection information */
  DIAGD_RSP_MOCA_GET_CONN_INFO   = DIAGD_PKT_RSP | DIAGD_SUB_CMD_MOCA_CONNECT_INFO,
  /* Response of getting MoCA log file */
  DIAGD_RSP_MOCA_GET_MOCA_LOG     = DIAGD_PKT_RSP | DIAGD_SUB_CMD_GET_MOCA_LOG,
  /* Response of getting MoCA initial parameters */
  DIAGD_RSP_MOCA_GET_MOCA_INITPARMS   = DIAGD_PKT_RSP | DIAGD_SUB_CMD_MOCA_INITPARMS,
  /* Response of getting status of the MoCA interface (self) */
  DIAGD_RSP_MOCA_GET_STATUS           = DIAGD_PKT_RSP | DIAGD_SUB_CMD_MOCA_STATUS,
  /* Query configuration of the MoCA interface (self) */
  DIAGD_RSP_MOCA_GET_CONFIG           = DIAGD_PKT_RSP | DIAGD_SUB_CMD_MOCA_CONFIG,
  /* Response of getting MoCA node status of connected nodes */
  DIAGD_RSQ_MOCA_GET_NODE_STATUS_TBL  = DIAGD_PKT_RSP | DIAGD_SUB_CMD_MOCA_NODE_STATUS_TBL,
  /* Response of getting MoCA node statistics of connected nodes */
  DIAGD_RSP_MOCA_GET_NODE_STATS_TBL   = DIAGD_PKT_RSP | DIAGD_SUB_CMD_MOCA_NODE_STATS_TBL,

} diagd_host_rsp_types_t;


/** The DIAGD SDU message header. */
typedef unsigned short    diagd_msg_footer;

typedef struct {
  /* refer to DIAGD_MSG_HEAD_MARKER */
  uint32_t  headerMarker;

  /* refer to diagd_host_req_types_t and diagd_host_rsp_types_t */
  uint32_t  msgType;

  /* The length of the diagd message in bytes, not including header. */
  uint32_t  len;

  /* Reserved */
  uint32_t  resv;

} diag_msg_header_t;

#define DIAG_MSG_HDR    sizeof(diag_msg_header_t)


/* TODO 2011/10/16 -
 *    TBD: the packet size
 */
#define DIAGD_MSG_MAX_SDU_SIZE    2048

/* Diagd SDU message header. */
typedef struct {
    diag_msg_header_t   header;

    /* The diagd SDU. */
    char payload[DIAGD_MSG_MAX_SDU_SIZE];

} diag_msg_t;

#endif /* end of _DIAG_HOST_CMD_H_ */
