/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "ccci_config.h"
#include <linux/clk.h>
/* #include <mach/mtk_pbm.h> */
#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif
//#include "pmic_api_buck.h"
//#include <mt-plat/upmu_common.h>
//#include <mtcmos_ctrl.h>
#include <linux/pm_runtime.h>
#ifdef VCORE_READY
#include <linux/soc/mediatek/mtk-pm-qos.h>
#endif
#include <linux/regulator/consumer.h> /* for MD PMIC */

#include "ccci_core.h"
#include "ccci_platform.h"
#include "md_sys1_platform.h"
#include "ccci_hif.h"
#include "modem_secure_base.h"
#include "ap_md_reg_dump.h"
#include "modem_reg_base.h"
#include "mtk_pmic_api_buck.h"

static struct ccci_clk_node clk_table[] = {
	{ NULL, "scp-sys-md1-main"},
	{ NULL, "infra-ccif-ap"},
	{ NULL, "infra-ccif-md"},
	{ NULL, "infra-ccif1-ap"},
	{ NULL, "infra-ccif1-md"},
};

unsigned int devapc_check_flag = 0;
#define TAG "mcd"

#define ROr2W(a, b, c) ccci_write32(a, b, (ccci_read32(a, b)|c))
#define RAnd2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)&c))
#define RabIsc(a, b, c) ((ccci_read32(a, b)&c) != c)

void md_cldma_hw_reset(unsigned char md_id)
{
}

int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	struct device_node *node = NULL;
	int idx = 0;
	int retval;

	if (dev_ptr->dev.of_node == NULL) {
		CCCI_ERROR_LOG(0, TAG, "modem OF node NULL\n");
		return -1;
	}

	memset(dev_cfg, 0, sizeof(struct ccci_dev_cfg));
	of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,md_id", &dev_cfg->index);
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"modem hw info get idx:%d\n", dev_cfg->index);
	if (!get_modem_is_enabled(dev_cfg->index)) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"modem %d not enable, exit\n", dev_cfg->index + 1);
		return -1;
	}

	switch (dev_cfg->index) {
	case 0:		/* MD_SYS1 */
		dev_cfg->major = 0;
		dev_cfg->minor_base = 0;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,modem_capability", &dev_cfg->capability);
		CCCI_BOOTUP_LOG(dev_cfg->index, TAG, "pccif:%x\n", MD_PCORE_PCCIF_BASE);
		hw_info->ap_ccif_base =
		(unsigned long)of_iomap(dev_ptr->dev.of_node, 0);
		hw_info->md_ccif_base =
		(unsigned long)of_iomap(dev_ptr->dev.of_node, 1);
		/* MD excption irq */
		hw_info->ap_ccif_irq0_id =
			irq_of_parse_and_map(dev_ptr->dev.of_node, 0);
		hw_info->ap_ccif_irq1_id =
			irq_of_parse_and_map(dev_ptr->dev.of_node, 1);
		hw_info->md_wdt_irq_id =
			irq_of_parse_and_map(dev_ptr->dev.of_node, 2);
		hw_info->ap_ccif_irq0_flags = IRQF_TRIGGER_NONE;
		hw_info->ap_ccif_irq1_flags = IRQF_TRIGGER_NONE;
		hw_info->md_wdt_irq_flags = IRQF_TRIGGER_NONE;
		hw_info->sram_size = CCIF_SRAM_SIZE;
		hw_info->md_rgu_base = MD_RGU_BASE;
		hw_info->md_boot_slave_En = MD_BOOT_VECTOR_EN;

		CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
			"get_hw_info:md_wdt_irq %d\n", hw_info->md_wdt_irq_id);

		for (idx = 0; idx < ARRAY_SIZE(clk_table); idx++) {
			clk_table[idx].clk_ref = devm_clk_get(&dev_ptr->dev,
				clk_table[idx].clk_name);
			if (IS_ERR(clk_table[idx].clk_ref)) {
				CCCI_ERROR_LOG(dev_cfg->index, TAG,
					 "md%d get %s failed\n",
						dev_cfg->index + 1,
						clk_table[idx].clk_name);
				clk_table[idx].clk_ref = NULL;
			}
		}
		node = of_find_compatible_node(NULL, NULL,
			"mediatek,apmixed");
		hw_info->ap_mixed_base = of_iomap(node, 0);
		node = of_find_compatible_node(NULL, NULL,
					"mediatek,topckgen");
		if (node)
			hw_info->ap_topclkgen_base = of_iomap(node, 0);

		else {
			CCCI_ERROR_LOG(-1, TAG,
				"%s:ioremap topclkgen base address fail\n",
				__func__);
			return -1;
		}

		break;
	default:
		return -1;
	}

	if (hw_info->ap_ccif_base == 0 ||
		hw_info->md_ccif_base == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"ap_ccif_base:0x%p, md_ccif_base:0x%p\n",
			(void *)hw_info->ap_ccif_base,
			(void *)hw_info->md_ccif_base);
		return -1;
	}
	if (hw_info->ap_ccif_irq0_id == 0 ||
		hw_info->ap_ccif_irq1_id == 0 ||
		hw_info->md_wdt_irq_id == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"ccif_irq0:%d,ccif_irq0:%d,md_wdt_irq:%d\n",
			hw_info->ap_ccif_irq0_id, hw_info->ap_ccif_irq1_id,
			hw_info->md_wdt_irq_id);
		return -1;
	}

	/* Get spm sleep base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	hw_info->spm_sleep_base = of_iomap(node, 0);
	if (!hw_info->spm_sleep_base) {
		CCCI_ERROR_LOG(0, TAG,
			"%s: spm_sleep_base of_iomap failed\n",
			__func__);
		return -1;
	}
	CCCI_INIT_LOG(-1, TAG, "spm_sleep_base:0x%lx\n",
			(unsigned long)hw_info->spm_sleep_base);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"dev_major:%d,minor_base:%d,capability:%d\n",
		dev_cfg->major, dev_cfg->minor_base, dev_cfg->capability);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ap_ccif_base:0x%p, md_ccif_base:0x%p\n",
					(void *)hw_info->ap_ccif_base,
					(void *)hw_info->md_ccif_base);
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ccif_irq0:%d,ccif_irq1:%d,md_wdt_irq:%d\n",
		hw_info->ap_ccif_irq0_id, hw_info->ap_ccif_irq1_id,
		hw_info->md_wdt_irq_id);
	pm_runtime_enable(&dev_ptr->dev);
	dev_pm_syscore_device(&dev_ptr->dev, true);

	CCCI_BOOTUP_LOG(dev_cfg->index, TAG, "md mtcmos pm get start\n");


	// LK already turn on MD, but SPM LK and kernel not sync,
	// So it needs ccci to turn on MD again to let SPM get/put status sync
	// and already make sure MD has no problem when power on MD twice
	retval = pm_runtime_get_sync(&dev_ptr->dev);
	if (retval)
		CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
			"md mtcmos pm getfail: ret = %d\n", retval);

	CCCI_BOOTUP_LOG(dev_cfg->index, TAG, "md mtcmos pm get done\n");

	return 0;
}

/* md1 sys_clk_cg no need set in this API*/
void ccci_set_clk_cg(struct ccci_modem *md, unsigned int on)
{
	int idx = 0;
	int ret = 0;

	CCCI_NORMAL_LOG(md->index, TAG, "%s: on=%d\n", __func__, on);
	for (idx = 1; idx < ARRAY_SIZE(clk_table); idx++) {
		if (clk_table[idx].clk_ref == NULL)
			continue;
		if (on) {
			ret = clk_prepare_enable(clk_table[idx].clk_ref);
			if (ret)
				CCCI_ERROR_LOG(md->index, TAG,
					"%s: on=%d,ret=%d\n",
					__func__, on, ret);
			devapc_check_flag = 1;
		} else {
			devapc_check_flag = 0;
			clk_disable_unprepare(clk_table[idx].clk_ref);
		}
	}
}
int md_cd_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_pll_reg *md_reg = NULL;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	/* call internal_dump io_remap */
	md_io_remap_internal_dump_register(md);

	md_info->md_boot_slave_En =
			ioremap_nocache(md->hw_info->md_boot_slave_En, 0x4);
	md_info->md_rgu_base =
			ioremap_nocache(md->hw_info->md_rgu_base, 0x300);

	md_reg = kzalloc(sizeof(struct md_pll_reg), GFP_KERNEL);
	if (md_reg == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc md reg map mem fail\n", __func__);
		return -1;
	}

	md_reg->md_boot_stats_select =
			ioremap_nocache(MD1_BOOT_STATS_SELECT, 4);
	md_reg->md_boot_stats = ioremap_nocache(MD1_CFG_BOOT_STATS, 4);
	/*just for dump end*/

	md_info->md_pll_base = md_reg;

#ifdef MD_PEER_WAKEUP
	md_info->md_peer_wakeup = ioremap_nocache(MD_PEER_WAKEUP, 0x4);
#endif
	return 0;
}

void md_cd_lock_cldma_clock_src(int locked)
{
	/* spm_ap_mdsrc_req(locked); */
}

void md_cd_lock_modem_clock_src(int locked)
{
	struct arm_smccc_res res;
	unsigned int settle;
	unsigned int ret;
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
			MD_REG_AP_MDSRC_REQ,
			locked, 0, 0, 0, 0, &res);

	if (locked) {
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL,
				MD_CLOCK_REQUEST,
				MD_REG_AP_MDSRC_SETTLE,
				0, 0, 0, 0, 0, &res);
		settle = res.a0;

		mdelay(settle);
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL,
				MD_CLOCK_REQUEST,
				MD_REG_AP_MDSRC_ACK,
				0, 0, 0, 0, 0, &res);
		ret = res.a0;

		CCCI_NOTICE_LOG(-1, TAG,
				"settle = %u; ret = %u\n", settle, ret);
	}
}

void md_cd_dump_md_bootup_status(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	/*To avoid AP/MD interface delay,
	 * dump 3 times, and buy-in the 3rd dump value.
	 */

	ccci_write32(md_reg->md_boot_stats_select, 0, 0);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats0:0x%X\n",
		ccci_read32(md_reg->md_boot_stats, 0));

	ccci_write32(md_reg->md_boot_stats_select, 0, 1);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats1:0x%X\n",
		ccci_read32(md_reg->md_boot_stats, 0));
}

void md_cd_get_md_bootup_status(
	struct ccci_modem *md, unsigned int *buff, int length)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	CCCI_NOTICE_LOG(md->index, TAG, "md_boot_stats len %d\n", length);

	if (length < 2 || buff == NULL) {
		md_cd_dump_md_bootup_status(md);
		return;
	}

	ccci_write32(md_reg->md_boot_stats_select, 0, 2);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	buff[0] = ccci_read32(md_reg->md_boot_stats, 0);

	ccci_write32(md_reg->md_boot_stats_select, 0, 3);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	buff[1] = ccci_read32(md_reg->md_boot_stats, 0);
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats0 / 1:0x%X / 0x%X\n", buff[0], buff[1]);

}


static int dump_emi_last_bm(struct ccci_modem *md)
{
	u32 buf_len = 1024;
	char *buf = NULL;

	buf = kzalloc(buf_len, GFP_ATOMIC);
	if (!buf) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"alloc memory failed for emi last bm\n");
		return -1;
	}

	//dump_last_bm(buf, buf_len);
	CCCI_MEM_LOG_TAG(md->index, TAG, "Dump EMI last bm\n");
	ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, buf, buf_len);

	kfree(buf);

	return 0;
}

#define PCCOM_ADD 0x20291E50
void md_pccom_verifty(int prt_count)
{
	void __iomem *pccon_vradd = ioremap_nocache(PCCOM_ADD, 0x4);
	while(prt_count-- > 0){
		CCCI_DEBUG_LOG(MD_SYS1, TAG, "MD PC: 0x%x \n", ccci_read32(pccon_vradd, 0x0));
		mdelay(50);
	}
}

int md_detect_coretracer(int md_id)
{
	int ret = 0;
	struct ccci_modem *md = (struct ccci_modem *)ccci_md_get_modem_by_id(md_id);
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;
	int coretracer_connect_status = 0;

	coretracer_connect_status = ccci_read32(md_reg->md_coretracer_base, 0x00);
	if ((coretracer_connect_status >> 9) & 0x07) {
		ret = 1;
	}

	return ret;
}

void __weak dump_emi_outstanding(void)
{
	CCCI_DEBUG_LOG(-1, TAG, "No %s\n", __func__);
}

void md_cd_dump_debug_register(struct ccci_modem *md)
{
	/* MD no need dump because of bus hang happened - open for debug */
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[
		CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)] = { 0 };

	/*dump_emi_latency();*/
	dump_emi_outstanding();
	dump_emi_last_bm(md);
	md_cd_get_md_bootup_status(md, reg_value, 2);
	md->ops->dump_info(md, DUMP_FLAG_CCIF, ccif_sram, 0);
	/* copy from HS1 timeout */
	if ((reg_value[0] == 0) && (ccif_sram[1] == 0)) {
		CCCI_NORMAL_LOG(-1, TAG, "%s, MD boot status fail\n",__func__);
		return;
	} else if (!((reg_value[0] == 0x5443000C) || (reg_value[0] == 0) ||
		(reg_value[0] >= 0x53310000 && reg_value[0] <= 0x533100FF))) {
		CCCI_NORMAL_LOG(-1, TAG, "%s, MD BROM fail\n",__func__);
		return;
	}

	md_cd_lock_modem_clock_src(1);

	internal_md_dump_debug_register(md->index);
	md_cd_lock_modem_clock_src(0);
}

void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}
/*
void __weak vmd1_pmic_setting_on(void){
   CCCI_NORMAL_LOG(-1, TAG, "%s is not defined\n",__func__);
}
void __weak vmd1_pmic_setting_off(void){
   CCCI_NORMAL_LOG(-1, TAG, "%s is not defined\n",__func__);
}*/

/* callback for system power off*/
void ccci_power_off(void)
{
	//vmd1_pmic_setting_on();
}

/*
void __attribute__((weak)) kicker_pbm_by_md(enum pbm_kicker kicker, bool status)
{
}
*/

int md_cd_vcore_config(unsigned int md_id, unsigned int hold_req)
{
	int ret = 0;
#ifdef VCORE_READY
	static int is_hold;
	static struct mtk_pm_qos_request md_qos_vcore_request;

	CCCI_BOOTUP_LOG(md_id, TAG,
		"%s: is_hold=%d, hold_req=%d\n",
		__func__, is_hold, hold_req);
	if (hold_req && is_hold == 0) {
		mtk_pm_qos_add_request(&md_qos_vcore_request,
		MTK_PM_QOS_VCORE_OPP, VCORE_OPP_0);
		is_hold = 1;
	} else if (hold_req == 0 && is_hold) {
		mtk_pm_qos_remove_request(&md_qos_vcore_request);
		is_hold = 0;
	} else
		CCCI_ERROR_LOG(md_id, TAG,
			"invalid hold_req: is_hold=%d, hold_req=%d\n",
			is_hold, hold_req);

	if (ret)
		CCCI_ERROR_LOG(md_id, TAG,
			"%s fail: ret=%d, hold_req=%d\n",
			__func__, ret, hold_req);
  #endif
	return ret;
}

int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode)
{
	return 0;
}

int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode)
{
	return 0;
}

int md_start_platform(struct ccci_modem *md)
{
	struct arm_smccc_res res;
	int timeout = 100; /* 100 * 20ms = 2s */
	int ret = -1;

	if ((md->per_md_data.config.setting&MD_SETTING_FIRST_BOOT) == 0)
		return 0;

	while (timeout > 0) {
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
				MD_CHECK_DONE,
				0, 0, 0, 0, 0, &res);
		ret = res.a0;
		if (!ret) {
			CCCI_BOOTUP_LOG(md->index, TAG, "BROM PASS\n");
			break;
		}
		timeout--;
		msleep(20);
	}

	CCCI_BOOTUP_LOG(md->index, TAG, "dummy md sys clk\n");
//	clk_prepare_enable(clk_table[0].clk_ref); /* match lk on */
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_CHECK_FLAG, 0, 0, 0, 0, 0, &res);
	CCCI_NORMAL_LOG(md->index, TAG,
			"flag_1=%lu, flag_2=%lu, flag_3=%lu, flag_4=%lu\n",
			res.a0, res.a1, res.a2, res.a3);

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_BOOT_STATUS, 0, 0, 0, 0, 0, &res);
	CCCI_BOOTUP_LOG(md->index, TAG,
			"AP: boot_ret=%lu, boot_status_0=%lX, boot_status_1=%lX\n",
			res.a0, res.a1, res.a2);
	CCCI_NORMAL_LOG(md->index, TAG,
			"AP: boot_ret=%lu, boot_status_0=%lX, boot_status_1=%lX\n",
			res.a0, res.a1, res.a2);
	if (ret != 0) {
		/* BROM */
		CCCI_ERROR_LOG(md->index, TAG, "BROM Failed\n");
	}

	CCCI_NORMAL_LOG(md->index, TAG, "start md_cd_power_off\n");
	md_cd_power_off(md, 0);
	CCCI_NORMAL_LOG(md->index, TAG, "md_cd_power_off end\n");
	return ret;
}

static int mtk_ccci_cfg_srclken_o1_on(struct ccci_modem *md)
{
	unsigned int val;
	struct md_hw_info *hw_info = md->hw_info;

	if (hw_info->spm_sleep_base) {
		ccci_write32(hw_info->spm_sleep_base, 0, 0x0B160001);
		val = ccci_read32(hw_info->spm_sleep_base, 0);
		CCCI_INIT_LOG(-1, TAG, "spm_sleep_base: val:0x%x\n", val);

		val = ccci_read32(hw_info->spm_sleep_base, 8);
		CCCI_INIT_LOG(-1, TAG, "spm_sleep_base+8: val:0x%x +\n", val);
		val |= 0x1<<21;
		ccci_write32(hw_info->spm_sleep_base, 8, val);
		val = ccci_read32(hw_info->spm_sleep_base, 8);
		CCCI_INIT_LOG(-1, TAG, "spm_sleep_base+8: val:0x%x -\n", val);
	}
	return 0;
}

int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;
	unsigned int reg_value;

	/* step 1: PMIC setting */
	CCCI_NOTICE_LOG(md->index, TAG, "vmd1_pmic_setting_on");
	vmd1_pmic_setting_on();

	/* step 2: MD srcclkena setting */
	reg_value = ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA);
	reg_value &= ~(0xFF);
	reg_value |= 0x21;
	ccci_write32(infra_ao_base, INFRA_AO_MD_SRCCLKENA, reg_value);
	CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_srcclkena bit(0x1000_0F0C)=0x%x\n",
			__func__, ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA));
	mtk_ccci_cfg_srclken_o1_on(md);

	/* step 3: power on MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
		CCCI_BOOTUP_LOG(md->index, TAG, "enable md sys clk\n");
		/* ret = clk_prepare_enable(clk_table[0].clk_ref); */
		pm_runtime_get_sync(&md->plat_dev->dev);
		CCCI_BOOTUP_LOG(md->index, TAG, "enable md sys clk done,ret = %d\n", ret);
		/* kicker_pbm_by_md(KR_MD1, true); */
		/*/ CCCI_BOOTUP_LOG(md->index, TAG, "Call end kicker_pbm_by_md(0,true)\n"); */
	break;
	}
	if (ret)
		return ret;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 1, 0);
#endif
	return 0;
}

int md_bootup_cleanup(struct ccci_modem *md, int success)
{
	return 0;
}

int md_cd_let_md_go(struct ccci_modem *md)
{
	struct arm_smccc_res res;

	if (MD_IN_DEBUG(md))
		return -1;
	CCCI_BOOTUP_LOG(md->index, TAG, "set MD boot slave\n");
	md_pccom_verifty(1);
	/* make boot vector take effect */
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
			MD_KERNEL_BOOT_UP, 0, 0, 0, 0, 0, &res);
	CCCI_BOOTUP_LOG(md->index, TAG,
			"MD: boot_ret=%lu, boot_status_0=%lu, boot_status_1=%lu\n",
			res.a0, res.a1, res.a2);
	md_pccom_verifty(5);
	return 0;
}

int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
	/* struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data; */
	int ret = 0;
	unsigned int reg_value;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 0, 0);
#endif
	/* power off MD_INFRA and MODEM_TOP */
	switch (md->index) {
	case MD_SYS1:
		pm_runtime_put_sync(&md->plat_dev->dev);
		CCCI_BOOTUP_LOG(md->index, TAG, "power off md1\n");
		reg_value = ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA);
		reg_value &= ~(0xFF);
		ccci_write32(infra_ao_base, INFRA_AO_MD_SRCCLKENA, reg_value);
		CCCI_BOOTUP_LOG(md->index, CORE,
			"%s: set md1_srcclkena=0x%x\n", __func__,
			ccci_read32(infra_ao_base, INFRA_AO_MD_SRCCLKENA));
		CCCI_BOOTUP_LOG(md->index, TAG, "Call vmd1_pmic_setting_off\n");
		vmd1_pmic_setting_off();

		ret = md_cd_check_md_power_off(md);
		CCCI_NORMAL_LOG(md->index, TAG,
				"md_cd_check_md_power_off() return %d\n", ret);

//		kicker_pbm_by_md(KR_MD1, false);
//		CCCI_BOOTUP_LOG(md->index, TAG,
//			"Call end kicker_pbm_by_md(0,false)\n");
		break;
	}
	return ret;
}

int md_cd_check_md_power_off(struct ccci_modem *md) {
	int ret = -1;  // return 0 for normal, -1 when timeout

	int cnt = 500; /*MD power off timeout is 5s*/
	int time_once = 10;
	static void __iomem *scpsys_base, *pwr_sta;
	unsigned int md_power_state;
	u32 val = 0;

	while (cnt > 0) {

			scpsys_base = ioremap(MD_SPM_BASE, PAGE_SIZE);
			pwr_sta = scpsys_base + MTCMOS_STA;
			val = readl(pwr_sta);
			CCCI_NORMAL_LOG(md->index, TAG,
					"SPM_BASE_MTCOMOS_STATE reg_value=0x%X\n", val);

			md_power_state =  val & MD_POWER_STATE_MASK;
			if (md_power_state) {
				CCCI_NORMAL_LOG(md->index, TAG,
						"poll md power state=0x%X reg=0x%X cnt:%d\n",
						md_power_state, val, cnt);
			} else {
				ret = 0;
				break;
			}
			msleep(time_once);
			cnt--;
	}
	return ret;
}

int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

void ccci_modem_shutdown(struct platform_device *dev)
{
}

int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

int ccci_modem_resume(struct platform_device *dev)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

int ccci_modem_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

int ccci_modem_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_resume(pdev);
}

int ccci_modem_pm_restore_noirq(struct device *device)
{
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;

	/* set flag for next md_start */
	md->per_md_data.config.setting |= MD_SETTING_RELOAD;
	md->per_md_data.config.setting |= MD_SETTING_FIRST_BOOT;
	/* restore IRQ */
	return 0;
}

void ccci_modem_restore_reg(struct ccci_modem *md)
{
}

int ccci_modem_syssuspend(void)
{
	CCCI_DEBUG_LOG(0, TAG, "%s\n", __func__);
	return 0;
}

void ccci_modem_sysresume(void)
{
	struct ccci_modem *md;

	CCCI_DEBUG_LOG(0, TAG, "%s\n", __func__);
	md = ccci_md_get_modem_by_id(0);
	if (md != NULL)
		ccci_modem_restore_reg(md);
}
