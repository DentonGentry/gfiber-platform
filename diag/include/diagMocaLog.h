/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides MoCA diagnostics related data structures and definitions.
 */

#ifndef _DIAG_MOCA_LOG_H_
#define _DIAG_MOCA__LOG_H_

void diagMoca_log_pqos_egress_numflows(bool logging, uint32_t * in);
void diagMoca_log_led_status(bool logging, uint32_t * in);
void diagMoca_log_preferred_nc(bool logging, uint32_t * in);
void diagMoca_log_single_channel_operation(bool logging, uint32_t * in);
void diagMoca_log_mac_addr(bool logging, struct moca_mac_addr * in);
void diagMoca_log_node_status(bool logging, struct moca_node_status * in);
void diagMoca_log_fw_version(bool logging, struct moca_fw_version * in);
void diagMoca_log_drv_info(bool logging, struct moca_drv_info * in);
void diagMoca_log_current_keys(bool logging, struct moca_current_keys * in);
void diagMoca_log_network_status(bool logging, struct moca_network_status * in);
void diagMoca_log_key_times(bool logging, struct moca_key_times *in);
void diagMoca_log_interface_status(bool logging, struct moca_interface_status * in);
void diagMoca_log_interface_status(bool logging, struct moca_interface_status * in);
void diagMoca_log_tx_gcd_power_reduction(bool logging, uint32_t *in);
void diagMoca_log_gen_node_status(bool logging, struct moca_gen_node_status * in);
void diagMoca_log_gen_node_ext_status(bool logging, uint32_t, struct moca_gen_node_ext_status * in);

void diagMocaStrLog(char *pLogMsg, diag_moca_status_t *pStatus);
void diagMocaMyStatusLog(bool logging, diag_moca_status_t *pStatus);

#endif
