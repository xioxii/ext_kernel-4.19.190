/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_debugfs.h"

#define INIT_SETTING_VERIFIED	1

/* TODO
 * PMIC debug level
 */
unsigned int pmic_dbg_level_set(unsigned int level)
{
	unsigned char Dlevel = (level >> 0) & 0xF;
	unsigned char HKlevel = (level >> 4) & 0xF;
	unsigned char IRQlevel = (level >> 8) & 0xF;
	unsigned char REGlevel = (level >> 12) & 0xF;

	gPMICDbgLvl = Dlevel > PMIC_LOG_DBG ? PMIC_LOG_DBG : Dlevel;
	gPMICHKDbgLvl = HKlevel > PMIC_LOG_DBG ? PMIC_LOG_DBG : HKlevel;
	gPMICIRQDbgLvl = IRQlevel > PMIC_LOG_DBG ? PMIC_LOG_DBG : IRQlevel;
	gPMICREGDbgLvl = REGlevel > PMIC_LOG_DBG ? PMIC_LOG_DBG : REGlevel;
	return 0;
}

/*
 * PMIC reg cmp log
 */
struct pmic_setting {
	unsigned short addr;
	unsigned char  val;
	unsigned char mask;
	unsigned char shift;
};

static struct pmic_setting init_setting[] = {
	{0x11, 0xA, 0xA, 0},
	{0x13, 0x1F, 0x1F, 0},
	{0x1B, 0x1, 0x1, 0},
	{0x3C, 0x0, 0xFF, 0},
	{0x3F, 0x0, 0x80, 0},
	{0x8E, 0xF0, 0xFF, 0},
	{0x91, 0x0, 0x3, 0},
	{0x10F, 0x10, 0x10, 0},
	{0x112, 0x4, 0x4, 0},
	{0x12D, 0x1, 0x1, 0},
	{0x139, 0x20, 0x20, 0},
	{0x14B, 0x3, 0x3, 0},
	{0x187, 0xFF, 0xFF, 0},
	{0x188, 0xFF, 0xFF, 0},
	{0x189, 0xFE, 0xFE, 0},
	{0x19A, 0x0, 0x1, 0},
	{0x413, 0x7, 0xFF, 0},
	{0x416, 0xD, 0xFF, 0},
	{0x417, 0x0, 0x7, 0},
	{0x418, 0x10, 0xF0, 0},
	{0x41A, 0x0, 0x20, 0},
	{0x444, 0xD, 0xFF, 0},
	{0x445, 0x0, 0x7, 0},
	{0x448, 0x0, 0x8, 0},
	{0x50C, 0x1, 0x1, 0},
	{0x557, 0x1, 0x1, 0},
	{0x790, 0x3, 0x3, 0},
	{0x7A7, 0xF8, 0xFC, 0},
	{0x7A9, 0x0, 0x2, 0},
	{0x98A, 0x10, 0x10, 0},
	{0xA08, 0x1, 0x1, 0},
	{0xA0C, 0x1, 0x3, 0},
	{0xA0E, 0x0, 0x2, 0},
	{0xA0F, 0x1, 0x1, 0},
	{0xA10, 0xE0, 0xE0, 0},
	{0xA1B, 0xFF, 0xFF, 0},
	{0xA1C, 0xFF, 0xFF, 0},
	{0xA1D, 0xFF, 0xFF, 0},
	{0xA93, 0x1, 0x1, 0},
	{0xF8C, 0x15, 0x15, 0},
	{0xF8D, 0x5, 0x5, 0},
	{0x100B, 0x4, 0x4, 0},
	{0x1188, 0x0, 0x80, 0},
	{0x1190, 0x13, 0xFF, 0},
	{0x1198, 0x60, 0x70, 0},
	{0x140E, 0x8, 0x8, 0},
	{0x1889, 0x24, 0x2C, 0},
	{0x188A, 0x8, 0xC, 0},
	{0x188B, 0x2C, 0x3F, 0},
	{0x188C, 0x1E, 0x1E, 0},
	{0x188D, 0x9F, 0xFF, 0},
	{0x188E, 0x86, 0xFF, 0},
	{0x1890, 0xC, 0x3C, 0},
	{0x1891, 0x98, 0xD8, 0},
	{0x1892, 0x4, 0xF, 0},
	{0x1894, 0x24, 0x2C, 0},
	{0x1895, 0x8, 0xC, 0},
	{0x1896, 0x2C, 0x3F, 0},
	{0x1897, 0x1E, 0x1E, 0},
	{0x1898, 0x9F, 0xFF, 0},
	{0x1899, 0x86, 0xFF, 0},
	{0x189B, 0xC, 0x3C, 0},
	{0x189C, 0x98, 0xD8, 0},
	{0x189D, 0x4, 0xF, 0},
	{0x189F, 0x6, 0x2E, 0},
	{0x18A0, 0x8, 0x8, 0},
	{0x18A1, 0x2C, 0x3F, 0},
	{0x18A2, 0x1E, 0x1E, 0},
	{0x18A3, 0xDF, 0xFF, 0},
	{0x18A4, 0x86, 0xFF, 0},
	{0x18A7, 0x49, 0x59, 0},
	{0x18A8, 0x4, 0x7, 0},
	{0x18AC, 0x8, 0x98, 0},
	{0x18AD, 0x40, 0x40, 0},
	{0x18AE, 0x58, 0x7E, 0},
	{0x18AF, 0x1E, 0x1E, 0},
	{0x18B1, 0xFF, 0xFF, 0},
	{0x18B2, 0x82, 0xFF, 0},
	{0x18B3, 0x80, 0x80, 0},
	{0x18B4, 0x9, 0x19, 0},
	{0x18B5, 0x4, 0xF, 0},
	{0x18B7, 0x8, 0x98, 0},
	{0x18B8, 0x40, 0x40, 0},
	{0x18B9, 0x58, 0x7E, 0},
	{0x18BA, 0x1E, 0x1E, 0},
	{0x18BC, 0xFF, 0xFF, 0},
	{0x18BD, 0x82, 0xFF, 0},
	{0x18BE, 0x80, 0x80, 0},
	{0x18BF, 0x9, 0x19, 0},
	{0x18C0, 0x4, 0xF, 0},
	{0x1B0D, 0xF, 0xF, 0},
	{0x1B0E, 0x1, 0x1, 0},
	{0x1B10, 0xFF, 0xFF, 0},
	{0x1B13, 0xFF, 0xFF, 0},
	{0x1B16, 0xFF, 0xFF, 0},
	{0x1B2A, 0x8, 0x8, 0},
	{0x1B89, 0x0, 0x80, 0},
	{0x1B99, 0x0, 0x80, 0},
	{0x1BA9, 0x0, 0x80, 0},
	{0x1BB9, 0x0, 0x80, 0},
	{0x1BBE, 0x40, 0x40, 0},
	{0x1BC9, 0x0, 0x80, 0},
	{0x1BCE, 0x40, 0x40, 0},
	{0x1BD9, 0x0, 0x80, 0},
	{0x1BDE, 0x40, 0x40, 0},
	{0x1C09, 0x0, 0x80, 0},
	{0x1C1A, 0x0, 0x80, 0},
	{0x1C2B, 0x0, 0x80, 0},
	{0x1C3B, 0x0, 0x80, 0},
	{0x1C4B, 0x0, 0x80, 0},
	{0x1C5B, 0x0, 0x80, 0},
	{0x1C89, 0x0, 0x80, 0},
	{0x1C99, 0x0, 0x80, 0},
	{0x1CA9, 0x0, 0x80, 0},
	{0x1CB9, 0x0, 0x80, 0},
	{0x1CC9, 0x0, 0x80, 0},
	{0x1CD9, 0x0, 0x80, 0},
	{0x1D09, 0x0, 0x80, 0},
	{0x1D19, 0x0, 0x80, 0},
	{0x1D29, 0x0, 0x80, 0},
	{0x1D89, 0x0, 0x80, 0},
	{0x1D8C, 0x10, 0x7F, 0},
	{0x1D90, 0xF, 0x7F, 0},
	{0x1D91, 0x14, 0x7F, 0},
	{0x1DA1, 0x0, 0x80, 0},
	{0x1DA4, 0x28, 0x7F, 0},
	{0x1DA8, 0xF, 0x7F, 0},
	{0x1DA9, 0x1F, 0x7F, 0},
	{0x1DB9, 0x10, 0x7F, 0},
	{0x1DBB, 0x7F, 0x7F, 0},
	{0x1E09, 0x0, 0x80, 0},
	{0x1E0C, 0x10, 0x7F, 0},
	{0x1E10, 0xF, 0x7F, 0},
	{0x1E11, 0x1F, 0x7F, 0},
};

void pmic_cmp_register(struct seq_file *m)
{
#if INIT_SETTING_VERIFIED
	unsigned int i = 0;
	unsigned int val = 0;

	seq_puts(m, "cmp: PMIC_addr\tInit_Val\tRead_val\tMASK\tshift\n");

	for (i = 0; i < ARRAY_SIZE(init_setting); i++) {
		pmic_read_interface(
			init_setting[i].addr, &val,
			init_setting[i].mask, init_setting[i].shift);
		if (val != init_setting[i].val) {
			seq_printf(m, "cmp: 0x%x\tval=0x%x\trval=0x%x\t0x%x\t%d\n"
				   , init_setting[i].addr
				   , init_setting[i].val
				   , val
				   , init_setting[i].mask
				   , init_setting[i].shift);
		}
	}
#else
	seq_printf(m, "%s: disable cmp PMIC register with initial setting\n"
		   , __func__);
#endif
}

/*
 * PMIC reg dump log
 */
void pmic_dump_register(struct seq_file *m)
{
	const PMU_FLAG_TABLE_ENTRY *pFlag =
			&pmu_flags_table[PMU_COMMAND_MAX - 1];
	unsigned int i = 0;

	PMICLOG("dump PMIC register\n");

	for (i = 0; i < pFlag->offset; i = i + 5) {
		pr_notice("Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x\n"
			  , i, upmu_get_reg_value(i)
			  , i + 1, upmu_get_reg_value(i + 1)
			  , i + 2, upmu_get_reg_value(i + 2)
			  , i + 3, upmu_get_reg_value(i + 3)
			  , i + 4, upmu_get_reg_value(i + 4));
		if (m != NULL) {
			seq_printf(m,
				"Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x\n"
				, i, upmu_get_reg_value(i)
				, i + 1, upmu_get_reg_value(i + 1)
				, i + 2, upmu_get_reg_value(i + 2)
				, i + 3, upmu_get_reg_value(i + 3)
				, i + 4, upmu_get_reg_value(i + 4));
		}
	}
}

/*
 * PMIC dump exception status
 */

/* Kernel dump log */
void kernel_dump_exception_reg(void)
{
	/* 1.UVLO off */
	kernel_output_reg(MT6330_TOP_RST_STATUS);
	kernel_output_reg(MT6330_PONSTS);
	kernel_output_reg(MT6330_POFFSTS0);
	kernel_output_reg(MT6330_POFFSTS1);
	/* 2.thermal shutdown 150 */
	kernel_output_reg(MT6330_THERMALSTATUS);
	/* 3.power not good */
	kernel_output_reg(MT6330_PG_SDN_STS0);
	kernel_output_reg(MT6330_PG_SDN_STS1);
	kernel_output_reg(MT6330_PG_SDN_STS2);
	kernel_output_reg(MT6330_PG_SDN_STS3);
	/* 4.BUCK OC status */
	kernel_output_reg(MT6330_OC_SDN_STS0);
	kernel_output_reg(MT6330_OC_SDN_STS1);
	kernel_output_reg(MT6330_OC_SDN_STS2);
	kernel_output_reg(MT6330_BUCK_TOP_OC_CON0);
	/* 4.5 BUCK OC shutdown control */
	kernel_output_reg(MT6330_BUCK_TOP_ELR0);
	/* 5.long press shutdown */
	kernel_output_reg(MT6330_STRUP_CON4);
	/* 6.WDTRST */
	kernel_output_reg(MT6330_TOP_RST_MISC1);
	/* 7.CLK TRIM */
	kernel_output_reg(MT6330_TOP_CLK_TRIM);
}

/* Kernel & UART dump log */
void both_dump_exception_reg(struct seq_file *s)
{
	/* 1.UVLO off */
	both_output_reg(MT6330_TOP_RST_STATUS);
	both_output_reg(MT6330_PONSTS);
	both_output_reg(MT6330_POFFSTS0);
	both_output_reg(MT6330_POFFSTS1);
	/* 2.thermal shutdown 150 */
	both_output_reg(MT6330_THERMALSTATUS);
	/* 3.power not good */
	both_output_reg(MT6330_PG_SDN_STS0);
	both_output_reg(MT6330_PG_SDN_STS1);
	both_output_reg(MT6330_PG_SDN_STS2);
	both_output_reg(MT6330_PG_SDN_STS3);
	/* 4.BUCK OC status */
	both_output_reg(MT6330_OC_SDN_STS0);
	both_output_reg(MT6330_OC_SDN_STS1);
	both_output_reg(MT6330_OC_SDN_STS2);
	both_output_reg(MT6330_BUCK_TOP_OC_CON0);
	/* 4.5 BUCK OC shutdown control */
	both_output_reg(MT6330_BUCK_TOP_ELR0);
	/* 5.long press shutdown */
	both_output_reg(MT6330_STRUP_CON4);
	/* 6.WDTRST */
	both_output_reg(MT6330_TOP_RST_MISC1);
	/* 7.CLK TRIM */
	both_output_reg(MT6330_TOP_CLK_TRIM);
}

/* dump exception reg in kernel and clean status */
int pmic_dump_exception_reg(void)
{
	int ret_val = 0;

	kernel_dump_exception_reg();

	/* clear UVLO off */
	ret_val = pmic_set_register_value(PMIC_TOP_RST_STATUS_CLR, 0xFF);

	/* clear thermal shutdown 150 */
	ret_val = pmic_set_register_value(PMIC_RG_STRUP_THR_CLR, 0x1);
	udelay(200);
	ret_val = pmic_set_register_value(PMIC_RG_STRUP_THR_CLR, 0x0);

	/* clear power off status(POFFSTS) and PG status and BUCK OC status */
	ret_val = pmic_set_register_value(PMIC_RG_POFFSTS_CLR, 0x1);
	udelay(200);
	ret_val = pmic_set_register_value(PMIC_RG_POFFSTS_CLR, 0x0);

	/* clear Long press shutdown */
	ret_val = pmic_set_register_value(PMIC_CLR_JUST_RST, 0x1);
	udelay(200);
	ret_val = pmic_set_register_value(PMIC_CLR_JUST_RST, 0x0);
	udelay(200);
	PMICLOG(PMICTAG "[pmic_boot_status] JUST_PWRKEY_RST=0x%x\n",
		pmic_get_register_value(PMIC_JUST_PWRKEY_RST));

	/* clear WDTRSTB_STATUS */
	ret_val = pmic_set_register_value(PMIC_TOP_RST_MISC1_SET, 0x8);
	udelay(100);
	ret_val = pmic_set_register_value(PMIC_TOP_RST_MISC1_CLR, 0x8);

	/* clear BUCK OC */
	ret_val = pmic_config_interface(MT6330_BUCK_TOP_OC_CON0, 0xFF, 0xFF, 0);
	udelay(200);

	/* add mdelay for output the log in buffer */
	mdelay(500);

	return ret_val;
}
