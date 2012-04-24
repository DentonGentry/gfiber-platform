/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides parsing dmsg and monitoring kernel message.
 * If any error or warning messaged matched in either
 * 1) /user/diag/diag_kern_err_msgs.tx
 * 2) or /user/diag/diag_kern_warn_msgs.txt
 *
 * For the formate of the above two files, please refer to 
 * HOWTO_create_kern_msg.txt
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
#define COMMAND_LEN 80
#define DATA_SIZE   30


/* Lists of kernel critical/error messages to be monitoring. */
#define KERN_ERR_MSGS_FILE    "/user/diag/diag_kern_err_msgs.txt"

/* Lists of kernel warning messages to be monitoring. */
#define KERN_WARN_MSGS_FILE   "/user/diag/diag_kern_warn_msgs.txt"

/* monitoring kernel message from /proc/kmsg */
#define KERN_PROC_KMSG_FS     "/proc/kmsg"

/* Instead of using /proc/kmsg, monitor the one defined in "/etc/syslog.conf".
 * Currently for the kernel messages of priority level from warning to critical,
 * it is directing to  "/var/log/kern.log". For the priority level from
 * alert and above, it is directing to "/var/log/kern0.log".
 */
#define KERN_SYSLOG_KMSG_FS           "/var/log/kern.log"
#define KERN_SYSLOG_PRECEDING_STR     "kernel:"
#define KERN_SYSLOG_PRECEDING_STR_SZ  strlen(KERN_SYSLOG_PRECEDING_STR)

#define DIAGD_DB_FS                   "/user/diag/diagdb.bin"
#define NUMBYTES                      (1024)
#define FILESIZE                      (NUMBYTES * sizeof(char))
#define DEFAULT_UTC_TS                "Jan  1 1970 00:00:00"
#define DEFAULT_UTC_TS_SZ             strlen(DEFAULT_UTC_TS)
#define DIAGD_FILE_POS_INDEX          (DEFAULT_UTC_TS_SZ +1)
#define DIAGD_MOCA_ERR_COUNTS_INDEX   (DIAGD_FILE_POS_INDEX + sizeof(long))
#define DIAGD_GENET_ERR_COUNTS_INDEX  (DIAGD_MOCA_ERR_COUNTS_INDEX + DIAG_MOCA_ERR_COUNTS_SZ)
#define DIAGD_NAND_ERR_COUNTS_INDEX   (DIAGD_GENET_ERR_COUNTS_INDEX + DIAG_GENET_ERR_COUNTS_SZ)
#define DIAGD_MCE_ERR_COUNTS_INDEX    (DIAGD_NAND_ERR_COUNTS_INDEX + DIAG_NAND_ERR_COUNTS_SZ)

/* "Mmm dd hh:mm:ss" is the timestamp format in the very beginning of
 * a kernel error/warning message.
 */
#define KERN_SYSLOG_TS_FORMAT         "Mmm dd hh:mm:ss"
#define KERN_SYSLOG_TS_SZ             strlen(KERN_SYSLOG_TS_FORMAT)
#define KERN_SYSLOG_TS_INDEX          KERN_SYSLOG_TS_SZ


/* Log message level in string.
 * NOTE - Must sync up to the definition of diag_log_msg_err_levels.
 */
const char *diagd_logmsg_lvl[] = {
  DIAGD_CRIT_MSG, 
  DIAGD_ALERT_MSG, 
  DIAGD_SWERR_MSG, 
  DIAGD_WARN_MSG, 
  DIAGD_INFO_MSG
};

/*
 * This routine splits input string which read from
 * diag_kern_err_msgs.txt or diag_kern_warn_msgs.txt
 * 
 * Input:
 * pMst -       Point to a string to be split.
 * pDkmsgInfo - information of the monitored kmsg if the message is split OK
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
static int diag_parse_dkmsg_split(char *pMsg, diag_dkmsg_t *pDkmsgInfo)
{
  char *str_ptr = NULL;
  char *tmpPtr = NULL;
  char *pMsgTmp = pMsg;
  int   rtn = DIAGD_RC_ERR;
  int   tokenLen;

  DIAGD_TRACE("%s: pMsg - %s", __func__, pMsg);

  do {
    /* Clear the memory */
    memset(pDkmsgInfo, 0, sizeof(*pDkmsgInfo));

    /* Parse "dtoken" token setting */
    if ((str_ptr = strstr(pMsgTmp, (char *)DIAG_PARSE_WO_DTOKEN_STR)) != NULL) {
      pDkmsgInfo->dtoken = DIAG_PARSE_DTOKEN_NONE;
      tokenLen = DIAG_PARSE_WO_DTOKEN_STR_LEN;
    }
    else if ((str_ptr = strstr(pMsgTmp, DIAG_PARSE_W_DTOKEN_STR)) != NULL) {
      pDkmsgInfo->dtoken = DIAG_PARSE_DTOKEN_EMBEDDED;
      tokenLen = DIAG_PARSE_W_DTOKEN_STR_LEN;
    }
    else {
      break;            /* Bad. Couldn't find the token */
    }

    /* To speed up, we don't want to search from the beginning of string.
     * Move the pointer to the end of "dtoken" token
     */
    pMsgTmp = str_ptr + tokenLen;

    /* Parse "dact" token setting */
    if ((str_ptr = strstr(pMsgTmp, DIAG_PARSE_ACT_LOG_ONLY_STR)) != NULL) {
      pDkmsgInfo->dact = DIAG_PARSE_ACT_LOG_ONLY;
      tokenLen = DIAG_PARSE_ACT_LOG_ONLY_STR_LEN;
    }
    else if ((str_ptr = strstr(pMsgTmp, DIAG_PARSE_ACT_HW_ERR_STR)) != NULL) {
      pDkmsgInfo->dact = DIAG_PARSE_ACT_HWERR;
      tokenLen = DIAG_PARSE_ACT_HW_ERR_STR_LEN;
    }
    else {
      break;            /* Bad. Couldn't find the token */
    }

    /* Same reason. Move the pointer to the end of "msglvl" token */
    pMsgTmp = str_ptr + tokenLen;

    /* Parse "msglvl" token setting */
    if ((str_ptr = strstr(pMsgTmp, DIAG_PARSE_MSG_LEVEL_STR)) != NULL) {
      str_ptr += DIAG_PARSE_MSG_LEVEL_STR_LEN;
      tokenLen = DIAG_PARSE_MSG_LEVEL_STR_LEN;
      pDkmsgInfo->msglvl = str_ptr[0] - '0';
      if (pDkmsgInfo->msglvl >= DIAG_LOG_MSG_LVL_MAX)
        break;          /* Bad. Invalid message error level. Couldn't find the token */
    }
    else {
      break;            /* Bad. Couldn't find the token */
    }

    pMsgTmp = str_ptr;

    /* Parse "code" token setting */
    if ((str_ptr = strstr(pMsgTmp, DIAG_PARSE_MSG_CODE_STR)) != NULL) {
      str_ptr += DIAG_PARSE_MSG_CODE_STR_LEN;
      tokenLen = DIAG_PARSE_MSG_CODE_STR_LEN;
      if ((tmpPtr = strchr(str_ptr, ' ')) != NULL) {
         tmpPtr[0] = '\0';
         sscanf(str_ptr, "%4x", (unsigned int *) &(pDkmsgInfo->code));
         tmpPtr[0] = ' ';
      }
      else {
         break;            /* Bad. Couldn't find the space after "code" token */
      }
    }
    else {
      break;            /* Bad. Couldn't find the token */
    }

    /* Same reason. Move the pointer to the end of "code" token */
    pMsgTmp = str_ptr + tokenLen;

    /* Parse "pDkmsg" token to get the monitored message */
    str_ptr = strstr(pMsgTmp, DELIM_DKMSG);
    if (str_ptr == NULL)
      break;            /* Bad. Couldn't find the token. Exit. */

    /* Point to the starting addr of the monitored kernel messages. */
    pDkmsgInfo->pDkmsg = str_ptr + DELIM_DKMSG_LEN;

    rtn = DIAGD_RC_OK;

  } while (0);

  DIAGD_TRACE("%s: rtn=%d, dtoken=%d, dact=%d, msglvl=%d, code =%4x, pDkmsg=%s",
              __func__, rtn, pDkmsgInfo->dtoken, pDkmsgInfo->dact, 
              pDkmsgInfo->msglvl, pDkmsgInfo->code, pDkmsgInfo->pDkmsg);

  if (rtn != DIAGD_RC_OK) {
/* ==> TODO 2011/10/06 Log as an SW error to indicate bad string in the file */
    DIAGD_LOG_SWERR("%s - Failed to parse %s", __func__, pMsg);
  }
  
  return (rtn);
  
} /* end of diag_parse_dkmsg_split */


/*
 * This routine logs messages and alert if any hardware related issue occurred.
 * 
 * Input:
 * pLogMsgInfo  -  Point to information of the logged message 
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagd_log_msg_and_alert(unsigned char dact, char *timestamp, unsigned char kmsgErrLevel, unsigned short code, char *pDkmsg)
{
  int   rtn = DIAGD_RC_OK;

  DIAGD_TRACE("%s", __func__);
  
  do {
    /* Check the "dact" setting if it is a hardware error */
    if (dact == DIAG_PARSE_ACT_HWERR) {
      /* It is hardware related error. */
      /* TODO 2011/10/O6 - 
       *    1) HOW TO alarm the error???
       *    2) Save the error to file in case of Bruno power-cycle???
       */
      DIAGD_DEBUG("%s: TODO - detect (%d) error. Alarm....", __func__, dact);
    }
    
    /* For just in case, sanity check */
    if (kmsgErrLevel >= DIAG_LOG_MSG_LVL_MAX) {
      /* We shouldn't get here.
       * If get here, set the message level to be INFO level
       */
      kmsgErrLevel = DIAG_LOG_MSG_LVL_INFO;
    }

    if (pDkmsg != NULL) {
      /* Log the message with timestamp to the diagd log file */
      DIAGD_LOG_W_TS("%s %s %4x %s", timestamp, diagd_logmsg_lvl[kmsgErrLevel], code, pDkmsg);
    }
  } while (0);

  DIAGD_TRACE("%s: rtn - %d", __func__, rtn);

  return (rtn);
  
} /* end of diagd_log_msg_and_alert */


/*
 * This routine does as follows:
 * 1) Read monitored (KERN_WARN or KERN_ERR) kernel messages 
 * 2) Compare to the input message which read from /var/log/kern.log
 * 3) If the input kernel msg is a monitored msg, 
 *
 * If it is a monitored message, inform calling routine.
 * Assumption:
 *  If the "pFileName" file can not be opened, assume that no kernel message
 *  need to be looked up.
 *
 * Input:
 * pKernMsg         - Point to a kernel message not included message level
 * kernMsgErrLevel  - Kernel message level of pKernMs
 * pFileName        - Point of filename to read the monitored messages.
 * 
 * Output:
 * true   - found a matched message in file.
 * false  - no message matched.
 */
bool diag_parse_cmp_dkmsg(char *pKernMsg, char *pFileName, char *timestamp)
{
  FILE         *ifp;
  char          monitoredKernMsg[DIAG_MSG_MAXLINELEN];
  char         *pMonMsg;
  char         *pMonMsgToken;
  char         *pKernMsgTmp;
  diag_dkmsg_t  dkmsgInfo;
  bool          msgMatched = false;


  DIAGD_TRACE("%s: pKernMsg=%s, FN=%s", __func__, pKernMsg, pFileName);

  do 
  {

    ifp = fopen(pFileName, "r");
    if (ifp == NULL) {
        /* TODO 2011/10/O3 - 
         *  TBD - log the error message.
         *  TBD - inform user???
         */
        DIAGD_TRACE("Can not open the %s file", pFileName);
        break;
    }

    while (fgets(monitoredKernMsg, DIAG_MSG_MAXLINELEN, ifp) != NULL) {
      
      monitoredKernMsg[strcspn(monitoredKernMsg, "\n")] = '\0';
      if (monitoredKernMsg[0] == '\0') {
          continue;
      }
      DIAGD_TRACE("msg= %s", monitoredKernMsg);

      /* Split the Msg */
      if (diag_parse_dkmsg_split(monitoredKernMsg, &dkmsgInfo) != 0) {
        DIAGD_DEBUG("%s: Failed to split the msg (msg=%s)", 
                    __func__, monitoredKernMsg);
        continue;             /* Failed to split the msg. Go to next msg */
      }

      /* 
       * To the message pointed by dkmsgInfo.pDkmsg,
       * 1) If the monitored message doesn't have any tokens (variables), compare 
       *    two messages directly.
       * 2) If the monitored message has token(s), compare the known string
       *    portions.
       */
      if (dkmsgInfo.dtoken == DIAG_PARSE_DTOKEN_NONE) {
        
        /* No token embedded in the monitored message. */
        if (strcmp(pKernMsg, dkmsgInfo.pDkmsg) == 0) {
          /* The pKernMsg is a monitored message. Handle accordingly */
          msgMatched = true;      /* matched. Exit. */
          break;
        }
      }
      else {
        /* Token(s) are embedded in the monitored message:
         * 1. Get the starting offset of the 1st token.
         * 2. Compare dkmsgInfo.pDkmsg and pKernMsg up to the starting offset
         *    of the token
         * 3. If identical, get the 2nd token and repeat steps 2 and 3 until
         *    reach to end of message.
         */
        pMonMsg = dkmsgInfo.pDkmsg;
        pKernMsgTmp = pKernMsg;

        while (1) {

          pMonMsgToken = strstr(pMonMsg, DIAG_PARSE_DKMSG_STR_TOKEN);

          if (pMonMsgToken != NULL) {
            /* Compare the substring up to the token */
            pMonMsgToken[0] = '\0';
          }

          pKernMsgTmp = strstr(pKernMsgTmp, pMonMsg);
          if (pKernMsgTmp == NULL) {
            msgMatched = false;
            break;                /* Mismatch. Next msg. */
          }

          /* Matched to up to the token */
          if (pMonMsgToken != NULL) {
            /* Move the pointer after the token charactors before search 
             * the next token in the message.
             */
            pMonMsg = pMonMsgToken + strlen((char *)DIAG_PARSE_DKMSG_STR_TOKEN);
          }
          else  {
            /* Reach to the end of msg. Set the flag and break 
             * from the while loop
             */
            msgMatched = true;      /* Matched */
            break;
          }

        } /* end of while (1) */

        /* Check if found the message */
        if (msgMatched == true) {
          break;
        }
      } /* end of while (fgets(monitoredKernMs.... */

    } /* end of if (dkmsgInfo.dtoken = DIAG_PARSE_DTOKEN_NONE) */

  } while (0);

  DIAGD_TRACE(":%s: msgMatched=%s\n", __func__, (msgMatched==true)? "true" : "false");

  /* If found a matched message, log the kernel message and handle per the dact setting */
  if (msgMatched == true) {
    DIAGD_LOG_ALERT_HANDLER(dkmsgInfo.dact, 
                            timestamp,
                            dkmsgInfo.msglvl, 
                            dkmsgInfo.code,
                            pKernMsg);
    
    /* update diag Error or Warning Count */
    diagUpdateErrorCount(timestamp, dkmsgInfo.code);
  }


  if (ifp != NULL)
    fclose(ifp);

  return(msgMatched);

} /* end of diag_parse_cmp_dkmsg */


int get_diagDb_mmap(char **mapPtr)
{
    int result;
    int fd;
    char *map = (char *) NULL;
    long *filePos = (long *) NULL;
    bool isNewFile = false;

    /* Open diagd database file for read and write.
     *  - Creating the file if it doesn't exist.
     *
     */
    if (access(DIAGD_DB_FS, F_OK) != 0) {
       /* file does not exist then create */
       fd = open(DIAGD_DB_FS, O_RDWR | O_CREAT, (mode_t)0644);
       if (fd == -1) {
          DIAGD_TRACE("Error create and open file %s for read and write!", DIAGD_DB_FS);
          return(-1);
       }

    /* Stretch the file size to the size of the (mmapped) array of ints
     */

       result = lseek(fd, FILESIZE-1, SEEK_SET);
       if (result == -1) {
          close(fd);
          DIAGD_TRACE("Error calling lseek() to stretch the file %s", DIAGD_DB_FS);
          return(-1);
       }

    /* Something needs to be written at the end of the file to
     * have the file actually have the new size.
     * Just writing an empty string or actually '\0'
     * at the current file position will do.
     *
     */
       result = write(fd, "", 1);
       if (result != 1) {
          close(fd);
          DIAGD_TRACE("Error writing last byte of the file %s", DIAGD_DB_FS);
          return(-1);
       }
       
       isNewFile = true;
    }
    else {
    /* diagd database file already exist. Open this file for read and write */
       fd = open(DIAGD_DB_FS, O_RDWR, (mode_t)0644);
       if (fd == -1) {
          DIAGD_TRACE("Error opening file %s for writing", DIAGD_DB_FS);
          return(-1);
       }
    }



    /* Now the file is ready to be mmapped.
     */
    map = mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        DIAGD_TRACE("Error mmapping the file %s", DIAGD_DB_FS);
        return(-1);
    }

    if (isNewFile) {
       /*
        * write default data to diagd database:
        *    timestamp "Jan  1 1970 00:00:00"
        *    filePosPrevRun = 0
        *    all error and warning counts = 0
        */
        strcpy(map, DEFAULT_UTC_TS);
        map[DEFAULT_UTC_TS_SZ] = '\0';
        filePos = (long *) &(map[DIAGD_FILE_POS_INDEX]);
        *filePos = (long) 0L;

        DIAGD_DEBUG("\nDIAG_ALL_ERR_COUNTS_SZ = %d\n", DIAG_ALL_ERR_COUNTS_SZ);
        memset(&map[DIAGD_MOCA_ERR_COUNTS_INDEX], 0, DIAG_ALL_ERR_COUNTS_SZ);
    }

    *mapPtr = map;

    return fd;
}

extern char *strptime(char *s, char*format, struct tm *tm);
/*
 * 1) Read kernel messages from a /var/log/kern.log
 * 2) If a kernel message level is KERN_EMERG or KERN_ALERT, (TBD) system issue
 * 3) If a kernel message level is KERN_CRIT or KERN_ERR, (TBD) compare to
 *    the list if it is HW related issues. 
 * If the message is a monitored message, handle based on the dact setting.
 *
 * Input:
 * 
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int Diag_Mon_ParseExamine_KernMsg(void)
{
  FILE *ifp = NULL;
  int   fd;
  int   flags;
  char  kmsgMsg[DIAG_MSG_MAXLINELEN];
  int   rtn = DIAGD_RC_OK;
  bool  msgFound = false;
  char  kernMsgErrLevel = DIAG_KERN_MSG_MAX;
  char  *kMsgPtr = NULL;
  long  currentFilePos = (long) 0L;
  long  *filePosPrevRun = (long *) NULL;
  char   newtimeStr[24];
  time_t now, newtime, diagdTimeStamp;
  struct tm tm, *ptm, diagdTm;
  static int    thisYear;
  int    diagdFd = 0;
  char   *diagdMap = NULL;

  DIAGD_TRACE("%s: enter", __func__);

  do 
  {
    if (diag_chkKernMsg_firstRun == false) {
      if (checkIfTimeout(DIAG_API_IDX_GET_CHK_KERN_KMSG) == false) {
        break;        /* Wait time is not expired. Exit */
      }
    }
    else {
      /* It is first time running routine after power-up. */
       diag_chkKernMsg_firstRun = false;     /* Clear the flag. */

      /*
       * get current year since year is missing in the timestamp
       * of syslog kernel messages.
       */
       time(&now);
       ptm = localtime(&now);
       thisYear = ptm->tm_year;
    }

    /* get mmap of diag databse file */
    if ((diagdFd = get_diagDb_mmap(&diagdMap)) < 0) {
       DIAGD_DEBUG("get_diagDb_mmap failed");
       break;
    }
    /* read in timestamp and file position in the previous run from DIAGD_DB_FS */
    else {
       strptime(diagdMap, "%b %d %Y %T", &diagdTm);
       diagdTimeStamp = mktime(&diagdTm);
       filePosPrevRun  = (long *) &diagdMap[DIAGD_FILE_POS_INDEX];

       /* read in error and warning counts from DIAGD_DB_FS */
       diagMocaErrCntsPtr  = (diagMocaErrCounts_t *) &diagdMap[DIAGD_MOCA_ERR_COUNTS_INDEX];
       diagGenetErrCntsPtr = (diagGenetErrCounts_t *) &diagdMap[DIAGD_GENET_ERR_COUNTS_INDEX];
       diagNandErrCntsPtr  = (diagNandErrCounts_t *) &diagdMap[DIAGD_NAND_ERR_COUNTS_INDEX];
       diagMceErrCntsPtr   = (diagMceErrCounts_t *) &diagdMap[DIAGD_MCE_ERR_COUNTS_INDEX];

    }

    /* Update the starting time of the api */
    time(&diagStartTm_chkKernMsg);

    ifp = fopen(KERN_SYSLOG_KMSG_FS, "r");
    if (ifp == NULL) {
        DIAGD_DEBUG("Can not open the %s file", KERN_SYSLOG_KMSG_FS);
        rtn = DIAGD_RC_ERR;
        break;
    }

    /* 
     * /var/log/kern.log is opened OK.
     * fgets() is default to be "blocking". To prevent fgets blocks when the
     * kmsg doesn't have no new message, set the fp to be non-blocking.
     */
    fd = fileno(ifp);
    flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    if (*filePosPrevRun) {
       if (fseek(ifp, *filePosPrevRun, SEEK_SET) != 0) {
          /* fail to fseek may be caused by log file rotation or file is corrupted */
          DIAGD_DEBUG("Can not fseek  the %s file to position %ld", KERN_SYSLOG_KMSG_FS, *filePosPrevRun);
       }
    }

    while (true) {
      /* 
       * If there is no message available, fgets will return NULL with errno set
       * to EWOULDBLCOK.
       */
      if (fgets(kmsgMsg, DIAG_MSG_MAXLINELEN, ifp) == NULL) 
      {
        /* No message available in /var/log/kern.log. Exit. */
        DIAGD_DEBUG("No new kernel message available in the %s file", KERN_SYSLOG_KMSG_FS);
        break;
      }

      kmsgMsg[strcspn(kmsgMsg, "\n")] = '\0';
      if (kmsgMsg[0] == '\0') {
          continue;
      }
      else {
         /* read in timestamp within the kernel message */
         kmsgMsg[KERN_SYSLOG_TS_INDEX]='\0';
         strptime(kmsgMsg, "%b %d %T", &tm);
         DIAGD_TRACE("original  timestamp:%s", kmsgMsg);
         tm.tm_year = thisYear;

         strftime(newtimeStr, sizeof(newtimeStr), "%b %d %Y %T", &tm);
         DIAGD_TRACE("converted timestamp:%s", newtimeStr);
         kmsgMsg[KERN_SYSLOG_TS_INDEX] = ' ';

         /* timestamp of this to-be-processed
          * kernel message is older than diagdTimeStamp.
          * No need to parse this kernel message.
          * Save the current file position and continue.
          */
         if (difftime(diagdTimeStamp, (newtime = mktime(&tm))) > 0) {
            goto saveFilePos;
         }
         
         kMsgPtr = &kmsgMsg[0];
         /* set priority level to warning since the priority level of
          * logged kernel messages in kern.log is from warning to critical
          */
         kernMsgErrLevel = DIAG_KERN_WARNING;

         /* skip "kernel:" and space or tab to advance index to the beginning of main kernel message */
         if ((kMsgPtr = strstr(kMsgPtr, KERN_SYSLOG_PRECEDING_STR)) != NULL) {
            kMsgPtr+= KERN_SYSLOG_PRECEDING_STR_SZ;
            while(*kMsgPtr == ' ' || *kMsgPtr == '\t') {kMsgPtr++;}
         }
         else {
         /* Cannot locate main kernel message. Should not happen */
            DIAGD_TRACE("Cannot find \"kernel:\" in the kernel message:%s", kmsgMsg);
            kernMsgErrLevel = DIAG_KERN_MSG_MAX;
         }
      }

      if (kernMsgErrLevel != DIAG_KERN_MSG_MAX) {
         DIAGD_TRACE("kernMsgErrLevel=%d, pKernMsg: %s", kernMsgErrLevel, kMsgPtr);
      }

      switch (kernMsgErrLevel) {
        case DIAG_KERN_EMERG:
        case DIAG_KERN_ALERT:
          /* TODO: 2011/10/03
           * If the kernel message is KERN_EMERG or KERN_ALERT, don't need to
           * check if it is a monitored message. Do,
           * 1) log the message.
           * 2) TBD - Inform user error occurred 
           */
          break;

        case DIAG_KERN_CRIT:
        case DIAG_KERN_ERR:
        case DIAG_KERN_WARNING:
          /* 
           * If it is a monitored kernel error, warning messages, handle it based on the
           * dact setting.
           */
          msgFound = diag_parse_cmp_dkmsg(kMsgPtr, KERN_ERR_MSGS_FILE, newtimeStr);
          if (msgFound == false) {
             msgFound = diag_parse_cmp_dkmsg(kMsgPtr, KERN_WARN_MSGS_FILE, newtimeStr);
          }
          break;

        default:
          break;        /* don't care of the other level of kernel messages. */
      }
      
      DIAGD_TRACE("errmsg: %s", kmsgMsg);

      /* save new timestamp */
      diagdTimeStamp = newtime;
      strcpy(diagdMap, newtimeStr);
      diagdMap[strlen(newtimeStr)] = '\0';

saveFilePos:
      if ((currentFilePos = ftell(ifp)) < 0) {
         DIAGD_DEBUG("ftell failed to get current position of the %s file", KERN_SYSLOG_KMSG_FS);
         break;
      }
      else {
         /* save file position to DIAGD_DB_FS */
         *filePosPrevRun = currentFilePos;

        /*
         * diagd error and warning count have been updated to DIAGD_DB_FS
         * in diagUpdateErorrCount() for each matched kernel message.
         */
      }
    }

  } while (0); /* TODO 03092012 remove while(0) later */ 

  if (ifp != NULL) {
    fclose(ifp);
  }

  if (diagdFd > 0) {
     close(diagdFd);

    /*  unmap the mapped virtual memory address space of the diag database file
     */
     if (munmap(diagdMap, FILESIZE) == -1) {
        DIAGD_TRACE("Error un-mmapping the file");
     }
  }

  DIAGD_TRACE("%s: exit", __func__);

  return(rtn);

} /* end of Diag_Mon_ParseExamine_KernMsg */



/* ======================================================================= */
#ifdef AW_COMMENTOUT

int main(void)
{  
  int     looping = 0;

  do {

    if (diagtOpenEventLogFile() != DIAGD_RC_OK) {
      
      DIAGD_DEBUG("%s: Failed to open diag log file", __func__);
      break;
    }

#if 0
  /* DIAG_LOG_MSG_LVL_CRIT_ERR
   * DIAG_LOG_MSG_LVL_SIGNIFICANT_ERR
   * DIAG_LOG_MSG_LVL_SW_ERR
   * DIAG_LOG_MSG_LVL_WARNING
   * DIAG_LOG_MSG_LVL_INFO
   * DIAG_LOG_MSG_LVL_MAX
   *
   * DIAG_PARSE_ACT_LOG_ONLY
   * DIAG_PARSE_ACT_HWERR
   */
  DIAGD_LOG_ALERT_HANDLER(DIAG_PARSE_ACT_LOG_ONLY, 
                          DIAG_LOG_MSG_LVL_SIGNIFICANT_ERR, 
                          "DIAG_PARSE_ACT_LOG_ONLY  DIAG_LOG_MSG_LVL_SIGNIFICANT_ERR")
#endif 

#if 0
    looping = 1;
    Diag_Mon_ParseExamine_KernMsg();
    printf("sleep 5sec.....\n");
    sleep(5);
#endif 

#if 1
    diag_parse_cmp_dkmsg("moca_m2m_xfer: DMA interrupt timed out, status 3344", 
                         KERN_WARN_MSGS_FILE, "");
#endif /* end of 0 */

#if 0
  {
    diag_dkmsg_t    dkmsg_info;
    char msg[] = "dtoken=0 dact=hwerr msglvl=3 dkmsg=Auto config phy.\0";
    diag_parse_dkmsg_split(msg, &dkmsg_info);
  }
#endif 

  } while (looping);

  /* Close the log file if it's opened */
  diagtOpenEventLogFile();
  return 0;
}
#endif
/* ======================================================================= */
