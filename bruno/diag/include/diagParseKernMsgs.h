/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics monitor related data structures and definitions.
 */

#ifndef _DIAG_PARSE_KERN_MSGS_H_
#define _DIAG_PARSE_KERN_MSGS_H_


#define DIAG_MSG_MAXLINELEN     256


/* Definition of Kernel message level */
typedef enum {
  DIAG_KERN_EMERG   = 0,        /* System is not usable       */
  DIAG_KERN_ALERT   = 1,        /* Action must be taken       */
  DIAG_KERN_CRIT    = 2,        /* Critical condition         */
  DIAG_KERN_ERR     = 3,        /* Error condition            */
  DIAG_KERN_WARNING = 4,        /* Warning condition          */
  DIAG_KERN_NOTICE  = 5,
  DIAG_KERN_INFO    = 6,
  DIAG_KERN_DEBUG   = 7,
  DIAG_KERN_MSG_MAX = 30
} diag_kern_msg_levels;


#define DELIM_DKMSG       "dkmsg="
#define DELIM_DKMSG_LEN   strlen((char *)DELIM_DKMSG)

/* Possible settings of DELIM_DACT */
#define DIAG_PARSE_ACT_LOG_ONLY_STR     "dact=log"       /* log only */
#define DIAG_PARSE_ACT_LOG_ONLY_STR_LEN strlen((char *)DIAG_PARSE_ACT_LOG_ONLY_STR)

#define DIAG_PARSE_ACT_HW_ERR_STR       "dact=hwerr"    /* log and inform HW error */
#define DIAG_PARSE_ACT_HW_ERR_STR_LEN   strlen((char *)DIAG_PARSE_ACT_HW_ERR_STR)

/* Definitions for dact member of structure diag_dkmsg_t */
typedef enum {
  DIAG_PARSE_ACT_NONE     = 0,
  DIAG_PARSE_ACT_LOG_ONLY = 1,
  DIAG_PARSE_ACT_HWERR    = 2,
  DIAG_PARSE_ACT_MAX
} diag_parse_dact;


/* Possible settings of DELIM_DTOKEN */
#define DIAG_PARSE_W_DTOKEN_STR       "dtoken=1"         /* Token in DKMSG */
#define DIAG_PARSE_W_DTOKEN_STR_LEN   strlen((char *)DIAG_PARSE_W_DTOKEN_STR)

#define DIAG_PARSE_WO_DTOKEN_STR      "dtoken=0"         /* No token in DKMSG */
#define DIAG_PARSE_WO_DTOKEN_STR_LEN   strlen((char *)DIAG_PARSE_WO_DTOKEN_STR)


/* Specify the monitored message error level */
#define DIAG_PARSE_MSG_LEVEL_STR        "msglvl="       /* specify the msg error level */
#define DIAG_PARSE_MSG_LEVEL_STR_LEN   strlen((char *)DIAG_PARSE_MSG_LEVEL_STR)

/* Specify the monitored message error code */
#define DIAG_PARSE_MSG_CODE_STR        "code="       /* specify the msg error code */
#define DIAG_PARSE_MSG_CODE_STR_LEN   strlen((char *)DIAG_PARSE_MSG_CODE_STR)

/*
 * The message error level is based on
 * 1) Type of kernel error messages,
 * 2) Statistics counters.
 * 3) Defined in the various APIs of diagd
 */
typedef enum {
  DIAG_LOG_MSG_LVL_CRIT_ERR         = 0,    /* Critical error- including KERN_EMERG,
                                             * DIAG_KERN_ALERT.
                                             */
  DIAG_LOG_MSG_LVL_SIGNIFICANT_ERR  = 1,    /* Suggesting possible HW error. */
  DIAG_LOG_MSG_LVL_SW_ERR           = 2,    /* Software error                */
  DIAG_LOG_MSG_LVL_WARNING          = 3,    /* Warning                       */
  DIAG_LOG_MSG_LVL_INFO             = 4,
  DIAG_LOG_MSG_LVL_MAX
} diag_log_msg_err_levels;


/* Token in DELIM_DKMSG string */
#define DIAG_PARSE_DKMSG_STR_TOKEN    "@@@"

/* Definitions for dtoken member of structure diag_dkmsg_t */
typedef enum {
  DIAG_PARSE_DTOKEN_NONE      = 0,
  DIAG_PARSE_DTOKEN_EMBEDDED  = 1,
  DIAG_PARSE_DTOKEN_MAX
} diag_parse_dtoken_;


/*
 * Refer to "howto_create_kern_msg.txt"
 * After split, each line would as
 * dtoken - 0: indicate there is no token in the dkmsg string.
 *          1: indicate there is at least one token in the dkmsg string.
 *          dtoken string is @@@
 * dact   - DIAGD_LOG:   indicate log the message
 *          DIAGD_HWERR: indicate it's a hardware releated issue, log the flag
 *                       to inform user.
 * msglvl - Specify the message error level of the monitored message
 * pDkmsg - Point to the monitored kernel string to be compared.
 */
typedef struct _diag_dkmsg_ {
  unsigned char   dtoken;       /* 1 - data is valid in the database. */
  unsigned char   dact;         /* Refer to diag_parse_dact           */
  unsigned char   msglvl;       /* Refer to diag_log_msg_err_levels   */
  unsigned short  code;         /* refer to ERROR_CODE_.... defined in diagError.h */
  char           *pDkmsg;       /* Point to the monitored message     */
} diag_dkmsg_t;


typedef struct _diag_logmsg_info_ {
  unsigned char   dact;         /* Refer to diag_parse_dact               */
  unsigned char   kmsgErrLevel; /* Refer to enum diag_log_msg_err_levels  */
  unsigned short  code;         /* refer to ERROR_CODE_.... defined in diagError.h */
  char           *pDkmsg;       /* Point to the kerel message to be logged*/
} diag_logmsg_info_t;


extern int diagd_log_msg_and_alert(unsigned char dact, char *timestamp, unsigned char kmsgErrLevel, unsigned short code, char *pDkmsg);

#define DIAGD_LOG_ALERT_HANDLER(_dact, _timestamp, _kmsgErrLevel, _code, _pDkmsg) { \
  diagd_log_msg_and_alert(_dact, _timestamp, _kmsgErrLevel, _code, _pDkmsg); \
}

extern void diagUpdateErrorCount(char *timestamp, unsigned short errorCode);
extern void diagUpdateWarnCount(char *timestamp, unsigned short errorCode);
extern diagMocaErrCounts_t  *diagMocaErrCntsPtr;
extern diagGenetErrCounts_t *diagGenetErrCntsPtr;
extern diagMtdNandErrCounts_t  *diagMtdNandErrCntsPtr;
extern diagSpiErrCounts_t   *diagSpiErrCntsPtr;

/*
 * Prototypes
 */

int Diag_Mon_ParseExamine_KernMsg(void);

#endif /* end of _DIAG_PARSE_KERN_MSGS_H_ */
