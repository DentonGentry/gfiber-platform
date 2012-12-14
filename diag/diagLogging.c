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
 * In Monitoring log file - Log all of monitoring events (exclude MoCA)
 */
FILE  *logFp = NULL;

/* Test Results logging
 * Log information in string format
 */
FILE  *testResultsFp = NULL;


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


#ifdef DIAGD_LOG_ROTATE_ON
extern int get_diagDb_mmap(char **);
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
 * Log to the monitoring log file
 *
 * Input:
 *   logging:
 *     true:  write information to diag log file
 *     false: write to stderr
 *   timestamp:
 *     true:  log information with timestamp
 *     false: log information without timestamp
 *
 * Output:
 */
void diagLog(bool logging, bool timestamp, const char *msgLvl, const char *format_str, ...)
{
  va_list     argList;
  time_t      currtime;
  char        dtstr[50];
  struct tm  *ptm;


  if (logging == true) {
    /* Don't log if the file is not opened */
    if (logFp != NULL) {

      // if print the time stamp
      if (timestamp == true) {
        time(&currtime);
        ptm = localtime(&currtime);
        strftime(dtstr, sizeof(dtstr), "%b %d %Y %T", ptm);
        dtstr[strlen(dtstr)] = '\0';
        fprintf(logFp, "%s ", dtstr);
      }

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
  }
  else {  /* write to stderr */
      // if print the time stamp
      if (timestamp == true) {
        time(&currtime);
        ptm = localtime(&currtime);
        strftime(dtstr, sizeof(dtstr), "%b %d %Y %T", ptm);
        dtstr[strlen(dtstr)] = '\0';
        fprintf(stderr, "%s ", dtstr);
      }

      if (msgLvl != NULL) {
        // logging message level
        fprintf(stderr, "%s ", msgLvl);
      }

      // Now print the caller's message
      va_start(argList, format_str);
      vfprintf(stderr, format_str, argList);
      fprintf(stderr, "\n");
      va_end(argList);
  }

} /* end of diagLog */


/*
 * Log information to the test result file
 *
 * Input:
 *   timestamp:
 *     true:  log information with timestamp
 *     false: log information without timestamp
 *
 * Output:
 */
void dtrLog(bool timestamp, const char *format_str, ...)
{
  va_list     argList;
  time_t      currtime;
  char        dtstr[50];
  struct tm  *ptm;

  /* Don't log if the file is not opened */
  if (testResultsFp != NULL) {
    // if print the time stamp
    if (timestamp == true) {
      time(&currtime);
      ptm = localtime(&currtime);
      strftime(dtstr, sizeof(dtstr), "%Y/%m/%d %H:%M:%S", ptm);
      dtstr[strlen(dtstr)] = '\0';
      fprintf(testResultsFp, "%s ", dtstr);
    }

    // Now print the caller's message
    va_start(argList, format_str);
    vfprintf(testResultsFp, format_str, argList);
    fprintf(testResultsFp, "\n");
    va_end(argList);
    fflush(testResultsFp);
  }

} /* end of dtrLog */


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

