// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/printk.h>
#include <pmic/upmu_hw.h>
#include <mt-plat/upmu_common.h>


unsigned int mt6330_upmu_set_rg_buck_vs1_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		MT6330_TOP_VRCTL_VR0_EN,
		val,
		PMIC_RG_BUCK_VS1_EN_MASK,
		PMIC_RG_BUCK_VS1_EN_SHIFT);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vs1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		MT6330_TOP_VRCTL_VR0_EN,
		&val,
		PMIC_RG_BUCK_VS1_EN_MASK,
		PMIC_RG_BUCK_VS1_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6330_upmu_get_da_vs1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		MT6330_BUCK_VS1_DBG0,
		&val,
		PMIC_DA_VS1_VOSEL_MASK,
		PMIC_DA_VS1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vs1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK5),
		(val),
		(PMIC_RG_BUCK_VS1_VOSEL_MASK),
		(PMIC_RG_BUCK_VS1_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vs1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK5),
		(&val),
		(PMIC_RG_BUCK_VS1_VOSEL_MASK),
		(PMIC_RG_BUCK_VS1_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vmd12_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(val),
		(PMIC_RG_BUCK_VMD12_EN_MASK),
		(PMIC_RG_BUCK_VMD12_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vmd12_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(&val),
		(PMIC_RG_BUCK_VMD12_EN_MASK),
		(PMIC_RG_BUCK_VMD12_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_get_da_vmd12_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_BUCK_VMD12_DBG0),
		(&val),
		(PMIC_DA_VMD12_VOSEL_MASK),
		(PMIC_DA_VMD12_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vmd12_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK2),
		(val),
		(PMIC_RG_BUCK_VMD12_VOSEL_MASK),
		(PMIC_RG_BUCK_VMD12_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vmd12_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK2),
		(&val),
		(PMIC_RG_BUCK_VMD12_VOSEL_MASK),
		(PMIC_RG_BUCK_VMD12_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vrfdig_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(val),
		(PMIC_RG_BUCK_VRFDIG_EN_MASK),
		(PMIC_RG_BUCK_VRFDIG_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vrfdig_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(&val),
		(PMIC_RG_BUCK_VRFDIG_EN_MASK),
		(PMIC_RG_BUCK_VRFDIG_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_get_da_vrfdig_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_BUCK_VRFDIG_DBG2),
		(&val),
		(PMIC_DA_VRFDIG_EN_MASK),
		(PMIC_DA_VRFDIG_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vrfdig_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK4),
		(val),
		(PMIC_RG_BUCK_VRFDIG_VOSEL_MASK),
		(PMIC_RG_BUCK_VRFDIG_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vrfdig_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK4),
		(&val),
		(PMIC_RG_BUCK_VRFDIG_VOSEL_MASK),
		(PMIC_RG_BUCK_VRFDIG_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vcore_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(val),
		(PMIC_RG_BUCK_VCORE_EN_MASK),
		(PMIC_RG_BUCK_VCORE_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vcore_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(&val),
		(PMIC_RG_BUCK_VCORE_EN_MASK),
		(PMIC_RG_BUCK_VCORE_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_get_da_vcore_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_BUCK_VCORE_DBG2),
		(&val),
		(PMIC_DA_VCORE_EN_MASK),
		(PMIC_DA_VCORE_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vcore_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK0),
		(val),
		(PMIC_RG_BUCK_VCORE_VOSEL_MASK),
		(PMIC_RG_BUCK_VCORE_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vcore_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK0),
		(&val),
		(PMIC_RG_BUCK_VCORE_VOSEL_MASK),
		(PMIC_RG_BUCK_VCORE_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vs2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(val),
		(PMIC_RG_BUCK_VS2_EN_MASK),
		(PMIC_RG_BUCK_VS2_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vs2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(&val),
		(PMIC_RG_BUCK_VS2_EN_MASK),
		(PMIC_RG_BUCK_VS2_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_get_da_vs2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_BUCK_VS2_DBG2),
		(&val),
		(PMIC_DA_VS2_EN_MASK),
		(PMIC_DA_VS2_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vs2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK6),
		(val),
		(PMIC_RG_BUCK_VS2_VOSEL_MASK),
		(PMIC_RG_BUCK_VS2_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vs2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK6),
		(&val),
		(PMIC_RG_BUCK_VS2_VOSEL_MASK),
		(PMIC_RG_BUCK_VS2_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vmd11_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(val),
		(PMIC_RG_BUCK_VMD11_EN_MASK),
		(PMIC_RG_BUCK_VMD11_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vmd11_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(&val),
		(PMIC_RG_BUCK_VMD11_EN_MASK),
		(PMIC_RG_BUCK_VMD11_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_get_da_vmd11_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_BUCK_VMD11_DBG2),
		(&val),
		(PMIC_DA_VMD11_EN_MASK),
		(PMIC_DA_VMD11_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vmd11_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK1),
		(val),
		(PMIC_RG_BUCK_VMD11_VOSEL_MASK),
		(PMIC_RG_BUCK_VMD11_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vmd11_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK1),
		(&val),
		(PMIC_RG_BUCK_VMD11_VOSEL_MASK),
		(PMIC_RG_BUCK_VMD11_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vsram_md_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(val),
		(PMIC_RG_BUCK_VSRAM_MD_EN_MASK),
		(PMIC_RG_BUCK_VSRAM_MD_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vsram_md_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(&val),
		(PMIC_RG_BUCK_VSRAM_MD_EN_MASK),
		(PMIC_RG_BUCK_VSRAM_MD_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_get_da_vsram_md_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_BUCK_VSRAM_MD_DBG2),
		(&val),
		(PMIC_DA_VSRAM_MD_EN_MASK),
		(PMIC_DA_VSRAM_MD_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vsram_md_vosel_delta(
	unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_BUCK_VSRAM_MD_TRACK0),
		(val),
		(PMIC_RG_BUCK_VSRAM_MD_VOSEL_DELTA_MASK),
		(PMIC_RG_BUCK_VSRAM_MD_VOSEL_DELTA_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vsram_md_vosel_delta(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_BUCK_VSRAM_MD_TRACK0),
		(&val),
		(PMIC_RG_BUCK_VSRAM_MD_VOSEL_DELTA_MASK),
		(PMIC_RG_BUCK_VSRAM_MD_VOSEL_DELTA_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vs3_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(val),
		(PMIC_RG_BUCK_VS3_EN_MASK),
		(PMIC_RG_BUCK_VS3_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vs3_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR0_EN),
		(&val),
		(PMIC_RG_BUCK_VS3_EN_MASK),
		(PMIC_RG_BUCK_VS3_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_get_da_vs3_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_BUCK_VS3_DBG2),
		(&val),
		(PMIC_DA_VS3_EN_MASK),
		(PMIC_DA_VS3_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_buck_vs3_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK7),
		(val),
		(PMIC_RG_BUCK_VS3_VOSEL_MASK),
		(PMIC_RG_BUCK_VS3_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_buck_vs3_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VBUCK7),
		(&val),
		(PMIC_RG_BUCK_VS3_VOSEL_MASK),
		(PMIC_RG_BUCK_VS3_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vsram_core_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR1_EN),
		(val),
		(PMIC_RG_LDO_VSRAM_CORE_EN_MASK),
		(PMIC_RG_LDO_VSRAM_CORE_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vsram_core_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR1_EN),
		(&val),
		(PMIC_RG_LDO_VSRAM_CORE_EN_MASK),
		(PMIC_RG_LDO_VSRAM_CORE_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vsram_core_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VSRAM1),
		(val),
		(PMIC_RG_LDO_VSRAM_CORE_VOSEL_MASK),
		(PMIC_RG_LDO_VSRAM_CORE_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vsram_core_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VSRAM1),
		(&val),
		(PMIC_RG_LDO_VSRAM_CORE_VOSEL_MASK),
		(PMIC_RG_LDO_VSRAM_CORE_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vsram_proc_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR1_EN),
		(val),
		(PMIC_RG_LDO_VSRAM_PROC_EN_MASK),
		(PMIC_RG_LDO_VSRAM_PROC_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vsram_proc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR1_EN),
		(&val),
		(PMIC_RG_LDO_VSRAM_PROC_EN_MASK),
		(PMIC_RG_LDO_VSRAM_PROC_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vsram_proc_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VSRAM0),
		(val),
		(PMIC_RG_LDO_VSRAM_PROC_VOSEL_MASK),
		(PMIC_RG_LDO_VSRAM_PROC_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vsram_proc_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VSRAM0),
		(&val),
		(PMIC_RG_LDO_VSRAM_PROC_VOSEL_MASK),
		(PMIC_RG_LDO_VSRAM_PROC_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vsram_rfdig_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VR1_EN),
		(val),
		(PMIC_RG_LDO_VSRAM_RFDIG_EN_MASK),
		(PMIC_RG_LDO_VSRAM_RFDIG_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vsram_rfdig_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VR1_EN),
		(&val),
		(PMIC_RG_LDO_VSRAM_RFDIG_EN_MASK),
		(PMIC_RG_LDO_VSRAM_RFDIG_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vsram_rfdig_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_TOP_VRCTL_VOSEL_VSRAM2),
		(val),
		(PMIC_RG_LDO_VSRAM_RFDIG_VOSEL_MASK),
		(PMIC_RG_LDO_VSRAM_RFDIG_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vsram_rfdig_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_TOP_VRCTL_VOSEL_VSRAM2),
		(&val),
		(PMIC_RG_LDO_VSRAM_RFDIG_VOSEL_MASK),
		(PMIC_RG_LDO_VSRAM_RFDIG_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vrfck_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VRFCK_CON0),
		(val),
		(PMIC_RG_LDO_VRFCK_EN_MASK),
		(PMIC_RG_LDO_VRFCK_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vrfck_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VRFCK_CON0),
		(&val),
		(PMIC_RG_LDO_VRFCK_EN_MASK),
		(PMIC_RG_LDO_VRFCK_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vrfck_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VRFCK_ANA_CON0),
		(val),
		(PMIC_RG_VRFCK_VOSEL_MASK),
		(PMIC_RG_VRFCK_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vrfck_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VRFCK_ANA_CON0),
		(&val),
		(PMIC_RG_VRFCK_VOSEL_MASK),
		(PMIC_RG_VRFCK_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vsim1_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VSIM1_CON0),
		(val),
		(PMIC_RG_LDO_VSIM1_EN_MASK),
		(PMIC_RG_LDO_VSIM1_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vsim1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VSIM1_CON0),
		(&val),
		(PMIC_RG_LDO_VSIM1_EN_MASK),
		(PMIC_RG_LDO_VSIM1_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vsim1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VSIM1_ANA_CON1),
		(val),
		(PMIC_RG_VSIM1_VOSEL_MASK),
		(PMIC_RG_VSIM1_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vsim1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VSIM1_ANA_CON1),
		(&val),
		(PMIC_RG_VSIM1_VOSEL_MASK),
		(PMIC_RG_VSIM1_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vio18_2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VIO18_2_CON0),
		(val),
		(PMIC_RG_LDO_VIO18_2_EN_MASK),
		(PMIC_RG_LDO_VIO18_2_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vio18_2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VIO18_2_CON0),
		(&val),
		(PMIC_RG_LDO_VIO18_2_EN_MASK),
		(PMIC_RG_LDO_VIO18_2_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vio18_2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VIO18_2_ANA_CON1),
		(val),
		(PMIC_RG_VIO18_2_VOSEL_MASK),
		(PMIC_RG_VIO18_2_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vio18_2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VIO18_2_ANA_CON1),
		(&val),
		(PMIC_RG_VIO18_2_VOSEL_MASK),
		(PMIC_RG_VIO18_2_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vio18_1_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VIO18_1_CON0),
		(val),
		(PMIC_RG_LDO_VIO18_1_EN_MASK),
		(PMIC_RG_LDO_VIO18_1_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vio18_1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VIO18_1_CON0),
		(&val),
		(PMIC_RG_LDO_VIO18_1_EN_MASK),
		(PMIC_RG_LDO_VIO18_1_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vio18_1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VIO18_1_ANA_CON1),
		(val),
		(PMIC_RG_VIO18_1_VOSEL_MASK),
		(PMIC_RG_VIO18_1_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vio18_1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VIO18_1_ANA_CON1),
		(&val),
		(PMIC_RG_VIO18_1_VOSEL_MASK),
		(PMIC_RG_VIO18_1_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vusb_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VUSB_CON0),
		(val),
		(PMIC_RG_LDO_VUSB_EN_MASK),
		(PMIC_RG_LDO_VUSB_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vusb_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VUSB_CON0),
		(&val),
		(PMIC_RG_LDO_VUSB_EN_MASK),
		(PMIC_RG_LDO_VUSB_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vusb_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VUSB_ANA_CON1),
		(val),
		(PMIC_RG_VUSB_VOSEL_MASK),
		(PMIC_RG_VUSB_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vusb_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VUSB_ANA_CON1),
		(&val),
		(PMIC_RG_VUSB_VOSEL_MASK),
		(PMIC_RG_VUSB_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vmdd2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VMDD2_CON0),
		(val),
		(PMIC_RG_LDO_VMDD2_EN_MASK),
		(PMIC_RG_LDO_VMDD2_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vmdd2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VMDD2_CON0),
		(&val),
		(PMIC_RG_LDO_VMDD2_EN_MASK),
		(PMIC_RG_LDO_VMDD2_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vmdd2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VMDD2_ANA_CON1),
		(val),
		(PMIC_RG_VMDD2_VOSEL_MASK),
		(PMIC_RG_VMDD2_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vmdd2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VMDD2_ANA_CON1),
		(&val),
		(PMIC_RG_VMDD2_VOSEL_MASK),
		(PMIC_RG_VMDD2_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vcn18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VCN18_CON0),
		(val),
		(PMIC_RG_LDO_VCN18_EN_MASK),
		(PMIC_RG_LDO_VCN18_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vcn18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VCN18_CON0),
		(&val),
		(PMIC_RG_LDO_VCN18_EN_MASK),
		(PMIC_RG_LDO_VCN18_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vcn18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VCN18_ANA_CON1),
		(val),
		(PMIC_RG_VCN18_VOSEL_MASK),
		(PMIC_RG_VCN18_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vcn18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VCN18_ANA_CON1),
		(&val),
		(PMIC_RG_VCN18_VOSEL_MASK),
		(PMIC_RG_VCN18_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vaux18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VAUX18_CON0),
		(val),
		(PMIC_RG_LDO_VAUX18_EN_MASK),
		(PMIC_RG_LDO_VAUX18_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vaux18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VAUX18_CON0),
		(&val),
		(PMIC_RG_LDO_VAUX18_EN_MASK),
		(PMIC_RG_LDO_VAUX18_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vaux18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VAUX18_ANA_CON1),
		(val),
		(PMIC_RG_VAUX18_VOSEL_MASK),
		(PMIC_RG_VAUX18_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vaux18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VAUX18_ANA_CON1),
		(&val),
		(PMIC_RG_VAUX18_VOSEL_MASK),
		(PMIC_RG_VAUX18_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vrf13_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VRF13_CON0),
		(val),
		(PMIC_RG_LDO_VRF13_EN_MASK),
		(PMIC_RG_LDO_VRF13_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vrf13_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VRF13_CON0),
		(&val),
		(PMIC_RG_LDO_VRF13_EN_MASK),
		(PMIC_RG_LDO_VRF13_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vrf13_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VRF13_ANA_CON1),
		(val),
		(PMIC_RG_VRF13_VOSEL_MASK),
		(PMIC_RG_VRF13_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vrf13_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VRF13_ANA_CON1),
		(&val),
		(PMIC_RG_VRF13_VOSEL_MASK),
		(PMIC_RG_VRF13_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vxo22_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VXO22_CON0),
		(val),
		(PMIC_RG_LDO_VXO22_EN_MASK),
		(PMIC_RG_LDO_VXO22_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vxo22_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VXO22_CON0),
		(&val),
		(PMIC_RG_LDO_VXO22_EN_MASK),
		(PMIC_RG_LDO_VXO22_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vxo22_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VXO22_ANA_CON0),
		(val),
		(PMIC_RG_VXO22_VOSEL_MASK),
		(PMIC_RG_VXO22_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vxo22_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VXO22_ANA_CON0),
		(&val),
		(PMIC_RG_VXO22_VOSEL_MASK),
		(PMIC_RG_VXO22_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vefuse_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VEFUSE_CON0),
		(val),
		(PMIC_RG_LDO_VEFUSE_EN_MASK),
		(PMIC_RG_LDO_VEFUSE_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vefuse_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VEFUSE_CON0),
		(&val),
		(PMIC_RG_LDO_VEFUSE_EN_MASK),
		(PMIC_RG_LDO_VEFUSE_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vefuse_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VEFUSE_ANA_CON1),
		(val),
		(PMIC_RG_VEFUSE_VOSEL_MASK),
		(PMIC_RG_VEFUSE_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vefuse_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VEFUSE_ANA_CON1),
		(&val),
		(PMIC_RG_VEFUSE_VOSEL_MASK),
		(PMIC_RG_VEFUSE_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_va12_2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VA12_2_CON0),
		(val),
		(PMIC_RG_LDO_VA12_2_EN_MASK),
		(PMIC_RG_LDO_VA12_2_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_va12_2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VA12_2_CON0),
		(&val),
		(PMIC_RG_LDO_VA12_2_EN_MASK),
		(PMIC_RG_LDO_VA12_2_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_va12_2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VA12_2_ANA_CON1),
		(val),
		(PMIC_RG_VA12_2_VOSEL_MASK),
		(PMIC_RG_VA12_2_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_va12_2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VA12_2_ANA_CON1),
		(&val),
		(PMIC_RG_VA12_2_VOSEL_MASK),
		(PMIC_RG_VA12_2_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_va12_1_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VA12_1_CON0),
		(val),
		(PMIC_RG_LDO_VA12_1_EN_MASK),
		(PMIC_RG_LDO_VA12_1_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_va12_1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VA12_1_CON0),
		(&val),
		(PMIC_RG_LDO_VA12_1_EN_MASK),
		(PMIC_RG_LDO_VA12_1_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_va12_1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VA12_1_ANA_CON1),
		(val),
		(PMIC_RG_VA12_1_VOSEL_MASK),
		(PMIC_RG_VA12_1_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_va12_1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VA12_1_ANA_CON1),
		(&val),
		(PMIC_RG_VA12_1_VOSEL_MASK),
		(PMIC_RG_VA12_1_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vemc_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VEMC_CON0),
		(val),
		(PMIC_RG_LDO_VEMC_EN_MASK),
		(PMIC_RG_LDO_VEMC_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vemc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VEMC_CON0),
		(&val),
		(PMIC_RG_LDO_VEMC_EN_MASK),
		(PMIC_RG_LDO_VEMC_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vemc_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VEMC_ANA_CON1),
		(val),
		(PMIC_RG_VEMC_VOSEL_MASK),
		(PMIC_RG_VEMC_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vemc_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VEMC_ANA_CON1),
		(&val),
		(PMIC_RG_VEMC_VOSEL_MASK),
		(PMIC_RG_VEMC_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vrf09_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VRF09_CON0),
		(val),
		(PMIC_RG_LDO_VRF09_EN_MASK),
		(PMIC_RG_LDO_VRF09_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vrf09_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VRF09_CON0),
		(&val),
		(PMIC_RG_LDO_VRF09_EN_MASK),
		(PMIC_RG_LDO_VRF09_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vrf09_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VRF09_ANA_CON1),
		(val),
		(PMIC_RG_VRF09_VOSEL_MASK),
		(PMIC_RG_VRF09_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vrf09_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VRF09_ANA_CON1),
		(&val),
		(PMIC_RG_VRF09_VOSEL_MASK),
		(PMIC_RG_VRF09_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vmddr_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VMDDR_CON0),
		(val),
		(PMIC_RG_LDO_VMDDR_EN_MASK),
		(PMIC_RG_LDO_VMDDR_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vmddr_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VMDDR_CON0),
		(&val),
		(PMIC_RG_LDO_VMDDR_EN_MASK),
		(PMIC_RG_LDO_VMDDR_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vmddr_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VMDDR_ANA_CON1),
		(val),
		(PMIC_RG_VMDDR_VOSEL_MASK),
		(PMIC_RG_VMDDR_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vmddr_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VMDDR_ANA_CON1),
		(&val),
		(PMIC_RG_VMDDR_VOSEL_MASK),
		(PMIC_RG_VMDDR_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vmddq_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VMDDQ_CON0),
		(val),
		(PMIC_RG_LDO_VMDDQ_EN_MASK),
		(PMIC_RG_LDO_VMDDQ_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vmddq_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VMDDQ_CON0),
		(&val),
		(PMIC_RG_LDO_VMDDQ_EN_MASK),
		(PMIC_RG_LDO_VMDDQ_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vmddq_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VMDDQ_ANA_CON1),
		(val),
		(PMIC_RG_VMDDQ_VOSEL_MASK),
		(PMIC_RG_VMDDQ_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vmddq_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VMDDQ_ANA_CON1),
		(&val),
		(PMIC_RG_VMDDQ_VOSEL_MASK),
		(PMIC_RG_VMDDQ_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vrf18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VRF18_CON0),
		(val),
		(PMIC_RG_LDO_VRF18_EN_MASK),
		(PMIC_RG_LDO_VRF18_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vrf18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VRF18_CON0),
		(&val),
		(PMIC_RG_LDO_VRF18_EN_MASK),
		(PMIC_RG_LDO_VRF18_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vrf18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VRF18_ANA_CON1),
		(val),
		(PMIC_RG_VRF18_VOSEL_MASK),
		(PMIC_RG_VRF18_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vrf18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VRF18_ANA_CON1),
		(&val),
		(PMIC_RG_VRF18_VOSEL_MASK),
		(PMIC_RG_VRF18_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vmc_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VMC_CON0),
		(val),
		(PMIC_RG_LDO_VMC_EN_MASK),
		(PMIC_RG_LDO_VMC_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vmc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VMC_CON0),
		(&val),
		(PMIC_RG_LDO_VMC_EN_MASK),
		(PMIC_RG_LDO_VMC_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vmc_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VMC_ANA_CON1),
		(val),
		(PMIC_RG_VMC_VOSEL_MASK),
		(PMIC_RG_VMC_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vmc_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VMC_ANA_CON1),
		(&val),
		(PMIC_RG_VMC_VOSEL_MASK),
		(PMIC_RG_VMC_VOSEL_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vbbck_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VBBCK_CON0),
		(val),
		(PMIC_RG_LDO_VBBCK_EN_MASK),
		(PMIC_RG_LDO_VBBCK_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vbbck_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VBBCK_CON0),
		(&val),
		(PMIC_RG_LDO_VBBCK_EN_MASK),
		(PMIC_RG_LDO_VBBCK_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_ldo_vsim2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_LDO_VSIM2_CON0),
		(val),
		(PMIC_RG_LDO_VSIM2_EN_MASK),
		(PMIC_RG_LDO_VSIM2_EN_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_ldo_vsim2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_LDO_VSIM2_CON0),
		(&val),
		(PMIC_RG_LDO_VSIM2_EN_MASK),
		(PMIC_RG_LDO_VSIM2_EN_SHIFT)
		);

	return val;
}

unsigned int mt6330_upmu_set_rg_vsim2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(MT6330_VSIM2_ANA_CON1),
		(val),
		(PMIC_RG_VSIM2_VOSEL_MASK),
		(PMIC_RG_VSIM2_VOSEL_SHIFT)
			);

	return ret;
}

unsigned int mt6330_upmu_get_rg_vsim2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(MT6330_VSIM2_ANA_CON1),
		(&val),
		(PMIC_RG_VSIM2_VOSEL_MASK),
		(PMIC_RG_VSIM2_VOSEL_SHIFT)
		);

	return val;
}
