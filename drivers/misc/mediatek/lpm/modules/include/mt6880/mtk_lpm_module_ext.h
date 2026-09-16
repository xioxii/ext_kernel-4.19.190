/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MTK_LPM_MODULE_EXT_H__
#define __MTK_LPM_MODULE_EXT_H__

#include <linux/types.h>
#include <mtk_lpm_module.h>

enum MT_SPM_SMC_EXT_UID {
	MT_SPM_SMC_EXT_UID_PLL_CTRL = MT_SPM_SMC_UID_PHYPLL_MODE + 1,
};

/* SPM PLL select type */
#define SPM_SGMII_PLL_SEL	(0x1 << 0U)
#define SPM_NETSYS_PLL_SEL	(0x1 << 1U)
#define SPM_UNIPLL_PLL_SEL	(0x1 << 2U)

#endif