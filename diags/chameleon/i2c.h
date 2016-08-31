/*
 * (C) Copyright 2014 Google, Inc.
 * All rights reserved.
 *
 */

#ifndef optimus_i2c_h
#define optimus_i2c_h

#include <inttypes.h>

#define I2C_DEV_FILE   "/dev/i2c-%d"
#define I2C_PAGE_SIZE  16
#define I2C_M_RD       0x01
#define FILENAME_SIZE  64

/*
 * I2C Message - used for pure i2c transaction, also from /dev interface
 */
struct i2c_msg {
  uint16_t addr;  /* slave address */
  uint16_t flags;
  uint16_t len;   /* msg length */
  uint8_t *buf;   /* pointer to msg data */
};

/* This is the structure as used in the I2C_RDWR ioctl call */
struct i2c_rdwr_ioctl_data {
  struct i2c_msg *msgs;  /* pointers to i2c_msgs */
  uint32_t nmsgs;        /* number of i2c_msgs */
};

#define I2C_SLAVE        0x0703  /* Change slave address */
#define I2C_SLAVE_FORCE  0x0706  /* Use this slave address, even if it
                                    is already in use by a driver! */
#define I2C_RDWR         0x0707  /* Combined R/W transfer (one stop only) */

int i2cr(int controller, uint8_t device_addr, uint32_t cell_addr,
         uint32_t addr_len, uint32_t data_len, uint8_t* buf);

int i2cw(int controller, uint8_t device_addr, uint32_t cell_addr,
         uint32_t addr_len, uint32_t data_len, uint8_t* buf);

#endif  // optimus_i2c_h
