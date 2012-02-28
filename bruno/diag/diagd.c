/*
 * Copyright 2011 Google Inc. All Rights Reserved.
 *
 * This file is the main diagd routines
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
 * Global variables
 *
 *--------------------------------------------------------------------------
 */

/* -------------------------------------------------------------------------
 *
 * Internal defines, types and variables
 *
 *--------------------------------------------------------------------------
 */
/* It is a temp definition. Define to enable the diagd_Cmd_Handler thread */
#if 1
 #define DIAGD_ENABLE_DIAG_THREAD
#endif

#if 1
 #define DIAGD_ENABLE_NETLINK_TRHREAD
#endif


/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */

/*
 * This routine is processing linkup/linkdown monitoring handler.
 *
 * TODO??? - 20111027
 *  This routine might be moved to the main hardware monitoring handler later.
 *  Now this is running in a thread.
 */
void *diagd_NetlinkProcess_Handler()
{
  while(true) {

    diagd_Rd_Netlink_Msgs();

  } /* end of while */

} /* end of diagd_NetlinkProcess_Handler */



/*
 * Process hardware monitoring
 */
void diagd_HwMon_Handler()
{
  int diag_counter = 0;
  int looping = 1;

  /*
   * TODO 20110919 -
   * Per discussion on 20110919, several enhancements are preferred -
   *  1) Each monitoring API has different poll period.
   *  2) Add throughput of each net interface
   */

  while(looping) {

    pthread_mutex_lock(&lock);

    /* get statistics of each network interface */
    if (Diag_MonNet_GetNetIfStatistics() != DIAGD_RC_OK) {
      looping = 0;    /* Failed. Exit */
    }

    if (Diag_Mon_ParseExamine_KernMsg() != DIAGD_RC_OK) {
      looping = 0;    /* Failed. Exit */
    }

    /* Monitor MoCA Tx/Rx discard packets counts */
//    Diag_MonMoca_Err_Counts();

    /* Monitor MoCA performance to each connected node in MoCA network */
    Diag_MonMoca_ServicePerf();

    DIAGD_TRACE("%s: Loop counts - %d", __func__, ++diag_counter );

    pthread_mutex_unlock(&lock);

    /* Sleep for DIAG_HW_MONITORING_INTERVAL */

    /*
     *  If needed, call the diag_rd_netlink_msg() in a thread.
     */
    sleep(DIAG_WAIT_TIME_PER_LOOP);

  }

  diagtCloseEventLogFile();

} /* diagd_HwMon_Handler */



int main()
{
  pthread_t tdiagHandler, tnetlinkMonHandler, tnetlink;

  diagd_Init();

#ifdef DIAGD_ENABLE_DIAG_THREAD
  pthread_create(&tdiagHandler, NULL, (void *)diagd_Cmd_Handler, NULL);
#endif
  pthread_create(&tnetlinkMonHandler, NULL, (void *)diagd_HwMon_Handler, NULL);

#ifdef DIAGD_ENABLE_NETLINK_TRHREAD
  pthread_create(&tnetlink, NULL, diagd_NetlinkProcess_Handler, NULL);
#endif

#ifdef DIAGD_ENABLE_DIAG_THREAD
  pthread_join(tdiagHandler, NULL);
#endif

  pthread_join(tnetlinkMonHandler, NULL);

#ifdef DIAGD_ENABLE_NETLINK_TRHREAD
  pthread_join(tnetlink, NULL);
#endif

  return (0);
}
