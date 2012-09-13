/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
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

/* Monitoring logging
 * Log information in string format
 *
 * In Monitoring log file - Log all of monitoring events (includes MoCA)
 */
FILE  *logFp = NULL;

/* Test Results logging
 * Log information in string format
 */
FILE  *testResultsFp = NULL;

/* MoCA logging file
 * Log information in binary format.
 * This log file logs only MoCA events
 *
 * For each message formats, refer to the diagmoca.h
 */
FILE  *mocaLogFp = NULL;


/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */

/*
 * Open the diagd test result log file
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagtOpenTestResultsLogFile(void)
{
  int       rtn = DIAGD_RC_ERR;

#ifdef DIAG_TEST_RESULT_LOGGING_ON

  DIR      *dir = NULL;
//  uint32_t  regData;


  do {

    DIAGD_TRACE("%s: check if dir of"DIAGD_LOG_DIR" exist", __func__);

    /* Check if the log directory exist. */
    if ((dir = opendir(DIAGD_LOG_DIR)) == NULL) {

      DIAGD_TRACE("%s: "DIAGD_LOG_DIR" doesn't exist. Create it.", __func__);
      system("mkdir "DIAGD_LOG_DIR);
    }
    else {

      /* The directory exists. */
      closedir(dir);
    }

    DIAGD_TRACE("%s: open "DIAGD_TEST_RESULTS_FILE, __func__);

    testResultsFp = fopen(DIAGD_TEST_RESULTS_FILE, "a");
    if (testResultsFp == NULL) {
      rtn = DIAGD_RC_FAILED_OPEN_LOG_FILE;
      DIAGD_ERROR("%s: Failed to open "DIAGD_LOG_FILE, __func__);
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

#else
  rtn = DIAGD_RC_OK;
#endif /* end of DIAG_TEST_RESULT_LOGGING_ON */

  return (rtn);
} /* end of diagtOpenTestResultsLogFile */


void diagtCloseTestResultsLogFile(void)
{
  if (testResultsFp != NULL) {
    fclose(testResultsFp);
    testResultsFp = NULL;
  }
} /* end of diagtCloseTestResultsLogFile */


/*
 * Open the diagd monitoring log file
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagtOpenEventLogFile(void)
{
  int       rtn = DIAGD_RC_ERR;

#ifdef DIAGD_LOGGING_ON

  DIR      *dir = NULL;


  do {

    DIAGD_TRACE("%s: check if dir of"DIAGD_LOG_DIR" exist", __func__);

    /* Check if the log directory exist. */
    if ((dir = opendir(DIAGD_LOG_DIR)) == NULL) {

      DIAGD_TRACE("%s: "DIAGD_LOG_DIR" doesn't exist. Create it.", __func__);
      system("mkdir "DIAGD_LOG_DIR);
    }
    else {

      /* The directory exists. */
      closedir(dir);
    }

    DIAGD_TRACE("%s: open "DIAGD_LOG_FILE, __func__);

    logFp = fopen(DIAGD_LOG_FILE, "a");
    if (logFp == NULL) {
      rtn = DIAGD_RC_FAILED_OPEN_LOG_FILE;
      DIAGD_DEBUG("%s: Failed to open "DIAGD_LOG_FILE, __func__);
      break;
    }

    /* Somehow fopen(.., "a") in uclibc does NOT implicitly set file position
     * to eof but glibc DOES.
     * Need to call fseek(..,SEEK_END) to set file position to eof
     */
    if (fseek(logFp, 0L, SEEK_END) < 0) {
      perror("fseek");
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

#else
  rtn = DIAGD_RC_OK;
#endif /* end of DIAGD_LOGGING_ON */

  return (rtn);
} /* end of diagtOpenEventLogFile */


void diagtCloseEventLogFile(void)
{
  if (logFp != NULL) {
    fclose(logFp);
    logFp = NULL;
  }
} /* end of diagtCloseEventLogFile */


/*
 * Open the diagd MoCA monitoring binary log file
 * comment out for now since Moca log in text
 * format is already written to diagd.log.
 *
 * Input:
 * None
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagtOpenMocaLogFile(void)
{
  int       rtn = DIAGD_RC_ERR;

#ifdef DIAGD_MOCA_LOGGING_ON

  DIR      *dir = NULL;

  do {

    DIAGD_TRACE("%s: check if dir of"DIAGD_LOG_DIR" exist", __func__);

    /* Check if the log directory exist. */
    if ((dir = opendir(DIAGD_LOG_DIR)) == NULL) {

      DIAGD_TRACE("%s: "DIAGD_LOG_DIR" doesn't exist. Create it.", __func__);
      system("mkdir "DIAGD_LOG_DIR);
    }
    else {
      /* The directory exists. */
      closedir(dir);
    }

    DIAGD_TRACE("%s: open "DIAGD_MOCA_LOG_FILE, __func__);

    mocaLogFp = fopen(DIAGD_MOCA_LOG_FILE, "a");
    if (mocaLogFp == NULL) {
      rtn = DIAGD_RC_FAILED_OPEN_LOG_FILE;
      DIAGD_DEBUG("%s: Failed to open "DIAGD_MOCA_LOG_FILE, __func__);
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

#else
  rtn = DIAGD_RC_OK;
#endif /* end of DIAGD_MOCA_LOGGING_ON */

  return (rtn);
} /* end of diagtOpenMocaLogFile */


void diagtCloseMocaLogFile(void)
{
#ifdef DIAGD_MOCA_LOGGING_ON
  if (mocaLogFp != NULL) {
    fclose(mocaLogFp);
    mocaLogFp = NULL;
  }
#endif
} /* end of diagtCloseMocaLogFile */

#ifdef DIAGD_LOG_ROTATE_ON
/*
 * Handle Diag log rotation.
 * If current log file size is greater
 * than 256K bytes, perform log rotation.
 * Maximum number of rotated files is 10
 * under diag log directory
 *
 * If file size limit is set to 1 Mega-byte or more,
 * there will be more than 10 Mega-byte used from /user
 * ubi fs for diag log rotation. That's why file size
 * limit is set to 256K bytes.
 */
void diagLogRotate()
{
  long fileSize;
  char newFilename[80];
  static uint16_t extNum = 0;
  char *diagdMap = NULL;
  uint16_t *extNumPtr = NULL;
  int diagdFd = -1;

  /* get the diag log file size */
  fileSize = ftell(logFp);
  DIAGD_DEBUG("fileSize=%ld, MAX_ROTATE_SZ=%d", fileSize, MAX_ROTATE_SZ);

  if (fileSize > MAX_ROTATE_SZ) {
    /* get mmap of diag databse file */
    if ((diagdFd = get_diagDb_mmap(&diagdMap)) < 0) {
      DIAGD_DEBUG("get_diagDb_mmap failed");
      /* use the current value in extNum */
      extNumPtr = &extNum;
    }
    else {
      /* read in log rotation extension number from diag database */
      extNumPtr =  (uint16_t *) &diagdMap[DIAGD_LOG_ROTATE_EXTNUM_INDEX];
    }

    DIAGD_DEBUG("extNum=%u", *extNumPtr);
    sprintf(newFilename, "%s.%1d", DIAGD_LOG_FILE, *extNumPtr);
    fprintf(stderr, "Diag log rotation ------> %s\n", newFilename);
    rename(DIAGD_LOG_FILE, newFilename);
    *extNumPtr = (*extNumPtr + 1)%MAX_NUM_OF_ROTATE_FILES;

    if (diagdFd >= 0) {
      close_diagDb_mmap(diagdFd, diagdMap);
    }
    /* open a new Diag log file */
    diagtOpenEventLogFile();
  }
}
#endif

/*
 * Print to the monitoring log file.
 *
 * Input:
 *
 * Output:
 */
void diagLog(const char *msgLvl, const char *format_str, ...)
{
  va_list   argList;



  /* Don't log if the file is not opened */
  if (logFp != NULL) {
    if (msgLvl != NULL) {
      // logging message level
      fprintf(logFp, "%s ", msgLvl);
      fprintf(stderr, "%s ", msgLvl);
    }

    /* send diag log to stderr as well.
     * when redirect stderr to logger, this
     * diag log will go to syslog. For example,
     * start "diagd" in the init script:
     * "diagd 2>&1 | logger -t diagd"
     */
    va_start(argList, format_str);
    vfprintf(logFp, format_str, argList);
    fprintf(logFp, "\n");
    vfprintf(stderr, format_str, argList);
    fprintf(stderr, "\n");
    va_end(argList);
    fflush(logFp);
  }

} /* end of diagLog */


/*
 * Log to the monitoring log file with time stamp
 *
 * Input:
 *
 * Output:
 */
void tDiagLog(const char *msgLvl, const char *format_str, ...)
{
  va_list     argList;
  time_t      currtime;
  char        dtstr[50];
  struct tm  *ptm;


  /* Don't log if the file is not opened */
  if (logFp != NULL) {

    // First print the time stamp
    time(&currtime);
    ptm = localtime(&currtime);
    strftime(dtstr, sizeof(dtstr), "%b %d %Y %T", ptm);
    dtstr[strlen(dtstr)] = '\0';
    fprintf(logFp, "%s ", dtstr);

    if (msgLvl != NULL) {
      // logging message level
      fprintf(logFp, "%s ", msgLvl);
      fprintf(stderr, "%s ", msgLvl);
    }

    /* send diag log to stderr as well.
     * when redirect stderr to logger, this
     * diag log will go to syslog. For example,
     * start "diagd" in the init script:
     * "diagd 2>&1 | logger -t diagd"
     */
    // Now print the caller's message
    va_start(argList, format_str);
    vfprintf(logFp, format_str, argList);
    fprintf(logFp, "\n");
    vfprintf(stderr, format_str, argList);
    fprintf(stderr, "\n");
    va_end(argList);
    fflush(logFp);
  }

} /* end of tDiagLog */


/*
 * Log information to the test result file
 *
 * Input:
 *
 * Output:
 */
void dtrLog(const char *format_str, ...)
{
  va_list   argList;

  if (testResultsFp != NULL) {
    va_start(argList, format_str);
    vfprintf(testResultsFp, format_str, argList);
    fprintf(testResultsFp, "\n");
    va_end(argList);
    fflush(testResultsFp);
  }
} /* end of dtrLog */


/*
 * Log information to the test result file with timestamp
 *
 * Input:
 *
 * Output:
 */
void tDtrLog(const char *format_str, ...)
{
  va_list     argList;
  time_t      currtime;
  char        dtstr[50];
  struct tm  *ptm;

  /* Don't log if the file is not opened */
  if (testResultsFp != NULL) {
    // First print the time stamp
    time(&currtime);
    ptm = localtime(&currtime);
    strftime(dtstr, sizeof(dtstr), "%Y/%m/%d %H:%M:%S", ptm);
    dtstr[strlen(dtstr)] = '\0';
    fprintf(testResultsFp, "%s ", dtstr);

    // Now print the caller's message
    va_start(argList, format_str);
    vfprintf(testResultsFp, format_str, argList);
    fprintf(testResultsFp, "\n");
    va_end(argList);
    fflush(testResultsFp);
  }

} /* end of tDtrLog */



/*
 * Write to the MoCA log file without timestamp
 *
 * Input:
 * pLogMsg -  Point to the message to be save to moca log file.
 *
 * Output:
 * None
 */
void diagMocaLog(char *pLogMsg)
{
#ifdef DIAGD_MOCA_LOGGING_ON
  uint32_t  msgSize;
  diag_moca_log_msg_hdr_t  *pMsgHdr = (diag_moca_log_msg_hdr_t *)pLogMsg;

  /* Don't log if the file is not opened */
  if (mocaLogFp != NULL) {

    msgSize = pMsgHdr->msgLen + sizeof(diag_moca_log_msg_hdr_t);

    fwrite(pLogMsg, msgSize, 1, mocaLogFp);

    fflush(mocaLogFp);
  }
#endif  /* DIAGD_MOCA_LOGGING_ON */

} /* end of diagMocaLog */

/*
 * Write MoCA interface statistics to diagd log file with timestamp
 *
 * Input:
 * dtstr  -  timestamp string
 * pStats -  Point to Diag MoCA interface statistics
 *
 * Output:
 * None
 */
void diagMocaStatsLog(char *dtstr, diag_mocaIf_stats_t *pStats)
{
   DIAGD_LOG_W_TS("%s       inUcPkts=%d", dtstr, pStats->inUcPkts);
   DIAGD_LOG_W_TS("%s       inDiscardPktsEcl=%d", dtstr, pStats->inDiscardPktsEcl);
   DIAGD_LOG_W_TS("%s       inDiscardPktsMac=%d", dtstr, pStats->inDiscardPktsMac);
   DIAGD_LOG_W_TS("%s       inUnKnownPkts=%d", dtstr, pStats->inUnKnownPkts);
   DIAGD_LOG_W_TS("%s       inMcPkts=%d", dtstr, pStats->inMcPkts);
   DIAGD_LOG_W_TS("%s       inBcPkts=%d", dtstr, pStats->inBcPkts);
   DIAGD_LOG_W_TS("%s       inOctets_low=%d", dtstr, pStats->inOctets_low);
   DIAGD_LOG_W_TS("%s       outUcPkts=%d", dtstr, pStats->outUcPkts);
   DIAGD_LOG_W_TS("%s       outDiscardPkts=%d", dtstr, pStats->outDiscardPkts);
   DIAGD_LOG_W_TS("%s       outBcPkts=%d", dtstr, pStats->outBcPkts);
   DIAGD_LOG_W_TS("%s       outOctets_low=%d", dtstr, pStats->outOctets_low);
   DIAGD_LOG_W_TS("%s       inOctets_hi=%d", dtstr, pStats->inOctets_hi);
   DIAGD_LOG_W_TS("%s       outOctets_hi=%d", dtstr, pStats->outOctets_hi);
   DIAGD_LOG_W_TS("%s       rxMapPkts=%d", dtstr, pStats->rxMapPkts);
   DIAGD_LOG_W_TS("%s       rxRRPkts=%d", dtstr, pStats->rxRRPkts);
   DIAGD_LOG_W_TS("%s       rxBeacons=%d", dtstr, pStats->rxBeacons);
   DIAGD_LOG_W_TS("%s       rxCtrlPkts=%d", dtstr, pStats->rxCtrlPkts);
   DIAGD_LOG_W_TS("%s       rxLcAdmReqCrcErr=%d", dtstr, pStats->rxLcAdmReqCrcErr);
   DIAGD_LOG_W_TS("%s       rxMapCrcError=%d", dtstr, pStats->rxMapCrcError);
   DIAGD_LOG_W_TS("%s       rxBeaconCrcError=%d", dtstr, pStats->rxBeaconCrcError);
   DIAGD_LOG_W_TS("%s       rxRrCrcError=%d", dtstr, pStats->rxRrCrcError);
   DIAGD_LOG_W_TS("%s       rxLcCrcError=%d", dtstr, pStats->rxLcCrcError);
}

/*
 * Write Diag MoCA service performance monitoring results to
 * diagd log file with timestamp.
 *
 * Input:
 * dtstr  -  timestamp string
 * pStats -  Point to Diag MoCA service performance monitoring results
 *
 * Output:
 * None
 */
void diagMocaPerfStatusLog(char *dtstr, diag_moca_ref_status_entry_t *pPerfStatus)
{
   DIAGD_LOG_W_TS("%s       valid=%s", dtstr, pPerfStatus->valid == true? "true":"false");
   DIAGD_LOG_W_TS("%s       nodeId=%d", dtstr, pPerfStatus->nodeId);
   DIAGD_LOG_W_TS("%s       rxUcPhyRate=%d", dtstr, pPerfStatus->rxUcPhyRate);
   DIAGD_LOG_W_TS("%s       rxUcGain=%d", dtstr, pPerfStatus->rxUcGain);
   DIAGD_LOG_W_TS("%s       rxUcAvgSnr=%d", dtstr, pPerfStatus->rxUcAvgSnr);
   DIAGD_LOG_W_TS("%s       rxUcBitLoading=%d", dtstr, pPerfStatus->rxUcBitLoading);
}

/*
 * Write MoCA Bit loading data to diagd log file with timestamp
 *
 * Input:
 * dtstr          -  timestamp string
 * pBitLoading    -  Point to first bit loading
 * pSecBitLoading -  Point to second bit loading
 *
 * Output:
 * None
 */
void diagMocaBitloadingLog(char *dtstr, UINT32 *pBitLoading, UINT32 *pSecBitLoading)
{
   UINT32  val, secVal;
   UINT32  subCarrier, secSubCarrier;
   char inBuf[128];
   char outBuf[128];

   sprintf(outBuf, "%s ", dtstr);
   secSubCarrier = 0;
   for (subCarrier = 0 ; subCarrier < (MoCA_MAX_SUB_CARRIERS/8); subCarrier++) {
      val = pBitLoading [subCarrier];
      val = (val<<28) | ((val&0xf0)<<20) | ((val&0xf00)<<12) | ((val&0xf000)<<4)
         | ((val&0xf0000)>>4) | ((val&0xf00000)>>12) | ((val&0xf000000)>>20) | val >>28;
      sprintf(inBuf, "%8.8x", val);
      strcat(outBuf, inBuf);
      if (((subCarrier+1) % 4) == 0) {
         if (pSecBitLoading == NULL) {
            DIAGD_LOG_W_TS(outBuf);
            sprintf(outBuf, "%s ", dtstr);
         }
         else {
            sprintf(inBuf, "\t   ");
            strcat(outBuf, inBuf);
            /* Display the second Bit Loading */
            for (;secSubCarrier < (MoCA_MAX_SUB_CARRIERS/8); secSubCarrier++) {
               secVal = pSecBitLoading [secSubCarrier];
               secVal = (secVal<<28) | ((secVal&0xf0)<<20) | ((secVal&0xf00)<<12) | ((secVal&0xf000)<<4)
                  | ((secVal&0xf0000)>>4) | ((secVal&0xf00000)>>12) | ((secVal&0xf000000)>>20) | secVal >>28;
               sprintf(inBuf, "%8.8x", secVal);
               strcat(outBuf, inBuf);
               if (((secSubCarrier+1) % 4) == 0) {
                  DIAGD_LOG_W_TS(outBuf);
                  sprintf(outBuf, "%s ", dtstr);
                  secSubCarrier++;
                  break;
               }
            } /* for (secSubCarrier) */
         }
      } /* if (subCarrier) */
   } /* for (subCarrier) */
}

/*
 * Write MoCA node status to diagd log file with timestamp
 *
 * Input:
 * dtstr       -  timestamp string
 * pNodeStatus -  Point to MoCA node status entry
 *
 * Output:
 * None
 */
void diagMocaNodeStatusLog(char *dtstr, PMoCA_NODE_STATUS_ENTRY pNodeStatus)
{
   MAC_ADDRESS macAddr;

   moca_u32_to_mac(macAddr, pNodeStatus->eui[0], pNodeStatus->eui[1]);

   DIAGD_LOG_W_TS("%s Node                             : %d ", dtstr, pNodeStatus->nodeId);
   DIAGD_LOG_W_TS("%s =============================================", dtstr);
   DIAGD_LOG_W_TS("%s MAC Address                      : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
           dtstr, macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
   DIAGD_LOG_W_TS("%s Freq Offset                      : %d KHz ", dtstr, pNodeStatus->freqOffset/1000) ;
   DIAGD_LOG_W_TS("%s Protocol Support                 : 0x%X", dtstr, pNodeStatus->protocolSupport);
   DIAGD_LOG_W_TS("%s    - Preferred NC                : %d", dtstr, (pNodeStatus->protocolSupport>>6)&1);
   DIAGD_LOG_W_TS("%s    - 256 QAM capable             : %d", dtstr, (pNodeStatus->protocolSupport>>4)&1);
   DIAGD_LOG_W_TS("%s    - Aggregated PDUs             : %s", dtstr, ((pNodeStatus->protocolSupport>>7)&3) == 0?"6":
           ((pNodeStatus->protocolSupport>>7)&3) == 2?"10":"Not allowed");
   DIAGD_LOG_W_TS("%s Other Node UC Pwr Backoff        : %d dB ", dtstr, pNodeStatus->otherNodeUcPwrBackOff) ;
   DIAGD_LOG_W_TS("%s Turbo Mode                       : %d", dtstr, pNodeStatus->txUc.turbo);
   DIAGD_LOG_W_TS("%s -------------------------------------------------------------------------", dtstr);
   DIAGD_LOG_W_TS("%s         Nbas  Preamble    CP    TxPower   RxPower   Rate              SNR", dtstr);
   DIAGD_LOG_W_TS("%s =========================================================================", dtstr);
   DIAGD_LOG_W_TS("%s TxUc    %4d     %2d      %3d    %3d dBm   N/A       %9u bps   %2.1lf dB", dtstr, pNodeStatus->txUc.nBas,
           pNodeStatus->txUc.preambleType, pNodeStatus->txUc.cp,
           pNodeStatus->txUc.txPower, pNodeStatus->maxPhyRates.txUcPhyRate,
           (float)pNodeStatus->txUc.avgSnr/2.0 );
   DIAGD_LOG_W_TS("%s RxUc    %4d     %2d      %3d    N/A      %6.2lf dBm %9u bps   %2.1lf dB", dtstr, pNodeStatus->rxUc.nBas,
           pNodeStatus->rxUc.preambleType, pNodeStatus->rxUc.cp,
           pNodeStatus->rxUc.rxGain/4.0, pNodeStatus->maxPhyRates.rxUcPhyRate,
           pNodeStatus->rxUc.avgSnr/2.0 );
   DIAGD_LOG_W_TS("%s RxBc    %4d     %2d      %3d    N/A      %6.2lf dBm %9u bps   %2.1lf dB", dtstr, pNodeStatus->rxBc.nBas,
           pNodeStatus->rxBc.preambleType, pNodeStatus->rxBc.cp,
           pNodeStatus->rxBc.rxGain/4.0, pNodeStatus->maxPhyRates.rxBcPhyRate,
           pNodeStatus->rxBc.avgSnr/2.0 );
   DIAGD_LOG_W_TS("%s RxMap   %4d     %2d      %3d    N/A      %6.2lf dBm %9u bps   %2.1lf dB", dtstr, pNodeStatus->rxMap.nBas,
           pNodeStatus->rxMap.preambleType, pNodeStatus->rxMap.cp,
           pNodeStatus->rxMap.rxGain/4.0,  pNodeStatus->maxPhyRates.rxMapPhyRate,
           pNodeStatus->rxMap.avgSnr/2.0 );
   DIAGD_LOG_W_TS("%s ===========================================================", dtstr);
   DIAGD_LOG_W_TS("%s ", dtstr) ;

   DIAGD_LOG_W_TS("%s    Tx Unicast Bit Loading Info  \t   Rx Unicast Bit Loading Info ", dtstr);
   DIAGD_LOG_W_TS("%s --------------------------------\t   -------------------------------", dtstr);

   diagMocaBitloadingLog(dtstr, &pNodeStatus->txUc.bitLoading[0], &pNodeStatus->rxUc.bitLoading[0]);
   DIAGD_LOG_W_TS("%s --------------------------------\t   -------------------------------", dtstr);

   DIAGD_LOG_W_TS("%s    Rx Broadcast Bit Loading Info  \t   Rx Map Bit Loading Info ", dtstr);
   DIAGD_LOG_W_TS("%s ----------------------------------\t   -----------------------------", dtstr);

   diagMocaBitloadingLog(dtstr, &pNodeStatus->rxBc.bitLoading[0], &pNodeStatus->rxMap.bitLoading[0]);
   DIAGD_LOG_W_TS("%s --------------------------------\t   -------------------------------", dtstr);
}

/*
 * Write MoCA node common status to diagd log file with timestamp
 *
 * Input:
 * dtstr  -  timestamp string
 * pNodeCommonStatus -  Point to MoCA node common status entry
 *
 * Output:
 * None
 */
void diagMocaNodeCommonStatusLog(char *dtstr, PMoCA_NODE_COMMON_STATUS_ENTRY pNodeCommonStatus)
{
   DIAGD_LOG_W_TS("%s All Node Information ", dtstr);
   DIAGD_LOG_W_TS("%s =====================", dtstr);
   DIAGD_LOG_W_TS("%s \tNbas  Preamble     CP    TxPower   RxPower  Rate ", dtstr);
   DIAGD_LOG_W_TS("%s ===========================================================", dtstr);
   DIAGD_LOG_W_TS("%s TxBc\t%4d      %d        %d    %3d dBm    N/A     %u bps ",
           dtstr, pNodeCommonStatus->txBc.nBas,
           pNodeCommonStatus->txBc.preambleType,
           pNodeCommonStatus->txBc.cp,
           pNodeCommonStatus->txBc.txPower,
           pNodeCommonStatus->maxCommonPhyRates.txBcPhyRate);
   DIAGD_LOG_W_TS("%s TxMap\t%4d      %d        %d    %3d dBm    N/A     %u bps ",
           dtstr, pNodeCommonStatus->txMap.nBas,
           pNodeCommonStatus->txMap.preambleType,
           pNodeCommonStatus->txMap.cp,
           pNodeCommonStatus->txMap.txPower,
           pNodeCommonStatus->maxCommonPhyRates.txMapPhyRate);
   DIAGD_LOG_W_TS("%s ===========================================================", dtstr);
   DIAGD_LOG_W_TS("%s    Tx Bcast Bit Loading Info    \t      Tx Map Bit Loading Info  ", dtstr);
   DIAGD_LOG_W_TS("%s --------------------------------\t    ---------------------------", dtstr);
   diagMocaBitloadingLog(dtstr, &pNodeCommonStatus->txBc.bitLoading[0],
                          &pNodeCommonStatus->txMap.bitLoading[0]);
   DIAGD_LOG_W_TS("%s --------------------------------\t   -------------------------------", dtstr);
}


/*
 * Write MoCA node statistics table to diagd log file with timestamp
 *
 * Input:
 * dtstr  -  timestamp string
 * pStats -  Point to Diag node statistics table
 *
 * Output:
 * None
 */
void diagMocaNodeStatsLog(char *dtstr, diag_moca_node_stats_table_t  *pNodeStats)
{
   int idx=0;
   int nodes=0;
   diag_moca_node_stats_entry_t *pNodeStatsEntry = &pNodeStats->Stats;

   nodes =  pNodeStats->nodeStatsTblSize/sizeof(diag_moca_node_stats_entry_t);
   
   for (idx=0; idx < nodes; idx ++) {
      DIAGD_LOG_W_TS("%s =============================================", dtstr);
      DIAGD_LOG_W_TS ("%s Node                             : %d ", dtstr, pNodeStatsEntry->nodeId);
      DIAGD_LOG_W_TS("%s MAC Address                      : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x", dtstr,
              pNodeStatsEntry->macAddr[0],
              pNodeStatsEntry->macAddr[1],
              pNodeStatsEntry->macAddr[2],
              pNodeStatsEntry->macAddr[3],
              pNodeStatsEntry->macAddr[4],
              pNodeStatsEntry->macAddr[5]);
      DIAGD_LOG_W_TS ("%s =============================================", dtstr);
      DIAGD_LOG_W_TS ("%s Unicast Tx Pkts To Node          : %d ", dtstr, pNodeStatsEntry->nodeStats.txPkts);
      DIAGD_LOG_W_TS ("%s Unicast Rx Pkts From Node        : %d ", dtstr, pNodeStatsEntry->nodeStats.rxPkts);
      DIAGD_LOG_W_TS ("%s Rx CodeWord NoError              : %d ", dtstr, pNodeStatsEntry->nodeStats.rxCwUnError);
      DIAGD_LOG_W_TS ("%s Rx CodeWord ErrorAndCorrected    : %d ", dtstr, pNodeStatsEntry->nodeStats.rxCwCorrected);
      DIAGD_LOG_W_TS ("%s Rx CodeWord ErrorAndUnCorrected  : %d ", dtstr, pNodeStatsEntry->nodeStats.rxCwUncorrected);
      DIAGD_LOG_W_TS ("%s Rx NoSync Errors                 : %d ", dtstr, pNodeStatsEntry->nodeStats.rxNoSync);
      DIAGD_LOG_W_TS ("%s =============================================", dtstr);
      DIAGD_LOG_W_TS ("%s        MoCA Extended Node Statistics Data", dtstr);
      DIAGD_LOG_W_TS ("%s =============================================", dtstr);
      DIAGD_LOG_W_TS ("%s NODE_RX_UC_CRC_ERROR                  : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxUcCrcError);
      DIAGD_LOG_W_TS ("%s NODE_RX_UC_TIMEOUT_ERROR              : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxUcTimeoutError);
      DIAGD_LOG_W_TS ("%s NODE_RX_BC_CRC_ERROR                  : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxBcCrcError);
      DIAGD_LOG_W_TS ("%s NODE_RX_BC_TIMEOUT_ERROR              : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxBcTimeoutError);

      DIAGD_LOG_W_TS ("%s NODE_RX_MAP_CRC_ERROR                 : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxMapCrcError);
      DIAGD_LOG_W_TS ("%s NODE_RX_MAP_TIMEOUT_ERROR             : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxMapTimeoutError);
      DIAGD_LOG_W_TS ("%s NODE_RX_BEACON_CRC_ERROR              : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxBeaconCrcError);
      DIAGD_LOG_W_TS ("%s NODE_RX_BEACON_TIMEOUT_ERROR          : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxBeaconTimeoutError);
      DIAGD_LOG_W_TS ("%s NODE_RX_RR_CRC_ERROR                  : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxRrCrcError);
      DIAGD_LOG_W_TS ("%s NODE_RX_RR_TIMEOUT_ERROR              : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxRrTimeoutError);

      DIAGD_LOG_W_TS ("%s NODE_RX_LC_CRC_ERROR                  : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxLcCrcError);
      DIAGD_LOG_W_TS ("%s NODE_RX_LC_TIMEOUT_ERROR              : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxLcTimeoutError);

      DIAGD_LOG_W_TS ("%s NODE_RX_P1_ERROR                      : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxP2Error);
      DIAGD_LOG_W_TS ("%s NODE_RX_P2_ERROR                      : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxP2Error);
      DIAGD_LOG_W_TS ("%s NODE_RX_P3_ERROR                      : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxP3Error);
      DIAGD_LOG_W_TS ("%s NODE_RX_P1_GCD_ERROR                  : %d ", dtstr, pNodeStatsEntry->nodeStatsExt.rxP1GcdError);
      DIAGD_LOG_W_TS ("%s =============================================", dtstr);

      pNodeStatsEntry++;
   }
}

/*
 * Write MoCA self node status to diagd log file with timestamp
 *
 * Input:
 * dtstr   -  timestamp string
 * pStatus -  Point to MoCA_STATUS
 *
 * Output:
 * None
 */
void diagMocaMyStatusLog(char *dtstr, PMoCA_STATUS pStatus)
{
   char inBuf[128];
   char outBuf[128];
   int count;
   UINT32 noOfNodes = 0;

   UINT32 coreversionMajor, coreversionMinor, coreversionBuild;
   UINT32 timeH, timeM, timeS;


   if (pStatus == NULL) {
     DIAGD_TRACE("%s pStatus is NULL", __func__);
     return;
   }

   coreversionMajor = pStatus->generalStatus.swVersion >> 28;
   coreversionMinor = (pStatus->generalStatus.swVersion << 4) >> 28;
   coreversionBuild = (pStatus->generalStatus.swVersion << 8) >> 8;

   DIAGD_LOG_W_TS("%s            MoCA Status(General)     ", dtstr);
   DIAGD_LOG_W_TS("%s ==================================  ", dtstr);

   sprintf(outBuf, "%s vendorId                  : %d \t", dtstr,
           pStatus->generalStatus.vendorId);
   sprintf(inBuf, " HwVersion                 : 0x%x ",
            pStatus->generalStatus.hwVersion);
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);
   sprintf(outBuf, "%s SwVersion                 : %d.%d.%d \t", dtstr,
           coreversionMajor, coreversionMinor, coreversionBuild);
   sprintf(inBuf, " self MoCA Version         : 0x%x ",
            pStatus->generalStatus.selfMoCAVersion);
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   sprintf(outBuf, "%s networkVersionNumber      : 0x%x \t", dtstr,
           pStatus->generalStatus.networkVersionNumber);
   sprintf(inBuf, " qam256Support             : %s ",
          (pStatus->generalStatus.qam256Support == MoCA_QAM_256_SUPPORT_ON) ?
           "supported" : "unknown");
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   if (pStatus->generalStatus.operStatus == MoCA_OPER_STATUS_ENABLED)
      sprintf(outBuf, "%s operStatus                : Enabled \t", dtstr);
   else
      sprintf(outBuf, "%s operStatus                : Hw Error \t", dtstr);
   if (pStatus->generalStatus.linkStatus == MoCA_LINK_UP)
      sprintf(inBuf, " linkStatus                : Up ");
   else
      sprintf(inBuf, " linkStatus                : Down ");
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   sprintf(outBuf, "%s connectedNodes BitMask    : 0x%x \t", dtstr,
           pStatus->generalStatus.connectedNodes);
   if (pStatus->generalStatus.nodeId >= MoCA_MAX_NODES)
      sprintf(inBuf, " nodeId                    : N/A ");
   else
      sprintf(inBuf, " nodeId                    : %u ",
              pStatus->generalStatus.nodeId);
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   if (pStatus->generalStatus.ncNodeId >= MoCA_MAX_NODES)
      sprintf(outBuf, "%s ncNodeId                  : N/A \t", dtstr);
   else
      sprintf(outBuf, "%s ncNodeId                  : %u \t\t", dtstr,
              pStatus->generalStatus.ncNodeId);
   diagMoca_ConvertUpTime(pStatus->miscStatus.MoCAUpTime, &timeH, &timeM, &timeS);
   sprintf(inBuf, " upTime                    : %02uh:%02um:%02us",
           timeH, timeM, timeS);
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   diagMoca_ConvertUpTime(pStatus->miscStatus.linkUpTime, &timeH, &timeM, &timeS);
   sprintf(outBuf, "%s linkUpTime                : %02uh:%02um:%02us",
            dtstr, timeH, timeM, timeS);
   if (pStatus->generalStatus.backupNcId >= MoCA_MAX_NODES)
      sprintf(inBuf, " backupNcId                : N/A ");
   else
      sprintf(inBuf, " backupNcId                : %u ",
               pStatus->generalStatus.backupNcId);
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   sprintf(outBuf, "%s rfChannel                 : %u Mhz\t", dtstr,
            pStatus->generalStatus.rfChannel);
   sprintf(inBuf, " bwStatus                  : 0x%x ",
            pStatus->generalStatus.bwStatus);
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   sprintf(outBuf, "%s NodesUsableBitMask        : 0x%x \t", dtstr,
            pStatus->generalStatus.nodesUsableBitmask);
   sprintf(inBuf, " NetworkTabooMask          : 0x%x ",
            pStatus->generalStatus.networkTabooMask);
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   sprintf(outBuf, "%s NetworkTabooStart         : %d \t", dtstr,
            pStatus->generalStatus.networkTabooStart);
   sprintf(inBuf, " txGcdPowerReduction       : %d ",
            pStatus->generalStatus.txGcdPowerReduction);
   strcat(outBuf, inBuf);
   DIAGD_LOG_W_TS(outBuf);

   sprintf(outBuf, "%s pqosEgressNumFlows        : %d \t\t", dtstr,
   pStatus->generalStatus.pqosEgressNumFlows);

   /* find the number of connected nodes from the connected nodes bitmask */
   for (count = 0 ; count < MoCA_MAX_NODES ; count++) {
      if (pStatus->generalStatus.connectedNodes & (0x1 << count))
         noOfNodes++;
      }
      sprintf(inBuf, " Num of connectedNodes     : %d ", noOfNodes);
      strcat(outBuf, inBuf);
      DIAGD_LOG_W_TS(outBuf);

      sprintf(outBuf, "%s ledStatus                 : %x ", dtstr,
              pStatus->generalStatus.ledStatus);
      DIAGD_LOG_W_TS(outBuf);

      DIAGD_LOG_W_TS("%s ==================================  ", dtstr);
      DIAGD_LOG_W_TS("%s            MoCA Status(Extended)    ", dtstr);
      DIAGD_LOG_W_TS("%s ==================================  ", dtstr);

      diagMoca_ConvertUpTime(pStatus->extendedStatus.lastPmkExchange,
                             &timeH, &timeM, &timeS);
      DIAGD_LOG_W_TS("%s lastPmkExchange           : %02uh:%02um:%02us", dtstr,
                     timeH, timeM, timeS);
      DIAGD_LOG_W_TS("%s lastPmkInterval           : %d sec", dtstr,
                     pStatus->extendedStatus.lastPmkInterval);

      diagMoca_ConvertUpTime(pStatus->extendedStatus.lastTekExchange, &timeH,
                             &timeM, &timeS);
      DIAGD_LOG_W_TS("%s lastTekExchange           : %02uh:%02um:%02us", dtstr,
                     timeH, timeM, timeS);
      DIAGD_LOG_W_TS("%s lastTekInterval           : %d sec", dtstr,
                     pStatus->extendedStatus.lastTekInterval);

      DIAGD_LOG_W_TS("%s PMK Even Key              : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s",
         dtstr,
         pStatus->extendedStatus.pmkEvenKey[0], pStatus->extendedStatus.pmkEvenKey[1],
         pStatus->extendedStatus.pmkEvenKey[2], pStatus->extendedStatus.pmkEvenKey[3],
         pStatus->extendedStatus.pmkEvenKey[4], pStatus->extendedStatus.pmkEvenKey[5],
         pStatus->extendedStatus.pmkEvenKey[6], pStatus->extendedStatus.pmkEvenKey[7],
         pStatus->extendedStatus.pmkEvenOdd==0?"(ACTIVE)":"");
      DIAGD_LOG_W_TS("%s PMK Odd Key               : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s",
         dtstr,
         pStatus->extendedStatus.pmkOddKey[0], pStatus->extendedStatus.pmkOddKey[1],
         pStatus->extendedStatus.pmkOddKey[2], pStatus->extendedStatus.pmkOddKey[3],
         pStatus->extendedStatus.pmkOddKey[4], pStatus->extendedStatus.pmkOddKey[5],
         pStatus->extendedStatus.pmkOddKey[6], pStatus->extendedStatus.pmkOddKey[7],
         pStatus->extendedStatus.pmkEvenOdd==1?"(ACTIVE)":"");
      DIAGD_LOG_W_TS("%s TEK Even Key              : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s",
         dtstr,
         pStatus->extendedStatus.tekEvenKey[0], pStatus->extendedStatus.tekEvenKey[1],
         pStatus->extendedStatus.tekEvenKey[2], pStatus->extendedStatus.tekEvenKey[3],
         pStatus->extendedStatus.tekEvenKey[4], pStatus->extendedStatus.tekEvenKey[5],
         pStatus->extendedStatus.tekEvenKey[6], pStatus->extendedStatus.tekEvenKey[7],
         pStatus->extendedStatus.tekEvenOdd==0?"(ACTIVE)":"");
      DIAGD_LOG_W_TS("%s TEK Odd Key               : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s",
         dtstr,
         pStatus->extendedStatus.tekOddKey[0], pStatus->extendedStatus.tekOddKey[1],
         pStatus->extendedStatus.tekOddKey[2], pStatus->extendedStatus.tekOddKey[3],
         pStatus->extendedStatus.tekOddKey[4], pStatus->extendedStatus.tekOddKey[5],
         pStatus->extendedStatus.tekOddKey[6], pStatus->extendedStatus.tekOddKey[7],
         pStatus->extendedStatus.tekEvenOdd==1?"(ACTIVE)":"");
      DIAGD_LOG_W_TS("%s ==================================  ", dtstr);
      DIAGD_LOG_W_TS("%s            MoCA Status(Misc)    ", dtstr);
      DIAGD_LOG_W_TS("%s ==================================  ", dtstr);
      DIAGD_LOG_W_TS("%s MAC GUID                  : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
         dtstr,
         pStatus->miscStatus.macAddr[0],
         pStatus->miscStatus.macAddr[1],
         pStatus->miscStatus.macAddr[2],
         pStatus->miscStatus.macAddr[3],
         pStatus->miscStatus.macAddr[4],
         pStatus->miscStatus.macAddr[5]);
      DIAGD_LOG_W_TS("%s Are we Network Controller : %s ", dtstr,
      (pStatus->miscStatus.isNC == 1) ? "yes" : "no");
      diagMoca_ConvertUpTime(pStatus->miscStatus.driverUpTime, &timeH, &timeM, &timeS);
      DIAGD_LOG_W_TS("%s Driver Up Time            : %02uh:%02um:%02us ", dtstr, timeH, timeM, timeS);
      DIAGD_LOG_W_TS("%s Link Reset Count          : %u ", dtstr, pStatus->miscStatus.linkResetCount);
      DIAGD_LOG_W_TS("%s ==================================  ", dtstr);
}

/*
 * Diag logged MoCA message type string.
 * Refer to enum diag_moca_log_msgs defined in diagMoca.h
 *
 */
char *diagMocaMsgTypeStr[] = {
   "DIAG_MOCA_LOG_NONE",
   "DIAG_MOCA_LOG_EXCESSIVE_TX_DISCARD_PKTS",
   "DIAG_MOCA_LOG_EXCESSIVE_RX_DISCARD_PKTS",
   "DIAG_MOCA_LOG_EXCESSIVE_TX_RX_DISCARD_PKTS"
};

/*
 * Write Moca log in text format to the diagd log file
 *
 * Input:
 * pLogMsg -  Point to the message to be save to moca log file.
 *
 * Output:
 * None
 */
void diagMocaStrLog(char *pLogMsg, PMoCA_STATUS pStatus)
{
  char        dtstr[50];
  uint32_t  msgSize;
  uint16_t  msgType;
  diag_moca_log_msg_hdr_t  *pMsgHdr = (diag_moca_log_msg_hdr_t *)pLogMsg;
  diag_mocaIf_stats_t *pPrevStats;
  diag_mocaIf_stats_t *pCurrStats;
  diag_moca_perf_status_t *pPerfStatus;
  diag_moca_node_stats_table_t *pNodeStats;
  diag_moca_ref_status_entry_t *pPerfResult;
  int i, j, nodes;

   if (pMsgHdr == NULL) {
      DIAGD_TRACE("%s: pMsgHdr is NULL!", __func__);
      return;
   }

    // First print the time stamp
   strftime(dtstr, sizeof(dtstr), "%b %d %Y %T", pMsgHdr->currTime);
   dtstr[strlen(dtstr)] = '\0';


   msgSize = pMsgHdr->msgLen + sizeof(diag_moca_log_msg_hdr_t);
   msgType = pMsgHdr->msgType;

   switch (msgType) {
      case DIAG_MOCA_LOG_NONE:
         DIAGD_TRACE("%s: Invalid MsgType= %d", __func__, msgType);
         DIAGD_LOG_W_TS("%s msgType= %s\n", dtstr, diagMocaMsgTypeStr[msgType]);
         break;
      case DIAG_MOCA_LOG_EXCESSIVE_TX_DISCARD_PKTS:
      case DIAG_MOCA_LOG_EXCESSIVE_RX_DISCARD_PKTS:
      case DIAG_MOCA_LOG_EXCESSIVE_TX_RX_DISCARD_PKTS:
         pPrevStats = (diag_mocaIf_stats_t *) &((diag_mocalog_discardpkts_exceed_t *)pLogMsg)->prevStats;
         pCurrStats = (diag_mocaIf_stats_t *) &((diag_mocalog_discardpkts_exceed_t *)pLogMsg)->currStats;
         pNodeStats = (diag_moca_node_stats_table_t *) &((diag_mocalog_discardpkts_exceed_t *)pLogMsg)->nodeStats;
         DIAGD_LOG_W_TS("%s msgType= %s", dtstr, diagMocaMsgTypeStr[msgType]);
         diagMocaMyStatusLog("", pStatus);

         DIAGD_LOG_W_TS("%s ##########Previous  Stats###########", "");
         diagMocaStatsLog("", pPrevStats);
         DIAGD_LOG_W_TS("%s ##########Current  Stats###########", "");
         diagMocaStatsLog("", pCurrStats);
         diagMocaNodeStatsLog("", pNodeStats);
         break;
     case DIAG_MOCA_LOG_POOR_PHY_RATE:
         pPerfStatus = (diag_moca_perf_status_t *)pLogMsg;
         DIAGD_LOG_W_TS("%s msgType= DIAG_MOCA_LOG_POOR_PHY_RATE", dtstr);
         diagMocaMyStatusLog("", pStatus);
         DIAGD_LOG_W_TS("%s noConnectedNodes = %d", "", pPerfStatus->noConnectedNodes);

         nodes = pPerfStatus->noConnectedNodes;
         pPerfResult = &pPerfStatus->perfResult[0];
          
         /* Log Moca Performance Status and Node Status */
         for (i=0; i < nodes; i++) {
            if (pPerfResult->valid) {
               diagMocaPerfStatusLog("", pPerfResult);
               for (j=0; j < nodes; j++) {
                  if (pPerfResult->nodeId == pPerfStatus->nodeStatus.nodeStatus[j].nodeId) {
                     diagMocaNodeStatusLog("", &pPerfStatus->nodeStatus.nodeStatus[j]);
                     break;
                  }
               }
            }

            pPerfResult++;
         }

         DIAGD_LOG_W_TS("%s ###############################", "");
         diagMocaNodeCommonStatusLog("", &pPerfStatus->nodeStatus.nodeCommonStatus);
         break;
      default:
         /* Shouldn't happen */
         DIAGD_TRACE("%s: Invalid MsgType= %d", __func__, msgType);
         DIAGD_LOG_W_TS("%s Invalid msgType= %d", dtstr, msgType);
         break;
   } /* switch (msgType) */
} /* end of diagMocaStrLog */

/*
 * Upload the whole log file through logger.
 * This routine is called only once in diagd_Init()
 * when diagd starts running.
 *
 * Input:
 * None
 *
 * Output:
 * None
 */
void diagUploadLogFile() {
  FILE  *iFp;
  char  inBuf[MAX_BUF_LEN];


  DIAGD_ENTRY("%s: ", __func__);

  /* Open the diag log file to be uploaded to syslog via logger */
  iFp = fopen(DIAGD_LOG_FILE, "r");
  if (iFp == NULL) {
    DIAGD_DEBUG("%s: open '%s' failed: %s\n",
        __func__, DIAGD_LOG_FILE, strerror(errno));
    return;
  }

  fprintf(stderr, "########## Beginning of Diag Log File Upload ##########\n");
  while (fgets(inBuf, MAX_BUF_LEN, iFp) != NULL) {
    fprintf(stderr, "%s", inBuf);
  }

  fprintf(stderr, "########## End of Diag Log File Upload ##########\n");

  if (iFp != NULL) {
    /* close descriptor for file that was sent */
    fclose(iFp);
  }

  DIAGD_EXIT("%s: ", __func__);
}
