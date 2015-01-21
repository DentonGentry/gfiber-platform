/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

#include "i2c.h"

extern int ioctl(int, int, void *);

int i2cr(int controller, uint8_t device_addr, uint32_t cell_addr,
         uint32_t addr_len, uint32_t data_len, uint8_t *buf) {
  char filename[FILENAME_SIZE];
  int file;
  int i;
  uint32_t temp;
  unsigned char addrbuf[4]; /* up to 4 byte addressing ! */
  int return_code = 0;
  struct i2c_msg message[2];
  struct i2c_rdwr_ioctl_data rdwr_arg;
  unsigned int read_data_len;

  /* open the I2C adapter */
  snprintf(filename, sizeof(filename), I2C_DEV_FILE, controller);
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("I2C Error open file %s: %s", filename, strerror(errno));
    return -1;
  }

  if ((return_code = flock(file, LOCK_EX)) < 0) {
    printf("I2C Error lock file %s: %s", filename, strerror(errno));
    goto i2cr_cleanup;
  }

  /* if we need to send addr, use combined transaction */
  if (addr_len > 0) {
    /* build struct i2c_msg 0 */
    message[0].addr = device_addr;
    message[0].flags = 0;
    message[0].len = addr_len;
    temp = cell_addr;
    /* store addr into buffer */
    for (i = addr_len - 1; i >= 0; i--) {
      addrbuf[i] = temp & 0xff;
      temp >>= 8;
    }
    message[0].buf = addrbuf;

    /* build struct i2c_msg 1 */
    message[1].addr = device_addr;
    message[1].flags = I2C_M_RD;
    message[1].len = data_len;
    message[1].buf = buf;

    /* build arg */
    rdwr_arg.msgs = message;
    rdwr_arg.nmsgs = 2;

    return_code = ioctl(file, I2C_RDWR, &rdwr_arg);

    /* since we pass in rdwr_arg.nmsgs = 2, expect a return of 2 on success */
    if (return_code == 2) {
      return_code = 0;
    }

    goto i2cr_cleanup;
  }

  /* setup device address */
  if ((return_code = ioctl(file, I2C_SLAVE, (void *)(uintptr_t)device_addr)) <
      0) {
    printf("I2C Error: Could not set device address to %x: %s", device_addr,
           strerror(errno));
    goto i2cr_cleanup;
  }

  /* now read data out of the device */
  read_data_len = read(file, buf, data_len);
  if (read_data_len == data_len) {
    return_code = 0;
  }

i2cr_cleanup:
  flock(file, LOCK_UN);
  close(file);

  return return_code;
}

int i2cw(int controller, uint8_t device_addr, uint32_t cell_addr,
         uint32_t addr_len, uint32_t data_len, uint8_t *buf) {
  char filename[FILENAME_SIZE];
  int file;
  int i;
  uint32_t temp;
  uint8_t tempbuf[I2C_PAGE_SIZE + 4];
  int return_code;
  uint8_t *writebuf = buf;
  unsigned int write_data_len;

  /* check data len */
  if (data_len > I2C_PAGE_SIZE) {
    return -1;
  }

  /* open the corrrsponding I2C adapter */
  snprintf(filename, sizeof(filename), I2C_DEV_FILE, controller);
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("I2C Error open file %s: %s", filename, strerror(errno));
    return -1;
  }

  if ((return_code = flock(file, LOCK_EX)) < 0) {
    printf("I2C Error lock file %s: %s", filename, strerror(errno));
    goto i2cw_cleanup;
  }

  /* setup device address */
  if ((return_code =
           ioctl(file, I2C_SLAVE_FORCE, (void *)(uintptr_t)device_addr)) < 0) {
    printf("I2C Error: Could not set device address to %x: %s", device_addr,
           strerror(errno));
    goto i2cw_cleanup;
  }

  /* if we need to send addr */
  if (addr_len > 0) {
    temp = cell_addr;
    /* store addr into buffer */
    for (i = (int)(addr_len - 1); i >= 0; i--) {
      tempbuf[i] = temp & 0xff;
      temp >>= 8;
    }
    /* copy data over into tempbuf, right after the cell address */
    for (i = 0; i < (int)(data_len); i++) {
      tempbuf[addr_len + i] = buf[i];
    }
    writebuf = tempbuf;
  }

  /* now write addr + data out to the device */
  write_data_len = write(file, writebuf, addr_len + data_len);

  if (write_data_len == (addr_len + data_len)) {
    return_code = 0;
  }

i2cw_cleanup:
  flock(file, LOCK_UN);
  close(file);

  return return_code;
}
