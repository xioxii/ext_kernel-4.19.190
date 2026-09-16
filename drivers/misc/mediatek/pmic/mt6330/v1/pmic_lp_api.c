/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <pmic/upmu_sw.h>
#include <pmic/upmu_hw.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_api_buck.h"

/* ---------------------------------------------------------------  */
/* pmic_<type>_<name>_lp (<user>, <op_en>, <op_cfg>)                */
/* parameter                                                        */
/* <type>   : BUCK / LDO                                            */
/* <name>   : BUCK name / LDO name                                  */
/* <user>   : SRCLKEN0 / SRCLKEN1 / SRCLKEN2 / SRCLKEN3 / SW / SPM  */
/* <op_mode>: user control mode                                     */
/*            0: multi-user mode                                    */
/*            1: low-power mode                                     */
/* <op_en>  : user control enable                                   */
/* <op_cfg> :                                                       */
/*     HW mode :                                                    */
/*            0 : define as ON/OFF control                          */
/*            1 : define as LP/No LP control                        */
/*     SW/SPM :                                                     */
/*            0 : OFF                                               */
/*            1 : ON                                                */
/* ---------------------------------------------------------------  */

#define pmic_set_sw_en(addr, val)             \
			pmic_config_interface_nolock(addr, val, 1, 0)
#define pmic_set_sw_lp(addr, val)             \
			pmic_config_interface_nolock(addr, val, 1, 1)
#define pmic_set_buck_op_mode(user, addr, val)   \
			pmic_config_interface_nolock(addr, val, 1, user)
#define pmic_set_ldo_op_mode(user, addr, val, shift)    \
			pmic_config_interface_nolock(addr, val, 1, (user) + shift)
#define pmic_set_ldo_ulp(user, addr, val)    \
			pmic_config_interface_nolock(addr, val, 1, 4)
#define pmic_set_op_en(user, addr, val)       \
			pmic_config_interface_nolock(addr, val, 1, user)
#define pmic_set_op_cfg(user, addr, val)      \
			pmic_config_interface_nolock(addr, val, 1, user)
#define pmic_get_buck_op_mode(user, addr, pval)      \
			pmic_read_interface_nolock(addr, pval, 1, user)
#define pmic_get_ldo_op_mode(user, addr, pval, shift)      \
			pmic_read_interface_nolock(addr, pval, 1, (user) + shift)
#define pmic_get_op_en(user, addr, pval)      \
			pmic_read_interface_nolock(addr, pval, 1, user)
#define pmic_get_op_cfg(user, addr, pval)     \
			pmic_read_interface_nolock(addr, pval, 1, user)

#define en_cfg_shift  0x6

#if defined(LGS) || defined(LGSWS)

const struct PMU_LP_TABLE_ENTRY pmu_lp_table[] = {
	PMIC_LP_BUCK_ENTRY(VCORE),
	PMIC_LP_BUCK_ENTRY(VRFDIG),
	PMIC_LP_BUCK_ENTRY(VMD11),
	PMIC_LP_BUCK_ENTRY(VMD12),
	PMIC_LP_BUCK_ENTRY(VSRAM_MD),
	PMIC_LP_BUCK_ENTRY(VS1),
	PMIC_LP_BUCK_ENTRY(VS2),
	PMIC_LP_BUCK_ENTRY(VS3),
	PMIC_LP_LDO_ENTRY(VSRAM_PROC),
	PMIC_LP_LDO_ENTRY(VSRAM_RFDIG),
	PMIC_LP_LDO_ENTRY(VSRAM_CORE),
	PMIC_LP_LDO_ENTRY(VMDD2),
	PMIC_LP_LDO_ENTRY(VMDDQ),
	PMIC_LP_LDO_ENTRY(VMDDR),
	PMIC_LP_LDO_ENTRY(VCN18),
	PMIC_LP_LDO_ENTRY(VRF18),
	PMIC_LP_LDO_ENTRY(VIO18_1),
	PMIC_LP_LDO_ENTRY(VIO18_2),
	PMIC_LP_LDO_ENTRY(VEFUSE),
	PMIC_LP_LDO_ENTRY(VRFCK),
	PMIC_LP_LDO_ENTRY(VA12_2),
	PMIC_LP_LDO_ENTRY(VBBCK),
	PMIC_LP_LDO_ENTRY(VAUX18),
	PMIC_LP_LDO_ENTRY(VXO22),
	PMIC_LP_LDO_ENTRY(VA12_1),
	PMIC_LP_LDO_ENTRY(VRF13),
	PMIC_LP_LDO_ENTRY(VRF09),
	PMIC_LP_LDO_ENTRY(VEMC),
	PMIC_LP_LDO_ENTRY(VMC),
	PMIC_LP_LDO_ENTRY(VSIM1),
	PMIC_LP_LDO_ENTRY(VSIM2),
	PMIC_LP_LDO_ENTRY(VUSB),
};

static int pmic_lp_golden_set(unsigned int en_adr,
		unsigned char op_en, unsigned char op_cfg)
{
	unsigned int en_cfg = 0, lp_cfg = 0;

	/*--op_cfg 0:SW_OFF, 1:SW_EN, 3: SW_LP (SPM)--*/
	if (op_en > 1 || op_cfg > 3) {
		pr_notice("p\n");
		return -1;
	}

	en_cfg = op_cfg & 0x1;
	lp_cfg = (op_cfg >> 1) & 0x1;
	pmic_set_sw_en(en_adr, en_cfg);
	pmic_set_sw_lp(en_adr, lp_cfg);
}
#endif

static int pmic_lp_type_set(
		unsigned short en_cfg_adr,
		enum PMU_LP_TABLE_ENUM name,
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	unsigned int rb_en = 0, rb_cfg = 0, max_cfg = 1;
	unsigned short op_en_adr = 0, op_cfg_adr = 0;
	int ret = 0, ret_en = 0, ret_cfg = 0;

	if (en_cfg_adr) {
		op_en_adr = en_cfg_adr;
		op_cfg_adr = (unsigned short)(en_cfg_adr + en_cfg_shift);
	}
	/*--else keep default addr = 0--*/
	if (user == SW || user == SPM) {
		max_cfg = 3;
		rb_cfg = 0;
		rb_en = 0;
	}

	if (op_en > 1 || op_cfg > max_cfg) {
		pr_notice("p\n");
		return -1;
	}

	pr_notice("0x%x,user %d\n", en_cfg_adr, user);

#if defined(LGS) || defined(LGSWS)
	const struct PMU_LP_TABLE_ENTRY *pFlag = &pmu_lp_table[name];

	if (user == SW || user == SPM)
		pmic_lp_golden_set((unsigned int)pFlag->en_adr, op_en, op_cfg);
#endif

	if (op_cfg_adr && op_en_adr) {
		pmic_set_op_en(user, op_en_adr, op_en);
		pmic_get_op_en(user, op_en_adr, &rb_en);
		pr_notice("user = %d, op en = %d\n", user, rb_en);
		(rb_en == op_en) ? (ret_en = 0) : (ret_en = -1);
		if (user != SW && user != SPM) {
			pmic_set_op_cfg(user, op_cfg_adr, op_cfg);
			pmic_get_op_cfg(user, op_cfg_adr, &rb_cfg);
			(rb_cfg == op_cfg) ? (ret_cfg = 0) : (ret_cfg - 1);
			pr_notice("user = %d, op cfg = %d\n", user, rb_cfg);
		}
	}

	((!ret_en) && (!ret_cfg)) ? (ret = 0) : (ret = -1);
	if (ret)
		pr_notice("%d, %d, %d\n", user, ret_en, ret_cfg);
	return ret;
}

int pmic_buck_vcore_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6330_BUCK_VCORE_OP_MODE_0,
				op_mode);
	}
	return pmic_lp_type_set(MT6330_BUCK_VCORE_OP_EN_0,
				VCORE, user, op_en, op_cfg);
}

int pmic_buck_vrfdig_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6330_BUCK_VRFDIG_OP_MODE_0,
				op_mode);
	}
	return pmic_lp_type_set(MT6330_BUCK_VRFDIG_OP_EN_0,
				VRFDIG, user, op_en, op_cfg);
}

int pmic_buck_vmd11_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6330_BUCK_VMD11_OP_MODE_0,
				op_mode);
	}
	return pmic_lp_type_set(MT6330_BUCK_VMD11_OP_EN_0,
				VMD11, user, op_en, op_cfg);
}

int pmic_buck_vmd12_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6330_BUCK_VMD12_OP_MODE_0,
				op_mode);
	}
	return pmic_lp_type_set(MT6330_BUCK_VMD12_OP_EN_0,
				VMD12, user, op_en, op_cfg);
}

int pmic_buck_vsram_md_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6330_BUCK_VSRAM_MD_OP_MODE_0,
				op_mode);
	}
	return pmic_lp_type_set(MT6330_BUCK_VSRAM_MD_OP_EN_0,
				VSRAM_MD, user, op_en, op_cfg);
}

int pmic_buck_vs1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6330_BUCK_VS1_OP_MODE_0,
				op_mode);
	}
	return pmic_lp_type_set(MT6330_BUCK_VS1_OP_EN_0,
				VS1, user, op_en, op_cfg);
}

int pmic_buck_vs2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6330_BUCK_VS2_OP_MODE_0,
				op_mode);
	}
	return pmic_lp_type_set(MT6330_BUCK_VS2_OP_EN_0,
				VS2, user, op_en, op_cfg);
}

int pmic_buck_vs3_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6330_BUCK_VS3_OP_MODE_0,
				op_mode);
	}
	return pmic_lp_type_set(MT6330_BUCK_VS3_OP_EN_0,
				VS3, user, op_en, op_cfg);
}

/* ----------------- LDO ---------------------- */
int pmic_ldo_vsram_proc_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VSRAM_PROC_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VSRAM_PROC_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VSRAM_PROC_OP_EN0,
				VSRAM_PROC, user, op_en, op_cfg);
}

int pmic_ldo_vsram_rfdig_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VSRAM_RFDIG_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VSRAM_RFDIG_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VSRAM_RFDIG_OP_EN0,
				VSRAM_RFDIG, user, op_en, op_cfg);
}

int pmic_ldo_vsram_core_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VSRAM_CORE_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VSRAM_CORE_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VSRAM_CORE_OP_EN0,
				VSRAM_CORE, user, op_en, op_cfg);
}

int pmic_ldo_vmdd2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VMDD2_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VMDD2_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VMDD2_OP_EN0,
				VMDD2, user, op_en, op_cfg);
}

int pmic_ldo_vmddq_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VMDDQ_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VMDDQ_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VMDDQ_OP_EN0,
				VMDDQ, user, op_en, op_cfg);
}

int pmic_ldo_vmddr_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VMDDR_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VMDDR_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VMDDR_OP_EN0,
				VMDDR, user, op_en, op_cfg);
}

int pmic_ldo_vcn18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VCN18_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VCN18_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VCN18_OP_EN0,
				VCN18, user, op_en, op_cfg);
}

int pmic_ldo_vrf18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VRF18_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VRF18_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VRF18_OP_EN0,
				VRF18, user, op_en, op_cfg);
}

int pmic_ldo_vio18_1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VIO18_1_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VIO18_1_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VIO18_1_OP_EN0,
				VIO18_1, user, op_en, op_cfg);
}

int pmic_ldo_vio18_2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VIO18_2_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VIO18_2_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VIO18_2_OP_EN0,
				VIO18_2, user, op_en, op_cfg);
}

int pmic_ldo_vefuse_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VEFUSE_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VEFUSE_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VEFUSE_OP_EN0,
				VEFUSE, user, op_en, op_cfg);
}

int pmic_ldo_vrfck_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VRFCK_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VRFCK_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VRFCK_OP_EN1,
				VRFCK, user, op_en, op_cfg);
}

int pmic_ldo_va12_2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VA12_2_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VA12_2_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VA12_2_OP_EN0,
				VA12_2, user, op_en, op_cfg);
}

int pmic_ldo_vbbck_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VBBCK_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VBBCK_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VBBCK_OP_EN1,
				VBBCK, user, op_en, op_cfg);
}

int pmic_ldo_vaux18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VAUX18_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VAUX18_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VAUX18_OP_EN0,
				VAUX18, user, op_en, op_cfg);
}

int pmic_ldo_vxo22_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VXO22_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VXO22_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VXO22_OP_EN0,
				VXO22, user, op_en, op_cfg);
}

int pmic_ldo_va12_1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VA12_1_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VA12_1_OP_MODE_SHIFT);
	return pmic_lp_type_set(MT6330_LDO_VA12_1_OP_EN0,
				VA12_1, user, op_en, op_cfg);
}

int pmic_ldo_vrf13_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VRF13_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VRF13_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VRF13_OP_EN0,
				VRF13, user, op_en, op_cfg);
}

int pmic_ldo_vrf09_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VRF09_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VRF09_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VRF09_OP_EN0,
				VRF09, user, op_en, op_cfg);
}

int pmic_ldo_vemc_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VEMC_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VEMC_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VEMC_OP_EN0,
				VEMC, user, op_en, op_cfg);
}

int pmic_ldo_vmc_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VMC_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VMC_OP_MODE_ADDR);

	return pmic_lp_type_set(MT6330_LDO_VMC_OP_EN0,
				VMC, user, op_en, op_cfg);
}

int pmic_ldo_vsim1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VSIM1_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VSIM1_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VSIM1_OP_EN0,
				VSIM1, user, op_en, op_cfg);
}

int pmic_ldo_vsim2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VSIM2_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VSIM2_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VSIM2_OP_EN0,
				VSIM2, user, op_en, op_cfg);
}

int pmic_ldo_vusb_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, PMIC_RG_LDO_VUSB_OP_MODE_ADDR,
				     op_mode,
				     PMIC_RG_LDO_VUSB_OP_MODE_SHIFT);

	return pmic_lp_type_set(MT6330_LDO_VUSB_OP_EN0,
				VUSB, user, op_en, op_cfg);
}
