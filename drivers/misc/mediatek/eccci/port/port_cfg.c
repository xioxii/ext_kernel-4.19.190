// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "ccci_config.h"
#include "ccci_hif.h"
#include "port_cfg.h"
#ifdef CONFIG_MTK_CCB
/* put it after ccci_config.h please. */
#include "ccif_hif_platform.h"
#endif

#define CCIF_CCMNI_Q    9
#define CCIF_L5_Q   5//10

#ifndef DATA_PLANE_DPMAIF
#define EXP_CTRL_Q		6
#define DATA_TX_Q		CCIF_CCMNI_Q
#define DATA_RX_Q		CCIF_CCMNI_Q
#define DATA_TX_ACK_Q		CCIF_CCMNI_Q
#define DATA1_TX_Q		CCIF_CCMNI_Q
#define DATA1_RX_Q		CCIF_CCMNI_Q
#define DATA2_Q			CCIF_CCMNI_Q
#define DATA2_RX_Q		CCIF_CCMNI_Q
//#define DATA_MDT_Q		0
//#define DATA_C2K_PPP_Q		3
#define DATA_FSD_Q		4
#define DATA_AT_CMD_Q	5
#else //For CPE/MIFI, data plane DPMAIF
#define EXP_CTRL_Q		6
#ifndef _DPMAIF_MED_SUPPORT_
#define DATA_TX_Q		0
#else
#define DATA_TX_Q		2	/*0 and 1 reassigned to MED */
#endif
#define DATA_RX_Q		0
#ifndef _DPMAIF_MED_SUPPORT_
#define DATA_TX_ACK_Q	1
#else
#define DATA_TX_ACK_Q	2	/*0 and 1 reassigned to MED */
#endif
#ifndef _DPMAIF_MED_SUPPORT_
#define DATA1_TX_Q		0
#else
#define DATA1_TX_Q		2	/*0 and 1 reassigned to MED */
#endif
#define DATA1_RX_Q		0
#define DATA2_Q			2
#define DATA2_RX_Q		0
#define DATA_MDT_Q		0
#define DATA_C2K_PPP_Q	3
#define DATA_FSD_Q		4
#define DATA_AT_CMD_Q	5
#endif

#define SMEM_Q			AP_MD_CCB_WAKEUP

static struct port_t md1_ccci_ports[] = {
	/* char port, notes ccci_monitor must be first for get_port_by_minor() implement */
	{CCCI_FS_TX, CCCI_FS_RX, DATA_FSD_Q, DATA_FSD_Q, 1, 1,
		MD1_NORMAL_HIF, PORT_F_USER_HEADER | PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 1, "ccci_fs",},
	{CCCI_IT_TX, CCCI_IT_RX, 0, 0, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_USER_HEADER | PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 2, "ccci_it",},
	{CCCI_LB_IT_TX, CCCI_LB_IT_RX, 0, 0, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 3, "ccci_lb_it",},
	{CCCI_AP_LOG_CTRL_TX, CCCI_AP_LOG_CTRL_RX, 1, 1, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 4, "ccci_ap_log_ctrl",},
	{CCCI_AP_LOG_TX, CCCI_AP_LOG_RX, 2, 2, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 5, "ccci_ap_log",},
	{CCCI_PCM_TX, CCCI_PCM_RX, 0, 0, 0xFF, 0xFF, MD1_NORMAL_HIF,
		PORT_F_USER_HEADER | PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 6, "ccci_aud",},
	{CCCI_DRAM_PROFILE_TX, CCCI_DRAM_PROFILE_RX, 1, 1, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_ADJUST_HEADER,
		&char_port_ops, 0xFF, "ccci_dram_profile",},

/* sys port */
	{CCCI_SYSTEM_TX, CCCI_SYSTEM_RX, 0, 0, 0xFF, 0xFF, MD1_NORMAL_HIF, 0,
		&sys_port_ops, 0xFF, "ccci_sys",},
	{CCCI_CONTROL_TX, CCCI_CONTROL_RX, 0, 0, 0, 0, MD1_NORMAL_HIF, 0,
		&ctl_port_ops, 0xFF, "ccci_ctrl",},
	{CCCI_STATUS_TX, CCCI_STATUS_RX, 0, 0, 0, 0,
		MD1_NORMAL_HIF, 0, &poller_port_ops, 0xFF, "ccci_poll",},
/* ttyCMIPC0 ~ 9 */
	{CCCI_MIPC0_CHANNEL_TX, CCCI_MIPC0_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 7, "ttyCMIPC0",},
	{CCCI_MIPC1_CHANNEL_TX, CCCI_MIPC1_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 8, "ttyCMIPC1",},
	{CCCI_MIPC2_CHANNEL_TX, CCCI_MIPC2_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 9, "ttyCMIPC2",},
	{CCCI_MIPC3_CHANNEL_TX, CCCI_MIPC3_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 10, "ttyCMIPC3",},
	{CCCI_MIPC4_CHANNEL_TX, CCCI_MIPC4_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 11, "ttyCMIPC4",},
	{CCCI_MIPC5_CHANNEL_TX, CCCI_MIPC5_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 12, "ttyCMIPC5",},
	{CCCI_MIPC6_CHANNEL_TX, CCCI_MIPC6_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 13, "ttyCMIPC6",},
	{CCCI_MIPC7_CHANNEL_TX, CCCI_MIPC7_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 14, "ttyCMIPC7",},
	{CCCI_MIPC8_CHANNEL_TX, CCCI_MIPC8_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 15, "ttyCMIPC8",},
	{CCCI_MIPC9_CHANNEL_TX, CCCI_MIPC9_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 16, "ttyCMIPC9",},
	{CCCI_MIPC10_CHANNEL_TX, CCCI_MIPC10_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 22, "ttyCMIPC10",},
	{CCCI_MIPC11_CHANNEL_TX, CCCI_MIPC11_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 23, "ttyCMIPC11",},
	{CCCI_MIPC12_CHANNEL_TX, CCCI_MIPC12_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 24, "ttyCMIPC12",},
	{CCCI_MIPC13_CHANNEL_TX, CCCI_MIPC13_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 25, "ttyCMIPC13",},
	{CCCI_MIPC14_CHANNEL_TX, CCCI_MIPC14_CHANNEL_RX, CCIF_L5_Q, CCIF_L5_Q,
		0xFF, 0xFF, MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 26, "ttyCMIPC14",},
	{CCCI_RIL_IPC0_TX, CCCI_RIL_IPC0_RX, 1, 1, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 27, "ccci_umts_ipc0",},
	{CCCI_RIL_IPC1_TX, CCCI_RIL_IPC1_RX, 1, 1, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&char_port_ops, 28, "ccci_cdma_ipc0",},
	{CCCI_GPS_CHANNEL_TX, CCCI_GPS_CHANNEL_RX, 1, 1, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE, &char_port_ops, 17,
		"ttyC2",},
	{CCCI_USB_FWD_CHANNEL_TX, CCCI_USB_FWD_CHANNEL_RX, 1, 1, 0xFF, 0xFF,
		MD1_NORMAL_HIF, (PORT_F_CCCI_KERNEL | PORT_F_WITH_CHAR_NODE),
		&char_port_ops, 18, "ccci_usbfwd",},
	{CCCI_IDC_WIFI_CHANNEL_TX, CCCI_IDC_WIFI_CHANNEL_RX, 1, 1, 0xFF, 0xFF,
		MD1_NORMAL_HIF, (PORT_F_CCCI_KERNEL | PORT_F_WITH_CHAR_NODE),
		&char_port_ops, 19, "ccci_wifi",},
	{CCCI_IDC_BT_CHANNEL_TX, CCCI_IDC_BT_CHANNEL_RX, 1, 1, 0xFF, 0xFF,
		MD1_NORMAL_HIF, (PORT_F_CCCI_KERNEL | PORT_F_WITH_CHAR_NODE),
		&char_port_ops, 20, "ccci_bt",},
	{CCCI_RPC_TX, CCCI_RPC_RX, 1, 1, 1, 1, MD1_NORMAL_HIF,
		PORT_F_USER_HEADER | PORT_F_WITH_CHAR_NODE,
		&rpc_port_ops, 21, "ccci_rpc",},
	{CCCI_RPC_TX, CCCI_RPC_RX, 1, 1, 1, 1, MD1_NORMAL_HIF, 0,
		&rpc_port_ops, 0xFF, "ccci_rpc_k",},

/* smem port */
#ifdef CONFIG_MTK_CCB
	{CCCI_CCB_CTRL, CCCI_CCB_CTRL, 0xFF, 0xFF, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&smem_port_ops, SMEM_USER_RAW_CCB_CTRL, "ccci_ccb_ctrl",},
	{CCCI_SMEM_CH, CCCI_SMEM_CH, SMEM_Q, SMEM_Q, SMEM_Q, SMEM_Q,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&smem_port_ops, SMEM_USER_CCB_DHL, "ccci_ccb_dhl",},
	{CCCI_SMEM_CH, CCCI_SMEM_CH, 0xFF, 0xFF, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&smem_port_ops, SMEM_USER_RAW_DHL, "ccci_raw_dhl",},
	{CCCI_SMEM_CH, CCCI_SMEM_CH, 0xFF, 0xFF, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&smem_port_ops, SMEM_USER_RAW_MDM, "ccci_raw_mdm",},
	{CCCI_SMEM_CH, CCCI_SMEM_CH, SMEM_Q, SMEM_Q, 0xFF, 0xFF,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE, &smem_port_ops,
		SMEM_USER_CCB_MD_MONITOR, "ccci_ccb_md_monitor",},
	{CCCI_SMEM_CH, CCCI_SMEM_CH, SMEM_Q, SMEM_Q, SMEM_Q, SMEM_Q,
		MD1_NORMAL_HIF, PORT_F_WITH_CHAR_NODE,
		&smem_port_ops, SMEM_USER_CCB_META, "ccci_ccb_meta",},
	{CCCI_SMEM_CH, CCCI_SMEM_CH, 0xFF, 0xFF, 0xFF, 0xFF,
		CCIF_HIF_ID, PORT_F_WITH_CHAR_NODE,
		&smem_port_ops, SMEM_USER_RAW_AUDIO, "ccci_raw_audio",},
#endif
#ifdef CONFIG_MTK_NET_CCMNI
#ifndef DATA_PLANE_DPMAIF
	{CCCI_CCMNI1_TX, CCCI_CCMNI1_RX, DATA_TX_Q, DATA_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 0, "ccmni0",},
	{CCCI_CCMNI2_TX, CCCI_CCMNI2_RX, DATA1_TX_Q, DATA1_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 1, "ccmni1",},
	{CCCI_CCMNI3_TX, CCCI_CCMNI3_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 2, "ccmni2",},
	{CCCI_CCMNI4_TX, CCCI_CCMNI4_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 3, "ccmni3",},
	{CCCI_CCMNI5_TX, CCCI_CCMNI5_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 4, "ccmni4",},
	{CCCI_CCMNI6_TX, CCCI_CCMNI6_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 5, "ccmni5",},
	{CCCI_CCMNI7_TX, CCCI_CCMNI7_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 6, "ccmni6",},
	{CCCI_CCMNI8_TX, CCCI_CCMNI8_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 7, "ccmni7",},
	{CCCI_CCMNI9_TX, CCCI_CCMNI9_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 8, "ccmni8",},
	{CCCI_CCMNI10_TX, CCCI_CCMNI10_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 9, "ccmni9",},
	{CCCI_CCMNI11_TX, CCCI_CCMNI11_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 10, "ccmni10",},
	{CCCI_CCMNI12_TX, CCCI_CCMNI12_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 11, "ccmni11",},
	{CCCI_CCMNI13_TX, CCCI_CCMNI13_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 12, "ccmni12",},
	{CCCI_CCMNI14_TX, CCCI_CCMNI14_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 13, "ccmni13",},
	{CCCI_CCMNI15_TX, CCCI_CCMNI15_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 14, "ccmni14",},
	{CCCI_CCMNI16_TX, CCCI_CCMNI16_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 15, "ccmni15",},
	{CCCI_CCMNI17_TX, CCCI_CCMNI17_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 16, "ccmni16",},
	{CCCI_CCMNI18_TX, CCCI_CCMNI18_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 17, "ccmni17",},
	{CCCI_CCMNI19_TX, CCCI_CCMNI19_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 18, "ccmni18",},
	{CCCI_CCMNI20_TX, CCCI_CCMNI20_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 19, "ccmni19",},
	{CCCI_CCMNI21_TX, CCCI_CCMNI21_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 20, "ccmni20",},
	/*{CCCI_NCCMNI22_TX, CCCI_NCCMNI22_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 21, "ccmni21",},
	{CCCI_NCCMNI23_TX, CCCI_NCCMNI23_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 22, "ccmni22",},
	{CCCI_NCCMNI24_TX, CCI_NCCMNI24_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 23, "ccmni23",},
	{CCCI_NCCMNI25_TX, CCCI_NCCMNI25_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 24, "ccmni24",},
	{CCCI_NCCMNI26_TX, CCCI_NCCMNI26_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 25, "ccmni25",},
	{CCCI_NCCMNI27_TX, CCCI_NCCMNI27_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 26, "ccmni26",},
	{CCCI_NCCMNI28_TX, CCCI_NCCMNI28_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 27, "ccmni27",},
	{CCCI_NCCMNI29_TX, CCCI_NCCMNI29_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 28, "ccmni28",},
	{CCCI_NCCMNI30_TX, CCCI_NCCMNI30_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 29, "ccmni29",},
	{CCCI_NCCMNI31_TX, CCCI_NCCMNI31_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 30, "ccmni30",},
	{CCCI_NCCMNI32_TX, CCCI_NCCMNI32_RX, DATA2_Q, DATA2_RX_Q,
		0xF0 | DATA2_Q, 0xFF, MD1_NORMAL_HIF, 0,
		&net_port_ops, 31, "ccmni31",}, */
#else
	{CCCI_CCMNI1_TX, CCCI_CCMNI1_RX, DATA_TX_Q, DATA_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 0, "ccmni0",},
	{CCCI_CCMNI2_TX, CCCI_CCMNI2_RX, DATA1_TX_Q, DATA1_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 1, "ccmni1",},
	{CCCI_CCMNI3_TX, CCCI_CCMNI3_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 2, "ccmni2",},
	{CCCI_CCMNI4_TX, CCCI_CCMNI4_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 3, "ccmni3",},
	{CCCI_CCMNI5_TX, CCCI_CCMNI5_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 4, "ccmni4",},
	{CCCI_CCMNI6_TX, CCCI_CCMNI6_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 5, "ccmni5",},
	{CCCI_CCMNI7_TX, CCCI_CCMNI7_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 6, "ccmni6",},
	{CCCI_CCMNI8_TX, CCCI_CCMNI8_RX, DATA1_TX_Q, DATA_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 7, "ccmni7",},

	{CCCI_CCMNI10_TX, CCCI_CCMNI10_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 9, "ccmni9",},
	{CCCI_CCMNI11_TX, CCCI_CCMNI11_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 10, "ccmni10",},
	{CCCI_CCMNI12_TX, CCCI_CCMNI12_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 11, "ccmni11",},
	{CCCI_CCMNI13_TX, CCCI_CCMNI13_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 12, "ccmni12",},
	{CCCI_CCMNI14_TX, CCCI_CCMNI14_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 13, "ccmni13",},
	{CCCI_CCMNI15_TX, CCCI_CCMNI15_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 14, "ccmni14",},
	{CCCI_CCMNI16_TX, CCCI_CCMNI16_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 15, "ccmni15",},
	{CCCI_CCMNI17_TX, CCCI_CCMNI17_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 16, "ccmni16",},
	{CCCI_CCMNI18_TX, CCCI_CCMNI18_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 17, "ccmni17",},
	{CCCI_CCMNI19_TX, CCCI_CCMNI19_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 18, "ccmni18",},
	{CCCI_CCMNI20_TX, CCCI_CCMNI20_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 19, "ccmni19",},
	{CCCI_CCMNI21_TX, CCCI_CCMNI21_RX, DATA1_TX_Q, DATA2_RX_Q,
		0xF0 | DATA_TX_ACK_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 20, "ccmni20",},
	/*ccmni-lan port minor id should be same as ccmni_idx
	 * in ccci_get_ccmni_channel() function
	 */
	/*
	{CCCI_CCMNILAN_TX, CCCI_CCMNILAN_RX, DATA_MDT_Q, DATA_MDT_Q,
		0xF0 | DATA_MDT_Q, 0xFF, MD1_NET_HIF, 0,
		&net_port_ops, 21, "ccmni-lan",},
	*/
	/* char port, notes ccci_monitor must be first
	 * for get_port_by_minor() implement
	 */
#endif
#endif
};

int port_get_cfg(int md_id, struct port_t **ports)
{
	int port_number = 0;

	switch (md_id) {
	case MD_SYS1:
		*ports = md1_ccci_ports;
		port_number = ARRAY_SIZE(md1_ccci_ports);
		break;
	default:
		*ports = NULL;
		port_number = 0;
		CCCI_ERROR_LOG(md_id, PORT, "md_port_cfg:no md enable\n");
		break;
	}
	return port_number;
}

int find_port_by_name(char *name, struct port_t **port)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(md1_ccci_ports); i++) {
		if (!strcmp(md1_ccci_ports[i].name, name)) {
			*port = &md1_ccci_ports[i];
			return i;
		}
	}
	CCCI_ERROR_LOG(-1, PORT, "can not find port %s", name);
	return -1;
}


int find_port_by_channel(int index, struct port_t **port)
{
	if (index < ARRAY_SIZE(md1_ccci_ports)) {
		*port = &md1_ccci_ports[index];
		return 0;
	}
	CCCI_ERROR_LOG(-1, PORT, "cannot find port by index %d\n", index);
	return -1;
}

