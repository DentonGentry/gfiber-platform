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

      /* TODO - 2011/11/08
       *  1) HOWTO - If failed to open the log file.
       *  2) For now, we treat it as a fatal error and abort the diagd.
       */
      rtn = DIAGD_RC_FAILED_OPEN_LOG_FILE;
      DIAGD_LOG_SWERR("%s: Failed to open "DIAGD_LOG_FILE, __func__);
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

      /* TODO - 2011/11/08
       *  1) HOWTO - If failed to open the log file.
       *  2) For now, we treat it as a fatal error and abort the diagd.
       */
      rtn = DIAGD_RC_FAILED_OPEN_LOG_FILE;
      DIAGD_DEBUG("%s: Failed to open "DIAGD_LOG_FILE, __func__);
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
 * Open the diagd MoCA monitoring log file
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

    DIAGD_TRACE("%s: open "DIAGD_MOCA_LOG_FILE, __func__);

    mocaLogFp = fopen(DIAGD_MOCA_LOG_FILE, "a");
    if (mocaLogFp == NULL) {

      /* TODO - 2011/11/28
       *  1) HOWTO - If failed to open the log file.
       *  2) For now, we treat it as a fatal error and abort the diagd.
       */
      rtn = DIAGD_RC_FAILED_OPEN_LOG_FILE;
      DIAGD_DEBUG("%s: Failed to open "DIAGD_MOCA_LOG_FILE, __func__);
      break;
    }

    rtn = DIAGD_RC_OK;

  } while (false);

#else
  rtn = DIAGD_RC_OK;
#endif /* end of DIAGD_LOGGING_ON */

  return (rtn);
} /* end of diagtOpenMocaLogFile */


void diagtCloseMocaLogFile(void)
{
  if (mocaLogFp != NULL) {
    fclose(mocaLogFp);
    mocaLogFp = NULL;
  }
} /* end of diagtCloseMocaLogFile */



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
    }

    va_start(argList, format_str);
    vfprintf(logFp, format_str, argList);
    fprintf(logFp, "\n");
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
    strftime(dtstr, sizeof(dtstr), "%Y/%m/%d %H:%M:%S", ptm);
    dtstr[strlen(dtstr)] = '\0';
    fprintf(logFp, "%s ", dtstr);

    if (msgLvl != NULL) {
      // logging message level
      fprintf(logFp, "%s ", msgLvl);
    }

    // Now print the caller's message
    va_start(argList, format_str);
    vfprintf(logFp, format_str, argList);
    fprintf(logFp, "\n");
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
  uint32_t  msgSize;
  diag_moca_log_msg_hdr_t  *pMsgHdr = (diag_moca_log_msg_hdr_t *)pLogMsg;

  /* Don't log if the file is not opened */
  if (mocaLogFp != NULL) {

    msgSize = pMsgHdr->msgLen + sizeof(diag_moca_log_msg_hdr_t);

    fwrite(pLogMsg, msgSize, 1, mocaLogFp);

    fflush(mocaLogFp);
  }

} /* end of diagMocaLog */


