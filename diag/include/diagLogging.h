/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * Logging related defines
 *
 */

#ifndef _DIAG_LOGGING_H
#define _DIAG_LOGGING_H


/*
 * diagd logging macros and definitions
 * - Log files need to be persistent data
 */
#define DIAGD_LOG_DIR            "/user/diag/log"
#define DIAGD_LOG_FILE           "/user/diag/log/diagd.log"
#define DIAGD_TEST_RESULTS_FILE  "/user/diag/log/diagd_test_results.log"
#define DIAGD_MOCA_LOG_FILE      "/user/diag/log/diagd_MoCA.log"


/* If defined, log the message into the diagd log file */
#define DIAGD_LOGGING_ON

#if 1
 /* If defined, log the message into the diagd_test_results.log log file */
 #define DIAG_TEST_RESULT_LOGGING_ON
#endif


#define DIAGD_CRIT_MSG      "<CRIT>"
#define DIAGD_ALERT_MSG     "<ALERT>"
#define DIAGD_SWERR_MSG     "<SWERR>"
#define DIAGD_WARN_MSG      "<WARN>"
#define DIAGD_INFO_MSG      "<INFO>"


/*
 * diagd diagnostics test results definitions
 */

#define DIAG_LOG_GET_TIME(dtstr, dtstr_len)  { \
  time_t    currtime; \
  struct tm   *ptm;   \
  time(&currtime);    \
  ptm = localtime(&currtime); \
  /* the converted string of strftime includes null byte */ \
  strftime(dtstr, dtstr_len, "%Y/%m/%d %H:%M:%S", ptm); \
}

#ifdef DIAGD_LOGGING_ON

  #define DIAGD_LOG_INFO(format, args...) \
    tDiagLog(DIAGD_INFO_MSG, format, ## args)

  #define DIAGD_LOG_WARN(format, args...) \
    tDiagLog(DIAGD_WARN_MSG, format,  ## args)

  #define DIAGD_LOG_SWERR(format, args...) \
    tDiagLog(DIAGD_SWERR_MSG, format,  ## args)

  #define DIAGD_LOG_ALERT(format, args...) \
    tDiagLog(DIAGD_ALERT_MSG, format,  ## args)

  #define DIAGD_LOG_CRIT(format, args...) \
    tDiagLog(DIAGD_CRIT_MSG, format,  ## args)

  #define DIAGD_LOG(format, args...) \
    tDiagLog(NULL, format,  ## args)

  #define DIAGD_LOG_W_TS(format, args...) \
    diagLog(NULL, format,  ## args)
#else

  #define DIAGD_LOG_INFO(format, args...) { \
  {   \
    printf(format"\n", ## args); \
  }
  #define DIAGD_LOG_WARN(format, args...) { \
  {   \
    printf(format"\n", ## args); \
  }
  #define DIAGD_LOG_SWERR(format, args...) { \
  {   \
    printf(format"\n", ## args); \
  }
  #define DIAGD_LOG_ALERT(format, args...) { \
  {   \
    printf(format"\n", ## args); \
  }
  #define DIAGD_LOG_CRIT(format, args...) { \
  {   \
    printf(format"\n", ## args); \
  }
  #define DIAGD_LOG(format, args...) { \
  {   \
    printf(format"\n", ## args); \
  }

#endif /* end of DIAGD_LOGGING_ON */


#ifdef DIAG_TEST_RESULT_LOGGING_ON

  /* Log time and test type */
  #define RESULT_TITLE_LOG(format, args...)   tDtrLog(format,  ## args)


  /* More detail information of test results which follows RESULT_TITLE_LOG */
  #define RESULT_LOG(format, args...)     dtrLog(format,  ## args)

#else
  #define RESULT_TITLE_LOG(format, args...) { \
  {   \
    printf(format"\n", ## args); \
  }
  #define RESULT_LOG(format, args...) { \
  {   \
    printf(format"\n", ## args); \
  }
#endif /* end of DIAG_TEST_RESULT_LOGGING_ON */


#define RESULT_LOG_SEPARATOR()  \
          RESULT_LOG("\n---------------------------------------------------")

#ifdef DIAGD_LOG_ROTATE_ON
  #define KBYTE_SZ 1024
  #define MAX_ROTATE_SZ  (256 * KBYTE_SZ)
  #define MAX_NUM_OF_ROTATE_FILES 10
#endif

#define MAX_BUF_LEN 256


/* Prototypes */
int diagtOpenTestResultsLogFile(void);
void diagtCloseTestResultsLogFile(void);
int diagtOpenEventLogFile(void);
void diagtCloseEventLogFile(void);
int diagtOpenMocaLogFile(void);
void diagtCloseMocaLogFile(void);
void diagLog(const char *msgLvl, const char *format_str, ...);
void tDiagLog(const char *msgLvl, const char *format_str, ...);
void dtrLog(const char *format_str, ...);
void tDtrLog(const char *format_str, ...);
void diagMocaLog(char *pLogMsg);
void diagMocaStrLog(char *pLogMsg, PMoCA_STATUS pStatus);
void diagMocaMyStatusLog(char *dtstr, PMoCA_STATUS pStatus);
void diagLogRotate(void);
void diagUploadLogFile(void);

#endif  // _DIAG_LOGGING_H
