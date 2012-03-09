/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 */

/*--------------------------------------------------------------------------
 *
 * Includes
 *
 *--------------------------------------------------------------------------
 */

#include "diagdIncludes.h"
#include "diagError.h"


/* -------------------------------------------------------------------------
 *
 * Internal defines, types and variables
 *
 *--------------------------------------------------------------------------
 */
/* MoCA error and warning counters */

struct {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_MOCA_ERROR_MAX];
   unsigned short   WarnCount[DIAG_MOCA_WARN_MAX];
} diagMocaErrCounts;

struct {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_GENET_ERROR_MAX];
   unsigned short   WarnCount[DIAG_GENET_WARN_MAX];
} diagGenetErrCounts;


struct {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_NAND_ERROR_MAX];
   unsigned short   WarnCount[DIAG_NAND_WARN_MAX];
} diagNandErrCounts;

struct {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_MCE_ERROR_MAX];
   unsigned short   WarnCount[DIAG_MCE_WARN_MAX];
} diagMceErrCounts;

char *diagMocaErrTypeStr[] = {
   "DIAG_MOCA_INIT_ERROR",
   "DIAG_MOCA_PROBE_ERROR"
};

char *diagGenetErrTypeStr[] = {
   "DIAG_GENET_OPEN_ERROR",
   "DIAG_GENET_XMIT_ERROR",
   "DIAG_GENET_REVC_ERROR",
   "DIAG_GENET_HFB_ERROR",
   "DIAG_GENET_PROBE_ERROR",
   "DIAG_GENET_PWR_DOWN_ERROR",
   "DIAG_GENET_PHY_ERROR"
};

char *diagNandErrTypeStr[] = {
   "DIAG_NAND_INIT_ERROR",
   "DIAG_NAND_BBT_ERROR",
   "DIAG_NAND_ECC_ERROR",
   "DIAG_NAND_NO_DEV_ERROR"
};

char *diagMceErrTypeStr[] = {
   "DIAG_MCE_MEM_CORRUPT_ERROR",
   "DIAG_MCE_OUT_OF_MEM_ERROR",
   "DIAG_MCE_HW_POISONED_ERROR"
};

char *diagMocaWarnTypeStr[] = {
   "DIAG_MOCA_M2M_XFER_WARN",
   "DIAG_MOCA_WRITE_WARN",
   "DIAG_MOCA_READ_WARN",
   "DIAG_MOCA_NO_MEM_WARN",
   "DIAG_MOCA_PROBE_WARN"
};

char *diagGenetWarnTypeStr[] = {
   "DIAG_GENET_DROP_FRAGMENTED_PKT_WARN"
};

char *diagNandWarnTypeStr[] = {
   "DIAG_NAND_BBT_WRITE_WARN",
   "DIAG_NAND_EDU_RBUS_WARN",
   "DIAG_NAND_READ_UNCORRECTABLE_WARN"
};

/* Errors, Warnings issued by Broadcom MoCA driver */
diagErrorCodeEntry_t diagMocaErrCodeTbl[] = {
   {MOCA_INIT_ERROR, DIAG_MOCA_INIT_ERROR},
   {MOCA_PROBE_ERROR, DIAG_MOCA_PROBE_ERROR},
};

/* Errors, Warnings issued by Broadcom Giga-bit Ethernet driver */
diagErrorCodeEntry_t diagGenetErrCodeTbl[] = {
   {GENET_OPEN_ERROR, DIAG_GENET_OPEN_ERROR},
   {GENET_TXRING_ERROR, DIAG_GENET_XMIT_ERROR},
   {GENET_TXDMA_MAP_ERROR, DIAG_GENET_XMIT_ERROR},
   {GENET_RING_XMIT_ERROR, DIAG_GENET_XMIT_ERROR},
   {GENET_RX_SKB_ALLOC_ERROR, DIAG_GENET_REVC_ERROR},
   {GENET_ASSIGN_RX_BUFFER_ERROR, DIAG_GENET_REVC_ERROR},
   {GENET_HFB_UPDATE_ERROR, DIAG_GENET_HFB_ERROR},
   {GENET_HFB_READ_ERROR, DIAG_GENET_HFB_ERROR},
   {GENET_PROBE_ERROR, DIAG_GENET_PROBE_ERROR},
   {GENET_PWR_DOWN_ERROR, DIAG_GENET_PWR_DOWN_ERROR},
   {GENET_PHY_INIT_ERROR, DIAG_GENET_PHY_ERROR},
   {GENET_DROP_FRAGMENTED_PKT_WARN, DIAG_GENET_DROP_FRAGMENTED_PKT_WARN}
};

/* Errors issued by mtd/nand */
diagErrorCodeEntry_t diagNandErrCodeTbl[] = {
   {NAND_INIT_ERROR, DIAG_NAND_INIT_ERROR},
   {NAND_BBT_WR_ERROR, DIAG_NAND_BBT_ERROR},
   {NAND_BBT_OUT_OF_MEM_ERROR, DIAG_NAND_BBT_ERROR},
   {NAND_BBT_SCAN_ERROR, DIAG_NAND_BBT_ERROR},
   {NAND_ECC_UNCORRECTABLE_ERROR, DIAG_NAND_ECC_ERROR},
   {NAND_NO_DEV_ERROR, DIAG_NAND_NO_DEV_ERROR}
};


/* errors issued by kernel memory management */
diagErrorCodeEntry_t diagMceErrCodeTbl[] = {
   {MCE_HW_MEM_CORRUPT_ERROR, DIAG_MCE_MEM_CORRUPT_ERROR},
   {MCE_OUT_OF_MEM_ERROR, DIAG_MCE_OUT_OF_MEM_ERROR},
   {MCE_HW_POISONED_ERROR, DIAG_MCE_HW_POISONED_ERROR}
};

#define DIAG_ERROR_CODE_ENTRY_SZ   sizeof(diagErrorCodeEntry_t)
#define DIAG_MOCA_NUM_OF_ERROR     sizeof(diagMocaErrCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_GENET_NUM_OF_ERROR    sizeof(diagGenetErrCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_NAND_NUM_OF_ERROR     sizeof(diagNandErrCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_MCE_NUM_OF_ERROR      sizeof(diagMceErrCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ

diagErrorCodeTbl_t diagErrorCodeTbl[ERROR_CODE_COMPONENT_MAX] = {
   {DIAG_MOCA_NUM_OF_ERROR,  diagMocaErrCodeTbl},
   {DIAG_GENET_NUM_OF_ERROR, diagGenetErrCodeTbl},
   {DIAG_NAND_NUM_OF_ERROR,  diagNandErrCodeTbl},
   {DIAG_MCE_NUM_OF_ERROR,   diagMceErrCodeTbl}
};

diagErrorCodeEntry_t diagMocaWarnCodeTbl[] = {
   {MOCA_M2M_XFER_WARN, DIAG_MOCA_M2M_XFER_WARN},
   {MOCA_WRT_MEM_WARN, DIAG_MOCA_WRITE_WARN},
   {MOCA_RD_MEM_WARN,  DIAG_MOCA_READ_WARN},
   {MOCA_GET_PAGES_WARN, DIAG_MOCA_WRITE_WARN},
   {MOCA_WRT_IMG_WARN, DIAG_MOCA_WRITE_WARN},
   {MOCA_RECVMSG_WARN, DIAG_MOCA_NO_MEM_WARN},
   {MOCA_WDT_WARN,     DIAG_MOCA_NO_MEM_WARN},
   {MOCA_CANNOT_GET_MBX_BASE_WARN, DIAG_MOCA_NO_MEM_WARN},
   {MOCA_RECVMSG_ASSERT_FAIL_WARN, DIAG_MOCA_NO_MEM_WARN},
   {MOCA_RECVMSG_CORE_REQ_FAIL_WARN, DIAG_MOCA_NO_MEM_WARN},
   {MOCA_RECVMSG_HOST_RSP_FAIL_WARN, DIAG_MOCA_NO_MEM_WARN},
   {MOCA_PROBE_REQ_INTERRUPT_FAIL_WARN, DIAG_MOCA_PROBE_WARN},
   {MOCA_PROBE_REG_CLASS_DEV_FAIL_WARN, DIAG_MOCA_PROBE_WARN}
};

diagErrorCodeEntry_t diagGenetWarnCodeTbl[] = {
   {GENET_DROP_FRAGMENTED_PKT_WARN, DIAG_GENET_DROP_FRAGMENTED_PKT_WARN}
};

diagErrorCodeEntry_t diagNandWarnCodeTbl[] = {
   {NAND_BBT_WRT_WARN, DIAG_NAND_BBT_WRITE_WARN},
   {NAND_EDU_RBUS_WARN, DIAG_NAND_EDU_RBUS_WARN},
   {NAND_RD_UNCORRECTABLE_WARN, DIAG_NAND_READ_UNCORRECTABLE_WARN}
};

#define DIAG_MOCA_NUM_OF_WARN     sizeof(diagMocaWarnCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_GENET_NUM_OF_WARN    sizeof(diagGenetWarnCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_NAND_NUM_OF_WARN     sizeof(diagNandWarnCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_MCE_NUM_OF_WARN      0

diagErrorCodeTbl_t diagWarnCodeTbl[ERROR_CODE_COMPONENT_MAX] = {
   {DIAG_MOCA_NUM_OF_WARN,  diagMocaWarnCodeTbl},
   {DIAG_GENET_NUM_OF_WARN, diagGenetWarnCodeTbl},
   {DIAG_NAND_NUM_OF_WARN,  diagNandWarnCodeTbl},
   {DIAG_MCE_NUM_OF_WARN,   NULL}
};

/* TODO 03092012 check threashold */
bool isDiagErrorCountReachThreshold(unsigned char componetType, unsigned char errType)
{
   return false;
}

/*
 * This routine will search the corresponding error or warning table based on
 * component type and error code.

 * If error code is matched, its mapped error or warning type is returned.
 *
 * Input:
 *    componentType
 *    errorCode
 *
 * Output:
 *    errorType if errorCode in the corresponding error or
 *    warning table is matched.
 *
 *    DIAG_UNKNOWN_ERROR_TYPE if errorCode is not matched.
 */
unsigned char diagGetErrType(unsigned char componentType, unsigned short errorCode)
{
   diagErrorCodeEntry_t *entryPtr;
   int numOfEntry = 0;
   int i;

   if (IS_DIAG_WARNING_CODE(errorCode)) {
      /* this is warning code */
      numOfEntry = diagWarnCodeTbl[componentType].numOfEntry;
      entryPtr = diagWarnCodeTbl[componentType].tbl;
   }
   else {
      /* this is error code */
      numOfEntry = diagErrorCodeTbl[componentType].numOfEntry;
      entryPtr = diagErrorCodeTbl[componentType].tbl;
   }

   for (i=0; i < numOfEntry; i++) {
      if (entryPtr[i].errorCode == errorCode) {
         return entryPtr[i].errorType;
      } 
   }

   return DIAG_UNKNOWN_ERROR_TYPE;
}

/*
 * This routine will update error or warning count table based on errorCode.
 *
 * If error code is matched in the corresponding diag error counts table:
 *    1. Increment indiviual error count and total error count of its
 *       component type.
 *    2. Writes out component type, error type, error count and total
 *       error count logging information to /var/log/diagd/diagd.log file.
 *
 * If error code is not matched, log the error message to
 *    /var/log/diagd/diagd.log file and return.
 *
 * Input:
 *    errorCode
 *
 * Output:
 *    none
 */

void diagUpdateErrorCount(char *timestamp, unsigned short errorCode)
{
   unsigned char errType;
   unsigned char componentType = GET_ERROR_CODE_COMPONENT_TYPE(errorCode);

   if(IS_DIAG_WARNING_CODE(errorCode)) {
      diagUpdateWarnCount(timestamp, errorCode);
      return;
   }

   if(componentType >= ERROR_CODE_COMPONENT_MAX) {
      DIAGD_TRACE("%s: unknown component type %d", __func__, componentType);
      DIAGD_LOG("Unknown component type %d", componentType);
      return;
   }

   errType = diagGetErrType(componentType, errorCode);

   if (errType == DIAG_UNKNOWN_ERROR_TYPE) {
      DIAGD_TRACE("%s: unknown errType %d", __func__, errType);
      DIAGD_LOG("Unknown errType %d", errType);
      return;
   }

   switch(componentType) {
      case ERROR_CODE_COMPONENT_BRCM_MOCA:
         diagMocaErrCounts.ErrCount[errType]++;
         diagMocaErrCounts.TotalErrCount++;

         DIAGD_TRACE("%s: componentType = BRCM_MOCA errType = %d" \
                     " counter=%d total errorCount=%d", __func__, errType,
                     diagMocaErrCounts.ErrCount[errType],
                     diagMocaErrCounts.TotalErrCount);

         DIAGD_LOG_W_TS("%s BRCM_MOCA errType = %s" \
                   " counter=%d total errorCount=%d",
                   timestamp,
                   diagMocaErrTypeStr[errType],
                   diagMocaErrCounts.ErrCount[errType],
                   diagMocaErrCounts.TotalErrCount);
         break;
      case ERROR_CODE_COMPONENT_BRCM_GENET:
         diagGenetErrCounts.ErrCount[errType]++;
         diagGenetErrCounts.TotalErrCount++;

         DIAGD_TRACE("%s: componentType = BRCM_GENET errType = %d" \
                     " counter=%d total errorCount=%d", __func__, errType,
                     diagGenetErrCounts.ErrCount[errType],
                     diagGenetErrCounts.TotalErrCount);

         DIAGD_LOG_W_TS("%s BRCM_GENET errtType = %s" \
                   " counter=%d total errorCount=%d",
                   timestamp,
                   diagGenetErrTypeStr[errType],
                   diagGenetErrCounts.ErrCount[errType],
                   diagGenetErrCounts.TotalErrCount);
         break;
      case ERROR_CODE_COMPONENT_MTD_NAND:
         diagNandErrCounts.ErrCount[errType]++;
         diagNandErrCounts.TotalErrCount++;

         DIAGD_TRACE("%s: componentType = MTD_NAND errType = %d" \
                     " counter=%d total errorCount=%d", __func__, errType,
                     diagNandErrCounts.ErrCount[errType],
                     diagNandErrCounts.TotalErrCount);

         DIAGD_LOG_W_TS("%s MTD_NAND errType = %s" \
                   " counter=%d total errorCount=%d",
                   timestamp,
                   diagNandErrTypeStr[errType],
                   diagNandErrCounts.ErrCount[errType],
                   diagNandErrCounts.TotalErrCount);
         break;
      case ERROR_CODE_COMPONENT_KERNEL_MM:
         diagMceErrCounts.ErrCount[errType]++;
         diagMceErrCounts.TotalErrCount++;

         DIAGD_TRACE("%s: componentType = KERNEL_MM errType = %d" \
                     " counter=%d total errorCount=%d", __func__, errType,
                     diagMceErrCounts.ErrCount[errType],
                     diagMceErrCounts.TotalErrCount);

         DIAGD_LOG_W_TS("%s KERNEL_MM errType = %s" \
                   " counter=%d total errorCount=%d",
                   timestamp,
                   diagMceErrTypeStr[errType],
                   diagMceErrCounts.ErrCount[errType],
                   diagMceErrCounts.TotalErrCount);
         break;
      default:
         break;
   }

   /* TODO : check threashold and issue alarm */
   if (isDiagErrorCountReachThreshold(componentType, errType) == true) {
      /* issue alarm */
     ;
   }
}

/*
 * This routine will update warning counts table based on errorCode.
 * If error code is matched in corresponding diag warning counts table,
 * increment indiviual warning count and total warning count of its
 * component type.
 *
 * If error code is not matched, log the error message and return.
 *
 * Input:
 *    errorCode
 *
 * Output:
 *    none
 */
void diagUpdateWarnCount(char *timestamp, unsigned short errorCode)
{
   unsigned char warnType;
   unsigned char componentType = GET_ERROR_CODE_COMPONENT_TYPE(errorCode);

   if(componentType >= ERROR_CODE_COMPONENT_MAX) {
      DIAGD_TRACE("%s: unknown component type %d", __func__, componentType);
      DIAGD_LOG("Unknown component type %d", componentType);
      return;
   }

   warnType = diagGetErrType(componentType, errorCode);

   if (warnType == DIAG_UNKNOWN_ERROR_TYPE) {
      DIAGD_TRACE("%s: unknown warnType %d", __func__, warnType);
      DIAGD_LOG("Unknown warnType %d", warnType);
      return;
   }

   switch(componentType) {
      case ERROR_CODE_COMPONENT_BRCM_MOCA:
         diagMocaErrCounts.WarnCount[warnType]++;
         diagMocaErrCounts.TotalWarnCount++;

         DIAGD_TRACE("%s: componentType = BRCM_MOCA warnType = %d" \
                     " counter=%d total warnCount=%d", __func__, warnType,
                     diagMocaErrCounts.WarnCount[warnType],
                     diagMocaErrCounts.TotalWarnCount);

         DIAGD_LOG_W_TS("%s BRCM_MOCA warnType = %s" \
                   " counter=%d total warnCount=%d",
                   timestamp,
                   diagMocaWarnTypeStr[warnType],
                   diagMocaErrCounts.WarnCount[warnType],
                   diagMocaErrCounts.TotalWarnCount);
         break;
      case ERROR_CODE_COMPONENT_BRCM_GENET:
         diagGenetErrCounts.WarnCount[warnType]++;

         diagGenetErrCounts.TotalWarnCount++;
         DIAGD_TRACE("%s: componentType = BRCM_GENET warnType = %d" \
                     " counter=%d total warnCount=%d", __func__, warnType,
                     diagGenetErrCounts.WarnCount[warnType],
                     diagGenetErrCounts.TotalWarnCount);

         DIAGD_LOG_W_TS("%s BRCM_GENET warnType = %s counter=%d total warnCount=%d",
                   timestamp,
                   diagGenetWarnTypeStr[warnType],
                   diagGenetErrCounts.WarnCount[warnType],
                   diagGenetErrCounts.TotalWarnCount);
         break;
      case ERROR_CODE_COMPONENT_MTD_NAND:
         diagNandErrCounts.WarnCount[warnType]++;
         diagNandErrCounts.TotalWarnCount++;

         DIAGD_TRACE("%s: componentType = MTD_NAND warnType = %d" \
                     " counter=%d total warnCount=%d", __func__, warnType,
                     diagNandErrCounts.WarnCount[warnType],
                     diagNandErrCounts.TotalWarnCount);

         DIAGD_LOG_W_TS("%s MTD_NAND warnType = %s" \
                   " counter=%d total warnCount=%d",
                   timestamp,
                   diagNandWarnTypeStr[warnType],
                   diagNandErrCounts.WarnCount[warnType],
                   diagNandErrCounts.TotalWarnCount);
         break;
      case ERROR_CODE_COMPONENT_KERNEL_MM:
         DIAGD_TRACE("%s: Shouldn't be here since there is no" \
                     " KERNEL_MM warnType defined yet!", __func__);

         break;
      default:
         break;
   }

   /* TODO : check threashold and issue alarm */
   if (isDiagErrorCountReachThreshold(componentType, warnType) == true) {
      /* issue alarm */
     ;
   }
}
