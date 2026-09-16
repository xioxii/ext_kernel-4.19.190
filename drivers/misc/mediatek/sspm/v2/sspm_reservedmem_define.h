/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _SSPM_RESERVEDMEM_DEFINE_H_
#define _SSPM_RESERVEDMEM_DEFINE_H_
#include <sspm_reservedmem.h>

enum {
	SSPM_MEM_ID = 0,
	PWRAP_MEM_ID,
	PMIC_MEM_ID,
	QOS_MEM_ID,
#if defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) || defined(CONFIG_MTK_MET_MEM_ALLOC)
#else
	MET_MEM_ID,
#endif
	NUMS_MEM_ID,
};

#define SSPM_PLT_LOGGER_BUF_LEN 0x40000

#ifdef _SSPM_INTERNAL_
/* The total size of sspm_reserve_mblock should less equal than
 * reserve-memory-sspm_share of device tree
 */
static struct sspm_reserve_mblock sspm_reserve_mblock[NUMS_MEM_ID] = {
	{
		.num = SSPM_MEM_ID,
		.size = 0x100 + SSPM_PLT_LOGGER_BUF_LEN,
		/* logger header + 256K log buffer */
	},
	{
		.num = PWRAP_MEM_ID,
		.size = 0x300,  /* 768 bytes */
	},
	{
		.num = PMIC_MEM_ID,
		.size = 0xC00,  /* 3K */
	},
	{
		.num = QOS_MEM_ID,
		.size = 0x1000, /* 4K */
	},

#if defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) || defined(CONFIG_MTK_MET_MEM_ALLOC)
#else
	{
		.num = MET_MEM_ID,
		.size = 0x400000, /* 4M */
	},
#endif

	/* TO align 64K, total is 1M(5M)+64K. The remaining size = 0x2000. */
};
#endif
#endif

#ifdef SSPM_SHARE_BUFFER_SUPPORT
#define SSPM_SHARE_REGION_BASE  0x20000
#define SSPM_SHARE_REGION_SIZE  0x8000
#endif
