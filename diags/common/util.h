/*
 * (C) Copyright 2015 Google, Inc.
 * All rights reserved.
 *
 */

#ifndef VENDOR_GOOGLE_DIAGS_WINDCHARGER_UTIL_H_
#define VENDOR_GOOGLE_DIAGS_WINDCHARGER_UTIL_H_

#include <inttypes.h>

#define UINT_MASK 0xFFFFFFFF
#define USHORT_MASK 0xFFFF

void safe_strncpy(char *dest, const char *src, int len);

void get_mask_shift(uint32_t msb, uint32_t lsb, uint32_t *mask,
                    uint32_t *shift);

int get_index(char *argv);

int get_text_from_file(char *text, int text_size, const char *filename);

void system_cmd(const char *cmd);

#endif  // VENDOR_GOOGLE_DIAGS_WINDCHARGER_UTIL_H_
