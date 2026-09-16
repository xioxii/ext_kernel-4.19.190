/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MD_SYS1_PLATFORM_H__
#define __MD_SYS1_PLATFORM_H__

#include <linux/skbuff.h>
#include "ccif_hif_platform.h"
//#include "dt-bindings/basereg/mt6297-mt_reg_base.h"

#define MD_POWER_STATE_MASK	(1 << 0)
#define MD_SPM_BASE	(0x10006000)
#define MTCMOS_STA	(0x16C)

struct ccci_clk_node {
	struct clk *clk_ref;
	unsigned char *clk_name;
};

struct md_pll_reg {
	void __iomem *md_top_clkSW;

	void __iomem *md_boot_stats_select;
	void __iomem *md_boot_stats;
	void __iomem *md_coretracer_base;
};

struct md_hw_info {
	/* HW info - Register Address */
	unsigned long md_rgu_base;
	unsigned long md_boot_slave_Vector;
	unsigned long md_boot_slave_Key;
	unsigned long md_boot_slave_En;
	unsigned long ap_ccif_base;
	unsigned long md_ccif_base;
	unsigned int sram_size;
	void __iomem *md_ccif4_base;
	void __iomem *spm_sleep_base;
	void __iomem *ap_topclkgen_base;//[new]
	unsigned __iomem *ap_mixed_base;//[new]
	/* HW info - Interrupt ID */
	unsigned int ap_ccif_irq0_id;
	unsigned int ap_ccif_irq1_id;
	unsigned int md_wdt_irq_id;
	/* HW info - Interrupt flags */
	unsigned long ap_ccif_irq0_flags;
	unsigned long ap_ccif_irq1_flags;
	unsigned long md_wdt_irq_flags;
	void *hif_hw_info;
};

typedef enum{
	sta_power_on = 0,
	sta_power_off = 1
}STA_SPM_POWER;
int ccci_modem_remove(struct platform_device *dev);
void ccci_modem_shutdown(struct platform_device *dev);
int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
int ccci_modem_resume(struct platform_device *dev);
int ccci_modem_pm_suspend(struct device *device);
int ccci_modem_pm_resume(struct device *device);
int ccci_modem_pm_restore_noirq(struct device *device);
int md_cd_power_on(struct ccci_modem *md);
int md_cd_power_off(struct ccci_modem *md, unsigned int timeout);
int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode);
int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode);
int md_cd_let_md_go(struct ccci_modem *md);
void md_cd_lock_cldma_clock_src(int locked);
void md_cd_lock_modem_clock_src(int locked);
//int md_bootup_cleanup(struct ccci_modem *md, int success);//no used
int md_cd_io_remap_md_side_register(struct ccci_modem *md);
void md_dump_debug_register(struct ccci_modem *md);
void md_cd_dump_md_bootup_status(struct ccci_modem *md);
void md_cd_dump_debug_register(struct ccci_modem *md);
void md_cd_get_md_bootup_status(struct ccci_modem *md, unsigned int *buff, int length);
void md_cd_check_emi_state(struct ccci_modem *md, int polling);
void md_cldma_hw_reset(unsigned char md_id);
//int md_get_mdsys1_hw_info(struct platform_device *dev_ptr, struct ccci_dev_cfg *dev_cfg, struct ccci_modem *md);
int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info);
int md_cd_vcore_config(unsigned int md_id, unsigned int hold_req);
int md_start_platform(struct ccci_modem *md);
//void md_pccom_verifty(int prt_count);//just in md_sys1_platform.c
int md_detect_coretracer(int md_id);// in CONFIG_MTK_EX_FLOW

int md_cd_check_md_power_off(struct ccci_modem *md);

/* ADD_SYS_CORE */
int ccci_modem_syssuspend(void);
void ccci_modem_sysresume(void);

extern void __iomem *infra_ao_base;
// @mt6297 {
extern void __iomem *infra_ao_mem_base;
// @mt6297 }
extern unsigned int devapc_check_flag;
extern void ccci_mem_dump(int md_id, void *start_addr, int len);
#endif				/* __MD_SYS1_PLATFORM_H__ */
