/*
 * Copyright 2011 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics related functions
 * 1. GENET Loopback test
 *    a) Internal loopback
 *    b) External loopback
 */

#ifndef _DIAG_NETWORK_TESTS_H_
#define _DIAG_NETWORK_TESTS_H_

/* Refer to linux/if_ether.h */
#define ETH_MAC_LEN         ETH_ALEN      /* Octets in one ethernet addr   */
#define ETH_HEADER_LEN      ETH_HLEN      /* Total octets in header.       */

#define ETH_DIAG_FRAME_LEN  1518          /*Header: 14 + User Data: 1500 FCS: 4*/

#define LOOPBACK_PKT_SIZE   1500          /* Loopback packet size          */

#define ETH0                "eth0"        /* Device used for communication*/

#define ETH_P_NULL          0x0           /* we are running without any protocol above the Ethernet Layer*/


/* MoCA NV Parms values & definitions */
typedef enum {
  DIAG_LOOPBACK_TYPE_INTERNAL  = 0,
  DIAG_LOOPBACK_TYPE_EXTERNAL  = 1,
  DIAG_LOOPBACK_TYPE_MAX
} DIAG_LOOPBACK_TYPES;


#define BUF_SIZE  ETH_DIAG_FRAME_LEN
/* Number of loopback packets */
#define NUMBER_OF_LOOPBACK_PACKETS      100000
// #define NUMBER_OF_LOOPBACK_PACKETS      1000
#define MAX_NUMBER_OF_MISSING_RX_PKTS   5


/* Prototypes */
int diagd_Loopback_Test(char *pNetIf_name, uint8_t loopbackType);

#endif /* end of _DIAG_NETWORK_TESTS_H_ */
