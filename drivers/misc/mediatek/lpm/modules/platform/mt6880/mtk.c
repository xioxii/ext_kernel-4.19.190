// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <mtk_lp_plat_apmcu.h>
#include <mtk_lp_plat_pll.h>
#include <mtk.h>

#include <suspend/mtk_suspend.h>

static unsigned int mtk_lp_pwr_state;

/*
 * Please make sure the race condition protection upfront
 * when calling mtk_lp_plat_do_mcusys_prepare_pdn()
 */
int mtk_do_mcusys_prepare_pdn(unsigned int status,
					   unsigned int *resource_req)
{
	mtk_lp_pwr_state |= (status | PLAT_GIC_MASKED
				| PLAT_MCUSYSOFF_PREPARED);
	return 0;
}

/*
 * Please make sure the race condition protection upfront
 * when calling mtk_lp_plat_do_mcusys_prepare_on()
 */
static int __mtk_do_mcusys_prepare_on(unsigned int clr_status)
{
	mtk_lp_pwr_state &= ~(clr_status | PLAT_GIC_MASKED
				  | PLAT_MCUSYSOFF_PREPARED);

	return 0;
}

int mtk_do_mcusys_prepare_on(void)
{
	unsigned int status = mtk_lp_pwr_state;

	return __mtk_do_mcusys_prepare_on(status);
}

int mtk_do_mcusys_prepare_on_ex(unsigned int clr_status)
{
	return __mtk_do_mcusys_prepare_on(clr_status);
}

static int __init mtk_init(void)
{
	mtk_lp_plat_apmcu_init();
	mtk_model_suspend_init();
	mtk_lp_plat_pll_init();
	return 0;
}
late_initcall_sync(mtk_init);

static int __init mtk_early_init(void)
{
	mtk_lp_plat_apmcu_early_init();
	return 0;
}
subsys_initcall(mtk_early_init);
