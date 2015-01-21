/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#ifndef VENDOR_GOOGLE_DIAGS_WINDCHARGER_IO_H_
#define VENDOR_GOOGLE_DIAGS_WINDCHARGER_IO_H_

#include <inttypes.h>

#define MEM_DEV_FILE "/dev/mem"
#define FILENAME_SIZE 64

int io_r_field(uint64_t addr, uint32_t* value, uint32_t msb, uint32_t lsb);
int io_w_field(uint64_t addr, uint32_t value, uint32_t msb, uint32_t lsb);
int io_w(uint64_t addr, uint32_t value);

int write_physical_addr(uint64_t addr, uint32_t value);
int read_physical_addr(uint64_t addr, uint32_t* value);

#endif  // VENDOR_GOOGLE_DIAGS_WINDCHARGER_IO_H_
