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
/* Diag error and warning counters */

diagMocaErrCounts_t  *diagMocaErrCntsPtr = NULL;
diagGenetErrCounts_t *diagGenetErrCntsPtr = NULL;
diagMtdNandErrCounts_t  *diagMtdNandErrCntsPtr = NULL;
diagSpiErrCounts_t   *diagSpiErrCntsPtr = NULL;

char *diagMocaErrTypeStr[] = {
   "DIAG_MOCA_INIT_ERROR",
   "DIAG_MOCA_PROBE_ERROR",
   "DIAG_MOCA_3450_INV_CHIP_ID_ERROR",
   "DIAG_MOCA_3450_I2C_TIMEOUT_ERROR"
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

char *diagMtdNandErrTypeStr[] = {
   "DIAG_MTD_NAND_INIT_ERROR",
   "DIAG_MTD_NAND_BBT_ERROR",
   "DIAG_MTD_NAND_ECC_ERROR",
   "DIAG_MTD_ALLOC_PARTITION_ERROR",
   "DIAG_MTD_INIT_ERROR"
};

char *diagSpiErrTypeStr[] = {
   "DIAG_SPI_PROBE_ERROR",
   "DIAG_SPI_UNRECOG_FLASH_TYPE_ERROR"
};

char *diagMocaWarnTypeStr[] = {
   "DIAG_MOCA_M2M_XFER_WARN",
   "DIAG_MOCA_WRITE_WARN",
   "DIAG_MOCA_READ_WARN",
   "DIAG_MOCA_NO_MEM_WARN",
   "DIAG_MOCA_PROBE_WARN",
   "DIAG_MOCA_REG_WARN"
};

char *diagGenetWarnTypeStr[] = {
   "DIAG_GENET_DROP_FRAGMENTED_PKT_WARN"
};

char *diagMtdNandWarnTypeStr[] = {
   "DIAG_MTD_NAND_BBT_WRITE_WARN",
   "DIAG_MTD_NAND_EDU_RBUS_WARN",
   "DIAG_MTD_NAND_READ_UNCORRECTABLE_WARN",
   "DIAG_MTD_NAND_NO_DEV_WARN",
   "DIAG_MTD_ALLOC_PARTITION_WARN",
   "DIAG_MTD_BLKTRANS_REG_WARN",
   "DIAG_MTD_ERASE_WRT_WARN",
   "DIAG_MTD_BRCMSTB_SETP_WARN"
};

char *diagSpiWarnTypeStr[] = {
   "DIAG_SPI_FLASH_SETUP_WARN",
   "DIAG_SPI_CS_SETUP_WARN"
};

/* Errors issued by Broadcom MoCA driver */
diagErrorCodeEntry_t diagMocaErrCodeTbl[] = {
   {MOCA_INIT_ERROR, DIAG_MOCA_INIT_ERROR},
   {MOCA_PROBE_ERROR, DIAG_MOCA_PROBE_ERROR},
   {MOCA_3450_INV_CHIP_ID_ERROR, DIAG_MOCA_3450_INV_CHIP_ID_ERROR},
   {MOCA_3450_I2C_TIMEOUT_ERROR, DIAG_MOCA_3450_I2C_TIMEOUT_ERROR}
};

/* Errors  issued by Broadcom Giga-bit Ethernet driver */
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
   {GENET_PHY_INIT_ERROR, DIAG_GENET_PHY_ERROR}
};

/* Errors issued by mtd, mtd/nand */
diagErrorCodeEntry_t diagMtdNandErrCodeTbl[] = {
   {MTD_NAND_INIT_ERROR, DIAG_MTD_NAND_INIT_ERROR},
   {MTD_NAND_BBT_WR_ERROR, DIAG_MTD_NAND_BBT_ERROR},
   {MTD_NAND_BBT_OUT_OF_MEM_ERROR, DIAG_MTD_NAND_BBT_ERROR},
   {MTD_NAND_BBT_SCAN_ERROR, DIAG_MTD_NAND_BBT_ERROR},
   {MTD_NAND_ECC_UNCORRECTABLE_ERROR, DIAG_MTD_NAND_ECC_ERROR},
   {MTD_ALLOC_PARTITION_ERROR, DIAG_MTD_ALLOC_PARTITION_ERROR},
   {MTD_INIT_ERROR, DIAG_MTD_INIT_ERROR}
};


/* Errors issued by SPI */
diagErrorCodeEntry_t diagSpiErrCodeTbl[] = {
   {SPI_PROBE_ERROR, DIAG_SPI_PROBE_ERROR},
   {SPI_UNRECOG_FLASH_TYPE_ERROR, DIAG_SPI_UNRECOG_FLASH_TYPE_ERROR}
};

#define DIAG_ERROR_CODE_ENTRY_SZ   sizeof(diagErrorCodeEntry_t)
#define DIAG_MOCA_NUM_OF_ERROR     sizeof(diagMocaErrCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_GENET_NUM_OF_ERROR    sizeof(diagGenetErrCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_MTD_NAND_NUM_OF_ERROR sizeof(diagMtdNandErrCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_SPI_NUM_OF_ERROR      sizeof(diagSpiErrCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ

diagErrorCodeTbl_t diagErrorCodeTbl[ERROR_CODE_COMPONENT_MAX] = {
   {DIAG_MOCA_NUM_OF_ERROR,  diagMocaErrCodeTbl},
   {DIAG_GENET_NUM_OF_ERROR, diagGenetErrCodeTbl},
   {DIAG_MTD_NAND_NUM_OF_ERROR, diagMtdNandErrCodeTbl},
   {DIAG_SPI_NUM_OF_ERROR,   diagSpiErrCodeTbl}
};

/* Warnings issued by Broadcom MoCA driver */
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
   {MOCA_PROBE_REG_CLASS_DEV_FAIL_WARN, DIAG_MOCA_PROBE_WARN},
   {MOCA_I2C_BASE_ADDR_NOT_SET_WARN, DIAG_MOCA_REG_WARN}
};

/* Warnings issued by Broadcom Giga-bit Ethernet driver */
diagErrorCodeEntry_t diagGenetWarnCodeTbl[] = {
   {GENET_DROP_FRAGMENTED_PKT_WARN, DIAG_GENET_DROP_FRAGMENTED_PKT_WARN}
};

/* Warnings issued by mtd, mtd/nand */
diagErrorCodeEntry_t diagMtdNandWarnCodeTbl[] = {
   {MTD_NAND_BBT_WRT_WARN, DIAG_MTD_NAND_BBT_WRITE_WARN},
   {MTD_NAND_EDU_RBUS_WARN, DIAG_MTD_NAND_EDU_RBUS_WARN},
   {MTD_NAND_RD_UNCORRECTABLE_WARN, DIAG_MTD_NAND_READ_UNCORRECTABLE_WARN},
   {MTD_NAND_NO_DEV_WARN, DIAG_MTD_NAND_NO_DEV_WARN},
   {MTD_ALLOC_PARTITION_WARN, DIAG_MTD_ALLOC_PARTITION_WARN},
   {MTD_BLKTRANS_REG_WARN, DIAG_MTD_BLKTRANS_REG_WARN},
   {MTD_ERASE_WRT_WARN, DIAG_MTD_ERASE_WRT_WARN},
   {MTD_BRCMSTB_SETP_WARN, DIAG_MTD_BRCMSTB_SETP_WARN}

};

/* Warnings issued by SPI */
diagErrorCodeEntry_t diagSpiWarnCodeTbl[] = {
   {SPI_FLASH_SETUP_WARN, DIAG_SPI_FLASH_SETUP_WARN},
   {SPI_CS_SETUP_WARN, DIAG_SPI_CS_SETUP_WARN}
};

#define DIAG_MOCA_NUM_OF_WARN     sizeof(diagMocaWarnCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_GENET_NUM_OF_WARN    sizeof(diagGenetWarnCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_MTD_NAND_NUM_OF_WARN sizeof(diagMtdNandWarnCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ
#define DIAG_SPI_NUM_OF_WARN      sizeof(diagSpiWarnCodeTbl)/DIAG_ERROR_CODE_ENTRY_SZ

diagErrorCodeTbl_t diagWarnCodeTbl[ERROR_CODE_COMPONENT_MAX] = {
   {DIAG_MOCA_NUM_OF_WARN,  diagMocaWarnCodeTbl},
   {DIAG_GENET_NUM_OF_WARN, diagGenetWarnCodeTbl},
   {DIAG_MTD_NAND_NUM_OF_WARN,  diagMtdNandWarnCodeTbl},
   {DIAG_SPI_NUM_OF_WARN,   diagSpiWarnCodeTbl}
};

diagErrsInfoEntry_t errsInfoTbl[ERROR_CODE_COMPONENT_MAX] = {
   {"BRCM_MOCA", DIAG_MOCA_RESERVED_1_ERROR, DIAG_MOCA_RESERVED_1_WARN,\
    diagMocaErrTypeStr, diagMocaWarnTypeStr},
   {"BRCM_GENET", DIAG_GENET_RESERVED_1_ERROR, DIAG_GENET_RESERVED_1_WARN,\
    diagGenetErrTypeStr, diagGenetWarnTypeStr},
   {"MTD_NAND", DIAG_MTD_RESERVED_1_ERROR, DIAG_MTD_RESERVED_1_WARN,\
    diagMtdNandErrTypeStr, diagMtdNandWarnTypeStr},
   {"BRCM_SPI", DIAG_SPI_RESERVED_1_ERROR, DIAG_SPI_RESERVED_1_WARN,\
    diagSpiErrTypeStr, diagSpiWarnTypeStr},
};


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
unsigned char diagGetErrType(diag_compType_e componentType, unsigned short errorCode)
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
 * This routine will return error type string and its associated count
 * based on component type and error type.
 *
 * Input:
 *    componentType
 *    errType
 *    pCount - address of count
 *
 * Output:
 *    errTypeStr - error type string
 *    *pCount    - error count
 *
 *    NULL - If unknown component type. It shouldn't happen if the calling
 *     routine has qualified componentType before this routine is called.
 */
const char *diagGetErrTypeStr(diag_compType_e componentType, unsigned short errType,
    unsigned short *pCount) {
  char *errTypeStr = NULL;


  switch (componentType) {
    case ERROR_CODE_COMPONENT_BRCM_MOCA:
      errTypeStr = diagMocaErrTypeStr[errType];
      *pCount = diagMocaErrCntsPtr->ErrCount[errType];
      break;
    case ERROR_CODE_COMPONENT_BRCM_GENET:
      errTypeStr = diagGenetErrTypeStr[errType];
      *pCount = diagGenetErrCntsPtr->ErrCount[errType];
      break;
    case ERROR_CODE_COMPONENT_MTD_NAND:
      errTypeStr = diagMtdNandErrTypeStr[errType];
      *pCount = diagMtdNandErrCntsPtr->ErrCount[errType];
      break;
    case ERROR_CODE_COMPONENT_BRCM_SPI:
      errTypeStr = diagSpiErrTypeStr[errType];
      *pCount = diagSpiErrCntsPtr->ErrCount[errType];
      break;
    default:
      break;
  }

  return errTypeStr;
}

/*
 * This routine will return warning type string and its associated count
 * based on component type and warn type.
 *
 * Input:
 *    componentType
 *    warnType
 *    pCount - address of count
 *
 * Output:
 *    warnTypeStr - warning type string
 *    *pCount     - warning count
 *
 *    NULL - If unknown component type. It shouldn't happen if the calling
 *           routine has qualified componentType before this routine is called.
 */
const char *diagGetWarnTypeStr(diag_compType_e componentType, unsigned short warnType,
    unsigned short *pCount) {
  char *warnTypeStr = NULL;


  switch (componentType) {
    case ERROR_CODE_COMPONENT_BRCM_MOCA:
      warnTypeStr = diagMocaWarnTypeStr[warnType];
      *pCount = diagMocaErrCntsPtr->WarnCount[warnType];
      break;
    case ERROR_CODE_COMPONENT_BRCM_GENET:
      warnTypeStr = diagGenetWarnTypeStr[warnType];
      *pCount = diagGenetErrCntsPtr->WarnCount[warnType];

      break;
    case ERROR_CODE_COMPONENT_MTD_NAND:
      warnTypeStr = diagMtdNandWarnTypeStr[warnType];
      *pCount = diagMtdNandErrCntsPtr->WarnCount[warnType];
      break;
    case ERROR_CODE_COMPONENT_BRCM_SPI:
      warnTypeStr = diagSpiWarnTypeStr[warnType];
      *pCount = diagSpiErrCntsPtr->WarnCount[warnType];
      break;
    default:
      break;
  }

  return warnTypeStr;
}

/*
 * This routine will return either error or warning type string, and
 * its associated count based on error code.
 *
 * Input:
 *    errCode
 *    pCount - address of count
 *
 * Output:
 *    errTypeStr  - error type string or warning type string
 *    *pCount     - error or warning count
 *
 *    NULL - If unknown component type. It shouldn't happen if the calling
 *           routine has qualified componentType before this routine is called.
 */
const char *diagGetErrTypeInfo(unsigned short errCode, unsigned short *pCount)
{
  const char *errTypeStr = NULL;
  unsigned char errType;
  diag_compType_e componentType = GET_ERROR_CODE_COMPONENT_TYPE(errCode);


  if(componentType >= ERROR_CODE_COMPONENT_MAX) {
    DIAGD_ERROR("%s: Unknown component type %d", __func__, componentType);
    return errTypeStr;
  }

  errType = diagGetErrType(componentType, errCode);

  if (errType == DIAG_UNKNOWN_ERROR_TYPE) {
    DIAGD_ERROR("%s: unknown ERROR TYPE.  errCode = %d", __func__, errCode);
    return errTypeStr;
  }

  if(IS_DIAG_WARNING_CODE(errCode)) {
    errTypeStr = diagGetWarnTypeStr(componentType, errType, pCount);
  }
  else {
    errTypeStr = diagGetErrTypeStr(componentType, errType, pCount);
  }

  return errTypeStr;
}

/*
 * This routine will update error or warning count table based on errorCode.
 *
 * If error code is matched in the corresponding diag error counts table:
 *    1. Increment indiviual error count and total error count of its
 *       component type.
 *    2. Writes out component type, error type, error count and total
 *       error count logging information to /user/diag/log/diagd.log file.
 *
 * If error code is not matched, log the error message to
 *    /user/diag/log/diagd.log file and return.
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
   diag_compType_e componentType = GET_ERROR_CODE_COMPONENT_TYPE(errorCode);

   if(IS_DIAG_WARNING_CODE(errorCode)) {
      diagUpdateWarnCount(timestamp, errorCode);
      return;
   }

   if(componentType >= ERROR_CODE_COMPONENT_MAX) {
      DIAGD_ERROR("%s: Unknown component type %d", __func__, componentType);
      return;
   }

   errType = diagGetErrType(componentType, errorCode);

   if (errType == DIAG_UNKNOWN_ERROR_TYPE) {
      DIAGD_ERROR("%s: unknown errType %d", __func__, errType);
      return;
   }

   switch(componentType) {
      case ERROR_CODE_COMPONENT_BRCM_MOCA:
         diagMocaErrCntsPtr->ErrCount[errType]++;
         diagMocaErrCntsPtr->TotalErrCount++;

         DIAGD_TRACE("%s: componentType = BRCM_MOCA errType = %d" \
                     " counter=%d total errorCount=%d", __func__, errType,
                     diagMocaErrCntsPtr->ErrCount[errType],
                     diagMocaErrCntsPtr->TotalErrCount);

         DIAGD_LOG_W_TS("%s BRCM_MOCA errType = %s" \
                   " counter=%d total errorCount=%d",
                   timestamp,
                   diagMocaErrTypeStr[errType],
                   diagMocaErrCntsPtr->ErrCount[errType],
                   diagMocaErrCntsPtr->TotalErrCount);
         break;
      case ERROR_CODE_COMPONENT_BRCM_GENET:
         diagGenetErrCntsPtr->ErrCount[errType]++;
         diagGenetErrCntsPtr->TotalErrCount++;

         DIAGD_TRACE("%s: componentType = BRCM_GENET errType = %d" \
                     " counter=%d total errorCount=%d", __func__, errType,
                     diagGenetErrCntsPtr->ErrCount[errType],
                     diagGenetErrCntsPtr->TotalErrCount);

         DIAGD_LOG_W_TS("%s BRCM_GENET errtType = %s" \
                   " counter=%d total errorCount=%d",
                   timestamp,
                   diagGenetErrTypeStr[errType],
                   diagGenetErrCntsPtr->ErrCount[errType],
                   diagGenetErrCntsPtr->TotalErrCount);
         break;
      case ERROR_CODE_COMPONENT_MTD_NAND:
         diagMtdNandErrCntsPtr->ErrCount[errType]++;
         diagMtdNandErrCntsPtr->TotalErrCount++;

         DIAGD_TRACE("%s: componentType = MTD_NAND errType = %d" \
                     " counter=%d total errorCount=%d", __func__, errType,
                     diagMtdNandErrCntsPtr->ErrCount[errType],
                     diagMtdNandErrCntsPtr->TotalErrCount);

         DIAGD_LOG_W_TS("%s MTD_NAND errType = %s" \
                   " counter=%d total errorCount=%d",
                   timestamp,
                   diagMtdNandErrTypeStr[errType],
                   diagMtdNandErrCntsPtr->ErrCount[errType],
                   diagMtdNandErrCntsPtr->TotalErrCount);
         break;
      case ERROR_CODE_COMPONENT_BRCM_SPI:
         diagSpiErrCntsPtr->ErrCount[errType]++;
         diagSpiErrCntsPtr->TotalErrCount++;

         DIAGD_TRACE("%s: componentType = BRCM_SPI errType = %d" \
                     " counter=%d total errorCount=%d", __func__, errType,
                     diagSpiErrCntsPtr->ErrCount[errType],
                     diagSpiErrCntsPtr->TotalErrCount);

         DIAGD_LOG_W_TS("%s BRCM_SPI errType = %s" \
                   " counter=%d total errorCount=%d",
                   timestamp,
                   diagSpiErrTypeStr[errType],
                   diagSpiErrCntsPtr->ErrCount[errType],
                   diagSpiErrCntsPtr->TotalErrCount);
         break;
      default:
         break;
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
   diag_compType_e componentType = GET_ERROR_CODE_COMPONENT_TYPE(errorCode);

   if(componentType >= ERROR_CODE_COMPONENT_MAX) {
      DIAGD_ERROR("%s: unknown component type %d", __func__, componentType);
      return;
   }

   warnType = diagGetErrType(componentType, errorCode);

   if (warnType == DIAG_UNKNOWN_ERROR_TYPE) {
      DIAGD_ERROR("%s: unknown warnType %d", __func__, warnType);
      return;
   }

   switch(componentType) {
      case ERROR_CODE_COMPONENT_BRCM_MOCA:
         diagMocaErrCntsPtr->WarnCount[warnType]++;
         diagMocaErrCntsPtr->TotalWarnCount++;

         DIAGD_TRACE("%s: componentType = BRCM_MOCA warnType = %d" \
                     " counter=%d total warnCount=%d", __func__, warnType,
                     diagMocaErrCntsPtr->WarnCount[warnType],
                     diagMocaErrCntsPtr->TotalWarnCount);

         DIAGD_LOG_W_TS("%s BRCM_MOCA warnType = %s" \
                   " counter=%d total warnCount=%d",
                   timestamp,
                   diagMocaWarnTypeStr[warnType],
                   diagMocaErrCntsPtr->WarnCount[warnType],
                   diagMocaErrCntsPtr->TotalWarnCount);
         break;
      case ERROR_CODE_COMPONENT_BRCM_GENET:
         diagGenetErrCntsPtr->WarnCount[warnType]++;
         diagGenetErrCntsPtr->TotalWarnCount++;

         DIAGD_TRACE("%s: componentType = BRCM_GENET warnType = %d" \
                     " counter=%d total warnCount=%d", __func__, warnType,
                     diagGenetErrCntsPtr->WarnCount[warnType],
                     diagGenetErrCntsPtr->TotalWarnCount);

         DIAGD_LOG_W_TS("%s BRCM_GENET warnType = %s counter=%d total warnCount=%d",
                   timestamp,
                   diagGenetWarnTypeStr[warnType],
                   diagGenetErrCntsPtr->WarnCount[warnType],
                   diagGenetErrCntsPtr->TotalWarnCount);
         break;
      case ERROR_CODE_COMPONENT_MTD_NAND:
         diagMtdNandErrCntsPtr->WarnCount[warnType]++;
         diagMtdNandErrCntsPtr->TotalWarnCount++;

         DIAGD_TRACE("%s: componentType = MTD_NAND warnType = %d" \
                     " counter=%d total warnCount=%d", __func__, warnType,
                     diagMtdNandErrCntsPtr->WarnCount[warnType],
                     diagMtdNandErrCntsPtr->TotalWarnCount);

         DIAGD_LOG_W_TS("%s MTD_NAND warnType = %s" \
                   " counter=%d total warnCount=%d",
                   timestamp,
                   diagMtdNandWarnTypeStr[warnType],
                   diagMtdNandErrCntsPtr->WarnCount[warnType],
                   diagMtdNandErrCntsPtr->TotalWarnCount);
         break;
      case ERROR_CODE_COMPONENT_BRCM_SPI:
         diagSpiErrCntsPtr->WarnCount[warnType]++;
         diagSpiErrCntsPtr->TotalWarnCount++;

         DIAGD_TRACE("%s: componentType = BRCM_SPI warnType = %d" \
                     " counter=%d total warnCount=%d", __func__, warnType,
                     diagSpiErrCntsPtr->WarnCount[warnType],
                     diagSpiErrCntsPtr->TotalWarnCount);

         DIAGD_LOG_W_TS("%s BRCM_SPI warnType = %s" \
                   " counter=%d total warnCount=%d",
                   timestamp,
                   diagSpiWarnTypeStr[warnType],
                   diagSpiErrCntsPtr->WarnCount[warnType],
                   diagSpiErrCntsPtr->TotalWarnCount);

         break;
      default:
         break;
   }

}

/*
 * This routine will initialize global diag error counts pointers.
 *
 * Input:
 *    diagdMap - mmap pointer to diag database file /user/diag/diagdb.bin
 *
 * Output:
 *    none
 */
void diagErrCnts_Init(char *diagdMap)
{
   /* read in error and warning counts from DIAGD_DB_FS */
   diagMocaErrCntsPtr  = (diagMocaErrCounts_t *) &diagdMap[DIAGD_MOCA_ERR_COUNTS_INDEX];
   diagGenetErrCntsPtr = (diagGenetErrCounts_t *) &diagdMap[DIAGD_GENET_ERR_COUNTS_INDEX];
   diagMtdNandErrCntsPtr  = (diagMtdNandErrCounts_t *) &diagdMap[DIAGD_MTD_NAND_ERR_COUNTS_INDEX];
   diagSpiErrCntsPtr   = (diagSpiErrCounts_t *) &diagdMap[DIAGD_SPI_ERR_COUNTS_INDEX];
}

/*
 * This routine will provide  the Diag error & warning counts and
 *  their corresponding error type string based on component type.
 *
 * Input:
 *    buffer - buffer to put the MoCA errors and error types string
 *    ptr    - void *
 *    type   - component type
 *
 * Output:
 *    none
 */
void diagGetErrsInfo(char *buffer, void *ptr, diag_compType_e type)
{
   int i;
   char inBuf[128];
   char *typeStr = NULL;
   char **errTypeStr;
   char **warnTypeStr;
   unsigned short *errCnts;
   unsigned short *warnCnts;
   unsigned char rsvdErrType;
   unsigned char rsvdWarnType;
   diagErrsInfoEntry_t *errsDetInfo = NULL;

   if(type >= ERROR_CODE_COMPONENT_MAX) {
      DIAGD_ERROR("%s: unknown component type %d", __func__, type);
      return;
   }

   errsDetInfo = &errsInfoTbl[type];
   typeStr = errsDetInfo->componentTypStr;
   errTypeStr = errsDetInfo->errTypeStr;
   warnTypeStr = errsDetInfo->warnTypeStr;
   rsvdErrType = errsDetInfo->rsvdErrType;
   rsvdWarnType = errsDetInfo->rsvdWarnType;

   switch (type) {
      case ERROR_CODE_COMPONENT_BRCM_MOCA:
         errCnts = ((diagMocaErrCounts_t *)ptr)->ErrCount;
         warnCnts = ((diagMocaErrCounts_t *)ptr)->WarnCount;
         break;
      case ERROR_CODE_COMPONENT_BRCM_GENET:
         errCnts = ((diagGenetErrCounts_t *)ptr)->ErrCount;
         warnCnts = ((diagGenetErrCounts_t *)ptr)->WarnCount;
         break;
      case ERROR_CODE_COMPONENT_MTD_NAND:
         errCnts = ((diagMtdNandErrCounts_t *)ptr)->ErrCount;
         warnCnts = ((diagMtdNandErrCounts_t *)ptr)->WarnCount;
         break;
      case ERROR_CODE_COMPONENT_BRCM_SPI:
         errCnts = ((diagSpiErrCounts_t *)ptr)->ErrCount;
         warnCnts = ((diagSpiErrCounts_t *)ptr)->WarnCount;
         break;
      default:
         DIAGD_ERROR("%s: unsupported component type %d", __func__, type);
         return;
         break;
   }

   strcat(buffer, typeStr);
   strcat(buffer, "   Error Counts:\n");
   for (i = 0; i < rsvdErrType; i++) {
      sprintf(inBuf, "   %s   = %d\n", errTypeStr[i], errCnts[i]);
      strcat(buffer, inBuf);
   }

   strcat(buffer, typeStr);
   strcat(buffer, "   Warning Counts:\n");
   for (i = 0; i < rsvdWarnType; i++) {
      sprintf(inBuf, "   %s   = %d\n", warnTypeStr[i], warnCnts[i]);
      strcat(buffer, inBuf);
   }
}
