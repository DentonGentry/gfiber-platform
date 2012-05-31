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
void diagPrintSelfNodeStatus(char *buffer, MoCA_STATUS *pStatus) {

  uint32_t  coreversionMajor, coreversionMinor, coreversionBuild;
  uint32_t  timeH, timeM, timeS;
  int count;
  uint32_t noOfNodes = 0;
  char inBuf[150];


  coreversionMajor = pStatus->generalStatus.swVersion >> 28;
  coreversionMinor = (pStatus->generalStatus.swVersion << 4) >> 28;
  coreversionBuild = (pStatus->generalStatus.swVersion << 8) >> 8;

  sprintf (buffer, "           MoCA Status(General)     \n");
  strcat (buffer, "==================================  \n");
  sprintf (inBuf, "vendorId                  : %3d \t",
           pStatus->generalStatus.vendorId);
  strcat (buffer, inBuf);

  sprintf (inBuf, "  HwVersion                 : 0x%x \n",
           pStatus->generalStatus.hwVersion);
  strcat (buffer, inBuf);

  sprintf (inBuf, "SwVersion                 : %d.%d.%d \t",
           coreversionMajor, coreversionMinor, coreversionBuild);
  strcat (buffer, inBuf);

  sprintf (inBuf, "  self MoCA Version         : 0x%x \n",
           pStatus->generalStatus.selfMoCAVersion);
  strcat (buffer, inBuf);

  sprintf (inBuf, "networkVersionNumber      : 0x%x \t",
           pStatus->generalStatus.networkVersionNumber);
  strcat (buffer, inBuf);

  sprintf (inBuf, "  qam256Support             : %s \n",
          (pStatus->generalStatus.qam256Support == MoCA_QAM_256_SUPPORT_ON) ?
           "supported" : "unknown" );
  strcat (buffer, inBuf);

  if (pStatus->generalStatus.operStatus == MoCA_OPER_STATUS_ENABLED)
    sprintf (inBuf, "operStatus                : Enabled \t");
  else
    sprintf (inBuf, "operStatus                : Hw Error \t");
  strcat (buffer, inBuf);

  if (pStatus->generalStatus.linkStatus == MoCA_LINK_UP)
    sprintf (inBuf, "  linkStatus                : Up \n");
  else
    sprintf (inBuf, "  linkStatus                : Down \n");
  strcat (buffer, inBuf);

  sprintf (inBuf, "connectedNodes BitMask    : 0x%x \t",
           pStatus->generalStatus.connectedNodes);
  strcat (buffer, inBuf);

  if (pStatus->generalStatus.nodeId >= MoCA_MAX_NODES)
    sprintf (inBuf, "  nodeId                    : N/A \n");
  else
    sprintf (inBuf, "  nodeId                    : %u \n",
             pStatus->generalStatus.nodeId);
  strcat (buffer, inBuf);

  if (pStatus->generalStatus.ncNodeId >= MoCA_MAX_NODES)
    sprintf (inBuf, "ncNodeId                  : N/A \n");
  else
    sprintf (inBuf, "ncNodeId                  : %u \t\t",
             pStatus->generalStatus.ncNodeId);
  strcat (buffer, inBuf);

  convertUpTime (pStatus->miscStatus.MoCAUpTime, &timeH, &timeM, &timeS);
  sprintf (inBuf, "  upTime                    : %02uh:%02um:%02us\n",
           timeH, timeM, timeS);
  strcat (buffer, inBuf);

  convertUpTime (pStatus->miscStatus.linkUpTime, &timeH, &timeM, &timeS);
  sprintf (inBuf, "linkUpTime                : %02uh:%02um:%02us",
           timeH, timeM, timeS);
  strcat (buffer, inBuf);

  if (pStatus->generalStatus.backupNcId >= MoCA_MAX_NODES)
    sprintf (inBuf, "  backupNcId                : N/A \n");
  else
    sprintf (inBuf, "  backupNcId                : %u \n",
             pStatus->generalStatus.backupNcId);
  strcat (buffer, inBuf);

  sprintf (inBuf, "rfChannel                 : %u Mhz\t",
           pStatus->generalStatus.rfChannel);
  strcat (buffer, inBuf);

  sprintf (inBuf, "  bwStatus                  : 0x%x \n",
           pStatus->generalStatus.bwStatus);
  strcat (buffer, inBuf);

  sprintf (inBuf, "NodesUsableBitMask        : 0x%x \t",
           pStatus->generalStatus.nodesUsableBitmask);
  strcat (buffer, inBuf);

  sprintf (inBuf, "  NetworkTabooMask          : 0x%x \n",
           pStatus->generalStatus.networkTabooMask);
  strcat (buffer, inBuf);

  sprintf (inBuf, "NetworkTabooStart         : %d \t\t",
           pStatus->generalStatus.networkTabooStart);
  strcat (buffer, inBuf);

  sprintf (inBuf, "  txGcdPowerReduction       : %d \n",
          pStatus->generalStatus.txGcdPowerReduction);
  strcat (buffer, inBuf);

  sprintf (inBuf, "pqosEgressNumFlows        : %d \t\t",
           pStatus->generalStatus.pqosEgressNumFlows);
  strcat (buffer, inBuf);
  /* find the number of connected nodes from the connected nodes bitmask */
  for (count = 0; count < MoCA_MAX_NODES; count++) {
    if (pStatus->generalStatus.connectedNodes & (0x1 << count))
       noOfNodes++;
  }
  sprintf (inBuf, "  Num of connectedNodes     : %d \n", noOfNodes);
  strcat (buffer, inBuf);

  sprintf (inBuf, "ledStatus                 : %x \n",
           pStatus->generalStatus.ledStatus);
  strcat (buffer, inBuf);

  sprintf (inBuf, "==================================  \n");
  strcat (buffer, inBuf);

  sprintf (inBuf, "           MoCA Status(Extended)    \n");
  strcat (buffer, inBuf);

  sprintf (inBuf, "==================================  \n");
  strcat (buffer, inBuf);

  convertUpTime (pStatus->extendedStatus.lastPmkExchange, &timeH, &timeM, &timeS);
  sprintf (inBuf, "lastPmkExchange           : %02uh:%02um:%02us\n",
           timeH, timeM, timeS );
  strcat (buffer, inBuf);

  sprintf (inBuf, "lastPmkInterval           : %d sec\n",
           pStatus->extendedStatus.lastPmkInterval );
  strcat (buffer, inBuf);

  convertUpTime (pStatus->extendedStatus.lastTekExchange,
                 &timeH, &timeM, &timeS);
  sprintf (inBuf, "lastTekExchange           : %02uh:%02um:%02us\n",
           timeH, timeM, timeS );
  strcat (buffer, inBuf);

  sprintf (inBuf, "lastTekInterval           : %d sec\n", pStatus->extendedStatus.lastTekInterval);
  strcat (buffer, inBuf);

  diagPrintMoCaStatusKey(inBuf,
      "PMK Even Key",
      pStatus->extendedStatus.pmkEvenKey,
      pStatus->extendedStatus.pmkEvenOdd==0?"(ACTIVE)":"");
  strcat (buffer, inBuf);

  /* for alignment, tailing an extra space */
  diagPrintMoCaStatusKey(inBuf,
      "PMK Odd Key ",
      pStatus->extendedStatus.pmkOddKey,
      pStatus->extendedStatus.pmkEvenOdd==1?"(ACTIVE)":"");
  strcat (buffer, inBuf);

  diagPrintMoCaStatusKey(inBuf,
      "TEK Even Key",
      pStatus->extendedStatus.tekEvenKey,
      pStatus->extendedStatus.tekEvenOdd==0?"(ACTIVE)":"");
  strcat (buffer, inBuf);

  /* for alignment, tailing an extra space */
  diagPrintMoCaStatusKey(inBuf,
      "TEK Odd Key ",
      pStatus->extendedStatus.tekOddKey,
      pStatus->extendedStatus.tekEvenOdd==1?"(ACTIVE)":"");
  strcat (buffer, inBuf);

  sprintf (inBuf, "==================================  \n");
  strcat (buffer, inBuf);

  sprintf (inBuf, "           MoCA Status(Misc)    \n");
  strcat (buffer, inBuf);

  sprintf (inBuf, "==================================  \n");
  strcat (buffer, inBuf);

  sprintf (inBuf, "MAC GUID                  : %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
      pStatus->miscStatus.macAddr[0],
      pStatus->miscStatus.macAddr[1],
      pStatus->miscStatus.macAddr[2],
      pStatus->miscStatus.macAddr[3],
      pStatus->miscStatus.macAddr[4],
      pStatus->miscStatus.macAddr[5]);
  strcat (buffer, inBuf);

  sprintf (inBuf, "Are we Network Controller : %s \n", (pStatus->miscStatus.isNC == 1) ? "yes" : "no");
  strcat (buffer, inBuf);

  convertUpTime (pStatus->miscStatus.driverUpTime, &timeH, &timeM, &timeS);
  sprintf (inBuf, "Driver Up Time            : %02uh:%02um:%02us \n", timeH, timeM, timeS);
  strcat (buffer, inBuf);

  sprintf (inBuf, "Link Reset Count          : %u \n", pStatus->miscStatus.linkResetCount);
  strcat (buffer, inBuf);

  sprintf (inBuf, "==================================  \n");
  strcat (buffer, inBuf);
}  /* end of diagPrintSelfNodeStatus */

/*
 * diag_get_info: a C API to retrieve Diag information
 * Currently it provides:
 * 1. Network interface eth0 linkstatus and statistics
 * 2. MoCA self node status
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

  diagPrintSelfNodeStatus(tmpBufPtr, (MoCA_STATUS *)pPayload);
  err = DIAG_LIB_RC_OK;

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
