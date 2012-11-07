/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file is for development testing purpose.
 *
 * Ignore the implementation in this file
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
#ifdef MOD_NAME
 #undef MOD_NAME
#endif
#define MOD_NAME  "diagTests\0"



/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */


int main(void)
{
  int i;


  diagd_Init(NULL);

  do {

    printf("diagMoca_connQltyTbl ----\n");
    for (i = 0; i < MOCA_MAX_NODES; i ++)
      printf("\trefPhyRate[%d] = %u\n", i, diagMoca_connQltyTbl.refPhyRate[i]);
    printf("\n");
    
    printf("diagMocaPerfReferenceTable ----\n");
    for (i = 0; i < DIAG_MOCA_PERF_LVL_MAX; i ++) {
      printf("\trxUcPhyRate_11[%d] = %u\n", i, diagMocaPerfReferenceTable[i].rxUcPhyRate_11);
      printf("\trxUcPhyRate_20[%d] = %u\n", i, diagMocaPerfReferenceTable[i].rxUcPhyRate_20);
      printf("\n");
    }

    printf("thresholds ----\n");
    printf("\tdiagNetThld_pctRxCrcErrs = %u\n", diagNetThld_pctRxCrcErrs);
    printf("\tdiagNetThld_pctRxFrameErrs = %u\n", diagNetThld_pctRxFrameErrs);
    printf("\tdiagNetThld_pctRxLenErrs = %u\n", diagNetThld_pctRxLenErrs);
    printf("\tdiagMocaThld_pctTxDiscardPkts = %u\n", diagMocaThld_pctTxDiscardPkts);
    printf("\tdiagMocaThld_pctRxDiscardPkts = %u\n", diagMocaThld_pctRxDiscardPkts);

    printf("\tdiagNetlink_linkStatCnts = %u\n", diagNetlinkThld_linkCnts);

    printf("wait time ----\n");
    printf("\tdiagWaitTime_getNetStats = %lu\n", (long unsigned int)diagWaitTime_getNetStats);
    printf("\tdiagWaitTime_chkKernMsgs = %lu\n", (long unsigned int)diagWaitTime_chkKernMsgs);
    printf("\tdiagWaitTime_MocaChkErrsu = %lu\n", (long unsigned int)diagWaitTime_MocaChkErrs);
    printf("\tdiagWaitTime_MocaMonPerf = %lu\n", (long unsigned int)diagWaitTime_MocaMonPerf);

  } while (false);
  
  return(0);
}

