/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * This file provides MoCA diagnostics related data structures and definitions.
 */

#ifndef _DIAG_ERROR_H_
#define _DIAG_ERROR_H_

/*
 * Codes: 2 bytes = 0xHHLL, High byte + Low byte
 * High byte: Bit 0 - Bit 3 : Components
 *            Bit 4 - Bit 7 : Severity Level
 *    Bit 0 - Bit 3:
 *    0x00 - Broadcom MoCA
 *    0x01 - Broadcom Giga-bit Ethernet
 *    0x02 - MTD/NAND
 *    0x03 - MCE, Kernel Memory Management
 *
 *    Bit 4 - Bit 7:
 *    0x00 - Error, Critial
 *    0x10 - Warning
 *
 *
 * Low Byte:
 *    specific error/warning/....
 */

#define COMPONENT_BITS_MASK                  0x0F00
#define SEVERITY_LEVEL_BITS_MASK             0xF000
#define SEVERITY_LEVEL_WARNING               0x1000

#define ERROR_CODE_COMPONENT_BRCM_MOCA       0x00
#define ERROR_CODE_COMPONENT_BRCM_GENET      0x01
#define ERROR_CODE_COMPONENT_MTD_NAND        0x02
#define ERROR_CODE_COMPONENT_KERNEL_MM       0x03
#define ERROR_CODE_COMPONENT_MAX             0x04
#define ERROR_CODE_UNKNOWN_COMPONENT_TYPE    0xFF
#define GET_ERROR_CODE_COMPONENT_TYPE(code)       ((code & COMPONENT_BITS_MASK) >> 8)
#define IS_DIAG_WARNING_CODE(code)   \
       ((code & SEVERITY_LEVEL_BITS_MASK) == SEVERITY_LEVEL_WARNING)

/* Errors issued by Broadcom MoCA driver */
#define MOCA_INIT_ERROR                     0x0000
#define MOCA_PROBE_ERROR                    0x0001

/* Errors issued by Broadcom Giga-bit Ethernet driver */
#define GENET_OPEN_ERROR                    0x0100
#define GENET_TXRING_ERROR                  0x0101
#define GENET_TXDMA_MAP_ERROR               0x0102
#define GENET_RING_XMIT_ERROR               0x0103
#define GENET_RX_SKB_ALLOC_ERROR            0x0104
#define GENET_ASSIGN_RX_BUFFER_ERROR        0x0105
#define GENET_HFB_UPDATE_ERROR              0x0106
#define GENET_HFB_READ_ERROR                0x0107
#define GENET_PROBE_ERROR                   0x0108
#define GENET_PWR_DOWN_ERROR                0x0109
#define GENET_PHY_INIT_ERROR                0x010A

/* Errors issued by mtd/nand */
#define NAND_INIT_ERROR                     0x0200
#define NAND_BBT_WR_ERROR                   0x0201
#define NAND_BBT_OUT_OF_MEM_ERROR           0x0202
#define NAND_BBT_SCAN_ERROR                 0x0203
#define NAND_ECC_UNCORRECTABLE_ERROR        0x0204
#define NAND_NO_DEV_ERROR                   0x0205

/* errors issued by kernel memory management */
#define MCE_HW_MEM_CORRUPT_ERROR            0x0300
#define MCE_OUT_OF_MEM_ERROR                0x0301
#define MCE_HW_POISONED_ERROR               0x0302


/* Warnings issued by Broadcom MoCA driver */
#define MOCA_M2M_XFER_WARN                   0x1000
#define MOCA_WRT_MEM_WARN                    0x1001
#define MOCA_RD_MEM_WARN                     0x1002
#define MOCA_GET_PAGES_WARN                  0x1003
#define MOCA_WRT_IMG_WARN                    0x1004
#define MOCA_RECVMSG_WARN                    0x1005
#define MOCA_WDT_WARN                        0x1006
#define MOCA_CANNOT_GET_MBX_BASE_WARN        0x1007
#define MOCA_RECVMSG_ASSERT_FAIL_WARN        0x1008
#define MOCA_RECVMSG_CORE_REQ_FAIL_WARN      0x1009
#define MOCA_RECVMSG_HOST_RSP_FAIL_WARN      0x100A
#define MOCA_PROBE_REQ_INTERRUPT_FAIL_WARN   0x100B
#define MOCA_PROBE_REG_CLASS_DEV_FAIL_WARN   0x100C

/* Warnings issued by Broadcom Giga-bit Ethernet driver */
#define GENET_DROP_FRAGMENTED_PKT_WARN       0x1100

/* Warnings issued by mtd/nand */
#define NAND_BBT_WRT_WARN                    0x1200
#define NAND_EDU_RBUS_WARN                   0x1201
#define NAND_RD_UNCORRECTABLE_WARN           0x1202

typedef enum {
   DIAG_MOCA_INIT_ERROR = 0,
   DIAG_MOCA_PROBE_ERROR,
   DIAG_MOCA_ERROR_MAX
} diag_moca_errType_e;

typedef enum {
   DIAG_MOCA_M2M_XFER_WARN = 0,
   DIAG_MOCA_WRITE_WARN,
   DIAG_MOCA_READ_WARN,
   DIAG_MOCA_NO_MEM_WARN,
   DIAG_MOCA_PROBE_WARN,
   DIAG_MOCA_WARN_MAX
} diag_moca_warnType_e;

typedef enum {
   DIAG_GENET_OPEN_ERROR = 0,
   DIAG_GENET_XMIT_ERROR,
   DIAG_GENET_REVC_ERROR,
   DIAG_GENET_HFB_ERROR,
   DIAG_GENET_PROBE_ERROR,
   DIAG_GENET_PWR_DOWN_ERROR,
   DIAG_GENET_PHY_ERROR,
   DIAG_GENET_ERROR_MAX
} diag_genet_errorType_e;

typedef enum {
   DIAG_GENET_DROP_FRAGMENTED_PKT_WARN = 0,
   DIAG_GENET_WARN_MAX
} diag_genet_warnType_e;

typedef enum {
   DIAG_NAND_INIT_ERROR = 0,
   DIAG_NAND_BBT_ERROR,
   DIAG_NAND_ECC_ERROR,
   DIAG_NAND_NO_DEV_ERROR,
   DIAG_NAND_ERROR_MAX
} diag_nand_errType_e;

typedef enum {
   DIAG_NAND_BBT_WRITE_WARN = 0,
   DIAG_NAND_EDU_RBUS_WARN,
   DIAG_NAND_READ_UNCORRECTABLE_WARN,
   DIAG_NAND_WARN_MAX
} diag_nand_warnType_e;

typedef enum {
   DIAG_MCE_MEM_CORRUPT_ERROR = 0,
   DIAG_MCE_OUT_OF_MEM_ERROR,
   DIAG_MCE_HW_POISONED_ERROR,
   DIAG_MCE_ERROR_MAX
} diag_mce_errorType_e;

typedef enum {
   DIAG_MCE_WARN_MAX = 0
} diag_mce_warnType_e;

typedef struct diagErrorCodeEntry_t_ {
   unsigned short errorCode;
   unsigned char errorType;
} diagErrorCodeEntry_t;

typedef struct diagErroCodeTbl_t_ {
   int numOfEntry;
   diagErrorCodeEntry_t *tbl;
} diagErrorCodeTbl_t;

typedef struct diagMocaErrCounts_t_ {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_MOCA_ERROR_MAX];
   unsigned short   WarnCount[DIAG_MOCA_WARN_MAX];
} diagMocaErrCounts_t;

typedef struct diagGeneErrCounts_t_ {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_GENET_ERROR_MAX];
   unsigned short   WarnCount[DIAG_GENET_WARN_MAX];
} diagGenetErrCounts_t;


typedef struct diagNandErrCounts_t_ {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_NAND_ERROR_MAX];
   unsigned short   WarnCount[DIAG_NAND_WARN_MAX];
} diagNandErrCounts_t;

typedef struct diagMceErrCounts_t_ {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_MCE_ERROR_MAX];
   unsigned short   WarnCount[DIAG_MCE_WARN_MAX];
} diagMceErrCounts_t;

#define DIAG_MOCA_ERR_COUNTS_SZ    sizeof(diagMocaErrCounts_t)
#define DIAG_GENET_ERR_COUNTS_SZ   sizeof(diagGenetErrCounts_t)
#define DIAG_NAND_ERR_COUNTS_SZ    sizeof(diagNandErrCounts_t)
#define DIAG_MCE_ERR_COUNTS_SZ     sizeof(diagMceErrCounts_t)
#define DIAG_ALL_ERR_COUNTS_SZ     (DIAG_MOCA_ERR_COUNTS_SZ + \
                                    DIAG_GENET_ERR_COUNTS_SZ + \
                                    DIAG_NAND_ERR_COUNTS_SZ + \
                                    DIAG_MCE_ERR_COUNTS_SZ)

#define DIAG_UNKNOWN_ERROR_TYPE 0xFF

#define DIAG_ERROR_CODE_ENTRY_SZ   sizeof(diagErrorCodeEntry_t)

#endif /* end of _DIAG_ERROR_H_ */
