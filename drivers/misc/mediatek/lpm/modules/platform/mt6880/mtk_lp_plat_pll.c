// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <mtk_lpm.h>
#include <mtk_lp_plat_pll.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_module_ext.h>

/* NetSYS PLL select */
static int __init mtk_lp_plat_netsys_pll_init(void)
{
	int ret = 0;
	struct device_node *mtk_lpm_eth;

	/* Detect Ethernet device is enable or not */
	mtk_lpm_eth = of_find_compatible_node(NULL, NULL, "mediatek,mt6890-eth");
	if (mtk_lpm_eth) {
		const char *pstatus = NULL;

		ret = of_property_read_string(mtk_lpm_eth, "status", &pstatus);

		if (pstatus) {
			if (!strcmp(pstatus, "enable"))
				mtk_lpm_smc_spm(MT_SPM_SMC_EXT_UID_PLL_CTRL,
					MT_LPM_SMC_ACT_SET, SPM_SGMII_PLL_SEL, 0);
			pr_info("[name:mtk_lpm][P] - mtk_lpm_eth:%s (%s:%d)\n",
						pstatus, __func__, __LINE__);
		}
		of_node_put(mtk_lpm_eth);
	}
	return ret;
}

int __init mtk_lp_plat_pll_init(void)
{
	int ret = 0;

	ret = mtk_lp_plat_netsys_pll_init();

	return ret;
}