/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics routines related data structures and definitions.
 */

#ifndef _DIAG_SUBS_H_
#define _DIAG_SUBS_H_


/* =================================================================
 * Network related definitions
 * =================================================================
 */
#define MDIO_START_BUSY               (1 << 29)
#define MDIO_READ_FAIL                (1 << 28)
#define MDIO_RD                       (2 << 26)
#define MDIO_WR                       (1 << 26)
#define MDIO_PHY_REG_ADDR_MASK        0x00FF0000
#define MDIO_PHY_REG_SHIFT            16
#define MDIO_PHY_REG_ADDR(_addr)  \
    ((_addr << MDIO_PHY_REG_SHIFT) & MDIO_PHY_REG_ADDR_MASK)
#define MDIO_REG_DATA_MASK            0x0000FFFF

/* BCRM external gphy registers */
#define PHY3450_CTRL_REG              0x00
 #define PHY3450_PHY_RESET            BIT(15)   /* Phy Reset              */
 #define PHY3450_CTRL_AUTO_ENG_EN     BIT(12)   /* auto-negotiation bit   */
 #define PHY3450_CTRL_I_LOOPBACK_EN   BIT(14)   /* interanl loopback mode */

/* GENET (eth0) MDIO Command Register */
#define GENET_0_UMAC_MDIO_CMD         0x10B80E14



/*--------------------------------------------------------------------------
 * Socket handling related definitions
 *--------------------------------------------------------------------------
 */
#define DIAG_SOCKET_NOT_OPEN    (-1)
#define DIAG_FD_NOT_OPEN        (-1)


/*--------------------------------------------------------------------------
 * Host command related definitions
 *--------------------------------------------------------------------------
 */

#define DIAG_HOSTCMD_PORT       50152         /* port number to use */
#define DIAG_HOSTREQ_BUF_LEN    (1024 * 1)    /* payload size include msg header */



/*--------------------------------------------------------------------------
 * CPU Temperature related definitions
 *--------------------------------------------------------------------------
 */

#if 1
 /* Per Humax engineer, BRCM makes the CPU Temperature register not available
  * to be access.
  * TBD -
  * 1) If BRCM will make the register to be accessed
  * 2) If item 1) is yes, need instructions of accessing the reg from BRCM
  */
 #define BRCM_7425_CPU_REG_ENABLE
#endif

/*
 * Per BRCM engineer,
 * The register records the PVTMON temperature measurement.
 * 1) Offset 0x00433300  (Physical Address - 0x10433300)
 * 2) Bit definitions
 */
#define AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS            0x10433300
 /* Done bit       - 1: Measure is done ; 0 - Measure is not done */
 #define  AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS_DONE_MASK    BIT(16)
 /* valid_data bit - 1: Valid measurement data */
 #define  AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS_VALID_DATA_MASK BIT(10)
 /* data bits (09:00) : Measure data from PVT monitor and check the "valid_data */
 #define  AVS_RO_REGISTERS_0_PVT_TEMPERATURE_MNTR_STATUS_DATA_MASK    0x000003FF

/* BRUNO LED CONTROL INTERFACE */
#define BRUNO_LED_CTRL_FNAME  "/tmp/gpio/leds"
#define SOLID_RED        "1"
#define SOLID_BLUE       "2"
#define BLINK_RED        "1 0"
#define BLINK_BLUE       "2 0"
#define FLASH_RED        "1 0 1 0"
#define FLASH_BLUE       "2 0 2 0"
#define FAST_FLASH_RED   "1 0 1 0 1 0"
#define FAST_FLASH_BLUE  "2 0 2 0 2 0"

typedef enum {
  DIAG_LED_SOLID_RED = 0,
  DIAG_LED_SOLID_BLUE,
  DIAG_LED_BLINK_RED,
  DIAG_LED_BLINK_BLUE,
  DIAG_LED_FLASH_RED,
  DIAG_LED_FLASH_BLUE,
  DIAG_LED_FASTFLASH_RED,
  DIAG_LED_FASTFLASH_BLUE,
  DIAG_LED_IND_MAX
} diag_led_indicator;

typedef struct diag_led_table_t_ {
  const char *name;
  const char *num_seq;
} diag_led_table_t;

/*
 * Declare diagd related global variables.
 */
extern diag_info_t *pDiagInfo;


/* Prototypes */
int diagAccessReg(off_t regAddr, uint32_t *pRegData, bool wr);
int diagRd_54612_PhyReg(uint8_t regAddr, uint16_t *pRegData);
int diagWr_54612_PhyReg(uint8_t regAddr, uint16_t *pRegData);
int diag_netIf_UpDown(char *netIf, bool netIfUp);
int diagtTestResultsLogFile(FILE  **testResultsFp);


#ifdef BRCM_7425_CPU_REG_ENABLE
 int diag_Read_CPU_Temperature(double *pTemperature, uint32_t *pRegData);
#endif /* end of diag_Read_CPU_Temperature */

void diag_GetStartingAddr_NetIfInfo(char *pNetif_name, diag_netIf_info_t **pNetIf);
int diag_Get_Netlink_State(netif_netlink_t *netif_linkstate);

int diag_Get_Netif_Counters(char *pNetif_name, unsigned char bNormalMode);

int diagd_Init(char *refFile);

int diag_CmdHandler_Init(void);
void diag_CmdHandler_Uninit(void);

int diagMoca_GetConfig(diag_moca_config_t *pCfg);

void diag_CloseFileDesc(int *pFd);
void diagSendAlarm(void);

#endif /* end of _DIAG_SUBS_H_ */
