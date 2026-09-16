// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yuesheng Wang <yuesheng.wang@mediatek.com>
 */

#include <linux/io.h>
#include <linux/uaccess.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#include "mtk_disp_aal.h"

static struct AalInfo aal_info[2];

static struct AALReg aal_reg[2];
static unsigned int aal_tuning_flag[2];

void __iomem *get_aal_base_addr_by_index(int aal_idx)
{
	return aal_info[aal_idx].base_addr;
}

irqreturn_t mtk_disp_aal_irq_handler(int irq, void *dev_id)
{
	int i;
	int aal_index;
	void __iomem *base_addr;

	for (i = 0; i < 2; i++) {
		if (aal_info[i].irq == (unsigned int)irq)
			break;
	}

	if (i > 1)
		i = 0;

	aal_index = i;
	base_addr = aal_info[i].base_addr;

	/* Clear frame completion interrupt */
	writel(0x0, base_addr + DISP_AAL_INTSTA);

	mtk_aal_update_max_hist(aal_index);

	update_color_hist(aal_index);

	mtk_aal_config(aal_index);

	return IRQ_HANDLED;
}

void mtk_aal_update_max_hist(int aal_idx)
{
	int i;
	void __iomem *base_addr = get_aal_base_addr_by_index(aal_idx);

	for (i = 0; i < 33; i++)
		aal_info[aal_idx].max_hist[i] =
			readl(base_addr + DISP_AAL_STATUS_00 + 4 * i);

	aal_info[aal_idx].has_hist = 1;
}

void update_color_hist(int aal_idx)
{
	aal_info[aal_idx].colorHist = 0;
}

void mtk_aal_mask(void __iomem *base_addr, u32 offset, u32 mask, u32 data)
{
	u32 tmp = readl(base_addr + (unsigned long)offset);

	writel((tmp & ~mask) | (data & mask),
			base_addr + (unsigned long)offset);
}

void mtk_aal_set_event(int aal_idx, int event)
{
	void __iomem *base_addr = get_aal_base_addr_by_index(aal_idx);
	static int old_event[2];

	old_event[aal_idx] =  event;

	if (event != 0)
		writel(0x2, base_addr + DISP_AAL_INTEN);
	else
		writel(0x2, base_addr + DISP_AAL_INTEN);

	/* need trigger refresh? */
}

void mtk_aal_function(int aal_idx)
{
	unsigned int func = aal_info[aal_idx].function;
	void __iomem *base_addr = get_aal_base_addr_by_index(aal_idx);

	pr_info("aal%d: mtk_aa_function = %d\n", aal_idx, func);

	mtk_aal_mask(base_addr, 0x20,
			(unsigned int)BIT(8U) | (unsigned int)BIT(9U),
			(unsigned int)BIT(8U) | (unsigned int)BIT(9U));
	if (func == AAL_FUNC_ALL_BYPASS) {
		/* all engine bypass */
		writel(1, base_addr + DISP_AAL_CFG_MAIN);
		mtk_aal_mask(base_addr, DISP_AAL_CABC_00,
				(unsigned int)BIT(31U), 0);
		mtk_aal_mask(base_addr, DISP_AAL_DRE_MAPPING_00,
				(unsigned int)BIT(4U), (unsigned int)BIT(4U));
	} else if (func == AAL_FUNC_CABC) {
		/* enable cabc and disable dre */
		writel(0, base_addr + DISP_AAL_CFG_MAIN);
		mtk_aal_mask(base_addr, DISP_AAL_CABC_00,
				(unsigned int)BIT(31U),
				(unsigned int)BIT(31U));
		mtk_aal_mask(base_addr, DISP_AAL_DRE_MAPPING_00,
				(unsigned int)BIT(4U), (unsigned int)BIT(4U));
	} else if (func == AAL_FUNC_DRE) {
		writel(0, base_addr + DISP_AAL_CFG_MAIN);
		/* enable dre and disable cabc */
		mtk_aal_mask(base_addr, DISP_AAL_CABC_00,
				(unsigned int)BIT(31U), 0);
		mtk_aal_mask(base_addr, DISP_AAL_DRE_MAPPING_00,
				(unsigned int)BIT(4U), 0);
	} else if (func == AAL_FUNC_CABC_DRE) {
		writel(0, base_addr + DISP_AAL_CFG_MAIN);
		/* enable cabc and dre */
		mtk_aal_mask(base_addr, DISP_AAL_CABC_00,
				(unsigned int)BIT(31U),
				(unsigned int)BIT(31U));
		mtk_aal_mask(base_addr, DISP_AAL_DRE_MAPPING_00,
				(unsigned int)BIT(4U), 0);
	} else {
		/* don't support */
	}
}

#define CABC_GAINLMT(v0, v1, v2) (((v2) << 20U) | ((v1) << 10U) | (v0))

#define DRE_REG_2(v0, off0, v1, off1) (((v1) << (off1)) | ((v0) << (off0)))
#define DRE_REG_3( \
		v0, off0, v1, off1, v2, off2) (((v2) << \
		(off2)) | ((v1) << (off1)) | ((v0) << (off0)))

void mtk_aal_set_cabc_gain_lmt(int aal_idx)
{
	int i, j;
	unsigned int tmp;
	unsigned int *gain = aal_info[aal_idx].cabc_gainlmt;
	void __iomem *base_addr = get_aal_base_addr_by_index(aal_idx);

	j = 0;
	for (i = 0; i < AAL_HIST_BIN / 3; i++) {
		tmp = CABC_GAINLMT(gain[j], gain[j + 1], gain[j + 2]);
		writel(tmp, base_addr + DISP_AAL_CABC_GAINLMT_TBL(i));
		j += 3;
	}
}


void mtk_aal_set_dre_gain_flt_status(int aal_idx)
{
	unsigned int *gain = aal_info[aal_idx].dre_gain_flt_status;
	unsigned int val;
	void __iomem *base_addr = get_aal_base_addr_by_index(aal_idx);
	/* dre_gain_force_en */
	mtk_aal_mask(base_addr, DISP_AAL_DRE_GAIN_FILTER_00,
			(unsigned int)BIT(8U), (unsigned int)BIT(8U));

	val = DRE_REG_2(gain[0], 0U, gain[1], 12U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(0));

	val = DRE_REG_2(gain[2], 0U, gain[3], 12U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(1));

	val = DRE_REG_2(gain[4], 0U, gain[5], 11U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(2));

	val = DRE_REG_3(gain[6], 0U, gain[7], 11U, gain[8], 21U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(3));

	val = DRE_REG_3(gain[9], 0U, gain[10], 10U, gain[11], 20U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(4));

	val = DRE_REG_3(gain[12], 0U, gain[13], 10U, gain[14], 20U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(5));

	val = DRE_REG_3(gain[15], 0U, gain[16], 10U, gain[17], 20U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(6));

	val = DRE_REG_3(gain[18], 0U, gain[19], 9U, gain[20], 18U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(7));

	val = DRE_REG_3(gain[21], 0U, gain[22], 9U, gain[23], 18U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(8));

	val = DRE_REG_3(gain[24], 0U, gain[25], 9U, gain[26], 18U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(9));

	val = DRE_REG_2(gain[27], 0U, gain[28], 9U);
	writel(val, base_addr + DISP_AAL_DRE_FLT_FORCE(10));

}

void mtk_aal_set_cabc_fltgain_force(int aal_idx)
{
	unsigned int cabc_fltgain_force;
	void __iomem *base_addr = get_aal_base_addr_by_index(aal_idx);

	cabc_fltgain_force = aal_info[aal_idx].cabc_fltgain_force;

	mtk_aal_mask(base_addr, DISP_AAL_CABC_00,
			(unsigned int)(1UL << 31U), (unsigned int)(1UL << 31U));
	mtk_aal_mask(base_addr, DISP_AAL_CABC_02,
			(unsigned int)((1UL << 26U) | cabc_fltgain_force),
			(unsigned int)((1UL << 26U) | 0x3ffU));
}

unsigned int mtk_get_color_hist(int aal_idx)
{
	/* don't use now */
	return aal_info[aal_idx].colorHist;
}

#include <linux/delay.h>
int mtk_aal_get_max_hist(int aal_idx, __user void *arg)
{
	unsigned long ret;
	static int loop;

	loop = 0;
	while (aal_info[aal_idx].has_hist == 0U) {
		udelay(1000UL);
		if (loop++ > 100) {
			pr_info("Get aal hist fail.\n");
			return -1;
		}
	}

	if (aal_info[aal_idx].has_hist != 0U) {
		DISP_AAL_HIST aal_hist;
		void *p;

		aal_hist.backlight = 0;
		aal_hist.requestPartial = 0;
		aal_hist.serviceFlags = 1;
		aal_hist.colorHist = mtk_get_color_hist(aal_idx);
		aal_hist.tuningFlag = aal_tuning_flag[aal_idx];
		p = memcpy(aal_hist.maxHist, aal_info[aal_idx].max_hist,
				33UL * sizeof(aal_info[aal_idx].max_hist[0]));
		ret = copy_to_user(arg, &aal_hist, sizeof(DISP_AAL_HIST));
	}

	aal_info[aal_idx].has_hist = 0U;
	return 0;
}

void mtk_aal_request_partial(int aal_idx)
{
	/* don't use now */
}

unsigned int mtk_aal_get_width(int aal_idx)
{
	void __iomem *base_addr = get_aal_base_addr_by_index(aal_idx);

	return readl(base_addr + DISP_AAL_SIZE) & 0xfffU;
}

unsigned int mtk_aal_get_height(int aal_idx)
{
	void __iomem *base_addr = get_aal_base_addr_by_index(aal_idx);

	return (readl(base_addr + DISP_AAL_SIZE) & 0xfff0000U) >> 16U;
}

int mtk_aal_get_resolution(int aal_idx, __user void *arg)
{
	unsigned long ret;
	unsigned int r;

	r = (mtk_aal_get_width(aal_idx) << 16U) |
		mtk_aal_get_height(aal_idx);
	ret = copy_to_user(arg, &r, sizeof(r));

	return 0;
}

int mtk_aal_eventctl(int aal_idx, __user void *arg)
{
	int enabled;

	if (copy_from_user(&enabled, (void *)arg,
				(unsigned long)sizeof(enabled)) != 0UL) {
		pr_info("DISP_IOCTL_AAL_EVENTCTL: copy_from_user() failed");
		return -EFAULT;
	}

	mtk_aal_set_event(aal_idx, enabled);

	return 0;
}

int mtk_aal_set_param(int aal_idx, __user void *arg)
{
	DISP_AAL_PARAM aal_param;
	unsigned long ret;
	void *p;

	ret = copy_from_user(&aal_param, arg,
			(unsigned long)sizeof(DISP_AAL_PARAM));
	aal_info[aal_idx].cabc_fltgain_force = aal_param.cabc_fltgain_force;
	p = memcpy(aal_info[aal_idx].cabc_gainlmt, aal_param.cabc_gainlmt,
			(unsigned int)sizeof(aal_param.cabc_gainlmt[0]) * 33U);
	p = memcpy(aal_info[aal_idx].dre_gain_flt_status,
			aal_param.DREGainFltStatus,
			(unsigned int)sizeof(aal_param.DREGainFltStatus[0])
			* 29U);

	aal_info[aal_idx].need_update = 2;

	return 0;
}

int mtk_aal_init_reg(int aal_idx, __user void *arg)
{
	DISP_AAL_INITREG aal_initreg;
	unsigned long ret;
	void *p;

	ret = copy_from_user(&aal_initreg, arg,
			(unsigned long)sizeof(DISP_AAL_INITREG));

	aal_info[aal_idx].function =
		(aal_initreg.dre_map_bypass == 1U) ? 2U : 6U;
	p = memcpy(aal_info[aal_idx].cabc_gainlmt, aal_initreg.cabc_gainlmt,
			sizeof(aal_initreg.cabc_gainlmt[0]) * 33U);

	aal_info[aal_idx].need_update = 1U;

	mtk_aal_function(aal_idx);

	return 0;
}

int mtk_aal_request_irq(int aal_idx)
{
	struct device_node *np;
	unsigned int irq;
	int ret;
	const char *name;

	if (aal_idx == 0)
		name = "aal0";
	else if (aal_idx == 1)
		name = "aal1";
	else
		return -1;

	np = of_find_node_by_name(NULL, name);

	irq = irq_of_parse_and_map(np, 0);

	aal_info[aal_idx].irq = irq;

	ret = request_irq(irq, mtk_disp_aal_irq_handler,
			IRQF_TRIGGER_NONE, name, NULL);
	if (ret != 0) {
		pr_info("request irq error: %s irq:%d return:%d\n",
				name, irq, ret);
		return -1;
	}
	pr_info("request irq success: %s irq:%d\n", name, irq);

	aal_info[aal_idx].base_addr = of_iomap(np, 0);

	return 0;
}

int mtk_aal_config2(int aal_idx)
{
	return 0;
}

void mtk_aal_config(int aal_idx)
{
	if (aal_info[aal_idx].need_update == 0U)
		return;

	if ((aal_info[aal_idx].need_update & 1U) != 0U)
		mtk_aal_function(aal_idx);

	mtk_aal_set_cabc_gain_lmt(aal_idx);

	if ((aal_info[aal_idx].need_update & 2U) != 0U) {
		mtk_aal_set_cabc_fltgain_force(aal_idx);
		mtk_aal_set_dre_gain_flt_status(aal_idx);
	}

	aal_info[aal_idx].need_update = 0U;
}


/* AAL Tuning Tool write SW Reg with debugfs */
int mtk_aal_write_reg(unsigned long addr, int value)
{
	unsigned long offset;
	int index;
	void *p;

	if (addr > AAL_SW_REG_END)
		return -1;

	if (addr > AAL0_SW_REG_BASE && addr < AAL0_SW_REG_END) {
		index = 0;
		offset = addr - AAL0_SW_REG_BASE;
	} else {
		index = 1;
		offset = addr - AAL1_SW_REG_BASE;
	}

	if (offset  >= sizeof(struct AALReg))
		return -1;

	p = memcpy(&(aal_reg[index].maxinfo_cnt_th) +
			offset / sizeof(int), &value, sizeof(int));

	aal_tuning_flag[index] =
		aal_tuning_flag[index] | TUNING_FLAG_WRITE;

	return 0;

}

/* AAL Tuning Tool read SW Reg with debugfs */
int mtk_aal_read_reg(unsigned long addr, int *value)
{
	unsigned long offset;
	int index;
	void *p;

	if (addr > AAL_SW_REG_END)
		return -1;

	if (addr < AAL0_SW_REG_END) {
		index = 0;
		offset = addr - AAL0_SW_REG_BASE;
	} else {
		index = 1;
		offset = addr - AAL1_SW_REG_BASE;
	}

	if (offset  >= sizeof(struct AALReg))
		return -1;

	p = memcpy(value, &(aal_reg[index].maxinfo_cnt_th) +
			offset / sizeof(int), sizeof(int));

	aal_tuning_flag[index] =
		aal_tuning_flag[index] | TUNING_FLAG_READ;

	return 0;
}

/* AAL Service get SW Reg from driver */
int mtk_aal_get_tuning_parameters(int aal_idx, __user void *arg)
{
	return (int)copy_to_user(arg,
			&(aal_reg[aal_idx]), sizeof(struct AALReg));
}

/* AAL Service set SW Reg to driver */
int mtk_aal_set_tuning_parameters(int aal_idx, __user void *arg)
{
	return (int)copy_from_user(&(aal_reg[aal_idx]),
			arg, sizeof(struct AALReg));
}
