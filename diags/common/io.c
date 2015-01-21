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
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "io.h"
#include "util.h"

int io_r_field(uint64_t addr, uint32_t *value, uint32_t msb, uint32_t lsb) {
  char filename[FILENAME_SIZE];
  int file;
  void *virt_addr;
  uint64_t file_start;
  uint32_t file_offset;
  int page_size;
  uint32_t mask, shift;

  snprintf(filename, sizeof(filename), MEM_DEV_FILE);
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("Error open file %s: %s", filename, strerror(errno));
    return -1;
  }

  page_size = getpagesize();

  /* map to file at file_start, which has to be page aligned */
  file_start = (addr / page_size) * page_size;
  file_offset = addr % page_size;

  virt_addr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, file,
                   file_start);
  if (virt_addr == MAP_FAILED) {
    printf("Error mmap file %s", filename);
    close(file);
    return -1;
  }

  *value = *(uint32_t *)((uintptr_t)virt_addr + file_offset);

  get_mask_shift(msb, lsb, &mask, &shift);
  *value &= mask;
  *value >>= shift;

  if (munmap(virt_addr, sizeof(int)) < 0) {
    printf("Error munmap file %s", filename);
    close(file);
    return -1;
  }

  close(file);
  return 0;
}

int io_w_field(uint64_t addr, uint32_t value, uint32_t msb, uint32_t lsb) {
  char filename[FILENAME_SIZE];
  int file;
  int return_code;
  void *virt_addr;
  uint64_t file_start;
  uint32_t file_offset;
  int page_size;
  uint32_t data;
  uint32_t mask, shift;

  snprintf(filename, sizeof(filename), MEM_DEV_FILE);
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("Error open file %s: %s", filename, strerror(errno));
    return -1;
  }

  if ((return_code = flock(file, LOCK_EX)) < 0) {
    printf("Error lock file %s: %s", filename, strerror(errno));
    goto io_w_field_cleanup;
  }

  page_size = getpagesize();

  /* map to file at file_start, which has to be page aligned */
  file_start = (addr / page_size) * page_size;
  file_offset = addr % page_size;

  virt_addr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, file,
                   file_start);

  if (virt_addr == MAP_FAILED) {
    printf("Error mmap file %s", filename);
    return_code = -1;
    goto io_w_field_cleanup;
  }

  data = *(uint32_t *)((uintptr_t)virt_addr + file_offset);

  get_mask_shift(msb, lsb, &mask, &shift);
  data &= ~mask;
  data |= ((value << shift) & mask);

  *(uint32_t *)((uintptr_t)virt_addr + file_offset) = data;

  if (munmap(virt_addr, sizeof(int)) < 0) {
    printf("Error munmap file %s", filename);
    return_code = -1;
  }

io_w_field_cleanup:
  flock(file, LOCK_UN);
  close(file);

  return return_code;
}

int io_w(uint64_t addr, uint32_t value) {
  char filename[FILENAME_SIZE];
  int file;
  int return_code;
  void *virt_addr;
  uint64_t file_start;
  uint32_t file_offset;
  int page_size;

  snprintf(filename, sizeof(filename), MEM_DEV_FILE);
  if ((file = open(filename, O_RDWR)) < 0) {
    printf("Error open file %s: %s", filename, strerror(errno));
    return -1;
  }

  if ((return_code = flock(file, LOCK_EX)) < 0) {
    printf("Error lock file %s: %s", filename, strerror(errno));
    goto io_w_cleanup;
  }

  page_size = getpagesize();

  /* map to file at file_start, which has to be page aligned */
  file_start = (addr / page_size) * page_size;
  file_offset = addr % page_size;

  virt_addr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, file,
                   file_start);
  if (virt_addr == MAP_FAILED) {
    printf("Error mmap file %s", filename);
    return_code = -1;
    goto io_w_cleanup;
  }

  *(uint32_t *)((uintptr_t)virt_addr + file_offset) = value;

  if (munmap(virt_addr, sizeof(int)) < 0) {
    printf("Error munmap file %s", filename);
    return_code = -1;
  }

io_w_cleanup:
  flock(file, LOCK_UN);
  close(file);

  return return_code;
}

int write_physical_addr(uint64_t addr, uint32_t value) {
  int rc = io_w(addr, value);
  unsigned int tmp;

  if (rc < 0) {
    tmp = addr & UINT_MASK;
    printf("write_physical_addr 0x%x, value 0x%x failed\n", tmp, value);
  }
  return rc;
}

int read_physical_addr(uint64_t addr, uint32_t *value) {
  unsigned int tmp;
  int rc = io_r_field(addr, value, 31, 0);
  if (rc < 0) {
    tmp = addr & UINT_MASK;
    printf("read_physical_addr 0x%x failed\n", tmp);
  }
  return rc;
}
