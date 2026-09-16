// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <generated/autoconf.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <mt-plat/mtk_devinfo.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_api_buck.h"
#include "include/regulator_codegen.h"
#include <mt6330_buck_manager.h>

static unsigned int g_vmd11_vosel, g_vsram_md_vosel;

void record_md_vosel(void)
{
	g_vmd11_vosel = pmic_get_register_value(PMIC_RG_BUCK_VMD11_VOSEL);
	g_vsram_md_vosel = pmic_get_register_value(PMIC_RG_BUCK_VSRAM_MD_VOSEL);
	pr_info("[%s] vmd11=0x%x, g_vsram_md_vosel=0x%x\n", __func__, g_vmd11_vosel, g_vsram_md_vosel);
}

/* [Export API] */
void vmd1_pmic_setting_on(void)
{
#if defined(CONFIG_MFD_MT6330)
	mt6330_vmd1_pmic_setting_on();
#else
	/* 1.Call PMIC driver API configure VMODEM voltage */
	if ((g_vmd11_vosel || g_vsram_md_vosel) != 0) {
		pmic_set_register_value(PMIC_RG_BUCK_VMD11_VOSEL,
					g_vmd11_vosel);
		pr_info("[%s] set vmd11=0x%x\n", __func__, g_vmd11_vosel);
		pmic_set_register_value(PMIC_RG_BUCK_VSRAM_MD_VOSEL,
					g_vsram_md_vosel);
		pr_info("[%s] set vsram_md=0x%x\n", __func__, g_vsram_md_vosel);
	} else {
		pr_notice("[%s] vmodem vosel has not recorded!\n", __func__);
		record_md_vosel();
	}
#endif
}

/* [Export API] */
void rtc_clk_ctrl(unsigned char en)
{
	unsigned int reg;

	if (en)
		reg = PMIC_SCK_TOP_CKPDN_CON0_L_CLR_ADDR;
	else
		reg = PMIC_SCK_TOP_CKPDN_CON0_L_SET_ADDR;

	pmic_config_interface_nolock(reg, 1,
		PMIC_RG_RTC_MCLK_PDN_MASK, PMIC_RG_RTC_MCLK_PDN_SHIFT);
	pmic_config_interface_nolock(reg, 1,
		PMIC_RG_RTC_32K_CK_PDN_MASK, PMIC_RG_RTC_32K_CK_PDN_SHIFT);

}

void vmd1_pmic_setting_off(void)
{
	PMICLOG("%s\n", __func__);
}

void pmic_enable_smart_reset(unsigned char smart_en,
	unsigned char smart_sdn_en)
{
	pr_notice("[%s] powerkey: %s, count=%d(ms) JUST_SMART_RST:%d\n"
		, __func__
		, pmic_get_register_value(PMIC_PWRKEY_DEB)?"released":"pressed"
		, pmic_get_register_value(PMIC_PWRKEY_LONG_PRESS_COUNT) << 5
		, pmic_get_register_value(PMIC_JUST_SMART_RST));
	pmic_set_register_value(PMIC_RG_SMART_RST_MODE, smart_en);
	pmic_set_register_value(PMIC_RG_SMART_RST_SDN_EN, smart_sdn_en);
	pr_info("[%s] smart_en:%d, smart_sdn_en:%d\n",
		__func__, smart_en, smart_sdn_en);
}

void enable_bat_temp_det(bool en)
{
	pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_FROZE_EN, !en);
}


/*****************************************************************************
 * PMIC charger detection
 ******************************************************************************/
unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int val = 0;

	val = pmic_get_register_value(PMIC_RGS_CHRDET);
	PMICLOG("[%s] CHRDET status = %d\n", __func__, val);

	return val;
}

/*****************************************************************************
 * Enternal BUCK status
 ******************************************************************************/

int is_ext_buck_gpio_exist(void)
{
	return pmic_get_register_value(PMIC_RG_STRUP_EXT_PMIC_EN);
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
