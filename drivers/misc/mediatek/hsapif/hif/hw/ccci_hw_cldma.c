// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "ccci_hw_cldma.h"
#include "ccci_hif_cldma.h"
#include "ccci_hif_driver.h"

#include "ccci_msg_id.h"
#include "ccci_msg_data.h"
#include "ccci_msg_center.h"
#include "ccci_hif.h"

#define TAG "cldma_hw"



struct ccci_cldma_hw_ctrl *ccci_hw_ctrls[HIF_ID_MAX] = {NULL};



unsigned int ccci_cldma_hw_queue_ismask(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx)
{
	if (!tx_rx)
		return cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMR0) & (1 << qno);

	else
		return cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_L2RIMR0) & (1 << qno);
}

unsigned int ccci_cldma_hw_queue_isactive(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx)
{
     if (!tx_rx)
        return (cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_STATUS) & (1 << qno)) ? 1 : 0;

	else
        return (cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_SO_STATUS) & (1 << qno)) ? 1 : 0;
}

void ccci_cldma_hw_dismask_txrxirq(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx)
{
	if (!tx_rx)
	    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMCR0, (1 << qno));

    else
	    cldma_write32(hw_info->cldma_ap_ao_base, REG_CLDMA_L2RIMCR0, (1 << qno));
}

void ccci_cldma_hw_dismask_eqirq(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx)
{
	if (!tx_rx)
	    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMCR0,((1 << qno) << EQ_STA_BIT_OFFSET));

	else
		cldma_write32(hw_info->cldma_ap_ao_base, REG_CLDMA_L2RIMCR0,((1 << qno) << EQ_STA_BIT_OFFSET));
}

void ccci_cldma_hw_resume_queue(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx)
{
    if (!tx_rx)
        cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_RESUME_CMD, (1 << qno));

    else
		cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_RESUME_CMD, (1 << qno));
}

void ccci_cldma_hw_mask_txrxirq(
		struct ccci_cldma_hw_info *hw_info ,
		unsigned char qno,
		unsigned char tx_rx)
{
	if (!tx_rx)
	    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMSR0, (1 << qno));

	else
        cldma_write32(hw_info->cldma_ap_ao_base, REG_CLDMA_L2RIMSR0, (1 << qno));
}

void ccci_cldma_hw_mask_eqirq(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx)
{
	if (!tx_rx)
	    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMSR0, ((1 << qno) << EQ_STA_BIT_OFFSET));

	else
		cldma_write32(hw_info->cldma_ap_ao_base, REG_CLDMA_L2RIMSR0, ((1 << qno) << EQ_STA_BIT_OFFSET));
}


unsigned int ccci_cldma_hw_tx_done(
		struct ccci_cldma_hw_info *hw_info,
		unsigned int bitmask)
{
	unsigned int ch_id;

	ch_id = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TISAR0) & bitmask;

	/*ack interrupt*/
	CCCI_DEBUG_LOG(-1, TAG, "[%s]: ch_id %x\n",__FUNCTION__, ch_id);

	cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TISAR0 , ch_id);

	return ch_id;
}

unsigned int ccci_cldma_hw_rx_done(
		struct ccci_cldma_hw_info *hw_info,
		unsigned int bitmask)
{
	unsigned int ch_id;

	ch_id = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2RISAR0) & bitmask;

	/*ack interrupt*/
	CCCI_DEBUG_LOG(-1, TAG, "[%s]: ch_id %x\n", __FUNCTION__, ch_id);

	cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2RISAR0 , ch_id);

	return ch_id;
}

void ccci_cldma_hw_error_check(struct ccci_cldma_hw_info *hw_info)
{
	CCCI_ERROR_LOG(-1, TAG, "[%s] HW ERROR\n", __func__);

	CCCI_ERROR_LOG(-1, TAG, "[%s] L2TISAR1: 0x%x, L2RISAR1: 0x%x, L2RISAR0: 0x%x\n",
		__func__,
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TISAR1),
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2RISAR1),
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2RISAR0)
	);

	CCCI_ERROR_LOG(-1, TAG, "[%s] L3TISAR0: 0x%x, L3TISAR1: 0x%x\n",
		__func__,
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L3TISAR0),
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L3TISAR1));

	CCCI_ERROR_LOG(-1, TAG, "[%s] L3RISAR0: 0x%x, L3RISAR1: 0x%x\n",
		__func__,
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L3RISAR0),
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L3RISAR1));

	CCCI_ERROR_LOG(-1, TAG, "[%s] L2TIMR0: 0x%x, L2RIMR0: 0x%x\n",
		__func__,
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMR0),
        cldma_read32(hw_info->cldma_ap_ao_base,  REG_CLDMA_L2RIMR0));

}

unsigned int ccci_cldma_hw_txrxirq_ismask(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char tx_rx)
{
	if (!tx_rx)
        return cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMR0) & TXRX_STATUS_BITMASK;

	else
        return cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_L2RIMR0) & TXRX_STATUS_BITMASK;
}

unsigned int ccci_cldma_hw_eqirq_ismask(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char tx_rx)
{
    if (!tx_rx)
        return cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMR0) & EMPTY_STATUS_BITMASK;

	else
		return cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_L2RIMR0) & EMPTY_STATUS_BITMASK;
}

unsigned int ccci_cldma_hw_get_status(
		struct ccci_cldma_hw_info *hw_info,
		unsigned int bitmask,
		unsigned char tx_rx)
{
	if (!tx_rx)
		return cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TISAR0) & bitmask;

	else
		return cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2RISAR0) & bitmask;
}

static int cldma_hw_create_md_cd_ctrl(u8 hif_id, ccci_init_data_t *pdata)
{
	struct ccci_md_cd_ctrl *md_ctrl;

	md_ctrl = kzalloc(sizeof(struct ccci_md_cd_ctrl), GFP_KERNEL);
	if (!md_ctrl) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kzalloc ccci_md_cd_ctrl.\n", __func__);
		return -1;
	}

	md_ctrl->hif_id = hif_id;

	ccci_md_ctrls[hif_id] = md_ctrl;

	return 0;
}

static struct ccci_cldma_hw_info *create_cldma_hw_info(
		struct ccci_cldma_hw_ctrl *hw_ctrl)
{
	struct ccci_cldma_hw_info *hw_info;

	hw_info = kzalloc(sizeof(struct ccci_cldma_hw_info), GFP_KERNEL);
	if (!hw_info) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kzalloc ccci_cldma_hw_ctrl.\n", __func__);
		return NULL;
	}

	hw_info->cldma_phy_ao_base = CLDMA_R_AO_BASE;
	hw_info->cldma_phy_pd_base = CLDMA_R_PD_BASE;
	hw_info->cldma_phy_interrupt_id = CLDMA0_R_INT;
	hw_info->direction = sAP_TO_eAP;
	hw_info->hw_mode = MODE_BIT_64;

	//hw_ctrl->irqops.register_isr = register_isr_legacy;
	//hw_ctrl->irqops.enable_irqs = enable_irq_legacy;
	//hw_ctrl->irqops.disable_irq = disable_irq_legacy;

	hw_info->hif_id   = hw_ctrl->hif_id;
	hw_info->pcie_dev = hw_ctrl->dma_dev;
	hw_ctrl->hw_info  = hw_info;

	ccci_md_ctrls[hw_ctrl->hif_id]->hif_hw_info = hw_info;

	return hw_info;
}

static int cldma_hw_info_init(u8 hif_id, ccci_init_data_t *pdata)
{
	struct ccci_cldma_hw_info *hw_info = NULL;

	if (!ccci_hw_ctrls[hif_id]) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: hw_ctrl%d is NULL.\n",
			__func__, hif_id);
		return -1;
	}

	if (hif_id == HIF_ID_CLDMA)
		hw_info = create_cldma_hw_info(ccci_hw_ctrls[hif_id]);


	return (hw_info ? 0 : -1);
}

static int cldma_hw_ctrl_init(u8 hif_id, ccci_init_data_t *pdata)
{
	struct ccci_cldma_hw_ctrl *hw_ctrl = NULL;

	CCCI_NORMAL_LOG(-1, TAG, "[%s]\n", __func__);

	hw_ctrl = kzalloc(sizeof(struct ccci_cldma_hw_ctrl), GFP_KERNEL);
	if (!hw_ctrl) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kzalloc ccci_cldma_hw_ctrl.\n", __func__);
		return -1;
	}

	hw_ctrl->hif_id = hif_id;
	hw_ctrl->dma_dev = &(pdata->plat_dev->dev);
	hw_ctrl->hw_info = NULL;

	ccci_hw_ctrls[hw_ctrl->hif_id] = hw_ctrl;

	return 0;
}

static void cldma_reg_set_domain_id(u8 hif_id)
{
	u32 cldma_domain_id_val = 0;
	u32 read_cldma_domain_id = 0;
	u32 cldma_int_sap_mask = 0;
	u32 read_cldma_int_sap_mask = 0;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[hif_id];

	cldma_domain_id_val =
		((DOMAIN_ID_PCIE << DOMAIN_ID_LEFT_HAND_SHIFT) |
		(DOMAIN_ID_SAP << DOMAIN_ID_RIGHT_HAND_SHIFT) |
		(DOMAIN_ID_PCIE << DOMAIN_ID_PCIE_SHIFT) |
		(DOMAIN_ID_MD << DOMAIN_ID_MD_SHIFT));

	CCCI_NORMAL_LOG(-1, TAG, "[%s] VA=%px, Wrtie REG_CLDMA_DOMAIN_ID=0x%08x\n",
		__func__,
		(hw_ctrl->hw_info->cldma_ap_ao_base+REG_CLDMA_DOMAIN_ID),
		cldma_domain_id_val);

	cldma_write32(hw_ctrl->hw_info->cldma_ap_ao_base,
		REG_CLDMA_DOMAIN_ID , cldma_domain_id_val);

	read_cldma_domain_id =
		(cldma_read32(hw_ctrl->hw_info->cldma_ap_ao_base, REG_CLDMA_DOMAIN_ID));

	CCCI_NORMAL_LOG(-1, TAG, "[%s] VA=%px, Read REG_CLDMA_DOMAIN_ID=0x%08x\n",
		__func__,
		(hw_ctrl->hw_info->cldma_ap_ao_base+REG_CLDMA_DOMAIN_ID),
		read_cldma_domain_id);

	CCCI_NORMAL_LOG(-1, TAG, "[%s] VA=%px, Wrtie REG_CLDMA_INT_SAP_MASK=%d\n",
		__func__,
		(hw_ctrl->hw_info->cldma_ap_ao_base+REG_CLDMA_INT_SAP_MASK),
		cldma_int_sap_mask);

	cldma_write32(hw_ctrl->hw_info->cldma_ap_ao_base,
		REG_CLDMA_INT_SAP_MASK, cldma_int_sap_mask);

	read_cldma_int_sap_mask =
		(cldma_read32(hw_ctrl->hw_info->cldma_ap_ao_base, REG_CLDMA_INT_SAP_MASK) & 0xFFFF);

	CCCI_NORMAL_LOG(-1, TAG, "[%s] VA=%px, Read REG_CLDMA_INT_SAP_MASK=%d\n",
		__func__,
		(hw_ctrl->hw_info->cldma_ap_ao_base+REG_CLDMA_INT_SAP_MASK),
		read_cldma_int_sap_mask);

}


void clear_cldma0_ip_busy_status_reg(CLEAR_CLDMA_IP_BUSY_STAGE stage)
{
	u32 cldmna_rh_ip_busy_status = 1;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[HIF_ID_CLDMA];

	/* clear IP busy register wake up cpu case */
	cldmna_rh_ip_busy_status =
		cldma_read32(hw_ctrl->hw_info->cldma_ap_pdn_base, REG_CLDMA_IP_BUSY);

	cldma_write32(hw_ctrl->hw_info->cldma_ap_pdn_base, REG_CLDMA_IP_BUSY,
		cldmna_rh_ip_busy_status);

	CCCI_NORMAL_LOG(-1, TAG,
	"[%s] S=%d,CLDMA IP_BUSY=0x%08x and Clear it to 0\n",
	__func__, stage, cldmna_rh_ip_busy_status);

}

void dump_cldma0_ip_busy_status_reg(DUMP_CLDMA_IP_BUSY_STAGE stage)
{
	u32 cldma0_rh_ip_busy_val = 1;
	u32 cldma0_lh_ip_busy_val = 1;
	u32 cldma0_rh_ip_busy_mask_val = 0;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[HIF_ID_CLDMA];

	/*(LH:Left Hand) CLDMA0_AP_IP_BUSY = 0x1021_D800+0xB4*/
	cldma0_lh_ip_busy_val =
		cldma_read32(hw_ctrl->hw_info->cldma0_lh_indma_pd_base, REG_CLDMA_IP_BUSY);


	/*(RH:Right Hand) CLDMA0_MD_IP_BUSY = 0x1021_E800+0xB4*/
		cldma0_rh_ip_busy_val =
			cldma_read32(hw_ctrl->hw_info->cldma_ap_pdn_base, REG_CLDMA_IP_BUSY);

	cldma0_rh_ip_busy_mask_val =
		cldma_read32(hw_ctrl->hw_info->cldma_ap_ao_base,
				REG_CLDMA_INT_BUSY_MASK);

	CCCI_NORMAL_LOG(-1, TAG, "[%s] S=%d CLDMA0_MD_IP_BUSY=0x%08x, CLDMA0_MD_IP_BUSY_MASK=%d, CLDMA0_AP_IP_BUSY=0x%08x\n",
		__func__, stage, cldma0_rh_ip_busy_val, cldma0_rh_ip_busy_mask_val, cldma0_lh_ip_busy_val);
}



static int cldma_hw_set_irq_and_base(u8 hif_id, ccci_init_data_t *pdata)
{
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[hif_id];

	if (!hw_ctrl || !hw_ctrl->hw_info)
		return -1;

#if 0
	hw_ctrl->hw_info->cldma_ap_ao_base =
		ioremap_nocache(hw_ctrl->hw_info->cldma_phy_ao_base, 0x1000);
	hw_ctrl->hw_info->cldma_ap_pdn_base =
		ioremap_nocache(hw_ctrl->hw_info->cldma_phy_pd_base, 0x1000);
#endif

	/*(LH:Left Hand) CLDMA0_INDMA_PD_AP = 0x1021_D000 (i.e., M80 coda's definition)*/
	/*LH is used in eAP Host side instead of sAP Device side*/
	/*IOMAP LH side is used for debugging purpose to dump Host side IP_BUSY status*/
	hw_ctrl->hw_info->cldma0_lh_indma_pd_base =
		ioremap_nocache(0x1021D000, 0x1000);

	/*Read from DTS setting*/
	hw_ctrl->hw_info->cldma_ap_ao_base =
		of_iomap(hw_ctrl->dma_dev->of_node, 0);
	hw_ctrl->hw_info->cldma_ap_pdn_base =
		of_iomap(hw_ctrl->dma_dev->of_node, 1);

	hw_ctrl->hw_info->cldma_irq_flags = IRQF_TRIGGER_NONE;
	hw_ctrl->hw_info->cldma_irq_id =
		irq_of_parse_and_map(hw_ctrl->dma_dev->of_node, 0);

	/*Bring-up:If DOMAIN_ID is configed & verified in LK, it can be skipped in kernel*/
	cldma_reg_set_domain_id(hif_id);

	CCCI_NORMAL_LOG(-1, TAG, "[%s] cldma_irq_id: %d\n",
		__func__, hw_ctrl->hw_info->cldma_irq_id);

	dump_cldma0_ip_busy_status_reg(DUMP_IP_BUSY_INIT);

	return 0;
}

int ccci_cldma_hw_set_info(struct ccci_cldma_hw_ctrl *hw_ctrl)
{
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;
	u32 ao_read_value, pdn_read_value, ao_write_value, pdn_write_value;
#ifdef __Apollo_M70__
	void __iomem *tmpreg;
#else
	u32 cldma_int_sap_mask = 0;
	u32 read_cldma_int_sap_mask = 0;
#endif

	/* set CLDMA to 64 bit mode GPD/BD */
	if (hw_info->hw_mode == MODE_BIT_64) {
		ao_write_value = (0x4 << 10);
		pdn_write_value = (0x4 << 5);

	} else if (hw_info->hw_mode == MODE_BIT_40) {
		ao_write_value = (0x2 << 10);
		pdn_write_value = (0x2 << 5);

	} else if (hw_info->hw_mode == MODE_BIT_36) {
		ao_write_value = (0x1 << 10);
		pdn_write_value = (0x1 << 5);

	} else {
		CCCI_NORMAL_LOG(0, TAG,
			"[%s] error: hw_mode value(%d) is incorrect\n",
			__func__, hw_info->hw_mode);
		return -1;

	}
	ao_read_value = cldma_read32(hw_info->cldma_ap_ao_base,
			REG_CLDMA_SO_CFG);
	pdn_read_value = cldma_read32(hw_info->cldma_ap_pdn_base,
			REG_CLDMA_UL_CFG);

	cldma_write32(hw_info->cldma_ap_ao_base, REG_CLDMA_SO_CFG,
			(ao_read_value & (~(0x7 << 10))) | ao_write_value);
	cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_CFG,
			(pdn_read_value & (~(0x7 << 5))) | pdn_write_value);

	if (hw_info->direction == sAP_TO_eAP) {
#ifdef __Apollo_M70__
		tmpreg = ioremap_nocache(0x1001C000 + 0x1A08, 0x8);
//		cldma_write32(tmpreg, 0, (cldma_read32(tmpreg, 0)&(~(0xFFF<<20)))|0x505<<20);

		/* mask interrupt signal */
		cldma_write32(hw_info->cldma_ap_ao_base,
				REG_CLDMA_INT_MASK, 0x6);

	    /* mask wakeup signal */
		cldma_write32(hw_info->cldma_ap_ao_base,
				REG_CLDMA_BUSY_MASK, 0x5);

		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] CLDMA0 register 0x%x: 0x%x\n",
			__func__, (0x1001C000+0x1A08),
			cldma_read32(tmpreg, 0));
#else
		/* mask interrupt signal: default is 0xFF, we change value to 0 (i.e.,Enable Interrupt)*/
		cldma_write32(hw_info->cldma_ap_ao_base,
				REG_CLDMA_INT_SAP_MASK, 0x0);
		CCCI_NORMAL_LOG(-1, TAG, "[%s] VA=%px, Wrtie REG_CLDMA_INT_SAP_MASK=%d\n",
			__func__,
			(hw_info->cldma_ap_ao_base+REG_CLDMA_INT_SAP_MASK),
			cldma_int_sap_mask);

		cldma_write32(hw_info->cldma_ap_ao_base,
			REG_CLDMA_INT_SAP_MASK, cldma_int_sap_mask);

		read_cldma_int_sap_mask =
			(cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_INT_SAP_MASK) & 0xFFFF);

		CCCI_NORMAL_LOG(-1, TAG, "[%s] VA=%px, Read REG_CLDMA_INT_SAP_MASK=%d\n",
			__func__,
			(hw_info->cldma_ap_ao_base+REG_CLDMA_INT_SAP_MASK),
			read_cldma_int_sap_mask);

		/* mask wakeup signal */
		/*BUSY_MASK register: bit#0: PCIE_BUSY, bit#1: sAP_BUSY, bit#2: MD_BUSY*/
		/*Now, left hand is PCIe and right hand is sAP => Both their BUSY signal must to report to SPM*/
		/*In this scenario, only MD_BUSY (i.e., bit#2) signal must be disabled: configure bit#2 as 1=> i.e., reg_value=4*/
		cldma_write32(hw_info->cldma_ap_ao_base,
				REG_CLDMA_INT_BUSY_MASK, 0x4);

#endif

	} else {
		/* mask interrupt signal */
		cldma_write32(hw_info->cldma_ap_ao_base,
				REG_CLDMA_INT_MASK, 0x5);

	    /* mask wakeup signal */
		cldma_write32(hw_info->cldma_ap_ao_base,
				REG_CLDMA_BUSY_MASK, 0x4);

	}

    /* diable TX and RX invalid address check */
	cldma_write32(hw_info->cldma_ap_pdn_base,
			REG_CLDMA_UL_DUMMY_0, 1);
	cldma_write32(hw_info->cldma_ap_pdn_base,
			REG_CLDMA_SO_DUMMY_0, 1);

	return 0;
}

void ccci_cldma_hw_set_start_address(
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		unsigned char qno,
		u64 address ,
		unsigned char rx_tx)//0 tx,1 rx
{
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;

	if (!rx_tx) {
		CCCI_DEBUG_LOG(-1, TAG, "[%s] Tx , address: 0x%llx in %p\n",
			__func__,
			address, (hw_info->cldma_ap_pdn_base) + REG_CLDMA_UL_START_ADDRL_0);

		W_CLDMA_QSAR32L(((hw_info->cldma_ap_pdn_base) + REG_CLDMA_UL_START_ADDRL_0), (qno), (address));
		W_CLDMA_QSAR32H(((hw_info->cldma_ap_pdn_base) + REG_CLDMA_UL_START_ADDRL_0), (qno), (address));

		CCCI_DEBUG_LOG(-1, TAG, "[%s] Tx , address:L %x H %x\n",
			__func__,
			cldma_read32(hw_info->cldma_ap_pdn_base,REG_CLDMA_UL_START_ADDRL_0 + qno * 8),
			cldma_read32(hw_info->cldma_ap_pdn_base,REG_CLDMA_UL_START_ADDRL_0 + qno * 8 +4));

	} else {
	    CCCI_DEBUG_LOG(-1, TAG, "[%s] Rx , address: 0x%llx in %p\n",
	    	__func__,
	    	address, (hw_info->cldma_ap_ao_base) + REG_CLDMA_SO_START_ADDRL_0);

	    W_CLDMA_QSAR32L(((hw_info->cldma_ap_ao_base)+REG_CLDMA_SO_START_ADDRL_0), (qno), (address));
		W_CLDMA_QSAR32H(((hw_info->cldma_ap_ao_base)+REG_CLDMA_SO_START_ADDRL_0), (qno), (address));

		CCCI_DEBUG_LOG(-1, TAG, "[%s] Rx , address:L %x H %x\n",
			__func__,
			cldma_read32(hw_info->cldma_ap_ao_base,REG_CLDMA_SO_START_ADDRL_0 + qno * 8),
			cldma_read32(hw_info->cldma_ap_ao_base,REG_CLDMA_SO_START_ADDRL_0 + qno * 8 +4));

	}
}

void ccci_cldma_hw_set_start_address_ao_backup(
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		unsigned char qno,
		u64 address ,
		unsigned char rx_tx)//0 tx,1 rx
{
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;

	if (!rx_tx) {
		CCCI_DEBUG_LOG(-1, TAG, "[%s] Tx , address: 0x%llx in %p\n",
			__func__,
			address, (hw_info->cldma_ap_ao_base) + REG_CLDMA_UL_START_ADDRL_0_AO);

		W_CLDMA_QSAR32L(((hw_info->cldma_ap_ao_base) + REG_CLDMA_UL_START_ADDRL_0_AO), (qno), (address));
		W_CLDMA_QSAR32H(((hw_info->cldma_ap_ao_base) + REG_CLDMA_UL_START_ADDRL_0_AO), (qno), (address));

		CCCI_DEBUG_LOG(-1, TAG, "[%s] Tx , address:L %x H %x\n",
			__func__,
			cldma_read32(hw_info->cldma_ap_ao_base,REG_CLDMA_UL_START_ADDRL_0_AO + qno * 8),
			cldma_read32(hw_info->cldma_ap_ao_base,REG_CLDMA_UL_START_ADDRL_0_AO + qno * 8 +4));

	} else {
		CCCI_ERROR_LOG(-1, TAG, "[%s] No need to set RX backup, because RX is in AO\n",
			__func__);
	}
}



/*****************************************************************************
 * FUNCTION
 *  cldma_hw_init
 * DESCRIPTION
 *  This function will init CLDMA HW: such as config GPD/BD mode, interrupt mask, busy signal mask, etc.
 * PARAMETERS
 *  base_info: mapped AO+PD base address structure pointer
 *****************************************************************************/
void ccci_cldma_hw_queue_start(
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		u8 qno,
		unsigned char tx_rx)//0 tx,1 rx, AP view
{
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;

	CCCI_DEBUG_LOG(-1, TAG, "[%s], %d\n", __func__, tx_rx);

	if (!tx_rx) {
		if (0xff == qno)
            cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_START_CMD, TXRX_STATUS_BITMASK);
		else
			cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_START_CMD, (1 << qno));

	} else {
	    if (0xff == qno)
	        cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_START_CMD, TXRX_STATUS_BITMASK);
		else
			cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_START_CMD, (1 << qno));

    }
}

void ccci_cldma_hw_start(struct ccci_cldma_hw_ctrl *hw_ctrl)
{
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;

	/*diamask txrx interrupt*/
	cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMCR0, TXRX_STATUS_BITMASK);
	cldma_write32(hw_info->cldma_ap_ao_base,  REG_CLDMA_L2RIMCR0, TXRX_STATUS_BITMASK);

	/*dismask empty queue interrupt*/
    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMCR0, EMPTY_STATUS_BITMASK);
    cldma_write32(hw_info->cldma_ap_ao_base,  REG_CLDMA_L2RIMCR0, EMPTY_STATUS_BITMASK);

}

void ccci_cldma_hw_stop_queue(
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		u8 qno,
		u8 rx_tx)
{
    struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;

    if (!rx_tx) {
		if (0xFF != qno) {
	        cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_STOP_CMD, 1<<qno);
			while(cldma_read32(hw_info->cldma_ap_pdn_base,REG_CLDMA_UL_STATUS) &&(1<<qno));

		} else {
		    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_STOP_CMD, qno);
			while(cldma_read32(hw_info->cldma_ap_pdn_base,REG_CLDMA_UL_STATUS) && qno);
		}

	} else {
		if (0xFF != qno){
		    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_STOP_CMD, 1<<qno);
		    while(cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_SO_STATUS) && (1 <<qno));

		} else {
            cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_STOP_CMD, qno);
		    while(cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_SO_STATUS) && qno);
		}

	}
}

void ccci_cldma_hw_stop(
		struct ccci_cldma_hw_ctrl *hw_ctrl)
{
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;

	/*mask txrx interrupt*/
    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMSR0, TXRX_STATUS_BITMASK);
    cldma_write32(hw_info->cldma_ap_ao_base,  REG_CLDMA_L2RIMSR0, TXRX_STATUS_BITMASK);

    /*mask empty queue interrupt*/
    cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMSR0, EMPTY_STATUS_BITMASK);
    cldma_write32(hw_info->cldma_ap_ao_base,  REG_CLDMA_L2RIMSR0, EMPTY_STATUS_BITMASK);
}

//int ccci_cldma_hw_init(
//		int           msg_id,
//		unsigned int  sub_id,
//		void         *msg_data,
//		void         *my_data)

int ccci_cldma_hw_init(ccci_init_data_t *pdata)
{
	if (cldma_hw_create_md_cd_ctrl(HIF_ID_CLDMA, pdata) < 0)
		return -1;

	if (cldma_hw_ctrl_init(HIF_ID_CLDMA, pdata) < 0)
		return -1;

	if (cldma_hw_info_init(HIF_ID_CLDMA, pdata) < 0)
		return -1;

	if (cldma_hw_set_irq_and_base(HIF_ID_CLDMA, pdata) < 0)
		return -1;


	return 0;
}
