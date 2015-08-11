/* Copyright 2012 Google Inc. All Rights Reserved.
 * Author: weixiaofeng@google.com (Xiaofeng Wei)
 */

#ifndef _SYSVAR_H_
#define _SYSVAR_H_

//#define SYSVAR_UBOOT

#ifndef SYSVAR_UBOOT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#else
typedef enum _bool{false, true} bool;
#endif
#include <malloc.h>

#define SYSVAR_WC32         4
#define SYSVAR_CRC32        4
#define SYSVAR_HEAD         (SYSVAR_WC32 + SYSVAR_CRC32)
#define SYSVAR_NAME         32
#define SYSVAR_MESSAGE      -1

#define SYSVAR_STR_TO_BUF   0x00
#define SYSVAR_BUF_TO_STR   0xff

#define SYSVAR_BLOCK_SIZE   0x00010000  /* size of system variables (64K) */

#define SYSVAR_SPI_BLOCK    4           /* number of SPI flash blocks */
#define SYSVAR_RW_OFFSET0   0x00100000  /* location of system variables(RW) */
#define SYSVAR_RW_OFFSET1   0x00120000  /* location of system variables(RW backup) */
#define SYSVAR_RO_OFFSET0   0x00140000  /* location of system variables(RO) */
#define SYSVAR_RO_OFFSET1   0x00160000  /* location of system variables(RO backup) */

#define SYSVAR_MTD_DEVICE   4           /* maximum number of MTD devices. */
#define SYSVAR_RW_NAME0     "/dev/mtd2" /* MTD device of system variables(RW) */
#define SYSVAR_RW_NAME1     "/dev/mtd3" /* MTD device of system variables(RW backup) */
#define SYSVAR_RO_NAME0     "/dev/mtd4" /* MTD device of system variables(RO) */
#define SYSVAR_RO_NAME1     "/dev/mtd5" /* MTD device of system variables(RO backup) */

#define SYSVAR_RW_DATA0     0
#define SYSVAR_RW_DATA1     1
#define SYSVAR_RO_DATA0     2
#define SYSVAR_RO_DATA1     3

#define SYSVAR_RW_BUF       0
#define SYSVAR_RO_BUF       2

#define SYSVAR_GET_MODE     0
#define SYSVAR_SET_MODE     1
#define SYSVAR_LOAD_MODE    2
#define SYSVAR_SAVE_MODE    3

#define SYSVAR_SUCCESS      0
#define SYSVAR_MEMORY_ERR   -1
#define SYSVAR_OPEN_ERR     -2
#define SYSVAR_READ_ERR     -3
#define SYSVAR_WRITE_ERR    -4
#define SYSVAR_ERASE_ERR    -5
#define SYSVAR_LOAD_ERR     -6
#define SYSVAR_SAVE_ERR     -7
#define SYSVAR_GET_ERR      -8
#define SYSVAR_SET_ERR      -9
#define SYSVAR_DELETE_ERR   -10
#define SYSVAR_PARAM_ERR    -11
#define SYSVAR_CRC_ERR      -12
#define SYSVAR_READONLY_ERR -13
#define SYSVAR_EXISTED_ERR  -14
#define SYSVAR_DEBUG_ERR    -15

struct sysvar_list {
  char name[SYSVAR_NAME + 1]; /* name of system variable */
  char *value;                /* value of system variable */
  int len;                    /* length of system variable */

  struct sysvar_list *next;
};

struct sysvar_buf {
  unsigned char *data;  /* data buffer to store system variables */
  int data_len;         /* buffer size */

  int total_len;        /* total space = buffer size - buffer header */
  int free_len;         /* free space in data buffer */
  int used_len;         /* total bytes of variables */

  bool loaded;          /* data buffer has been loaded from SPI flash */
  bool modified;        /* data modified in the data buffer */
  bool readonly;        /* read only system variables */
  bool failed[2];       /* failed to read data from SPI flash */

  struct sysvar_list *list;
};

extern unsigned long get_wc32(struct sysvar_buf *buf);
extern void set_wc32(struct sysvar_buf *buf);
extern unsigned long get_crc32(struct sysvar_buf *buf);
extern void set_crc32(struct sysvar_buf *buf);

extern int load_var(struct sysvar_buf *buf);
extern int save_var(struct sysvar_buf *buf);
extern int get_var(struct sysvar_list *var, char *name, char *value, int len);
extern int set_var(struct sysvar_buf *buf, char *name, char *value);
extern int delete_var(struct sysvar_buf *buf, struct sysvar_list *var);
extern int clear_var(struct sysvar_buf *buf);
extern struct sysvar_list *find_var(struct sysvar_buf *buf, char *name);
extern int check_var(struct sysvar_buf *buf, int mode);
extern void print_var(struct sysvar_buf *buf);

extern void clear_buf(struct sysvar_buf *buf);
extern void dump_buf(struct sysvar_buf *buf, int start, int len);

#endif  /* _SYSVAR_H_ */
