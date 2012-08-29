/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics monitoring related functions
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
#define FILE_BUF_LEN  128
#define STR_BUF_LEN   32


/*--------------------------------------------------------------------------
 *
 * Global Variables
 *
 *--------------------------------------------------------------------------
 */
/* -------------------------------------------------------------------------
 *
 * Internal defines, types and variables
 *
 *--------------------------------------------------------------------------
 */
static diagWaitTimeTbl_t diagWaitTimeTbl[MAX_NUM_OF_MONITOR_TIMER] = {
  {"GET_NET_STATS", &diagWaitTime_getNetStats},
  {"CHK_KERN_MSGS", &diagWaitTime_chkKernMsgs},
  {"MOCA_CHK_ERRS", &diagWaitTime_MocaChkErrs},
  {"MOCA_MON_PERF", &diagWaitTime_MocaMonPerf},
  {"LOG_MON_ROTATION", &diagWaitTime_LogMonRotate}
};


/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */
/*
 * This routine updates the uint32_t data if input data is valid, otherwise
 * keep the default value.
 * 
 * Input:
 * pValue - Point to data string.
 * pData  - Point to data value that converted successfully.
 *
 * Output: None
 */
void diagSetUint32Value(char *pValue, uint32_t *pData)
{
  uint32_t value;
  int    rtn;

  if (pValue[0] == '0' && (pValue[1] == 'x' || pValue[1] == 'X'))
    rtn = sscanf(pValue + 2, "%lx", (unsigned long *)&value);  /* unsigned hexadecimal integer */ 
  else
    rtn = sscanf(pValue, "%lu", (unsigned long *)&value);      /* unsigned decimal integer */

  /* The return value is the number of input items successfully matched */
  if (rtn == 1)
    *pData = value;
} /* end of diagSetUint32Value */


/*
 * This routine updates the float data if input data is valid, otherwise
 * keep the default value.
 * 
 * Input:
 * pValue - Point to data string.
 * pData  - (OUT)Point to data value that converted successfully.
 *
 * Output: None
 */
void diagSetFloatValue(char *pValue, float *pData)
{
  float value;
  int   rtn;

  rtn = sscanf(pValue, "%f", &value); /* signed floating-point number */

  /* The return value is the number of input items successfully matched */
  if (rtn == 1)
    *pData = value;
} /* end of diagSetFloatValue */


/*
 * This routine updates the diag thresholds
 * If the data is invalid, the data will remain the default value
 * 
 * Input:
 * pMemberName - Point to a 'Member' string.
 * pValue -    Point to a 'Value' string.
 *
 * Output: None
 */
void diagSetDiagtThresholds(char *pMemberName, char *pValue)
{
  do {
    /* Percentage of net Rx CRC errors */
    if (strcasecmp(pMemberName, "PCT_NET_CRC_ERRS") == 0) {
      /* Update the data if is valid, otherwise remain the default value */
      diagSetUint32Value(pValue, &diagNetThld_pctRxCrcErrs);
      break;
    }

    /* Percentage of net Rx Frame errors */
    if (strcasecmp(pMemberName, "PCT_NET_FRAMES_ERRS") == 0) {
      /* Update the data if is valid, otherwise remain the default value */
      diagSetUint32Value(pValue, &diagNetThld_pctRxFrameErrs);
      break;
    }

    /* Percentage of net Rx length errors */
    if (strcasecmp(pMemberName, "PCT_NET_LEN_ERRS") == 0) {
      /* Update the data if is valid, otherwise remain the default value */
      diagSetUint32Value(pValue, &diagNetThld_pctRxLenErrs);
      break;
    }

    /* Percentage of of MoCA Tx Discard packets */
    if (strcasecmp(pMemberName, "PCT_MOCA_TX_DISCARD_PKTS") == 0) {
      /* Update the data if is valid, otherwise remain the default value */
      diagSetUint32Value(pValue, &diagMocaThld_pctTxDiscardPkts);
      break;
    }
    
    /* Percentage of of MoCA Rx Discard packets */
    if (strcasecmp(pMemberName, "PCT_MOCA_RX_DISCARD_PKTS") == 0) {
      /* Update the data if is valid, otherwise remain the default value */
      diagSetUint32Value(pValue, &diagMocaThld_pctRxDiscardPkts);
      break;
    }
    
  } while (false);
  
} /* end of diagSetDiagtThresholds */

/*
 * This routine lookup diagWaitTimeTbl
 * If pMemberName string matches timerName
 * return address of global diag wait time variable.
 *
 * Input:
 * pMemberName - Point to a 'Member' string.
 *
 * Output:
 * pDiagWaitTime - if found
 * NULL          - Otherwise
 *
 */
time_t *diagGetWaitTime(char *pMemberName)
{
  int i;

  for (i = 0; i < MAX_NUM_OF_MONITOR_TIMER; i++) {
    if (strcasecmp(pMemberName, diagWaitTimeTbl[i].timerName) == 0) {
      return  diagWaitTimeTbl[i].pDiagWaitTime;
    }
  }

  return (time_t *) NULL;
}

/*
 * This routine updates the diag intervals
 * If the data is invalid, the data will remain the default value
 * 
 * Input:
 * pMemberName - Point to a 'Member' string.
 * pValue -    Point to a 'Value' string.
 *
 * Output: None
 */
void diagSetDiagWaitTime(char *pMemberName, char *pValue)
{
  time_t *pDiagWaitTime = NULL;

  DIAGD_ENTRY("%s: pMemberName= %s, pValue= %s", __func__, pMemberName, pValue);

  pDiagWaitTime = diagGetWaitTime(pMemberName);

  if (pDiagWaitTime != NULL) {
    diagSetUint32Value(pValue, (uint32_t *)pDiagWaitTime);
  }
  else {
    DIAGD_DEBUG("%s: pMemberName = %s diagGetWaitTime() return NULL!",
                __func__, pMemberName);
  }
} /* end of diagSetDiagWaitTime */


/*
 * This routine splits input string to 'Class', 'Member' and 'Value' respectively
 * 
 * Input:
 * pDataBuf - Point to data buffer.
 *
 * Output: None
 */
void diagGetDiagData(char *pDataBuf)
{
  int  i;
  char class[STR_BUF_LEN + 1];
  char member[STR_BUF_LEN + 1];
  char value[STR_BUF_LEN + 1];

  do {
  
    /* Is a comment line? */
    if (*pDataBuf == '#')
      break;    /* exit */
  
    /* Get the 'Class' string */
    class[0] = '\0';
    for (i = 0; i < STR_BUF_LEN && (*pDataBuf != '\0'); i ++) {
      if (*pDataBuf == '.') {
        class[i] = '\0';
        /* Move to next character */
        pDataBuf ++;
        break;
      }
      else
        class[i] = *pDataBuf ++;
    }
  
    /* Get the 'Member' string */
    member[0] = '\0';
    for (i = 0; i < STR_BUF_LEN && (*pDataBuf != '\0'); i ++) {
      if (*pDataBuf == '=') {
        member[i] = '\0';
        /* Move to next character */
        pDataBuf ++;
        break;
      }
      else
        member[i] = *pDataBuf ++;
    }
  
    /* Get the 'Value' string */
    value[0] = '\0';
    for (i = 0; i < STR_BUF_LEN && (*pDataBuf != '\0'); i ++) {
      value[i] = *pDataBuf ++;
    }
    value[i] = '\0';
  
    /* Set the input data to data structures */
    diagSetDiagData(class, member, value);
  
  } while (false);
}


/*
 * This routine updates the diag data structure using the input data
 * If the data is invalid, the data will remain the default value
 * 
 * Input:
 * pClassName -  Point to a 'Class' string.
 * pMemberName - Point to a 'Member' string.
 * pValue -    Point to a 'Value' string.
 *
 * Output: None
 */
void diagSetDiagData(char *pClassName, char *pMemberName, char *pValue)
{
  int  i, j;
  char str[STR_BUF_LEN];

  DIAGD_ENTRY("%s: pClassName= %s, pMemberName= %s, pValue= %s", __func__, pClassName, pMemberName, pValue);

  do {
    /* Moca connection quality reference table */
    if (strcasecmp(pClassName, "MOCA_CONN") == 0) {
      /* Match the 'refPhyRate' member name */
      for (i = 0; i < MoCA_MAX_NODES; i ++) {
        sprintf(str, "PHY_RATE[%d]", i);
        if (strcasecmp(pMemberName, str) == 0) {
          /* Update the data if is valid, otherwise remain the default value */
          diagSetUint32Value(pValue, &diagMoca_connQltyTbl.refPhyRate[i]);
          break;
        }
      } /* end of for */
      break;    /* Finish parsing one line. Exit. */
    } /* end of if (MOCA_CONN) */

    /* Net interface link up/down counts */
    if (strcasecmp(pClassName, "NETLINK") == 0) {
      sprintf(str, "NET_LINK_CNTS");
      if (strcasecmp(pMemberName, "NET_LINK_CNTS") == 0) {
        /* Update the data if is valid, otherwise remain the default value */
        diagSetUint32Value(pValue, &diagNetlinkThld_linkCnts);
        break;
      }
    }
    
    /* Thresholds */
    if (strcasecmp(pClassName, "THRESHOLD") == 0) {
      diagSetDiagtThresholds(pMemberName, pValue);
    }

    /* Monitoring intervals (wait time in seconds) */
    if (strcasecmp(pClassName, "WAITTIME") == 0) {
       DIAGD_DEBUG("%s: WAITTIME.....", __func__);
      diagSetDiagWaitTime(pMemberName, pValue);
    }

    /* Moca performance reference table */
    for (i = 0; i < DIAG_MOCA_PERF_LVL_MAX; i ++) {
      sprintf(str, "MOCA_PERF[%d]", i);
      if (strcasecmp(pClassName, str) == 0) {
        /* Match the 'rxUcPhyRate' member name */
        if (strcasecmp(pMemberName, "RATE") == 0) {
          /* Update the data if is valid, otherwise remain the default value */
          diagSetUint32Value(pValue, &diagMocaPerfReferenceTable[i].rxUcPhyRate);
          break;    /* Finish parsing one line. Exit. */
        }
        /* Match the 'rxUcGain' member name */
        else if (strcasecmp(pMemberName, "GAIN") == 0) {
          /* Update the data if is valid, otherwise remain the default value */
          diagSetFloatValue(pValue, &diagMocaPerfReferenceTable[i].rxUcGain);
          break;    /* Finish parsing one line. Exit. */
        }
        /* Match the 'rxUcAvgSnr' member name */
        else if (strcasecmp(pMemberName, "SNR") == 0) {
          /* Update the data if is valid, otherwise remain the default value */
          diagSetFloatValue(pValue, &diagMocaPerfReferenceTable[i].rxUcAvgSnr);
          break;    /* Finish parsing one line. Exit. */
        }
        else {
          for (j = 0; j < BIT_LOADING_LEN; j ++) {
            /* Match the 'rxUcBitLoading' member name */
            sprintf(str, "BIT_LOADING[%d]", j);
            if (strcasecmp(pMemberName, str) == 0) {
              /* Update the data if is valid, otherwise remain the default value */
              diagSetUint32Value(pValue, &diagMocaPerfReferenceTable[i].rxUcBitLoading[j]);
              break;    /* Fount the member. Exit. */
            }
          } /* end of for (DIAG_MOCA_PERF_LVL_MAX) */
          break;    /* Finish parsing one line. Exit. */
        } /* end of pClassName */
      }
    } /* end of for */
  } while (false);
  
} /* end of diagSetDiagData */


/*
 * This routine reads the data from input file diag_data.txt
 * The file formats:
 *
 *    Class.Member = Value
 *
 * Class  - Name of diag data structure
 * Member - Member of data structure
 * Value  - Data value that include: uint32_t (dec), uint32_t (hex) and float
 *  
 * Input:
 * pFileName - Point to input file.
 *
 * Output:
 * DIAGD_RC_OK  -    OK
 * DIAGD_RC_ERR -    failed
 */
int diagReadDiagDataFile(char *pFileName)
{
  int   index, rtn = DIAGD_RC_OK;
  FILE  *fd;
  char  ch;
  char  buf[FILE_BUF_LEN + 1];

  /* Open the data file with TEXT mode to read */ 
  fd = fopen(pFileName, "rt");
  if (fd != NULL)
  {
    index = 0;
    
    /* Parse the data file */ 
    while (!feof(fd)) {
      /* Read a line from data file and skip 'Space' characters */  
      ch = fgetc(fd);
      if (ch == '\r' || ch == '\n') {
        if (index != 0) {
          buf[index] = '\0';
          /* Parse the text line to get data */
          diagGetDiagData(buf);

          index = 0;
        }
      }
      else if (ch != ' ' && ch != '\t') {
        /* Copy the characters into buffer */ 
        if (index < FILE_BUF_LEN)
          buf[index ++] = ch;
      }
    }

    /* Last line of data file */
    if (index != 0) {
      buf[index] = '\0';
      /* Parse the text line to get data */
      diagGetDiagData(buf);
    }
    
    /* Close the data file */ 
    fclose(fd);
  }
  else
    rtn = DIAGD_RC_ERR;

  return (rtn);
} /* end of diagReadDiagDataFile */

