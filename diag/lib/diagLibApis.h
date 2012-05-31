/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics library routines related APIs and definitions.
 */

#ifndef _DIAG_LIB_APIS_H_
#define _DIAG_LIB_APIS_H_

#define DIAG_LIB_RC_OK    0
#define DIAG_LIB_RC_ERR  -1

/*
 *  diag_get_info: a C API to retrieve Diag information
 *  Currently it provides:
 *  1. Network interface eth0 linkstatus and statistics
 *  2. MoCA self node status
 *
 *  Input:
 *    buffer  - pointer to a char buffer.
 *              caller needs to provide the buffer of
 *              minimum size of 4096 bytes.
 *
 *    bufSize - size of buffer
 *
 *  Return:
 *    DIAG_LIB_RC_OK if successful
 *
 *    DIAG_LIB_RC_ERR if
 *      1. socket connection fails or
 *      2. send/recv failure happens or
 *      3. buffer is not big enough to hold the returned diag information
 */
int diag_get_info(char *buffer, int bufSize);

#endif /* end of _DIAG_LIB_APIS_H_ */
