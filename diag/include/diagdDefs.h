/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * General diagd definitions
 *
 */

#ifndef _DIAGD_DEFS_H_
#define _DIAGD_DEFS_H_


#ifdef TRUE
 #undef TRUE
#endif
#define TRUE    1

#ifdef FALSE
 #undef FALSE
#endif
#define FALSE   0

#ifndef BIT
 #define BIT(x)  (1L << x)
#endif


/* Return status
 * NOTES -
 *  Reserve values which starts from 0x9000 to CmsRet in BRCM's cms.h
 */
typedef enum {
  DIAGD_RC_OK = 0,                          /* No error                   */
  DIAGD_RC_ERR = 1,                         /* generic error code         */
  DIAGD_RC_FAILED_OPEN_LOG_FILE = 2,
  DIAGD_RC_FAILED_OPEN_NETLINK_SOCKET = 3,
  DIAGD_RC_NO_NETIF_ENTRY_AVAIL = 4,        /* No netif entry avail       */
  DIAGD_RC_FAILED_OPEN_MOCAD = 5,           /* Fail to open mocad         */
  DIAGD_RC_OUT_OF_MEM = 6,                  /* Fail to allocate memory    */
  DIAGD_RC_PTHREAD_WAIT_TIMEOUT = 7,        /* Used as ETIMEDOUT          */
} diag_rtn_code;


#define DIAG_RESULT_MSG_MAX_LEN   256

/* It is for the release build */
#define DIAG_REL_BUILD

#ifdef DIAG_REL_BUILD
  /* If defined, print out error message to stderr */
  #define DIAGD_PERROR_ON
  #define DIAGD_ERROR_ON
#else
  /* If defined, print out debugging message to stdout */
  #define DIAGD_TRACE_ENTRY_ON
  /* If defined, print out debugging message to stdout */
  #define DIAGD_TRACE_ON
  /* If defined, print out error message to stderr */
  #define DIAGD_DEBUG_ON
  /* If defined, print out error message to stderr */
  #define DIAGD_PERROR_ON
#endif /* end of DIAG_REL_BUILD */

#ifdef DIAGD_MOCA_LOGGING_ON
  #undef DIAGD_MOCA_LOGGIN_ON
#endif

/*
 * Debugging macro
 */

/*
 * TODO: Change to logging mechanism (refer to syslog(3))
 */
#ifdef DIAGD_TRACE_ENTRY_ON
  #define DIAGD_ENTRY(format, args...)  \
    fprintf(stdout, "%s: Entry >>> "format"\n", MOD_NAME, ## args)

  #define DIAGD_EXIT(format, args...)   \
    fprintf(stdout, "%s: Exit <<< "format"\n", MOD_NAME, ## args)

#else
  #define DIAGD_ENTRY(format, args...)
  #define DIAGD_EXIT(format, args...)
#endif /* end of DIAGD_TRACE_ENTRY_ON */

#ifdef DIAGD_TRACE_ON
  #define DIAGD_TRACE(format, args...)  \
    fprintf(stdout, "%s: "format"\n", MOD_NAME, ## args)

#else
  #define DIAGD_TRACE(format, args...)
#endif /* end of DIAG_DBG_ON */


#ifdef DIAGD_DEBUG_ON
  #define DIAGD_DEBUG(format, args...)  { \
    fprintf(stderr, "%s: "format"\n", MOD_NAME, ## args);  \
  }
#else
  #define DIAGD_DEBUG(format, args...)
#endif /* end of DIAG_DBG_ON */

#ifdef DIAGD_PERROR_ON
  #define DIAGD_PERROR(format)  perror(format)
#else
  #define DIAGD_PERROR(format)
#endif /* end of DIAG_DBG_ON */

#ifdef DIAGD_ERROR_ON
#define DIAGD_ERROR(format, args...)  { \
    fprintf(stderr, "%s: "format"\n", MOD_NAME, ## args);  \
  }
#else
  #define DIAGD_ERROR(format, args...)
#endif /* end of DIAGD_ERROR_ON */


#endif /* end of _DIAGD_DEFS_H_ */
