// SPDX-License-Identifier: GPL-2.0

/*

 * Copyright (c) 2019 MediaTek Inc.

 */

#include <linux/clk-provider.h>

#include <dt-bindings/power/mt6880-power.h>

#define TAG			"[scpchk] "
#define BUG_ON_CHK_ENABLE	0

/*
 * The clk names in Mediatek CCF.
 */
/* audiosys */
struct scpsys_check_swcg audiosys_swcgs[] = {
	SWCG("aud_afe"),
	SWCG("aud_22m"),
	SWCG("aud_24m"),
	SWCG("aud_apll2_tuner"),
	SWCG("aud_apll_tuner"),
	SWCG("aud_tdm_ck"),
	SWCG("aud_adc"),
	SWCG("aud_dac"),
	SWCG("aud_dac_predis"),
	SWCG("aud_tml"),
	SWCG("aud_i2s0_bclk"),
	SWCG("aud_i2s1_bclk"),
	SWCG("aud_i2s2_bclk"),
	SWCG("aud_i2s4_bclk"),
	SWCG("aud_i2s5_bclk"),
	SWCG("aud_i2s6_bclk"),
	SWCG("aud_general1_asrc"),
	SWCG("aud_general2_asrc"),
	SWCG("aud_adda6_adc"),
	SWCG("aud_connsys_i2s_asrc"),
	SWCG("aud_afe_src_pcm_tx"),
	SWCG("aud_afe_src_pcm_tx2"),
	SWCG("aud_afe_src_pcm_tx3"),
	SWCG("aud_afe_src_pcm_rx"),
	SWCG("aud_afe_src_i2sin"),
	SWCG("aud_afe_src_i2sout"),
	SWCG(NULL),
};
/* mfgsys */
struct scpsys_check_swcg mfgsys_swcgs[] = {
	SWCG("mfgcfg_bg3d"),
	SWCG(NULL),
};

struct subsys_cgs_check mtk_subsys_check[] = {
	{MT6880_POWER_DOMAIN_AUDIO, audiosys_swcgs},
	{MT6880_POWER_DOMAIN_MFG0, mfgsys_swcgs},
};

static unsigned int check_cg_state(struct scpsys_check_swcg *swcg)
{
	int enable_count = 0;

	if (!swcg)
		return 0;

	while (swcg->name) {
		if (!IS_ERR_OR_NULL(swcg->c)) {
			if (__clk_get_enable_count(swcg->c) > 0) {
				pr_notice("%s[%-17s: %3d]\n",
				__func__,
				__clk_get_name(swcg->c),
				__clk_get_enable_count(swcg->c));
				enable_count++;
			}
		}
		swcg++;
	}

	return enable_count;
}

void mtk_check_subsys_swcg(enum subsys_id id)
{
	int i;
	unsigned int ret = 0;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].id != id)
			continue;

		/* check if Subsys CGs are still on */
		ret = check_cg_state(mtk_subsys_check[i].swcgs);
		if (ret)
			pr_notice("%s:(%d) warning!\n", __func__, id);
	}

	if (ret) {
		pr_err("%s(%d): %d\n", __func__, id, ret);
#if BUG_ON_CHK_ENABLE
		BUG_ON(1);
#endif
	}
}

static void __init scpsys_check_swcg_init_common(struct scpsys_check_swcg *swcg)
{
	if (!swcg)
		return;

	while (swcg->name) {
		struct clk *c = __clk_lookup(swcg->name);

		if (IS_ERR_OR_NULL(c))
			pr_notice("[%17s: NULL]\n", swcg->name);
		else
			swcg->c = c;
		swcg++;
	}
}

static int __init scpchk_init(void)
{
	/* fill the 'struct clk *' ptr of every CGs*/
	int i;

	if (!of_machine_is_compatible("mediatek,MT6880"))
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++)
		scpsys_check_swcg_init_common(mtk_subsys_check[i].swcgs);

	return 0;
}
subsys_initcall(scpchk_init);