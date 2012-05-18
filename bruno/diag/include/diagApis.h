/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics routines related data structures and definitions.
 */

#ifndef _DIAG_APIS_H_
#define _DIAG_APIS_H_

void Diag_runEthLoopBackTest();
int diag_CmdHandler_GetMonitorLog(void);
int diag_CmdHandler_GetTestResultLog(void);
int diag_CmdHandler_RunTests(void);
int diag_CmdHandler_Moca_GetInitParams(void);
int diag_CmdHandler_Moca_GetSelfStatus(void);
int diag_CmdHandler_Moca_GetSelfConfig(void);
int diag_CmdHandler_Moca_GetNodeStatus(void);
int diag_CmdHandler_Moca_GetNodeStatistics(void);
int diag_CmdHandler_Moca_GetMocaLog(void);
int diag_CmdHandler_Moca_GetNodeConnectInfo(void);
int diag_CmdHandler_GetMonKernMsgsCntsSum(void);
int diag_CmdHandler_GetMonKernMsgsCntsDet(void);

void diagd_Cmd_Handler();

#endif /* end of _DIAG_APIS_H_ */
