/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __DRV_CLKDBG_MT6880_H
#define __DRV_CLKDBG_MT6880_H

enum dbg_sys_id {
	top,
	dbgsys_dem,
	ifrao,
	infracfg_ao_bus,
	peri,
	spm,
	apmixed,
	gce,
	audsys,
	impe,
	mfgcfg,
	mm,
    dbg_sys_num,
};

extern void subsys_if_on(void);

extern unsigned int mt_get_ckgen_freq(unsigned int ID);

/*ram console api*/
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_clk(int id, u32 val);
#endif

extern const char * const *get_mt6880_all_clk_names(void);
extern void print_enabled_clks_once(void);
extern void print_subsys_reg(enum dbg_sys_id id);

#endif	/* __DRV_CLKDBG_MT6758_H */
