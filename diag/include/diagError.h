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
 *    0x02 - MTD, MTD/NAND
 *    0x03 - SPI
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

typedef enum {
   ERROR_CODE_COMPONENT_BRCM_MOCA = 0,
   ERROR_CODE_COMPONENT_BRCM_GENET,
   ERROR_CODE_COMPONENT_MTD_NAND,
   ERROR_CODE_COMPONENT_BRCM_SPI,
   ERROR_CODE_COMPONENT_MAX,
   ERROR_CODE_UNKNOWN_COMPONENT_TYPE = 0xFF
} diag_compType_e;

#define GET_ERROR_CODE_COMPONENT_TYPE(code)       ((code & COMPONENT_BITS_MASK) >> 8)
#define IS_DIAG_WARNING_CODE(code)   \
       ((code & SEVERITY_LEVEL_BITS_MASK) == SEVERITY_LEVEL_WARNING)

/* Errors issued by Broadcom MoCA driver */
#define MOCA_INIT_ERROR                      0x0000
#define MOCA_PROBE_ERROR                     0x0001

/* Errors issued by Broadcom Giga-bit Ethernet driver */
#define GENET_OPEN_ERROR                     0x0100
#define GENET_TXRING_ERROR                   0x0101
#define GENET_TXDMA_MAP_ERROR                0x0102
#define GENET_RING_XMIT_ERROR                0x0103
#define GENET_RX_SKB_ALLOC_ERROR             0x0104
#define GENET_ASSIGN_RX_BUFFER_ERROR         0x0105
#define GENET_HFB_UPDATE_ERROR               0x0106
#define GENET_HFB_READ_ERROR                 0x0107
#define GENET_PROBE_ERROR                    0x0108
#define GENET_PWR_DOWN_ERROR                 0x0109
#define GENET_PHY_INIT_ERROR                 0x010A

/* Errors issued by mtd, mtd/nand */
#define MTD_NAND_INIT_ERROR                  0x0200
#define MTD_NAND_BBT_WR_ERROR                0x0201
#define MTD_NAND_BBT_OUT_OF_MEM_ERROR        0x0202
#define MTD_NAND_BBT_SCAN_ERROR              0x0203
#define MTD_NAND_ECC_UNCORRECTABLE_ERROR     0x0204
#define MTD_ALLOC_PARTITION_ERROR            0x0205
#define MTD_INIT_ERROR                       0x0206

/* Errors issued by Broadcom SPI */
#define SPI_PROBE_ERROR                      0x0300
#define SPI_UNRECOG_FLASH_TYPE_ERROR         0x0301


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
#define MOCA_I2C_BASE_ADDR_NOT_SET_WARN      0x100D

/* Warnings issued by Broadcom Giga-bit Ethernet driver */
#define GENET_DROP_FRAGMENTED_PKT_WARN       0x1100

/* Warnings issued by mtd, mtd/nand */
#define MTD_NAND_BBT_WRT_WARN                0x1200
#define MTD_NAND_EDU_RBUS_WARN               0x1201
#define MTD_NAND_RD_UNCORRECTABLE_WARN       0x1202
#define MTD_NAND_NO_DEV_WARN                 0x1203
#define MTD_ALLOC_PARTITION_WARN             0x1204
#define MTD_BLKTRANS_REG_WARN                0x1205
#define MTD_ERASE_WRT_WARN                   0x1206
#define MTD_BRCMSTB_SETP_WARN                0x1207

/* Warnings issued by SPI */
#define SPI_FLASH_SETUP_WARN                 0x1300
#define SPI_CS_SETUP_WARN                    0x1301

typedef enum {
   DIAG_MOCA_INIT_ERROR = 0,
   DIAG_MOCA_PROBE_ERROR,
   DIAG_MOCA_RESERVED_1_ERROR,
   DIAG_MOCA_RESERVED_2_ERROR,
   DIAG_MOCA_RESERVED_3_ERROR,
   DIAG_MOCA_RESERVED_4_ERROR,
   DIAG_MOCA_ERROR_MAX
} diag_moca_errType_e;

typedef enum {
   DIAG_MOCA_M2M_XFER_WARN = 0,
   DIAG_MOCA_WRITE_WARN,
   DIAG_MOCA_READ_WARN,
   DIAG_MOCA_NO_MEM_WARN,
   DIAG_MOCA_PROBE_WARN,
   DIAG_MOCA_REG_WARN,
   DIAG_MOCA_RESERVED_1_WARN,
   DIAG_MOCA_RESERVED_2_WARN,
   DIAG_MOCA_RESERVED_3_WARN,
   DIAG_MOCA_RESERVED_4_WARN,
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
   DIAG_GENET_RESERVED_1_ERROR,
   DIAG_GENET_RESERVED_2_ERROR,
   DIAG_GENET_RESERVED_3_ERROR,
   DIAG_GENET_RESERVED_4_ERROR,
   DIAG_GENET_ERROR_MAX
} diag_genet_errorType_e;

typedef enum {
   DIAG_GENET_DROP_FRAGMENTED_PKT_WARN = 0,
   DIAG_GENET_RESERVED_1_WARN,
   DIAG_GENET_RESERVED_2_WARN,
   DIAG_GENET_RESERVED_3_WARN,
   DIAG_GENET_RESERVED_4_WARN,
   DIAG_GENET_WARN_MAX
} diag_genet_warnType_e;

typedef enum {
   DIAG_MTD_NAND_INIT_ERROR = 0,
   DIAG_MTD_NAND_BBT_ERROR,
   DIAG_MTD_NAND_ECC_ERROR,
   DIAG_MTD_ALLOC_PARTITION_ERROR,
   DIAG_MTD_INIT_ERROR,
   DIAG_MTD_RESERVED_1_ERROR,
   DIAG_MTD_RESERVED_2_ERROR,
   DIAG_MTD_RESERVED_3_ERROR,
   DIAG_MTD_RESERVED_4_ERROR,
   DIAG_MTD_RESERVED_5_ERROR,
   DIAG_MTD_RESERVED_6_ERROR,
   DIAG_MTD_RESERVED_7_ERROR,
   DIAG_MTD_RESERVED_8_ERROR,
   DIAG_MTD_RESERVED_9_ERROR,
   DIAG_MTD_RESERVED_10_ERROR,
   DIAG_MTD_NAND_ERROR_MAX
} diag_mtd_nand_errType_e;

typedef enum {
   DIAG_MTD_NAND_BBT_WRITE_WARN = 0,
   DIAG_MTD_NAND_EDU_RBUS_WARN,
   DIAG_MTD_NAND_READ_UNCORRECTABLE_WARN,
   DIAG_MTD_NAND_NO_DEV_WARN,
   DIAG_MTD_ALLOC_PARTITION_WARN,
   DIAG_MTD_BLKTRANS_REG_WARN,
   DIAG_MTD_ERASE_WRT_WARN,
   DIAG_MTD_BRCMSTB_SETP_WARN,
   DIAG_MTD_RESERVED_1_WARN,
   DIAG_MTD_RESERVED_2_WARN,
   DIAG_MTD_RESERVED_3_WARN,
   DIAG_MTD_RESERVED_4_WARN,
   DIAG_MTD_RESERVED_5_WARN,
   DIAG_MTD_RESERVED_6_WARN,
   DIAG_MTD_RESERVED_7_WARN,
   DIAG_MTD_RESERVED_8_WARN,
   DIAG_MTD_RESERVED_9_WARN,
   DIAG_MTD_RESERVED_10_WARN,
   DIAG_MTD_NAND_WARN_MAX
} diag_mtd_nand_warnType_e;

typedef enum {
   DIAG_SPI_PROBE_ERROR = 0,
   DIAG_SPI_UNRECOG_FLASH_TYPE_ERROR,
   DIAG_SPI_RESERVED_1_ERROR,
   DIAG_SPI_RESERVED_2_ERROR,
   DIAG_SPI_RESERVED_3_ERROR,
   DIAG_SPI_RESERVED_4_ERROR,
   DIAG_SPI_RESERVED_5_ERROR,
   DIAG_SPI_RESERVED_6_ERROR,
   DIAG_SPI_RESERVED_7_ERROR,
   DIAG_SPI_RESERVED_8_ERROR,
   DIAG_SPI_RESERVED_9_ERROR,
   DIAG_SPI_RESERVED_10_ERROR,
   DIAG_SPI_ERROR_MAX
} diag_spi_errorType_e;

typedef enum {
   DIAG_SPI_FLASH_SETUP_WARN = 0,
   DIAG_SPI_CS_SETUP_WARN,
   DIAG_SPI_RESERVED_1_WARN,
   DIAG_SPI_RESERVED_2_WARN,
   DIAG_SPI_RESERVED_3_WARN,
   DIAG_SPI_RESERVED_4_WARN,
   DIAG_SPI_RESERVED_5_WARN,
   DIAG_SPI_RESERVED_6_WARN,
   DIAG_SPI_RESERVED_7_WARN,
   DIAG_SPI_RESERVED_8_WARN,
   DIAG_SPI_RESERVED_9_WARN,
   DIAG_SPI_RESERVED_10_WARN,
   DIAG_SPI_WARN_MAX
} diag_spi_warnType_e;

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

typedef struct diagMtdNandErrCounts_t_ {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_MTD_NAND_ERROR_MAX];
   unsigned short   WarnCount[DIAG_MTD_NAND_WARN_MAX];
} diagMtdNandErrCounts_t;

typedef struct diagSpiErrCounts_t_ {
   unsigned int     TotalErrCount;
   unsigned int     TotalWarnCount;
   unsigned short   ErrCount[DIAG_SPI_ERROR_MAX];
   unsigned short   WarnCount[DIAG_SPI_WARN_MAX];
} diagSpiErrCounts_t;

typedef struct diagErrsInfoEntry_t_ {
   char           *componentTypStr;
   unsigned char  rsvdErrType;
   unsigned char  rsvdWarnType;
   char           **errTypeStr;
   char           **warnTypeStr;
} diagErrsInfoEntry_t;

#define DIAG_MOCA_ERR_COUNTS_SZ       sizeof(diagMocaErrCounts_t)
#define DIAG_GENET_ERR_COUNTS_SZ      sizeof(diagGenetErrCounts_t)
#define DIAG_MTD_NAND_ERR_COUNTS_SZ   sizeof(diagMtdNandErrCounts_t)
#define DIAG_SPI_ERR_COUNTS_SZ        sizeof(diagSpiErrCounts_t)
#define DIAG_ALL_ERR_COUNTS_SZ        (DIAG_MOCA_ERR_COUNTS_SZ + \
                                       DIAG_GENET_ERR_COUNTS_SZ + \
                                       DIAG_MTD_NAND_ERR_COUNTS_SZ + \
                                       DIAG_SPI_ERR_COUNTS_SZ)

#define DIAGD_MOCA_ERR_COUNTS_INDEX   (0)
#define DIAGD_GENET_ERR_COUNTS_INDEX  (DIAGD_MOCA_ERR_COUNTS_INDEX + DIAG_MOCA_ERR_COUNTS_SZ)
#define DIAGD_MTD_NAND_ERR_COUNTS_INDEX (DIAGD_GENET_ERR_COUNTS_INDEX + DIAG_GENET_ERR_COUNTS_SZ)
#define DIAGD_SPI_ERR_COUNTS_INDEX    (DIAGD_MTD_NAND_ERR_COUNTS_INDEX + DIAG_MTD_NAND_ERR_COUNTS_SZ)

#define DIAG_UNKNOWN_ERROR_TYPE 0xFF

#define DIAG_ERROR_CODE_ENTRY_SZ   sizeof(diagErrorCodeEntry_t)

#endif /* end of _DIAG_ERROR_H_ */
