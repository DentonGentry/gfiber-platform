/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics library routines related routines and definitions.
 */

#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "diagdIncludes.h"
#include "diagLibApis.h"

#ifdef MOD_NAME
  #undef MOD_NAME
#endif
#define MOD_NAME  "libbrunodiag.so\0"

const char diagdMsgHeaderMarker[] = {"DIag"};
#define DIAG_MSG_MARKER_LEN     sizeof(uint32_t)
#define DIAG_HOSTCMD_PORT   50152   /* the port client will be connecting to */
#define DIAG_BUF_LEN        (1024 * 1)
#define LOCAL_HOST_IP "127.0.0.1"

void convertUpTime (
    uint32_t timeInSecs, uint32_t *pTimeInHrs,
    uint32_t *pTimeInMin, uint32_t *pTimeInSecs) {
  *pTimeInHrs  = timeInSecs / (NO_OF_SECS_IN_MIN * NO_OF_MINS_IN_HR);
  timeInSecs   = timeInSecs % (NO_OF_SECS_IN_MIN * NO_OF_MINS_IN_HR);
  *pTimeInMin  = timeInSecs / NO_OF_SECS_IN_MIN;
  timeInSecs   = timeInSecs % NO_OF_SECS_IN_MIN;
  *pTimeInSecs = timeInSecs;
  return;
}  /* end of convertUpTime */

/*
 * Function to establish a socket connection
 * to "diagd" running on the same thin Bruno
 * Input:
 *    None
 *
 * Return:
 *    -1 If it cannot create a socket connection
 *    Otherwise, socket handler
 *
 */
int diagdConnect() {
  struct sockaddr_in my_addr;
  int   hsock = -1;

  /* open a socket connection to "diagd" on Bruno box */
  hsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(hsock == -1) {
    fprintf(stderr, "Error initializing socket %d\n",errno);
    return -1;
  }

  memset(&my_addr, 0, sizeof(my_addr));

  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST_IP);
  my_addr.sin_port = htons(DIAG_HOSTCMD_PORT);

  if (connect( hsock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
    if (errno != EINPROGRESS) {
      fprintf(stderr, "Error connecting socket (errno:%s)\n", strerror(errno));
      return -1;
    }
  }

  return hsock;
}

/*
 * Function to build diag request header
 *
 * Input:
 *   CmdIdx  - diag request command
 *   pBuffer - buffer to hold diag msg header
 *
 * Return:
 *   None
 */
void diagBldRqCmdHdr(int cmdIdx, char *pBuffer) {
  diag_msg_header_t  *pMsgHdr = NULL;

  /* Compose the reqeust to diagd */
  pMsgHdr = (diag_msg_header_t *)pBuffer;
  memset(pMsgHdr, 0, sizeof(diag_msg_header_t));
  bcopy((void *)diagdMsgHeaderMarker,
        (void *)&pMsgHdr->headerMarker,
        DIAG_MSG_MARKER_LEN);
  pMsgHdr->len = 0;

  /* Set the msgType */
  pMsgHdr->msgType = cmdIdx;
}

/*
 * Function to send diag request
 *
 * Input:
 *   hsock   - socket handler
 *   pBuffer - buffer to hold diag msg header
 *
 * Return:
 *    0  if successful.
 *   -1  if socket send fails
 */
int diagSendRq(int hsock, char *pBuffer) {
  int err  = 0;
  int bytecount = 0;

  if ((bytecount = send(hsock, pBuffer, sizeof(diag_msg_header_t), 0))== -1) {
    err = errno;
    fprintf(stderr, "Error sending data %s\n", strerror(errno));
    return -1;
  }

  return err;
}

/*
 * Function to get get diag information based on command cmdIdx
 *
 * Input:
 *   cmdIdx  -  Diag request command
 *   pBuffer -  buffer to hold diag_msg_header
 *
 * Output:
 *   *recv_bytecount - this function will put
 *    the length of received payload to this output parameter.
 *
 *  Return:
 *     NULL if no information is returned
 *     Otherwise, pointer to received payload buffer
 */
char *diagGetInfo(int cmdIdx, char *pBuffer, int *recv_bytecount) {
  int  bytecount;
  int  err = 0;
  int  hsock = -1;
  char *pPayload = NULL;
  char *pTmpPayload;
  uint32_t  msgLen;
  diag_msg_header_t  *pMsgHdr = (diag_msg_header_t *)pBuffer;

  /* open a socket connection to "diagd" on Bruno box */
  hsock = diagdConnect();
  if(hsock == -1){
    fprintf(stderr, "Cannot connect to diagd! (errno:%s)\n",strerror(errno));
    return NULL;
  }

  diagBldRqCmdHdr(cmdIdx, pBuffer);
  if ((err = diagSendRq(hsock, pBuffer)) == -1) {
    goto err_exit;
  }


  /* Read rsp header first to get the length*/
  if ((bytecount = recv(hsock, pBuffer, sizeof(diag_msg_header_t), 0))== -1){
    err = errno;
    fprintf(stderr, "Error receiving data %s\n", strerror(errno));
    goto err_exit;
  }

  pTmpPayload = pPayload;
  msgLen = pMsgHdr->len;

  /* Prepare buffer to receive payload */
  pPayload = malloc(msgLen);
  memset(pPayload, 0, msgLen);

  pTmpPayload = pPayload;

  do {
    /* Receive payload of response msg from server */
    if ((bytecount = recv(hsock, pBuffer, DIAG_BUF_LEN, 0))== -1) {
      err = errno;
      fprintf(stderr, "Error receiving data %s\n", strerror(errno));
      break;
    } else if (bytecount == 0) {
      /* command completed */
      break;
    }

    if (msgLen < (*recv_bytecount + bytecount)) {
      fprintf(stderr, "Recved too many data(expected=%u, actual=%u)\n",
              msgLen, (*recv_bytecount + bytecount));
      break;
    }

    memcpy(pTmpPayload, pBuffer, bytecount);
    pTmpPayload =  pTmpPayload + bytecount;

    *recv_bytecount += bytecount;

  } while (true);


err_exit:
  /* free the resource */
  if (hsock != -1) {
    close(hsock);
    hsock = -1;
  }

  return pPayload;
}

/*
 * Function to print MoCA Key elements within extended status
 *
 * Input:
 *   buffer - destination buffer to hold key elements
 *   fmt    - format string
 *   key    - array of 8 key elements
 *   actStr - "(ACTIVE)" or ""
 *
 * Return:
 *   None
 */
void diagPrintMoCaStatusKey(char *buffer, char *fmt,
                            uint8_t *key, char *actStr) {

  sprintf (buffer,
           "%s              : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %s\n",
           fmt, key[0], key[1], key[2], key[3],
           key[4], key[5], key[6], key[7], actStr);
}

/*
 * Function to prepare MoCA Self Node status
 * Information and put them to input buffer
 *
 * Input:
 *   buffer  - char buffer to hold MoCA self node
 *             status information.
 *   pStatus - pointer to MoCA_STATUS data structure
 *
 * Return:
 *   None
 */
void diagPrintSelfNodeStatus(char *buffer, diag_moca_status_t *pStatus) {
/* Remove the code for now since this is based on MoCa 1.1 API
 * and diag_get_info() is not used by other application or diagd program
 * TODO 10/18/2012 need to rewrite this function based on MoCa 2.0 API
 */
}  /* end of diagPrintSelfNodeStatus */

/*
 * diag_get_info: a C API to retrieve Diag information
 * Currently it provides:
 * 1. Network interface eth0 linkstatus and statistics
 * 2. MoCA self node status
 *
 * It also print out the same info to stderr and expects
 * it is redirect to logger.
 *
 * Input:
 *   buffer  - pointer to a char buffer.
 *             caller needs to provide the buffer of
 *             minimum size of 4096 bytes.
 *
 *   bufSize - size of buffer
 *
 * Return:
 *     DIAG_LIB_RC_OK if successful
 *
 *     DIAG_LIB_RC_ERR if
 *       1. socket connection fails or
 *       2. send/recv failure happens or
 *       3. buffer is not big enough to hold the returned diag information
 */
#if 1
/* TODO 10/18/2012:
 *  Return "The information you request is not available!"
 *  for now.  Need to rewrite diag_get_info() for MoCa 2.0
 */
int diag_get_info(char *buffer, int bufSize) {
  int   err = DIAG_LIB_RC_ERR;

  if (!buffer || bufSize < 4096) {
    return err;
  }

  strcpy(buffer, "The information you request is not avaialble!");
  return DIAG_LIB_RC_OK;
}
#else
int diag_get_info(char *buffer, int bufSize) {
  char  *pBuffer =  NULL;
  int   buffer_len = 0;
  int   cmdIdx;
  int   recv_bytecount = 0;
  int   total_recv_bytecount = 0;
  char  *pPayload = NULL;
  int   err = DIAG_LIB_RC_ERR;
  char  *tmpBufPtr = NULL;


  if (!buffer || bufSize < 4096) {
    return err;
  }


  cmdIdx = DIAGD_REQ_GET_NET_LINK_STATS;

  /* get memory for diag message header */
  buffer_len = DIAG_BUF_LEN;
  pBuffer = malloc(buffer_len);
  memset(pBuffer, '\0', buffer_len);

  pPayload = diagGetInfo(cmdIdx, pBuffer, &recv_bytecount);

  if(pPayload == NULL) {
    goto cleanup_exit;
  }

  total_recv_bytecount += recv_bytecount;
  /* copy diag information string to buffer */
  if (total_recv_bytecount > bufSize) {
    strncpy(buffer, pPayload, bufSize);
    buffer[bufSize - 1] = '\0';
    goto cleanup_exit;
  } else {
    strncpy(buffer, pPayload, total_recv_bytecount);
    buffer[total_recv_bytecount - 1] = '\0';
    tmpBufPtr = buffer + total_recv_bytecount;
  }

  if (pPayload != NULL) {
    free(pPayload);
    pPayload = NULL;
  }

  buffer[total_recv_bytecount -1] = '\n';

  recv_bytecount = 0;
  cmdIdx = DIAGD_REQ_MOCA_GET_STATUS;
  memset(pBuffer, '\0', buffer_len);

  pPayload = diagGetInfo(cmdIdx, pBuffer, &recv_bytecount);

  if(pPayload == NULL) {
    goto cleanup_exit;
  }

  diagPrintSelfNodeStatus(tmpBufPtr, (diag_moca_status_t *)pPayload);
  err = DIAG_LIB_RC_OK;

  fprintf(stderr, "%s", buffer);
cleanup_exit:
  /* free the resource */

  if (pBuffer != NULL) {
    free(pBuffer);
    pBuffer = NULL;
  }

  if (pPayload != NULL) {
    free(pPayload);
    pPayload = NULL;
  }

  return(err);
}
#endif
