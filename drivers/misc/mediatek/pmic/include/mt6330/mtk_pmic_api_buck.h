/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MT_PMIC_API_BUCK_H_
#define _MT_PMIC_API_BUCK_H_

void vmd1_pmic_setting_on(void);
void vmd1_pmic_setting_off(void);
void rtc_clk_ctrl(unsigned char en);

#ifdef LP_GOLDEN_SETTING
#define LGS
#endif

#ifdef LP_GOLDEN_SETTING_W_SPM
#define LGSWS
#endif

#if defined(LGS) || defined(LGSWS)
#define PMIC_LP_BUCK_ENTRY(reg) {reg, MT6330_BUCK_##reg##_CON0}
#define PMIC_LP_LDO_ENTRY(reg) {reg, MT6330_LDO_##reg##_CON0}
#endif

enum BUCK_LDO_EN_USER {
	SRCLKEN0,
	SRCLKEN1,
	SRCLKEN2,
	SRCLKEN3,
	SRCLKEN4,
	SRCLKEN5,
	SRCLKEN6,
	SRCLKEN7,
	SRCLKEN8,
	SRCLKEN9,
	SRCLKEN10,
	SRCLKEN11,
	SRCLKEN12,
	SRCLKEN13,
	SRCLKEN14,
	SW,
	SPM = SW,
};

#define HW_OFF		0
#define HW_LP		1
#define HW_ULP		2
#define SW_OFF		0
#define SW_ON		1
#define SW_LP		3
#define SPM_OFF		0
#define SPM_ON		1
#define SPM_LP		3

enum PMU_LP_TABLE_ENUM {
	VCORE,
	VRFDIG,
	VMD11,
	VMD12,
	VSRAM_MD,
	VS1,
	VS2,
	VS3,
	VSRAM_PROC,
	VSRAM_RFDIG,
	VSRAM_CORE,
	VMDD2,
	VMDDQ,
	VMDDR,
	VCN18,
	VRF18,
	VIO18_1,
	VIO18_2,
	VEFUSE,
	VRFCK,
	VA12_2,
	VBBCK,
	VAUX18,
	VXO22,
	VA12_1,
	VRF13,
	VRF09,
	VEMC,
	VMC,
	VSIM1,
	VSIM2,
	VUSB,
	TABLE_COUNT_END
};

struct PMU_LP_TABLE_ENTRY {
	enum PMU_LP_TABLE_ENUM flagname;
#if defined(LGS) || defined(LGSWS)
	unsigned short en_adr;
#endif
};

extern int pmic_buck_vcore_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vrfdig_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vmd11_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vmd12_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vsram_md_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vs1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vs2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vs3_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsram_proc_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsram_rfdig_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsram_core_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vmdd2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vmddq_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vmddr_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vcn18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vrf18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vio18_1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vio18_2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vefuse_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vrfck_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_va12_2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vbbck_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vaux18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vxo22_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_va12_1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vrf13_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vrf09_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vemc_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vmc_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsim1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsim2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vusb_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
#endif
