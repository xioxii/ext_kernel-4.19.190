// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/bootmem.h>
#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/list.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/barrier.h>
#include <soc/mediatek/smi.h>
#include <linux/sched/clock.h>
#include "mtk_iommu.h"
#ifdef CONFIG_MTK_IOMMU_MISC_DBG
#include "m4u_debug.h"
#endif

#define REG_MMU_PT_BASE_ADDR			0x000
#define MMU_PT_ADDR_MASK			GENMASK(31, 7)
#define F_MMU_PT_BASE_ADDR_BIT32	GENMASK(2, 0)

#define REG_MMU_INVALIDATE			0x020
#define F_ALL_INVLD				0x2
#define F_MMU_INV_RANGE				0x1
#define F_MMU_INVLDT_VICT_ALL			0x4

#define REG_MMU_INVLD_START_A			0x024
#define REG_MMU_INVLD_END_A			0x028
#define F_MMU_INVLD_BIT32			GENMASK(1, 0)
#define F_MMU_INVLD_BIT31_12			GENMASK(31, 12)

#define REG_MMU_INV_SEL				0x038
#define REG_MMU_INV_SEL_MT6779			0x02c
#define F_INVLD_EN0				BIT(0)
#define F_INVLD_EN1				BIT(1)

#define REG_MMU_STANDARD_AXI_MODE		0x048

#define REG_MMU_MISC_CRTL_MT6779		0x048
#define REG_MMU_STANDARD_AXI_MODE_MT6779	(BIT(3) | BIT(19))
#define REG_MMU_COHERENCE_EN			(BIT(0) | BIT(16))
#define REG_MMU_IN_ORDER_WR_EN			(BIT(1) | BIT(17))
#define F_MMU_HALF_ENTRY_MODE_L			(BIT(5) | BIT(21))
#define F_MMU_BLOCKING_MODE_L			(BIT(4) | BIT(20))

#define REG_MMU_DCM_DIS				0x050

#define REG_MMU_WR_LEN				0x054
#define F_MMU_WR_THROT_DIS			(BIT(5) |  BIT(21))

#define REG_MMU_CTRL_REG			0x110
#define F_MMU_TF_PROT_TO_PROGRAM_ADDR		(2 << 4)
#define F_MMU_PREFETCH_RT_REPLACE_MOD		BIT(4)
#define F_MMU_TF_PROT_TO_PROGRAM_ADDR_MT8173	(2 << 5)

#define REG_MMU_TFRP_PADDR			0x114
#define F_RP_PA_REG_BIT31_7			GENMASK(31, 7)
#define F_RP_PA_REG_BIT32			GENMASK(2, 0)
#define F_MMU_TFRP_PA_SET(PA, EXT) (\
	(((unsigned long long)PA) & F_RP_PA_REG_BIT31_7) | \
	 ((((unsigned long long)PA) >> 32) & F_RP_PA_REG_BIT32))

#define REG_MMU_VLD_PA_RNG			0x118
#define F_MMU_VLD_PA_RNG(EA, SA)		(((EA) << 8) | (SA))

#define REG_MMU_INT_CONTROL0			(0x120)
#define F_L2_MULIT_HIT_EN		BIT(0)
#define F_TABLE_WALK_FAULT_INT_EN		BIT(1)
#define F_PREETCH_FIFO_OVERFLOW_INT_EN		BIT(2)
#define F_MISS_FIFO_OVERFLOW_INT_EN		BIT(3)
#define F_INT_L2_INVALID_DONE			BIT(4)
#define F_PREFETCH_FIFO_ERR_INT_EN			BIT(5)
#define F_MISS_FIFO_ERR_INT_EN			BIT(6)
#define F_INT_CTL0_L2_CDB_SLICE_ERR		BIT(7)
#define F_INT_CLR_BIT			BIT(12)


#define REG_MMU_INT_MAIN_CONTROL		0x124
#define F_INT_TRANSLATION_FAULT(MMU)                 BIT(0+(MMU)*7)
#define F_INT_MAIN_MULTI_HIT_FAULT(MMU)              BIT(1+(MMU)*7)
#define F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(MMU)    BIT(2+(MMU)*7)
#define F_INT_ENTRY_REPLACEMENT_FAULT(MMU)           BIT(3+(MMU)*7)
#define F_INT_TLB_MISS_FAULT(MMU)                    BIT(4+(MMU)*7)
#define F_INT_MISS_FIFO_ERR(MMU)                     BIT(5+(MMU)*7)
#define F_INT_PFH_FIFO_ERR(MMU)                      BIT(6+(MMU)*7)
#define F_INT_MAIN_MAU_INT_EN(MMU)		BIT(14 + MMU)
#define F_INT_MMU_MAIN_MSK(MMU)			GENMASK((6+(MMU)*7), ((MMU)*7))

#define REG_MMU_CPE_DONE			0x12C
#define REG_MMU_L2_FAULT_ST			(0x130)
#define F_MMU_L2_FAULT_MHIT			BIT(0)
#define F_MMU_L2_FAULT_TBW			BIT(1)
#define F_MMU_L2_FAULT_PFQ_FIFO_FULL		BIT(2)
#define F_MMU_L2_FAULT_MQ_FIFO_FULL		BIT(3)
#define F_MMU_L2_FAULT_INVLDT_DONE		BIT(4)
#define F_INT_L2_PFH_OUT_FIFO_ERROR		BIT(5)
#define F_INT_L2_PFH_IN_FIFO_ERROR		BIT(6)
#define F_INT_L2_MISS_OUT_FIFO_ERROR		BIT(7)
#define F_INT_L2_MISS_IN_FIFO_ERR		BIT(8)
#define F_MMU_L2_FAULT_L2_CDB_SLICE_ERR		BIT(9)

#define REG_MMU_FAULT_ST1			0x134
#define F_REG_MMU0_FAULT_MASK			GENMASK(6, 0)
#define F_REG_MMU1_FAULT_MASK			GENMASK(13, 7)

#define REG_MMU_TBWALK_FAULT_VA			(0x138)
#define F_MMU_TBWALK_FAULT_LAYER		BIT(0)
#define F_MMU_TBWALK_FAULT_BIT32		GENMASK(10, 9)
#define F_MMU_TBWALK_FAULT_BIT31_12		GENMASK(31, 12)

#define REG_MMU_FAULT_STATUS(MMU)		(0x13c+((MMU)<<3))
#define F_MMU_FAULT_VA_BIT31_12			GENMASK(31, 12)
#define F_MMU_FAULT_VA_BIT32			GENMASK(11, 9)
#define F_MMU_FAULT_PA_BIT32			GENMASK(8, 6)
#define F_MMU_FAULT_INVPA			BIT(5)
#define F_MMU_FAULT_TF				BIT(4)
#define F_MMU_FAULT_VA_WRITE_BIT		BIT(1)
#define F_MMU_FAULT_VA_LAYER_BIT		BIT(0)

#define REG_MMU_INVLD_PA(MMU)			(0x140+((MMU)<<3))

#define REG_MMU_INT_ID(MMU)			(0x150+((MMU)<<2))
#define REG_MMU1_FAULT_VA			0x144
#define REG_MMU1_INVLD_PA			0x148
#define REG_MMU1_INT_ID				0x154
#define F_MMU_INT_ID_COMM_ID(a)			(((a) >> 9) & 0x7)
#define F_MMU_INT_ID_SUB_COMM_ID(a)		(((a) >> 7) & 0x3)
#define F_MMU_INT_ID_LARB_ID(a)			(((a) >> 7) & 0x7)
#define F_MMU_INT_ID_PORT_ID(a)			(((a) >> 2) & 0x1f)
#define F_MMU_INT_ID_COMM_APU_ID(a)		((a) & 0x3)
#define F_MMU_INT_ID_SUB_APU_ID(a)		(((a) >> 2) & 0x3)

#define MTK_PROTECT_PA_ALIGN			256

/*
 * Get the local arbiter ID and the portid within the larb arbiter
 * from mtk_m4u_id which is defined by MTK_M4U_ID.
 */
#define MTK_M4U_ID(larb, port)		(((larb) << 5) | (port))
#define MTK_M4U_TO_LARB(id)		(((id) >> 5) & 0xf)
#define MTK_M4U_TO_PORT(id)		((id) & 0x1f)
#define MTK_MMU_NUM_OF_IOMMU(m4u_id)	(2)

#define MMU_INT_REPORT(mmu, mmu_2nd_id, id) \
	pr_notice( \
	"iommu%d_%d " #id "(0x%x) int happens!!\n",\
		mmu, mmu_2nd_id, id)

//#define IOMMU_CLK_SUPPORT
struct mtk_iommu_resv_iova_region {
	unsigned int dom_id;
	dma_addr_t iova_base;
	size_t iova_size;
	enum iommu_resv_type type;
};

static const struct iommu_ops mtk_iommu_ops;

/*
 * In M4U 4GB mode, the physical address is remapped as below:
 *  CPU PA         ->   M4U HW PA
 *  0x4000_0000         0x1_4000_0000 (Add bit32)
 *  0x8000_0000         0x1_8000_0000 ...
 *  0xc000_0000         0x1_c000_0000 ...
 *  0x1_0000_0000       0x1_0000_0000 (No change)
 *
 * Thus, We always add BIT32 in the iommu_map and disable BIT32 if PA is >=
 * 0x1_4000_0000 in the iova_to_phys.
 */
#define MTK_IOMMU_4GB_MODE_PA_140000000     0x140000000UL

static LIST_HEAD(m4ulist);	/* List all the M4U HWs */

#define for_each_m4u(data)	list_for_each_entry(data, &m4ulist, list)

/*
 * There may be 1 or 2 M4U HWs, But we always expect they are in the same domain
 * for the performance.
 *
 * Here always return the mtk_iommu_data of the first probed M4U where the
 * iommu domain information is recorded.
 */

/*
 * reserved IOVA Domain for IOMMU users of HW limitation.
 */

/*
 * struct mtk_domain_data:	domain configuration
 * @min_iova:	Start address of iova
 * @max_iova:	End address of iova
 * @port_mask:	User can specify mtk_iommu_domain by smi larb and port.
 *		Different mtk_iommu_domain have different iova space,
 *		port_mask is made up of larb_id and port_id.
 *		The format of larb and port can refer to mtxxxx-larb-port.h.
 *		bit[4:0] = port_id  bit[11:5] = larb_id.
 * Note: one user can only belong to one IOVAD,
 * the port mask is in unit of SMI larb.
 */
#define MTK_MAX_PORT_NUM	5

struct mtk_domain_data {
	unsigned long min_iova;
	unsigned long max_iova;
	unsigned int port_mask[MTK_MAX_PORT_NUM];
};

const struct mtk_domain_data single_dom = {
	.min_iova = 0x1000,
	.max_iova = DMA_BIT_MASK(32)
};

/*
 * related file: mt6779-larb-port.h
 */
const struct mtk_domain_data mt6779_multi_dom[] = {
	/* normal  domain */
	{
	 .min_iova = 0x0,
	 .max_iova = DMA_BIT_MASK(32),
	},
	/* ccu domain */
	{
	 .min_iova = 0x40000000,
	 .max_iova = 0x48000000 - 1,
	 .port_mask = {MTK_M4U_ID(9, 21), MTK_M4U_ID(9, 22),
		       MTK_M4U_ID(12, 0), MTK_M4U_ID(12, 1)}
	},
	/* vpu domain */
	{
	 .min_iova = 0x7da00000,
	 .max_iova = 0x7fc00000 - 1,
	 .port_mask = {MTK_M4U_ID(13, 0)}
	}
};

struct mtk_iommu_domain {
	unsigned int			id;
	struct iommu_domain		domain;
	struct iommu_group		*group;
	struct mtk_iommu_pgtable	*pgtable;
	struct mtk_iommu_data		*data;
	struct list_head		list;
};

struct mtk_iommu_pgtable {
	spinlock_t		pgtlock; /* lock for page table */
	struct io_pgtable_cfg	cfg;
	struct io_pgtable_ops	*iop;
	const struct mtk_domain_data	*dom_region;
	struct list_head	m4u_dom_v2;
	spinlock_t		domain_lock; /* lock for page table */
	unsigned int		domain_count;
	unsigned int		init_domain_id;
};

static struct mtk_iommu_pgtable *share_pgtable;

static struct mtk_iommu_pgtable *mtk_iommu_get_pgtable(void)
{
	return share_pgtable;
}

static unsigned int __mtk_iommu_get_domain_id(
		struct mtk_iommu_data *data,
		unsigned int portid)
{
	unsigned int dom_id = 0;
	const struct mtk_domain_data *mtk_dom_array = data->plat_data->dom_data;
	int i, j;

	for (i = 0; i < data->plat_data->dom_cnt; i++) {
		for (j = 0; j < MTK_MAX_PORT_NUM; j++) {
			if (portid == mtk_dom_array[i].port_mask[j])
				return i;
		}
	}

	return dom_id;
}

static unsigned int mtk_iommu_get_domain_id(
					struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	int portid = fwspec->ids[0];

	return __mtk_iommu_get_domain_id(data, portid);
}

static struct iommu_domain *__mtk_iommu_get_domain(
				struct mtk_iommu_data *data,
				unsigned int larbid, unsigned int portid)
{
	unsigned int domain_id = 0; /* single dom id */
	unsigned int port_mask = MTK_M4U_ID(larbid, portid);
	struct mtk_iommu_domain *dom;

	domain_id = __mtk_iommu_get_domain_id(data, port_mask);

	list_for_each_entry(dom, &data->pgtable->m4u_dom_v2, list) {
		if (dom->id == domain_id)
			return &dom->domain;
	}
	return NULL;
}

static struct mtk_iommu_domain *__mtk_iommu_get_mtk_domain(
					struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	struct mtk_iommu_domain *dom;
	unsigned int domain_id;

	domain_id = mtk_iommu_get_domain_id(dev);
	if (domain_id == data->plat_data->dom_cnt)
		return NULL;

	list_for_each_entry(dom, &data->pgtable->m4u_dom_v2, list) {
		if (dom->id == domain_id)
			return dom;
	}
	return NULL;
}

static struct iommu_group *mtk_iommu_get_group(
					struct device *dev)
{
	struct mtk_iommu_domain *dom;

	dom = __mtk_iommu_get_mtk_domain(dev);
	if (dom)
		return dom->group;

	return NULL;
}

static struct mtk_iommu_domain *to_mtk_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct mtk_iommu_domain, domain);
}

static int mtk_iommu_power_switch(struct mtk_iommu_data *data,
			bool enable, char *master)
{
#ifdef IOMMU_CLK_SUPPORT
	int ret = 0;

	if (data->plat_data->has_bclk != M4U_POWER_V2)
		return 0;

	if (!data->dev)
		return 0;

	if (enable)
		ret = pm_runtime_get_sync(data->dev);
	else
		ret = pm_runtime_put_sync(data->dev);
	pr_notice("%s, %d, %s power %s at iommu%d, for %s, ret=%d\n",
		  __func__, __LINE__,
		  enable ? "enable" : "disable",
		  ret ? "error" : "pass",
		  data->m4u_id,
		  master ? master : "NULL",
		  ret);

	return ret;
#endif
	return 0;
}

static int mtk_iommu_reg_backup(struct mtk_iommu_data *data)
{
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;

	if (data->plat_data->has_bclk == M4U_POWER_V2 && !data->poweron)
		return 0;

	if (!base || IS_ERR((void *)(unsigned long)base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return -1;
	}

	reg->wr_len = readl_relaxed(base + REG_MMU_WR_LEN);
	reg->standard_axi_mode = readl_relaxed(base +
					       REG_MMU_STANDARD_AXI_MODE);
	reg->dcm_dis = readl_relaxed(base + REG_MMU_DCM_DIS);
	reg->ctrl_reg = readl_relaxed(base + REG_MMU_CTRL_REG);
	reg->int_control0 = readl_relaxed(base + REG_MMU_INT_CONTROL0);
	reg->int_main_control = readl_relaxed(base + REG_MMU_INT_MAIN_CONTROL);
	reg->ivrp_paddr = readl_relaxed(base + REG_MMU_TFRP_PADDR);
	reg->vld_pa_rng = readl_relaxed(base + REG_MMU_VLD_PA_RNG);
	reg->pt_base = readl_relaxed(base +
					   REG_MMU_PT_BASE_ADDR);

	return 0;
}

static int mtk_iommu_reg_restore(struct mtk_iommu_data *data)
{
	struct mtk_iommu_suspend_reg *reg = &data->reg;
	void __iomem *base = data->base;

	if (data->plat_data->has_bclk == M4U_POWER_V2 && !data->poweron)
		return 0;

	if (!base || IS_ERR(base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return -1;
	}

	writel_relaxed(reg->wr_len, base + REG_MMU_WR_LEN);
	writel_relaxed(reg->standard_axi_mode,
		       base + REG_MMU_STANDARD_AXI_MODE);
	writel_relaxed(reg->dcm_dis, base + REG_MMU_DCM_DIS);
	writel_relaxed(reg->ctrl_reg, base + REG_MMU_CTRL_REG);
	writel_relaxed(reg->int_control0, base + REG_MMU_INT_CONTROL0);
	writel_relaxed(reg->int_main_control, base + REG_MMU_INT_MAIN_CONTROL);
	writel_relaxed(reg->ivrp_paddr, base + REG_MMU_TFRP_PADDR);
	writel_relaxed(reg->vld_pa_rng, base + REG_MMU_VLD_PA_RNG);
	writel_relaxed(reg->pt_base, base +
		   REG_MMU_PT_BASE_ADDR);
	wmb(); /*make sure the registers have been restored.*/

	return 0;
}

#ifdef IOMMU_CLK_SUPPORT
static struct mtk_iommu_data *mtk_iommu_get_m4u_data(int id)
{
	struct mtk_iommu_data *data;

	list_for_each_entry(data, &m4ulist, list) {
		if (data && data->m4u_id == id &&
		    data->base && !IS_ERR(data->base))
			return data;
	}

	pr_notice("%s, %d, failed to get data of %d\n", __func__, __LINE__, id);
	return NULL;
}

static int __maybe_unused mtk_iommu_pm_resume(struct device *dev)
{
	struct mtk_iommu_data *data;
	int ret = 0, i;
	unsigned long flags;

	for (i = 0; i < MTK_MAX_IOMMU_COUNT; i++) {
		data = mtk_iommu_get_m4u_data(i);
		if (!data || !data->dev || !data->power_initialized) {
			pr_notice("%s, %d iommu %d is not intialized\n",
				  __func__, __LINE__, i);
			continue;
		}
		if (data->dev != dev)
			continue;

		spin_lock_irqsave(&data->reg_lock, flags);
		data->poweron = true;

		ret = mtk_iommu_reg_restore(data);
		if (ret) {
			pr_notice("%s, %d, iommu:%u, restore failed %d\n",
				  __func__, __LINE__, data->m4u_id, ret);
			data->poweron = false;
			spin_unlock_irqrestore(&data->reg_lock, flags);
			continue;
		}
		spin_unlock_irqrestore(&data->reg_lock, flags);
		/*pr_notice("%s,%d,iommu:%d restore after on\n",
		 *	  __func__, __LINE__, data->m4u_id);
		 */
	}

	return 0;
}

static int __maybe_unused mtk_iommu_pm_suspend(struct device *dev)
{
	struct mtk_iommu_data *data;
	int ret = 0, i;
	unsigned long flags;
	unsigned long long start = 0, end = 0;

	for (i = 0; i < MTK_MAX_IOMMU_COUNT; i++) {
		data = mtk_iommu_get_m4u_data(i);
		if (!data || !data->dev || !data->power_initialized) {
			pr_notice("%s, %d iommu %d is not intialized\n",
				  __func__, __LINE__, i);
			continue;
		}

		if (data->dev != dev)
			continue;

		spin_lock_irqsave(&data->reg_lock, flags);
		if (data->isr_ref) {
			spin_unlock_irqrestore(&data->reg_lock, flags);
			start = sched_clock();
			/* waiting for irs handling done */
			while (data->isr_ref) {
				end = sched_clock();
				if (end - start > 1000000000ULL) { //10ms
					break;
				}
			}
			if (end)
				pr_notice("%s pg waiting isr:%lluns, ref:%lu\n",
					  __func__, end - start, data->isr_ref);
			spin_lock_irqsave(&data->reg_lock, flags);
		}
		ret = mtk_iommu_reg_backup(data);
		if (ret) {
			pr_notice("%s, %d, iommu:%u, backup failed %d\n",
				  __func__, __LINE__, data->m4u_id, ret);
			data->poweron = false;
			spin_unlock_irqrestore(&data->reg_lock, flags);
			continue;
		}
		data->poweron = false;
		spin_unlock_irqrestore(&data->reg_lock, flags);
		/*pr_notice("%s,%d,iommu:%d backup before off\n",
		 *	  __func__, __LINE__, data->m4u_id);
		 */
	}

	return 0;
}
#endif

static void __mtk_iommu_tlb_flush_all(const struct mtk_iommu_data *data)
{
	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base\n",
			  __func__, __LINE__);
		return;
	}

	if (data->plat_data->has_bclk == M4U_POWER_V2 && !data->poweron)
		return;

	writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
		data->base + data->plat_data->inv_sel_reg);
	writel_relaxed(F_ALL_INVLD, data->base + REG_MMU_INVALIDATE);
	wmb(); /* Make sure the tlb flush all done */
}

static void mtk_iommu_tlb_flush_all_lock(void *cookie, bool lock)
{
	struct mtk_iommu_data *data, *temp;
	unsigned long flags = 0;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		if (data->plat_data->has_bclk == M4U_POWER_V2 && lock)
			spin_lock_irqsave(&data->reg_lock, flags);
		__mtk_iommu_tlb_flush_all(data);
		if (data->plat_data->has_bclk == M4U_POWER_V2 && lock)
			spin_unlock_irqrestore(&data->reg_lock, flags);
	}
}

void mtk_iommu_tlb_flush_all(void *cookie)
{
	mtk_iommu_tlb_flush_all_lock(cookie, true);
}


static void __mtk_iommu_tlb_add_flush_nosync(
					   struct mtk_iommu_data *data,
					   unsigned long iova_start,
					   unsigned long iova_end)
{
	unsigned int regval;
	int ret = 0;
	u32 tmp;
	unsigned long start, end;

	if (!data->base  || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return;
	}

	start = round_down(iova_start, SZ_4K);
	end = round_down(iova_end, SZ_4K);
	writel_relaxed(F_INVLD_EN1 | F_INVLD_EN0,
	       data->base + data->plat_data->inv_sel_reg);

	regval = (unsigned int)(start &
				F_MMU_INVLD_BIT31_12);
	regval |= (start >> 32) & F_MMU_INVLD_BIT32;
	writel_relaxed(regval, data->base + REG_MMU_INVLD_START_A);
	regval = (unsigned int)(end &
				F_MMU_INVLD_BIT31_12);
	regval |= (end >> 32) & F_MMU_INVLD_BIT32;
	writel_relaxed(regval,
	       data->base + REG_MMU_INVLD_END_A);
	writel(F_MMU_INV_RANGE,
	       data->base + REG_MMU_INVALIDATE);

	ret = readl_poll_timeout_atomic(data->base + REG_MMU_CPE_DONE,
					tmp, tmp != 0, 10, 3000);
	if (ret) {
		dev_warn(data->dev,
		 "Partial TLB flush time out, iommu:%d,start=0x%lx(0x%lx),end=0x%lx(0x%lx)\n",
		 data->m4u_id, iova_start, start,
		 iova_end, end);
		mtk_iommu_tlb_flush_all(data);
	}
	writel_relaxed(0, data->base + REG_MMU_CPE_DONE);
	wmb(); /*make sure the TLB status has been cleared*/
}

static void mtk_iommu_tlb_add_flush_nosync(unsigned long iova,
					   size_t size,
					   size_t granule, bool leaf,
					   void *cookie)
{
	struct mtk_iommu_data *data, *temp;
	unsigned long iova_start = iova;
	unsigned long iova_end = iova + size - 1;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		__mtk_iommu_tlb_add_flush_nosync(data, iova_start, iova_end);
	}
}

static void mtk_iommu_tlb_sync(void *cookie)
{

}

static const struct iommu_gather_ops mtk_iommu_gather_ops = {
	.tlb_flush_all = mtk_iommu_tlb_flush_all,
	.tlb_add_flush = mtk_iommu_tlb_add_flush_nosync,
	.tlb_sync = mtk_iommu_tlb_sync,
};

static inline void mtk_iommu_intr_modify_all(unsigned long enable)
{
	struct mtk_iommu_data *data, *temp;

	list_for_each_entry_safe(data, temp, &m4ulist, list) {
		if (!data->base  || IS_ERR(data->base)) {
			pr_notice("%s, %d, invalid base addr\n",
				  __func__, __LINE__);
			continue;
		}

		if (enable) {
			writel_relaxed(0x6f,
				   data->base +
				   REG_MMU_INT_CONTROL0);
			writel_relaxed(0xffffffff,
				   data->base +
				   REG_MMU_INT_MAIN_CONTROL);
		} else {
			writel_relaxed(0,
				   data->base +
				   REG_MMU_INT_CONTROL0);
			writel_relaxed(0,
				   data->base +
				   REG_MMU_INT_MAIN_CONTROL);
		}
	}
}

static void mtk_iommu_isr_restart(struct timer_list * unused)
{
	mtk_iommu_intr_modify_all(1);
}

static int mtk_iommu_isr_pause_timer_init(struct mtk_iommu_data *data)
{
	timer_setup(&data->iommu_isr_pause_timer,
			mtk_iommu_isr_restart, 0);
	return 0;
}

static int mtk_iommu_isr_pause(int delay, struct mtk_iommu_data *data)
{
	mtk_iommu_intr_modify_all(0); /* disable all intr */
	/* delay seconds */
	data->iommu_isr_pause_timer.expires = jiffies + delay * HZ;
	if (!timer_pending(&data->iommu_isr_pause_timer))
		add_timer(&data->iommu_isr_pause_timer);
	return 0;
}

static void mtk_iommu_isr_record(struct mtk_iommu_data *data)
{
	static int isr_cnt;
	static unsigned long first_jiffies;

	/* we allow one irq in 1s, or we will disable them after 5s. */
	if (!isr_cnt || time_after(jiffies, first_jiffies + isr_cnt * HZ)) {
		isr_cnt = 1;
		first_jiffies = jiffies;
	} else {
		isr_cnt++;
		if (isr_cnt >= 5) {
			/* 5 irqs come in 5s, too many ! */
			/* disable irq for a while, to avoid HWT timeout */
			mtk_iommu_isr_pause(10, data);
			isr_cnt = 0;
		}
	}
}
static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova);

static irqreturn_t mtk_iommu_isr(int irq, void *dev_id)
{
	struct mtk_iommu_data *data = dev_id;
	struct iommu_domain *domain;
	u32 int_state, int_state_l2, regval, int_id;
	unsigned int fault_iova;
	unsigned long fault_pa;
	unsigned int fault_larb, fault_port;
	bool layer, write, is_vpu = false;
	int slave_id = 0, i, port_id;
	unsigned int m4uid, sub_comm = 0;
	phys_addr_t pa;
	int ret = 0;
	unsigned long flags = 0;

	if (!data) {
		pr_notice("%s, Invalid normal irq %d\n",
				__func__, irq);
		return 0;
	}

	if (data->plat_data->has_bclk == M4U_POWER_V2) {
		spin_lock_irqsave(&data->reg_lock, flags);
		if (!data->poweron) {
			spin_unlock_irqrestore(&data->reg_lock, flags);
			return 0;
		}
		data->isr_ref++;
		spin_unlock_irqrestore(&data->reg_lock, flags);
	}

	m4uid = data->m4u_id;
	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return 0;
	}
	/* Read error info from registers */
	int_state_l2 = readl_relaxed(data->base + REG_MMU_L2_FAULT_ST);
	int_state = readl_relaxed(data->base + REG_MMU_FAULT_ST1);

	if (!int_state_l2 && !int_state) {
		ret = 0;
		goto out;
	}
	for (i = 0; i < MTK_MMU_NUM_OF_IOMMU(m4uid); i++) {
		if (int_state & (F_INT_MMU_MAIN_MSK(i) |
		    F_INT_MAIN_MAU_INT_EN(i))) {
			slave_id = i;
			break;
		}
	}
	pr_notice("iommu:%u, slave:%d, L2 int sta(0x130)=0x%x, main sta(0x134)=0x%x\n",
		  m4uid, slave_id,
		  int_state_l2, int_state);

	if (int_state_l2 & F_L2_MULIT_HIT_EN)
		MMU_INT_REPORT(m4uid, slave_id, (unsigned int)F_L2_MULIT_HIT_EN);

	if (int_state_l2 & F_TABLE_WALK_FAULT_INT_EN) {
		unsigned int layer;

		MMU_INT_REPORT(m4uid, slave_id, (unsigned int)F_TABLE_WALK_FAULT_INT_EN);
		regval = readl_relaxed(data->base +
				REG_MMU_TBWALK_FAULT_VA);
		fault_iova = ((unsigned long)regval & F_MMU_FAULT_VA_BIT31_12);
		layer = regval & 1;
		MMU_INT_REPORT(m4uid, slave_id,
			(unsigned int)F_TABLE_WALK_FAULT_INT_EN);
		pr_notice(
			  "L2 table walk fault: iova=0x%x, layer=%d\n",
			  fault_iova, layer);
	}

	if (int_state_l2 & F_PREETCH_FIFO_OVERFLOW_INT_EN)
		MMU_INT_REPORT(m4uid, slave_id,
			(unsigned int)F_PREETCH_FIFO_OVERFLOW_INT_EN);

	if (int_state_l2 & F_MISS_FIFO_OVERFLOW_INT_EN)
		MMU_INT_REPORT(m4uid, slave_id,
			(unsigned int)F_MISS_FIFO_OVERFLOW_INT_EN);

	if (int_state_l2 & F_INT_L2_INVALID_DONE)
		MMU_INT_REPORT(m4uid, slave_id, (unsigned int)F_INT_L2_INVALID_DONE);

	if (int_state_l2 & F_INT_L2_PFH_OUT_FIFO_ERROR)
		MMU_INT_REPORT(m4uid, slave_id,
			(unsigned int)F_INT_L2_PFH_OUT_FIFO_ERROR);

	if (int_state_l2 & F_INT_L2_PFH_IN_FIFO_ERROR)
		MMU_INT_REPORT(m4uid, slave_id,
			(unsigned int)F_INT_L2_PFH_IN_FIFO_ERROR);

	if (int_state_l2 & F_INT_L2_MISS_OUT_FIFO_ERROR)
		MMU_INT_REPORT(m4uid, slave_id,
			(unsigned int)F_INT_L2_MISS_OUT_FIFO_ERROR);

	if (int_state_l2 & F_INT_L2_MISS_IN_FIFO_ERR)
		MMU_INT_REPORT(m4uid, slave_id, (unsigned int)F_INT_L2_MISS_IN_FIFO_ERR);

	if (i == MTK_MMU_NUM_OF_IOMMU(m4uid)) {
		pr_info("m4u interrupt error: status = 0x%x\n", int_state);
		regval = readl_relaxed(data->base + REG_MMU_INT_CONTROL0);
		regval |= F_INT_CLR_BIT;
		writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);
		ret = 0;
		goto out;
	}

	if (int_state & F_INT_TRANSLATION_FAULT(slave_id)) {
#ifdef CONFIG_MTK_IOMMU_MISC_DBG
		mtk_dump_pgtable();
#endif
		int_id = readl_relaxed(data->base + REG_MMU_INT_ID(slave_id));
		fault_port = F_MMU_INT_ID_PORT_ID(int_id);
		if (data->plat_data->has_sub_comm[data->m4u_id]) {
			/* m4u1 is VPU in mt6779.*/
			if (data->m4u_id && data->plat_data->m4u_plat == M4U_MT6779) {
				fault_larb = F_MMU_INT_ID_COMM_APU_ID(int_id);
				sub_comm = F_MMU_INT_ID_SUB_APU_ID(int_id);
				fault_port = 0; /* for mt6779 APU ID is irregular */
				is_vpu = true;
			} else {
				fault_larb = F_MMU_INT_ID_COMM_ID(int_id);
				sub_comm = F_MMU_INT_ID_SUB_COMM_ID(int_id);
			}
		} else {
			fault_larb = F_MMU_INT_ID_LARB_ID(int_id);
		}

		fault_larb = data->plat_data->larbid_remap[data->m4u_id][fault_larb];
		port_id = MTK_M4U_ID(fault_larb, fault_port);
		pr_notice("iommu:%d, slave:%d, port_id=0x%x(%u-%u), tf_id:0x%x\n",
			  m4uid, slave_id, port_id,
			  fault_larb, fault_port, int_id);

		if (port_id < 0) {
			WARN_ON(1);
			ret = 0;
			goto out;
		}
		/*pseudo_dump_port(port_id, true);*/

		regval = readl_relaxed(data->base +
					REG_MMU_FAULT_STATUS(slave_id));
		layer = regval & F_MMU_FAULT_VA_LAYER_BIT;
		write = regval & F_MMU_FAULT_VA_WRITE_BIT;
		fault_iova = ((unsigned long)regval & F_MMU_FAULT_VA_BIT31_12);
		pr_notice("%s, %d, fault_iova=%x, regval=0x%x\n",
			  __func__, __LINE__, fault_iova, regval);

		domain = __mtk_iommu_get_domain(data,
					fault_larb, fault_port);
		if (!domain) {
			WARN_ON(1);
			ret = 0;
			goto out;
		}
		pa = mtk_iommu_iova_to_phys(domain, fault_iova & PAGE_MASK);

		fault_pa = readl_relaxed(data->base +
					REG_MMU_INVLD_PA(slave_id));
		fault_pa |= (unsigned long)(regval &
					F_MMU_FAULT_PA_BIT32) << 26;
		pr_notice("fault_pa=0x%lx, get pa=%x, tfrp=0x%x, ptbase=0x%x\n",
			  fault_pa, (unsigned int)pa,
			  readl_relaxed(data->base + REG_MMU_TFRP_PADDR),
			  readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR));

#ifdef CONFIG_MTK_IOMMU_MISC_DBG
		report_custom_iommu_fault(
					  fault_iova, fault_pa,
					  int_id, is_vpu);
#endif
		if (report_iommu_fault(domain, data->dev, fault_iova,
			       write ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ)) {
			dev_err_ratelimited(
				data->dev,
				"iommu fault type=0x%x iova=0x%x pa=0x%lx larb=%d port=%d is_vpu=%d layer=%d %s\n",
				int_state, fault_iova, fault_pa,
				fault_larb, fault_port, is_vpu,
				layer, write ? "write" : "read");
		}
	}

	if (int_state &
	    F_INT_MAIN_MULTI_HIT_FAULT(slave_id)) {
		MMU_INT_REPORT(m4uid, slave_id,
			(unsigned int)F_INT_MAIN_MULTI_HIT_FAULT(slave_id));
	}
	if (int_state &
	    F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(slave_id)) {
		if (!(int_state &
		    F_INT_TRANSLATION_FAULT(slave_id))) {
			MMU_INT_REPORT(m4uid, slave_id,
				(unsigned int)F_INT_INVALID_PHYSICAL_ADDRESS_FAULT(slave_id));

		}
	}
	if (int_state & F_INT_ENTRY_REPLACEMENT_FAULT(slave_id)) {
		MMU_INT_REPORT(m4uid, slave_id,
			(unsigned int)F_INT_ENTRY_REPLACEMENT_FAULT(slave_id));
	}
	if (int_state & F_INT_TLB_MISS_FAULT(slave_id))
		MMU_INT_REPORT(m4uid, slave_id,
				(unsigned int)F_INT_TLB_MISS_FAULT(slave_id));

	if (int_state & F_INT_MISS_FIFO_ERR(slave_id))
		MMU_INT_REPORT(m4uid, slave_id,
				(unsigned int)F_INT_MISS_FIFO_ERR(slave_id));

	if (int_state & F_INT_PFH_FIFO_ERR(slave_id))
		MMU_INT_REPORT(m4uid, slave_id,
				(unsigned int)F_INT_PFH_FIFO_ERR(slave_id));

	/* Interrupt clear */
	regval = readl_relaxed(data->base + REG_MMU_INT_CONTROL0);
	regval |= F_INT_CLR_BIT;
	writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);

	mtk_iommu_tlb_flush_all_lock(data, false);
	mtk_iommu_isr_record(data);

	ret = IRQ_HANDLED;
out:
	if (data->plat_data->has_bclk == M4U_POWER_V2) {
		spin_lock_irqsave(&data->reg_lock, flags);
		data->isr_ref--;
		spin_unlock_irqrestore(&data->reg_lock, flags);
	}
	return ret;
}

static void mtk_iommu_config(struct mtk_iommu_data *data,
			     struct device *dev, bool enable)
{
	struct mtk_smi_larb_iommu    *larb_mmu;
	unsigned int                 larbid, portid;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct device_link *link;
	int i;

	for (i = 0; i < fwspec->num_ids; ++i) {
		larbid = MTK_M4U_TO_LARB(fwspec->ids[i]);
		portid = MTK_M4U_TO_PORT(fwspec->ids[i]);
		larb_mmu = &data->smi_imu.larb_imu[larbid];

		dev_dbg(dev, "%s iommu port: %d\n",
			enable ? "enable" : "disable", portid);

		if (enable) {
			larb_mmu->mmu |= MTK_SMI_MMU_EN(portid);
			/* Link the consumer with the larb device(supplier) */
			link = device_link_add(dev, larb_mmu->dev,
					       DL_FLAG_PM_RUNTIME |
					       DL_FLAG_AUTOREMOVE_CONSUMER);
			if (!link) {
				dev_err(dev, "Unable to link %s\n",
					dev_name(larb_mmu->dev));
				return;
			}
		} else {
			larb_mmu->mmu &= ~MTK_SMI_MMU_EN(portid);
		}
	}
}

static struct iommu_group *mtk_iommu_create_domain(
			struct mtk_iommu_data *data, struct device *dev)
{
	struct mtk_iommu_pgtable *pgtable = mtk_iommu_get_pgtable();
	struct mtk_iommu_domain *dom;
	struct iommu_group *group;
	unsigned long flags;

	pr_notice("%s, %d\n", __func__, __LINE__);
	group = mtk_iommu_get_group(dev);

	if (group) {
		iommu_group_ref_get(group);
		return group;
	}

	/* init mtk_iommu_domain */
	dom = kzalloc(sizeof(*dom), GFP_KERNEL);
	if (!dom)
		return ERR_PTR(-ENOMEM);

	/* init iommu_group */
	group = iommu_group_alloc();
	if (IS_ERR(group)) {
		dev_notice(dev, "Failed to allocate M4U IOMMU group\n");
		goto free_dom;
	}
	dom->group = group;
	dom->id = mtk_iommu_get_domain_id(dev);
	dom->data = data;
	dom->pgtable = pgtable;
	spin_lock_irqsave(&pgtable->domain_lock, flags);
	if (pgtable->domain_count >= data->plat_data->dom_cnt) {
		spin_unlock_irqrestore(&pgtable->domain_lock, flags);
		pr_notice("%s, %d, too many domain, count=%d\n",
			__func__, __LINE__, pgtable->domain_count);
		goto free_dom;
	}
	pgtable->init_domain_id = dom->id;
	pgtable->domain_count++;
	spin_unlock_irqrestore(&pgtable->domain_lock, flags);
	list_add_tail(&dom->list, &pgtable->m4u_dom_v2);

	pr_notice("%s: dev:%s, dom_id:%u, dom_cnt:%u\n",
		    __func__, dev_name(dev),
		    dom->id, pgtable->domain_count);

	return group;

free_dom:
	kfree(dom);
	return NULL;
}

static struct mtk_iommu_pgtable *mtk_iommu_create_pgtable(
			struct mtk_iommu_data *data)
{
	struct mtk_iommu_pgtable *pgtable;

	pgtable = kzalloc(sizeof(*pgtable), GFP_KERNEL);
	if (!pgtable)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&pgtable->pgtlock);
	spin_lock_init(&pgtable->domain_lock);
	pgtable->domain_count = 0;
	INIT_LIST_HEAD(&pgtable->m4u_dom_v2);

	pgtable->cfg = (struct io_pgtable_cfg) {
		.quirks = IO_PGTABLE_QUIRK_ARM_NS |
			IO_PGTABLE_QUIRK_NO_PERMS |
			IO_PGTABLE_QUIRK_TLBI_ON_MAP,
		.pgsize_bitmap = mtk_iommu_ops.pgsize_bitmap,
		.ias = 32,
		.oas = 32,
		.tlb = &mtk_iommu_gather_ops,
		.iommu_dev = data->dev,
	};

	if (data->dram_is_4gb) /* need it ? */
		pgtable->cfg.quirks |= IO_PGTABLE_QUIRK_ARM_MTK_4GB;

	pgtable->iop = alloc_io_pgtable_ops(ARM_V7S, &pgtable->cfg, data);
	if (!pgtable->iop) {
		dev_err(data->dev, "Failed to alloc io pgtable\n");
		return ERR_PTR(-EINVAL);
	}

	pgtable->dom_region = data->plat_data->dom_data;

	pr_notice("%s create pgtable done\n", __func__);

	return pgtable;
}

int __mtk_iommu_get_pgtable_base_addr(
		struct mtk_iommu_pgtable *pgtable,
		unsigned int *pgd_pa)
{
	if (!pgtable)
		pgtable = mtk_iommu_get_pgtable();

	if (!pgtable) {
		pr_notice("%s, %d, cannot find pgtable\n",
			  __func__, __LINE__);
		return -1;
	}
	*pgd_pa = pgtable->cfg.arm_v7s_cfg.ttbr[0] & MMU_PT_ADDR_MASK;
	if (pgtable->cfg.arm_v7s_cfg.ttbr[1] <
	    (1 << 3)) {
		*pgd_pa |= pgtable->cfg.arm_v7s_cfg.ttbr[1] &
			  F_MMU_PT_BASE_ADDR_BIT32;
	} else {
		pr_notice("%s, %d, invalid pgtable base addr, 0x%x_%x\n",
			  __func__, __LINE__,
			  pgtable->cfg.arm_v7s_cfg.ttbr[1],
			  pgtable->cfg.arm_v7s_cfg.ttbr[0]);
		return -2;
	}

	return 0;
}

static int mtk_iommu_attach_pgtable(struct mtk_iommu_data *data,
			struct device *dev)
{
	struct mtk_iommu_pgtable *pgtable = mtk_iommu_get_pgtable();
	unsigned int regval = 0;
	unsigned int pgd_pa_reg = 0;
	unsigned long flags;
	struct mtk_iommu_suspend_reg *reg = &data->reg;

	/* create share pgtable */
	if (!pgtable) {
		pgtable = mtk_iommu_create_pgtable(data);
		if (IS_ERR(pgtable)) {
			dev_err(data->dev, "Failed to create pgtable\n");
			return -ENOMEM;
		}

		share_pgtable = pgtable;
	}

	/* binding to pgtable */
	data->pgtable = pgtable;

	/* update HW settings */
	if (__mtk_iommu_get_pgtable_base_addr(pgtable, &pgd_pa_reg))
		return -EFAULT;

	if (data->plat_data->has_bclk == M4U_POWER_V2) {
		spin_lock_irqsave(&data->reg_lock, flags);
		if (data->poweron) {
			writel(pgd_pa_reg, data->base + REG_MMU_PT_BASE_ADDR);
			regval = readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR);
			pr_notice("%s, %d, iommu:%d config pgtable base addr=0x%x, quiks=0x%lx\n",
				  __func__, __LINE__, data->m4u_id,
				  regval, pgtable->cfg.quirks);
			spin_unlock_irqrestore(&data->reg_lock, flags);
		} else {
			spin_unlock_irqrestore(&data->reg_lock, flags);
			reg->pt_base = pgd_pa_reg;
			pr_notice("%s, %d, iommu:%d backup pgtable base addr=0x%x, quiks=0x%lx\n",
				  __func__, __LINE__, data->m4u_id,
				  reg->pt_base, pgtable->cfg.quirks);
		}
	} else {
		writel(pgd_pa_reg, data->base + REG_MMU_PT_BASE_ADDR);
		regval = readl_relaxed(data->base + REG_MMU_PT_BASE_ADDR);
		pr_notice("%s, %d, iommu:%d config pgtable base addr=0x%x, quiks=0x%lx\n",
			  __func__, __LINE__, data->m4u_id,
			  regval, pgtable->cfg.quirks);
	}

	return 0;
}

static struct iommu_domain *mtk_iommu_domain_alloc(unsigned int type)
{
	struct mtk_iommu_domain *dom_tmp, *dom = NULL;
	struct mtk_iommu_pgtable *pgtable = mtk_iommu_get_pgtable();
	unsigned int id = pgtable->init_domain_id;
	/* allocated at device_group for IOVA  space management by iovad */
	unsigned int domain_type = IOMMU_DOMAIN_DMA;

	pr_notice("%s, %d\n", __func__, __LINE__);
	if (type != domain_type) {
		pr_notice("%s, %d, err type%d\n",
			__func__, __LINE__, type);
		return NULL;
	}

	list_for_each_entry(dom_tmp, &pgtable->m4u_dom_v2, list) {
		if (dom_tmp->id == id) {
			dom = dom_tmp;
			break;
		}
	}

	if (!dom)
		return NULL;

	if (iommu_get_dma_cookie(&dom->domain))
		goto free_dom;

	dom->domain.geometry.aperture_start =
				pgtable->dom_region[dom->id].min_iova;
	dom->domain.geometry.aperture_end =
				pgtable->dom_region[dom->id].max_iova;
	dom->domain.geometry.force_aperture = true;

	/* Update our support page sizes bitmap */
	dom->domain.pgsize_bitmap = pgtable->cfg.pgsize_bitmap;

	pr_notice("%s, allocated the %u start:%pa, end:%pa\n",
		    __func__,
		    dom->id,
		    &dom->domain.geometry.aperture_start,
		    &dom->domain.geometry.aperture_end);

	return &dom->domain;

free_dom:
	kfree(dom);
	return NULL;
}

static void mtk_iommu_domain_free(struct iommu_domain *domain)
{
	unsigned long flags;
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;

	pr_notice("%s, %d, domain_count=%d, free the %d domain\n",
		    __func__, __LINE__, pgtable->domain_count, dom->id);

	iommu_put_dma_cookie(domain);

	kfree(dom);

	spin_lock_irqsave(&pgtable->domain_lock, flags);
	pgtable->domain_count--;
	if (pgtable->domain_count > 0) {
		spin_unlock_irqrestore(&pgtable->domain_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&pgtable->domain_lock, flags);
	free_io_pgtable_ops(pgtable->iop);
	kfree(pgtable);
}

static int mtk_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;

	if (!data)
		return -ENODEV;

	mtk_iommu_config(data, dev, true);
	return 0;
}

static void mtk_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;

	if (!data)
		return;

	mtk_iommu_config(data, dev, false);
}

static int mtk_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_data *data = dom->data;
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	int ret;

	/* The "4GB mode" M4U physically can not use the lower remap of Dram. */
	if (data->plat_data->has_4gb_mode && data->dram_is_4gb)
		paddr |= BIT_ULL(32);

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	ret = pgtable->iop->map(pgtable->iop, iova, paddr, size, prot);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	return ret;
}

static size_t mtk_iommu_unmap(struct iommu_domain *domain,
			      unsigned long iova, size_t size)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	unsigned long flags;
	size_t unmapsz;

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	unmapsz = pgtable->iop->unmap(pgtable->iop, iova, size);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	return unmapsz;
}

static void mtk_iommu_iotlb_sync(struct iommu_domain *domain)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);

	mtk_iommu_tlb_sync(dom->data);
}

static phys_addr_t mtk_iommu_iova_to_phys(struct iommu_domain *domain,
					  dma_addr_t iova)
{
	struct mtk_iommu_domain *dom = to_mtk_domain(domain);
	struct mtk_iommu_pgtable *pgtable = dom->pgtable;
	struct mtk_iommu_data *data = dom->data;
	unsigned long flags;
	phys_addr_t pa;

	spin_lock_irqsave(&pgtable->pgtlock, flags);
	pa = pgtable->iop->iova_to_phys(pgtable->iop, iova);
	spin_unlock_irqrestore(&pgtable->pgtlock, flags);

	if (data->plat_data->has_4gb_mode && data->dram_is_4gb &&
	    pa >= MTK_IOMMU_4GB_MODE_PA_140000000)
		pa &= ~BIT_ULL(32);

	return pa;
}

static int mtk_iommu_add_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;
	struct iommu_group *group;

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return -ENODEV; /* Not a iommu client device */

	data = fwspec->iommu_priv;
	iommu_device_link(&data->iommu, dev);

	group = iommu_group_get_for_dev(dev);
	if (IS_ERR(group))
		return PTR_ERR(group);

	iommu_group_put(group);
	return 0;
}

static void mtk_iommu_remove_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct mtk_iommu_data *data;

	if (!fwspec || fwspec->ops != &mtk_iommu_ops)
		return;

	data = fwspec->iommu_priv;
	iommu_device_unlink(&data->iommu, dev);

	iommu_group_remove_device(dev);
	iommu_fwspec_free(dev);
}

static struct iommu_group *mtk_iommu_device_group(struct device *dev)
{
	struct mtk_iommu_data *data = dev->iommu_fwspec->iommu_priv;
	struct mtk_iommu_pgtable *pgtable;
	int ret = 0;

	if (!data)
		return ERR_PTR(-ENODEV);

	pgtable = data->pgtable;
	if (!pgtable) {
		ret = mtk_iommu_attach_pgtable(data, dev);
		if (ret) {
			dev_err(data->dev, "Failed to device_group\n");
			return NULL;
		}
	}

	pr_notice("%s, init data:%u\n", __func__, data->m4u_id);
	return mtk_iommu_create_domain(data, dev);
}

static int mtk_iommu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct platform_device *m4updev;

	if (args->args_count != 1) {
		dev_err(dev, "invalid #iommu-cells(%d) property for IOMMU\n",
			args->args_count);
		return -EINVAL;
	}

	if (!fwspec->iommu_priv) {
		/* Get the m4u device */
		m4updev = of_find_device_by_node(args->np);
		if (WARN_ON(!m4updev))
			return -EINVAL;

		fwspec->iommu_priv = platform_get_drvdata(m4updev);
	}

	return iommu_fwspec_add_ids(dev, args->args, 1);
}

#ifdef CONFIG_ARM64
/* reserve/dir-map iova region for arm64 evb */
static void mtk_iommu_get_resv_regions(struct device *dev,
				       struct list_head *head)
{
	struct mtk_iommu_data *data = dev_iommu_fwspec_get(dev)->iommu_priv;
	unsigned int i, total_cnt = data->plat_data->resv_cnt;
	unsigned int dom_id = mtk_iommu_get_domain_id(dev);
	const struct mtk_iommu_resv_iova_region *resv_data;
	struct iommu_resv_region *region;
	unsigned long base = 0;
	size_t size = 0;
	int prot = IOMMU_WRITE | IOMMU_READ;

	resv_data = data->plat_data->resv_region;

	for (i = 0; i < total_cnt; i++) {
		size = 0;
		if (resv_data[i].iova_size) {
			base = (unsigned long)resv_data[i].iova_base;
			size = resv_data[i].iova_size;
		}
		if (!size || resv_data[i].dom_id != dom_id)
			continue;

		region = iommu_alloc_resv_region(base, size, prot,
						 resv_data[i].type);
		if (!region)
			return;

		list_add_tail(&region->list, head);

		dev_dbg(data->dev, "%s iova 0x%x ~ 0x%x\n",
			(resv_data[i].type == IOMMU_RESV_DIRECT) ? "dm" : "rsv",
			(unsigned int)base, (unsigned int)(base + size - 1));
	}
}

static void mtk_iommu_put_resv_regions(struct device *dev,
				      struct list_head *head)
{
	struct iommu_resv_region *entry, *next;

	list_for_each_entry_safe(entry, next, head, list)
		kfree(entry);
}
#endif

static const struct iommu_ops mtk_iommu_ops = {
	.domain_alloc	= mtk_iommu_domain_alloc,
	.domain_free	= mtk_iommu_domain_free,
	.attach_dev	= mtk_iommu_attach_device,
	.detach_dev	= mtk_iommu_detach_device,
	.map		= mtk_iommu_map,
	.unmap		= mtk_iommu_unmap,
	.map_sg		= default_iommu_map_sg,
	.flush_iotlb_all = mtk_iommu_iotlb_sync,
	.iotlb_sync	= mtk_iommu_iotlb_sync,
	.iova_to_phys	= mtk_iommu_iova_to_phys,
	.add_device	= mtk_iommu_add_device,
	.remove_device	= mtk_iommu_remove_device,
	.device_group	= mtk_iommu_device_group,
	.of_xlate	= mtk_iommu_of_xlate,
#ifdef CONFIG_ARM64
	.get_resv_regions = mtk_iommu_get_resv_regions,
	.put_resv_regions = mtk_iommu_put_resv_regions,
#endif
	.pgsize_bitmap	= SZ_4K | SZ_64K | SZ_1M | SZ_16M,
};

static int mtk_iommu_hw_init(const struct mtk_iommu_data *data)
{
	u32 regval;
	int ret;

	if (!data->base || IS_ERR(data->base)) {
		pr_notice("%s, %d, invalid base addr\n",
			  __func__, __LINE__);
		return -1;
	}

	if (data->plat_data->has_bclk == M4U_POWER_V1) {
		ret = clk_prepare_enable(data->bclk);
		if (ret) {
			dev_err(data->dev, "Failed to enable iommu bclk(%d)\n", ret);
			return ret;
		}
	} else if (data->plat_data->has_bclk == M4U_POWER_V2 &&
	    !data->poweron) {
		pr_notice("%s, iommu%u power off\n",
			  __func__, data->m4u_id);
		return 0;
 	}

	regval = readl_relaxed(data->base + REG_MMU_CTRL_REG);
	if (data->plat_data->m4u_plat == M4U_MT8173)
		regval |= F_MMU_PREFETCH_RT_REPLACE_MOD |
			 F_MMU_TF_PROT_TO_PROGRAM_ADDR_MT8173;
	else
		regval |= F_MMU_TF_PROT_TO_PROGRAM_ADDR;
	writel_relaxed(regval, data->base + REG_MMU_CTRL_REG);

	regval = F_L2_MULIT_HIT_EN |
		F_TABLE_WALK_FAULT_INT_EN |
		F_PREETCH_FIFO_OVERFLOW_INT_EN |
		F_MISS_FIFO_OVERFLOW_INT_EN |
		F_PREFETCH_FIFO_ERR_INT_EN |
		F_MISS_FIFO_ERR_INT_EN;
	writel_relaxed(regval, data->base + REG_MMU_INT_CONTROL0);

	regval = F_INT_MMU_MAIN_MSK(0) |
		F_INT_MMU_MAIN_MSK(1);
	writel_relaxed(regval, data->base + REG_MMU_INT_MAIN_CONTROL);

	if (data->plat_data->m4u_plat == M4U_MT8173)
		regval = (data->protect_base >> 1) | (data->dram_is_4gb << 31);
	else
		regval = lower_32_bits(data->protect_base) |
			 upper_32_bits(data->protect_base);
	writel_relaxed(regval, data->base + REG_MMU_TFRP_PADDR);

	if (data->dram_is_4gb && data->plat_data->has_vld_pa_rng) {
		/*
		 * If 4GB mode is enabled, the validate PA range is from
		 * 0x1_0000_0000 to 0x1_ffff_ffff. here record bit[32:30].
		 */
		regval = F_MMU_VLD_PA_RNG(7, 4);
		writel_relaxed(regval, data->base + REG_MMU_VLD_PA_RNG);
	}
	writel_relaxed(0, data->base + REG_MMU_DCM_DIS);

	if (data->plat_data->reset_axi)
		writel_relaxed(0, data->base + REG_MMU_STANDARD_AXI_MODE);

	if (data->plat_data->has_wr_len) {
		/* write command throttling mode */
		regval = readl_relaxed(data->base + REG_MMU_WR_LEN);
		regval &= ~F_MMU_WR_THROT_DIS;
		writel_relaxed(regval, data->base + REG_MMU_WR_LEN);
	}
	/* special settings for mmu0 (multimedia iommu) */
	if (data->plat_data->has_misc_ctrl[data->m4u_id]) {
		regval = readl_relaxed(data->base + REG_MMU_MISC_CRTL_MT6779);
		pr_notice("0x%x, REG_MMU_MISC_CRTL_MT6779\n", regval);
		/* non-standard AXI mode */
		regval &= ~REG_MMU_STANDARD_AXI_MODE_MT6779;
		writel_relaxed(regval, data->base + REG_MMU_MISC_CRTL_MT6779);
	}

	if (devm_request_irq(data->dev, data->irq, mtk_iommu_isr, 0,
			     dev_name(data->dev), (void *)data)) {
		writel_relaxed(0, data->base + REG_MMU_PT_BASE_ADDR);
		clk_disable_unprepare(data->bclk);
		dev_err(data->dev, "Failed @ IRQ-%d Request\n", data->irq);
		return -ENODEV;
	}

	return 0;
}

static const struct component_master_ops mtk_iommu_com_ops = {
	.bind		= mtk_iommu_bind,
	.unbind		= mtk_iommu_unbind,
};

static int mtk_iommu_probe(struct platform_device *pdev)
{
	struct mtk_iommu_data   *data;
	struct device           *dev = &pdev->dev;
	struct resource         *res;
	resource_size_t		ioaddr;
	struct component_match  *match = NULL;
	void                    *protect;
	int                     i, larb_nr, ret;

	pr_notice("%s, ++\n", __func__);
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = dev;
	data->plat_data = of_device_get_match_data(dev);

	/* Protect memory. HW will access here while translation fault.*/
	protect = devm_kzalloc(dev, MTK_PROTECT_PA_ALIGN * 2, GFP_KERNEL);
	if (!protect)
		return -ENOMEM;
	data->protect_base = ALIGN(virt_to_phys(protect), MTK_PROTECT_PA_ALIGN);

	/* Whether the current dram is 4GB. */
	data->dram_is_4gb = !!(max_pfn > (BIT_ULL(32) >> PAGE_SHIFT));
	spin_lock_init(&data->reg_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);
	ioaddr = res->start;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0)
		return data->irq;

	if (data->plat_data->has_bclk == M4U_POWER_V1) {
		data->bclk = devm_clk_get(dev, "bclk");
		if (IS_ERR(data->bclk))
			return PTR_ERR(data->bclk);
	} else if (data->plat_data->has_bclk == M4U_POWER_V2) {
		data->power_initialized = false;
		pm_runtime_enable(dev);
		data->isr_ref = 0;

		ret = mtk_iommu_power_switch(data, true, "iommu_probe");
		if (ret) {
			pr_notice("%s, failed to power switch on\n", __func__);
			return ret;
		}
		data->poweron = true;
	}

	larb_nr = of_count_phandle_with_args(dev->of_node,
					     "mediatek,larbs", NULL);
	if (larb_nr < 0) {
		pr_notice("%s, larb count:%d\n",
			  __func__, larb_nr);
		return larb_nr;
	}

	for (i = 0; i < larb_nr; i++) {
		struct device_node *larbnode;
		struct platform_device *plarbdev;
		u32 id;

		larbnode = of_parse_phandle(dev->of_node, "mediatek,larbs", i);
		if (!larbnode) {
			pr_notice("%s, larb:%d is not existed\n",
				  __func__, i);
			return -EINVAL;
		}

		if (!of_device_is_available(larbnode)) {
			pr_notice("%s, device of larb:%d is not available\n",
				  __func__, i);
			continue;
		}

		ret = of_property_read_u32(larbnode, "mediatek,larb-id", &id);
		if (ret)/* The id is consecutive if there is no this property */
			id = i;

		plarbdev = of_find_device_by_node(larbnode);
		if (!plarbdev || !plarbdev->dev.driver) {
			pr_notice("%s, device of larb:%d is not initialized\n",
				  __func__, i);
			ret = -EPROBE_DEFER;
			goto err_pm_disable;
		}
		data->smi_imu.larb_imu[id].dev = &plarbdev->dev;
		pr_notice("%s, %d probe of larb:%d\n", __func__, __LINE__, id);

		if (data->plat_data->m4u1_mask == (1 << id))
			data->m4u_id = 1;

		component_match_add_release(dev, &match, release_of,
					    compare_of, larbnode);
	}

	platform_set_drvdata(pdev, data);

	ret = mtk_iommu_hw_init(data);
	if (ret)
		return ret;
	mtk_iommu_isr_pause_timer_init(data);

	ret = iommu_device_sysfs_add(&data->iommu, dev, NULL,
				     "mtk-iommu.%pa", &ioaddr);
	if (ret)
		return ret;

	iommu_device_set_ops(&data->iommu, &mtk_iommu_ops);
	iommu_device_set_fwnode(&data->iommu, &pdev->dev.of_node->fwnode);

	ret = iommu_device_register(&data->iommu);
	if (ret)
		return ret;

	list_add_tail(&data->list, &m4ulist);

	if (!iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, &mtk_iommu_ops);

	if (data->plat_data->has_bclk == M4U_POWER_V2) {
		data->power_initialized = true;
		ret = mtk_iommu_power_switch(data, false, "iommu_probe");
		if (ret)
			pr_notice("%s, failed to power switch off\n", __func__);
	}

	pr_notice("%s, %d iommu:%d probe done\n", __func__, __LINE__, data->m4u_id);
	return component_master_add_with_match(dev, &mtk_iommu_com_ops, match);

err_pm_disable:
	if (data->plat_data->has_bclk == M4U_POWER_V2)
		pm_runtime_disable(dev);
	return ret;
}

static void mtk_iommu_shutdown(struct platform_device *pdev)
{
	struct mtk_iommu_data *data = platform_get_drvdata(pdev);

	iommu_device_sysfs_remove(&data->iommu);
	iommu_device_unregister(&data->iommu);

	if (iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, NULL);

	if (data->plat_data->has_bclk == M4U_POWER_V1)
		clk_disable_unprepare(data->bclk);
	devm_free_irq(&pdev->dev, data->irq, data);

	component_master_del(&pdev->dev, &mtk_iommu_com_ops);
}

static int __maybe_unused mtk_iommu_suspend(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->plat_data->has_bclk == M4U_POWER_V2)
		return 0; /* power control by user*/

	ret = mtk_iommu_reg_backup(data);
	if (ret)
		pr_notice("%s, %d, iommu:%d, backup failed %d\n",
			  __func__, __LINE__, data->m4u_id, ret);

	if (data->plat_data->has_bclk == M4U_POWER_V1)
		clk_disable_unprepare(data->bclk);

	return 0;
}

static int __maybe_unused mtk_iommu_resume(struct device *dev)
{
	struct mtk_iommu_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (data->plat_data->has_bclk == M4U_POWER_V2)
		return 0; /* power control by user*/

	if (data->plat_data->has_bclk == M4U_POWER_V1) {
		ret = clk_prepare_enable(data->bclk);
		if (ret) {
			dev_err(data->dev, "Failed to enable clk(%d) in resume\n", ret);
			return ret;
		}
	}
	ret = mtk_iommu_reg_restore(data);
	if (ret)
		pr_notice("%s, %d, iommu:%d, restore failed %d\n",
			  __func__, __LINE__, data->m4u_id, ret);
	return ret;
}

static const struct dev_pm_ops mtk_iommu_pm_ops = {
#ifdef IOMMU_CLK_SUPPORT
	SET_RUNTIME_PM_OPS(mtk_iommu_pm_suspend, mtk_iommu_pm_resume, NULL)
#endif
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_iommu_suspend, mtk_iommu_resume)
};

static const struct mtk_iommu_resv_iova_region mt2712_iommu_rsv_list[] = {
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	{
		.dom_id = 0,
		.iova_base = 0x4d000000UL,	   /* FastRVC */
		.iova_size = 0x8000000,
		.type = IOMMU_RESV_DIRECT,
	},
#endif
};

static const struct mtk_iommu_resv_iova_region mt6779_iommu_rsv_list[] = {
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && \
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
	{
		.dom_id = 0,			/* Secure Region */
		.iova_base = 0x0,
		.iova_size = 0x13000000,
		.type = IOMMU_RESV_RESERVED,
	},
#endif
	{
		.dom_id = 0,
		.iova_base = 0x40000000,	/* CCU */
		.iova_size = 0x8000000,
		.type = IOMMU_RESV_RESERVED,
	},
	{
		.dom_id = 0,
		.iova_base = 0x7da00000,	/* VPU/MDLA */
		.iova_size = 0x2700000,
		.type = IOMMU_RESV_RESERVED,
	},
};

static const struct mtk_iommu_plat_data mt2712_data = {
	.m4u_plat     = M4U_MT2712,
	.has_4gb_mode = true,
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	.resv_cnt     = ARRAY_SIZE(mt2712_iommu_rsv_list),
	.resv_region  = mt2712_iommu_rsv_list,
#endif
	.has_bclk     = M4U_POWER_V1,
	.has_vld_pa_rng   = true,
	.dom_cnt = 1,
	.dom_data = &single_dom,
	.larbid_remap[0] = {0, 1, 2, 3},
	.larbid_remap[1] = {4, 5, 7, 8, 9},
	.inv_sel_reg = REG_MMU_INV_SEL,
	.m4u1_mask =  BIT(4),
};

static const struct mtk_iommu_plat_data mt6779_data = {
	.m4u_plat = M4U_MT6779,
	.resv_cnt     = ARRAY_SIZE(mt6779_iommu_rsv_list),
	.resv_region  = mt6779_iommu_rsv_list,
	.dom_cnt = ARRAY_SIZE(mt6779_multi_dom),
	.dom_data = mt6779_multi_dom,
	.larbid_remap[0] = {0, 1, 2, 3, 5, 7, 10, 9},
	/* vp6a, vp6b, mdla/core2, mdla/edmc*/
	.larbid_remap[1] = {2, 0, 3, 1},
	.has_sub_comm = {true, true},
	.has_wr_len = true,
	.has_misc_ctrl = {true, false},
	.inv_sel_reg = REG_MMU_INV_SEL_MT6779,
	.m4u1_mask =  BIT(6),
	.has_bclk     = M4U_POWER_NONE,
};

static const struct mtk_iommu_plat_data mt8173_data = {
	.m4u_plat     = M4U_MT8173,
	.has_4gb_mode = true,
	.has_bclk     = M4U_POWER_V1,
	.reset_axi    = true,
	.dom_cnt = 1,
	.dom_data = &single_dom,
	.larbid_remap[0] = {0, 1, 2, 3, 4, 5}, /* Linear mapping. */
	.inv_sel_reg = REG_MMU_INV_SEL,
};

static const struct mtk_iommu_plat_data mt8183_data = {
	.m4u_plat     = M4U_MT8183,
	.reset_axi    = true,
	.dom_cnt = 1,
	.dom_data = &single_dom,
	.larbid_remap[0] = {0, 4, 5, 6, 7, 2, 3, 1},
	.inv_sel_reg = REG_MMU_INV_SEL,
	.has_bclk     = M4U_POWER_NONE,
};

static const struct mtk_iommu_plat_data mt6880_data = {
	.m4u_plat = M4U_MT6880,
	.dom_cnt = 1,
	.dom_data = &single_dom,
	.larbid_remap[0] = {0}, /* 4 */
#ifdef IOMMU_CLK_SUPPORT
	.has_bclk     = M4U_POWER_V2,
#else
	.has_bclk     = M4U_POWER_NONE,
#endif
	.has_wr_len = true,
	.has_misc_ctrl = {true},
	.inv_sel_reg = REG_MMU_INV_SEL_MT6779,
};

static const struct of_device_id mtk_iommu_of_ids[] = {
	{ .compatible = "mediatek,mt2712-m4u", .data = &mt2712_data},
	{ .compatible = "mediatek,mt6779-m4u", .data = &mt6779_data},
	{ .compatible = "mediatek,mt8173-m4u", .data = &mt8173_data},
	{ .compatible = "mediatek,mt8183-m4u", .data = &mt8183_data},
	{ .compatible = "mediatek,mt6880-m4u", .data = &mt6880_data},
	{}
};

static struct platform_driver mtk_iommu_driver = {
	.probe	= mtk_iommu_probe,
	.shutdown = mtk_iommu_shutdown,
	.driver	= {
		.name = "mtk-iommu",
		.of_match_table = of_match_ptr(mtk_iommu_of_ids),
		.pm = &mtk_iommu_pm_ops,
	}
};
builtin_platform_driver(mtk_iommu_driver);
