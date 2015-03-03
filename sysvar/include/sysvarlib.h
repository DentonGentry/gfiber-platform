/* Copyright 2012 Google Inc. All Rights Reserved.
 * Author: weixiaofeng@google.com (Xiaofeng Wei)
 */

#ifndef _SYSVARLIB_H_
#define _SYSVARLIB_H_

#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <mtd/mtd-abi.h>

#include "sysvar.h"

#define SYSVAR_MTD_READ     0
#define SYSVAR_MTD_WRITE    1
#define SYSVAR_MTD_ERASE    2

#define SYSVAR_MTD_OFFSET   0

extern struct sysvar_buf *sv_buf(int idx);

extern int open_mtd(void);
extern void close_mtd(void);
extern void set_mtd_verbose(bool v);

extern int loadvar(void);
extern int savevar(void);
extern int getvar(char *name, char *value, int len);
extern int setvar(char *name, char *value);

extern void sysvar_info(int idx);
extern void sysvar_dump(int idx, int start, int len);
extern int sysvar_io(int idx, int mode);

#endif  /* _SYSVARLIB_H_ */
