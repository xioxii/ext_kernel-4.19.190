/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <mt-plat/upmu_common.h>

#include <linux/io.h>
#include <linux/of_address.h>

#include "include/pmic.h"
#include "include/pmic_api.h"
#include "include/pmic_api_buck.h"

#define LP_INIT_SETTING_VERIFIED 1

unsigned int g_pmic_chip_version = 1;

int PMIC_MD_INIT_SETTING_V1(void)
{
	/* No need for PMIC MT6330 */
	return 0;
}

int PMIC_check_wdt_status(void)
{
	unsigned int ret = 0;

	is_wdt_reboot_pmic = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC1_SET, 0x8);
	udelay(50);
	is_wdt_reboot_pmic_chk = pmic_get_register_value(PMIC_WDTRSTB_STATUS);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC1_CLR, 0x8);
	ret = pmic_set_register_value(PMIC_TOP_RST_MISC1_SET, 0x1);
	ret = pmic_get_register_value(PMIC_RG_WDTRSTB_EN);
	return ret;
}

int PMIC_check_pwrhold_status(void)
{
	unsigned int val = 0;

	pmic_read_interface(PMIC_RG_PWRHOLD_ADDR, &val,
		PMIC_RG_PWRHOLD_MASK,
		PMIC_RG_PWRHOLD_SHIFT);
	return val;
}

int PMIC_POWER_HOLD(unsigned int hold)
{
	if (hold > 1) {
		pr_notice("[%s] hold = %d only 0 or 1\n", __func__, hold);
		return -1;
	}

	if (hold)
		PMICLOG("[%s] ON\n", __func__);
	else
		PMICLOG("[%s] OFF\n", __func__);

	pmic_config_interface_nolock(PMIC_RG_PWRHOLD_ADDR, hold,
				     PMIC_RG_PWRHOLD_MASK,
				     PMIC_RG_PWRHOLD_SHIFT);
	PMICLOG("[PMIC_KERNEL] PowerHold = 0x%x\n"
		, pmic_get_register_value(PMIC_RG_PWRHOLD));

	return 0;
}

unsigned int PMIC_CHIP_VER(void)
{
	unsigned int ret = 0;
	unsigned short chip_ver = 0;

	chip_ver = pmic_get_register_value(PMIC_SWCID1);

	ret = ((chip_ver & 0xF0) >> 4);

	return ret;
}

void PMIC_LP_INIT_SETTING(void)
{
	g_pmic_chip_version = PMIC_CHIP_VER();
#if LP_INIT_SETTING_VERIFIED
	/* For RF setting: If PL set Multi-user mode, need to sync it */
#if defined(CONFIG_REGULATOR_MT6315)
	/* Suspend */
	pmic_buck_vcore_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vrfdig_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vmd11_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vmd12_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vsram_md_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs3_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vsram_proc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_rfdig_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_ldo_vsram_core_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vmdd2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vmddq_lp(SRCLKEN0, 1, 1, HW_OFF);
	pmic_ldo_vmddr_lp(SW, 1, 1, SW_LP);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vio18_2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_vaux18_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_va12_1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vrf13_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vrf09_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vmc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN0, 1, 1, HW_LP);
	/* Deepidle */
	pmic_buck_vcore_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vrfdig_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vmd11_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vmd12_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vsram_md_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs3_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vsram_proc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_rfdig_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_ldo_vsram_core_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vmdd2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vmddq_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vmddr_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vio18_2_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_2_lp(SRCLKEN2, 1, 1, HW_LP);
	/* SRCLKEN14 HW_ON no need to setting */
	//pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_ON);
	pmic_ldo_vaux18_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_va12_1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vrf13_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vrf09_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vmc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN2, 1, 1, HW_LP);
#else
	/*Suspend*/
	pmic_buck_vcore_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vrfdig_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vmd11_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vmd12_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vsram_md_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_buck_vs3_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vsram_proc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_rfdig_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_ldo_vsram_core_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vmdd2_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vmddq_lp(SRCLKEN0, 1, 1, HW_OFF);
	pmic_ldo_vmddr_lp(SW, 1, 1, SW_LP);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vio18_2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_2_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_vaux18_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_va12_1_lp(SRCLKEN0, 1, 1, HW_LP);
	pmic_ldo_vrf13_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vrf09_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vmc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN0, 1, 1, HW_LP);

	/*Deepidle*/
	pmic_buck_vcore_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vrfdig_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_buck_vmd11_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vmd12_lp(SW, 1, 1, SW_OFF);
	pmic_buck_vsram_md_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs2_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_buck_vs3_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vsram_proc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsram_rfdig_lp(SRCLKEN1, 1, 1, HW_LP);
	pmic_ldo_vsram_core_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vmdd2_lp(SW, 1, 1, SW_ON);
	pmic_ldo_vmddq_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vmddr_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vcn18_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrf18_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vio18_1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vio18_2_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vefuse_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vrfck_lp(SRCLKEN14, 1, 1, HW_OFF);
	pmic_ldo_va12_2_lp(SRCLKEN2, 1, 1, HW_LP);
	/* SRCLKEN14 HW_ON no need to setting */
	//pmic_ldo_vbbck_lp(SRCLKEN14, 1, 1, HW_ON);
	pmic_ldo_vaux18_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vxo22_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_va12_1_lp(SRCLKEN2, 1, 1, HW_LP);
	pmic_ldo_vrf13_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vrf09_lp(SRCLKEN1, 0, 1, HW_OFF);
	pmic_ldo_vemc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vmc_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim1_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vsim2_lp(SW, 1, 1, SW_OFF);
	pmic_ldo_vusb_lp(SRCLKEN2, 1, 1, HW_LP);
#endif /*CONFIG_REGULATOR_MT6315*/
	pr_info("[%s] Chip Ver = %d\n", __func__, g_pmic_chip_version);
#endif /*LP_INIT_SETTING_VERIFIED*/
}
