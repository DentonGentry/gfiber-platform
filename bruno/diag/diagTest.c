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
  int i, j;


  diagd_Init();

  do {

    printf("diagMoca_connQltyTbl ----\n");
    for (i = 0; i < MoCA_MAX_NODES; i ++)
      printf("\trefPhyRate[%d] = %u\n", i, diagMoca_connQltyTbl.refPhyRate[i]);
    printf("\n");
    
    printf("diagMocaPerfReferenceTable ----\n");
    for (i = 0; i < DIAG_MOCA_PERF_LVL_MAX; i ++) {
      printf("\trxUcPhyRate[%d] = %u\n", i, diagMocaPerfReferenceTable[i].rxUcPhyRate);
      printf("\trxUcPhyRate[%d] = %.1f\n", i, diagMocaPerfReferenceTable[i].rxUcGain);
      printf("\trxUcPhyRate[%d] = %.1f\n", i, diagMocaPerfReferenceTable[i].rxUcAvgSnr);
      for (j = 0; j < BIT_LOADING_LEN; j ++) {
        printf("\t\trxUcBitLoading[%d] = 0x%08x\n", j, diagMocaPerfReferenceTable[i].rxUcBitLoading[j]);
      }
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

