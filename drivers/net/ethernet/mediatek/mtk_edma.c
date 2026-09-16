// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2018-2019 MediaTek Inc.
/* A library for MediaTek EDMA
 *
 * Author: FangRu Wang <fangju.wang@mediatek.com>
 *
 */


#include <linux/of_device.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/if_vlan.h>
#include <linux/reset.h>
#include <linux/tcp.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/mdio.h>
#include <linux/regulator/consumer.h>

#include "mtk_eth_soc.h"
#include "mtk_eth_dbg.h"

#if defined(CONFIG_HW_NAT)
#include <net/ra_nat.h>
#endif

extern int (*ppe_hook_rx_ext)(struct sk_buff *skb);

static inline int mtk_max_frag_size(int mtu)
{
	/* make sure buf_size will be at least MTK_MAX_RX_LENGTH */
	if (mtu + MTK_RX_ETH_HLEN < MTK_MAX_RX_LENGTH)
		mtu = MTK_MAX_RX_LENGTH - MTK_RX_ETH_HLEN;

	return SKB_DATA_ALIGN(MTK_RX_HLEN + mtu) +
		SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
}

static inline int mtk_max_buf_size(int frag_size)
{
	int buf_size = frag_size - NET_SKB_PAD - NET_IP_ALIGN -
		       SKB_DATA_ALIGN(sizeof(struct skb_shared_info));

	WARN_ON(buf_size < MTK_MAX_RX_LENGTH);

	return buf_size;
}


static inline void mtk_edma_rx_get_desc(struct mtk_rx_edma *rxd,
				   struct mtk_rx_edma *dma_rxd)
{
	rxd->rxd1 = READ_ONCE(dma_rxd->rxd1);
	rxd->rxd2 = READ_ONCE(dma_rxd->rxd2);
	rxd->rxd3 = READ_ONCE(dma_rxd->rxd3);
	rxd->rxd4 = READ_ONCE(dma_rxd->rxd4);
	rxd->rxd5 = READ_ONCE(dma_rxd->rxd5);
	rxd->rxd6 = READ_ONCE(dma_rxd->rxd6);
	rxd->rxd7 = READ_ONCE(dma_rxd->rxd7);
	rxd->rxd8 = READ_ONCE(dma_rxd->rxd8);
}

static inline void mtk_rx_irq_disable_edma0(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_EDMA0_INT_MASK);
	mtk_w32(eth, val & ~mask, MTK_EDMA0_INT_MASK);
}

static inline void mtk_rx_irq_enable_edma0(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_EDMA0_INT_MASK);
	mtk_w32(eth, val | mask , MTK_EDMA0_INT_MASK);
}

static inline void mtk_rx_irq_disable_edma1(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_EDMA1_INT_MASK);
	mtk_w32(eth, val & ~mask, MTK_EDMA1_INT_MASK);
}

static inline void mtk_rx_irq_enable_edma1(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_EDMA1_INT_MASK);
	mtk_w32(eth, val | mask , MTK_EDMA1_INT_MASK);
}


irqreturn_t mtk_handle_irq_rx_edma0(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (likely(napi_schedule_prep(&eth->rx_napi_edma0))) {
		__napi_schedule(&eth->rx_napi_edma0);
	mtk_rx_irq_disable_edma0(eth, MTK_RX_DONE_DLY);

	}

	return IRQ_HANDLED;
}

irqreturn_t mtk_handle_irq_rx_edma1(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (likely(napi_schedule_prep(&eth->rx_napi_edma1))) {
		__napi_schedule(&eth->rx_napi_edma1);
		mtk_rx_irq_disable_edma1(eth, MTK_RX_DONE_DLY);
	}

	return IRQ_HANDLED;
}


int mtk_edma_request_irq(struct mtk_eth *eth) {

	int err = 0;

	err = devm_request_irq(eth->dev, eth->irq[6], mtk_handle_irq_rx_edma0,
				   IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
				   "eth-e0rx0", eth);

	err = devm_request_irq(eth->dev, eth->irq[7], mtk_handle_irq_rx_edma1,
				   IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
				   "eth-e1rx0", eth);
	return err;
}


int mtk_poll_rx_edma0(struct napi_struct *napi, int budget,
		       struct mtk_eth *eth)
{
	struct mtk_rx_ring *ring;
	int idx;
	struct sk_buff *skb;
	u8 *data, *new_data;
	struct mtk_rx_edma *rxd, trxd;
	int done = 0;

	struct EDMA_RXD2_T *rxd2_ptr;
	struct EDMA_RXD3_T *rxd3_ptr;
	struct EDMA_RXD5_T *rxd5_ptr;

	while (done < budget) {
		struct net_device *netdev;
		unsigned int pktlen;
		dma_addr_t dma_addr;
		int mac = 0;

		ring = &eth->rx_ring_edma0[0];
		idx = (ring->calc_idx + 1) % (ring->dma_size);

		if (unlikely(!ring))
			goto rx_done;

		rxd = &ring->edma[idx];
		data = ring->data[idx];
		mtk_edma_rx_get_desc(&trxd, rxd);

		rxd2_ptr = (struct EDMA_RXD2_T *) &trxd.rxd2;
		if (rxd2_ptr->DDONE_bit  == 1) {
			ring->calc_idx_update = true;
		} else {
			ring = NULL;
		}

		if (unlikely(!ring))
			goto rx_done;

		if (rxd2_ptr->DDONE_bit  == 0){
			break;
		}

		//SPORT
		rxd5_ptr = (struct EDMA_RXD5_T *) &ring->edma[idx].rxd5;
		mac = rxd5_ptr->SP & 0xf;
		mac--;
/////////////////////////////////////////////
#if defined(CONFIG_HW_NAT)
		if (mac > 1)
			mac = 1;
#endif
		if (unlikely(mac < 0 || mac >= MTK_MAC_COUNT ||
			     !eth->netdev[mac]))
			goto release_desc;

		netdev = eth->netdev[mac];

		if (unlikely(test_bit(MTK_RESETTING, &eth->state)))
			goto release_desc;

		/* alloc new buffer */
		new_data = napi_alloc_frag(ring->frag_size);
		if (unlikely(!new_data)) {
			netdev->stats.rx_dropped++;
			goto release_desc;
		}
		dma_addr = dma_map_single(eth->dev,
					  new_data + NET_SKB_PAD,
					  ring->buf_size,
					  DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(eth->dev, dma_addr))) {
			skb_free_frag(new_data);
			netdev->stats.rx_dropped++;
			goto release_desc;
		}
		/* receive data */
		skb = build_skb(data, ring->frag_size);
		if (unlikely(!skb)) {
			skb_free_frag(new_data);
			netdev->stats.rx_dropped++;
			goto release_desc;
		}
		skb_reserve(skb, NET_SKB_PAD + NET_IP_ALIGN);

		dma_unmap_single(eth->dev, trxd.rxd1,
				 ring->buf_size, DMA_FROM_DEVICE);
		pktlen = (trxd.rxd2 >> 8) & 0xffff;

		skb->dev = netdev;
		skb_put(skb, pktlen);

		rxd3_ptr = (struct EDMA_RXD3_T *) &ring->edma[idx].rxd3;
		/* rx checksum offload */
		if (rxd3_ptr->L4VLD)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb_checksum_none_assert(skb);

#if defined(CONFIG_HW_NAT)
		if (ppe_hook_rx_ext) {
			if (IS_SPACE_AVAILABLE_HEAD(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_HEAD(skb)) = *(u32 *)&ring->edma[idx].rxd5;
				FOE_ALG(skb) = 0;
				FOE_MAGIC_TAG(skb) = FOE_MAGIC_EDMA0;
				FOE_TAG_PROTECT(skb) = TAG_PROTECT;
			}
			if (IS_SPACE_AVAILABLE_TAIL(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_TAIL(skb) + 2) = *(u32 *)&ring->edma[idx].rxd5;
				FOE_ALG_TAIL(skb) = 0;
				FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_EDMA0;
				FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
			}
		}
#endif

		skb->protocol = eth_type_trans(skb, netdev);

		//TODO: remove HW_VLAN code already

/* ppe_hook_rx_eth return 1 --> continue
 * ppe_hook_rx_eth return 0 --> FWD & without netif_rx
 */
#if defined(CONFIG_HW_NAT)
		if ((!ppe_hook_rx_ext) || (ppe_hook_rx_ext && ppe_hook_rx_ext(skb))) {
#endif
			skb_record_rx_queue(skb, 0);
			napi_gro_receive(napi, skb);
#if defined(CONFIG_HW_NAT)  || defined(CONFIG_HW_NAT_MODULE)
		}
#endif
		ring->data[idx] = new_data;
		ring->edma[idx].rxd1 = (unsigned int)dma_addr;

		rxd2_ptr = (struct EDMA_RXD2_T *) &ring->edma[idx].rxd2;
		rxd2_ptr->PLEN0 = ring->buf_size;
		rxd2_ptr->LS0 = 0;
		rxd2_ptr->DDONE_bit = 0;

release_desc:

		rxd2_ptr->PLEN0 = ring->buf_size;
		ring->calc_idx = idx;
		done++;
	}

rx_done:
	if (done) {
		/* make sure that all changes to the dma ring are flushed before
		 * we continue
		 */
		wmb();
		ring = &eth->rx_ring_edma0[0];
		if (ring->calc_idx_update) {
			ring->calc_idx_update = false;
			mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg + 0x3000);
		}
	}

	return done;
}

int mtk_poll_rx_edma1(struct napi_struct *napi, int budget,
		       struct mtk_eth *eth)
{
	struct mtk_rx_ring *ring;
	int idx;
	struct sk_buff *skb;
	u8 *data, *new_data;
	struct mtk_rx_edma *rxd, trxd;
	int done = 0;

	struct EDMA_RXD2_T *rxd2_ptr;
	struct EDMA_RXD3_T *rxd3_ptr;
	struct EDMA_RXD5_T *rxd5_ptr;

	while (done < budget) {
		struct net_device *netdev;
		unsigned int pktlen;
		dma_addr_t dma_addr;
		int mac = 0;

		ring = &eth->rx_ring_edma1[0];
		idx = (ring->calc_idx + 1) % (ring->dma_size);
		if (unlikely(!ring))
			goto rx_done;

		rxd = &ring->edma[idx];
		data = ring->data[idx];
		mtk_edma_rx_get_desc(&trxd, rxd);
		rxd2_ptr = (struct EDMA_RXD2_T *) &trxd.rxd2;
		if (rxd2_ptr->DDONE_bit  == 1) {
			ring->calc_idx_update = true;
		} else {
			ring = NULL;
		}
		if (unlikely(!ring))
			goto rx_done;

		if (rxd2_ptr->DDONE_bit  == 0)
			break;

		//SPORT
		rxd5_ptr = (struct EDMA_RXD5_T *) &ring->edma[idx].rxd5;
		mac = rxd5_ptr->SP & 0xf;
		mac--;
/////////////////////////////////////////////
#if defined(CONFIG_HW_NAT)
		if (mac > 1)
			mac = 1;
#endif
		if (unlikely(mac < 0 || mac >= MTK_MAC_COUNT ||
			     !eth->netdev[mac]))
			goto release_desc;

		netdev = eth->netdev[mac];

		if (unlikely(test_bit(MTK_RESETTING, &eth->state)))
			goto release_desc;

		/* alloc new buffer */
		new_data = napi_alloc_frag(ring->frag_size);
		if (unlikely(!new_data)) {
			netdev->stats.rx_dropped++;
			goto release_desc;
		}
		dma_addr = dma_map_single(eth->dev,
					  new_data + NET_SKB_PAD,
					  ring->buf_size,
					  DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(eth->dev, dma_addr))) {
			skb_free_frag(new_data);
			netdev->stats.rx_dropped++;
			goto release_desc;
		}
		/* receive data */
		skb = build_skb(data, ring->frag_size);
		if (unlikely(!skb)) {
			skb_free_frag(new_data);
			netdev->stats.rx_dropped++;
			goto release_desc;
		}
		skb_reserve(skb, NET_SKB_PAD + NET_IP_ALIGN);

		dma_unmap_single(eth->dev, trxd.rxd1,
				 ring->buf_size, DMA_FROM_DEVICE);
		pktlen = (trxd.rxd2 >> 8) & 0xffff;
		skb->dev = netdev;
		skb_put(skb, pktlen);

		rxd3_ptr = (struct EDMA_RXD3_T *) &ring->edma[idx].rxd3;
		/* rx checksum offload */
		if (rxd3_ptr->L4VLD)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb_checksum_none_assert(skb);

#if defined(CONFIG_HW_NAT)
		if (ppe_hook_rx_ext) {
			if (IS_SPACE_AVAILABLE_HEAD(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_HEAD(skb)) = *(u32 *)&ring->edma[idx].rxd5;
				FOE_ALG(skb) = 0;
				FOE_MAGIC_TAG(skb) = FOE_MAGIC_EDMA1;
				FOE_TAG_PROTECT(skb) = TAG_PROTECT;
			}
			if (IS_SPACE_AVAILABLE_TAIL(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_TAIL(skb) + 2) = *(u32 *)&ring->edma[idx].rxd5;
				FOE_ALG_TAIL(skb) = 0;
				FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_EDMA1;
				FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
			}
		}
#endif

		skb->protocol = eth_type_trans(skb, netdev);

		//TODO: remove HW_VLAN code already

/* ppe_hook_rx_eth return 1 --> continue
 * ppe_hook_rx_eth return 0 --> FWD & without netif_rx
 */
#if defined(CONFIG_HW_NAT)
		if ((!ppe_hook_rx_ext) || (ppe_hook_rx_ext && ppe_hook_rx_ext(skb))) {
#endif
			skb_record_rx_queue(skb, 0);
			napi_gro_receive(napi, skb);
#if defined(CONFIG_HW_NAT)  || defined(CONFIG_HW_NAT_MODULE)
		}
#endif
		ring->data[idx] = new_data;
		ring->edma[idx].rxd1 = (unsigned int)dma_addr;

		rxd2_ptr = (struct EDMA_RXD2_T *) &ring->edma[idx].rxd2;
		rxd2_ptr->PLEN0 = ring->buf_size;
		rxd2_ptr->LS0 = 0;
		rxd2_ptr->DDONE_bit = 0;

release_desc:

		rxd2_ptr->PLEN0 = ring->buf_size;
		ring->calc_idx = idx;
		done++;
	}

rx_done:
	if (done) {
		/* make sure that all changes to the dma ring are flushed before
		 * we continue
		 */
		wmb();
		ring = &eth->rx_ring_edma1[0];

		if (ring->calc_idx_update) {
			ring->calc_idx_update = false;
			mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg + 0x3400);
		}
	}
	return done;
}

int mtk_napi_rx_edma0(struct napi_struct *napi, int budget)
{
	struct mtk_eth *eth = container_of(napi, struct mtk_eth, rx_napi_edma0);
	u32 status, mask;
	int rx_done = 0;
	int remain_budget = budget;

	mtk_handle_status_irq(eth);

poll_again:
	mtk_w32(eth, MTK_RX_DONE_DLY, MTK_EDMA0_INT_STATUS);
	rx_done = mtk_poll_rx_edma0(napi, remain_budget, eth);

	if (unlikely(netif_msg_intr(eth))) {
		status = mtk_r32(eth, MTK_EDMA0_INT_STATUS);
		mask = mtk_r32(eth, MTK_EDMA0_INT_MASK);
		dev_info(eth->dev,
			 "done rx %d, intr 0x%08x/0x%x\n",
			 rx_done, status, mask);
	}
	if (rx_done == remain_budget)
		return budget;

	status = mtk_r32(eth, MTK_EDMA0_INT_STATUS);
	if (status & (MTK_RX_DONE_DLY)) {
		remain_budget -= rx_done;
		goto poll_again;
	}

	napi_complete(napi);
	mtk_rx_irq_enable_edma0(eth, MTK_RX_DONE_DLY);
	return rx_done + budget - remain_budget;
}

int mtk_napi_rx_edma1(struct napi_struct *napi, int budget)
{
	struct mtk_eth *eth = container_of(napi, struct mtk_eth, rx_napi_edma1);
	u32 status, mask;
	int rx_done = 0;
	int remain_budget = budget;

	mtk_handle_status_irq(eth);

poll_again:

	mtk_w32(eth, MTK_RX_DONE_DLY, MTK_EDMA1_INT_STATUS);
	rx_done = mtk_poll_rx_edma1(napi, remain_budget, eth);

	if (unlikely(netif_msg_intr(eth))) {
		status = mtk_r32(eth, MTK_EDMA1_INT_STATUS);
		mask = mtk_r32(eth, MTK_EDMA1_INT_MASK);
		dev_info(eth->dev,
			 "done rx %d, intr 0x%08x/0x%x\n",
			 rx_done, status, mask);
	}
	if (rx_done == remain_budget)
		return budget;

	status = mtk_r32(eth, MTK_EDMA1_INT_STATUS);
	if (status & (MTK_RX_DONE_DLY)) {
		remain_budget -= rx_done;
		goto poll_again;
	}

	napi_complete(napi);
	mtk_rx_irq_enable_edma1(eth, MTK_RX_DONE_DLY);
	return rx_done + budget - remain_budget;
}


void netif_napi_add_edma(struct mtk_eth *eth) {

	netif_napi_add(&eth->dummy_dev, &eth->rx_napi_edma0, mtk_napi_rx_edma0,
			   MTK_NAPI_WEIGHT);
	netif_napi_add(&eth->dummy_dev, &eth->rx_napi_edma1, mtk_napi_rx_edma1,
			   MTK_NAPI_WEIGHT);

}

void napi_enable_edma(struct mtk_eth *eth) {
	napi_enable(&eth->rx_napi_edma0);
	napi_enable(&eth->rx_napi_edma1);
}


void mtk_poll_edma(struct mtk_eth *eth, struct net_device *dev) {

	mtk_rx_irq_disable_edma0(eth, MTK_RX_DONE_DLY);
	mtk_rx_irq_disable_edma1(eth, MTK_RX_DONE_DLY);
	mtk_handle_irq_rx_edma0(eth->irq[6], dev);
	mtk_handle_irq_rx_edma1(eth->irq[7], dev);
	mtk_rx_irq_enable_edma0(eth, MTK_RX_DONE_DLY);
	mtk_rx_irq_enable_edma1(eth, MTK_RX_DONE_DLY);
}



void mtk_rx_irq_enable_edma_all(struct mtk_eth *eth) {
	mtk_rx_irq_enable_edma0(eth, MTK_RX_DONE_DLY);
	mtk_rx_irq_enable_edma1(eth, MTK_RX_DONE_DLY);
}


void mtk_rx_irq_disable_edma_all(struct mtk_eth *eth) {

	mtk_rx_irq_disable_edma0(eth, MTK_RX_DONE_DLY);
	mtk_rx_irq_disable_edma1(eth, MTK_RX_DONE_DLY);
}


void mtk_start_dma_edma(struct mtk_eth *eth) {
	//EDMA_GLO_CFG: TX_WB_DDONE | RX_DMA_EN | PDMA_BT_SIZE_16DWORDS | PDMA_DESC_32B_E |
	//   MULTI_EN | ADMA_RX_BT_SIZE_32DWORDS  RX_2B_OFFSET

	mtk_w32(eth, 0x80001C64, MTK_EDMA0_GLO_CFG);
	mtk_w32(eth, 0x80001C64, MTK_EDMA1_GLO_CFG);
}

void mtk_stop_dma_edma(struct mtk_eth *eth, u32 glo_cfg)
{
	u32 val;
	int i;

	val = mtk_r32(eth, glo_cfg);

	val &= ~(MTK_TX_WB_DDONE | MTK_RX_DMA_EN | MTK_TX_DMA_EN);
	mtk_w32(eth, val, glo_cfg);

	/* wait for dma stop */
	for (i = 0; i < 10; i++) {
		val = mtk_r32(eth, glo_cfg);
		if (val & (MTK_TX_DMA_BUSY | MTK_RX_DMA_BUSY)) {
			msleep(20);
			continue;
		}
		break;
	}
}

void mtk_rx_clean_edma(struct mtk_eth *eth, struct mtk_rx_ring *ring)
{
	int i;

	if (ring->data && ring->edma) {
		for (i = 0; i < ring->dma_size; i++) {
			if (!ring->data[i])
				continue;
			if (!ring->edma[i].rxd1)
				continue;
			dma_unmap_single(eth->dev,
					 ring->edma[i].rxd1,
					 ring->buf_size,
					 DMA_FROM_DEVICE);
			skb_free_frag(ring->data[i]);
		}
		kfree(ring->data);
		ring->data = NULL;
	}

	if (ring->edma) {
		dma_free_coherent(eth->dev,
				  ring->dma_size * sizeof(*ring->edma),
				  ring->edma,
				  ring->phys);
		ring->edma = NULL;
	}
}


int mtk_rx_alloc_edma(struct mtk_eth *eth, int ring_no, int rx_flag)
{
	struct mtk_rx_ring *ring;
	int rx_data_len = ETH_DATA_LEN, rx_dma_size = MTK_HW_EDMA_RX_DMA_SIZE_RING1;
	int i;
	u32 offset = 0;
	struct EDMA_RXD2_T* rxd2_ptr;

	if (rx_flag == MTK_RX_FLAGS_EDMA0){  //EDMA0 base 15103800
		ring = &eth->rx_ring_edma0[ring_no];
		offset = 0x3000;
	} else if (rx_flag == MTK_RX_FLAGS_EDMA1){ //EDMA1 base 15103C00
		ring = &eth->rx_ring_edma1[ring_no];
		offset = 0x3400;
	} else {
		pr_info("%s: rx edma flag wrong\n", __func__);
		return -ENOMEM;
	}

	if (rx_flag == MTK_RX_FLAGS_EDMA0 || rx_flag == MTK_RX_FLAGS_EDMA1) {
		rx_data_len = ETH_DATA_LEN;
		if (ring_no == 0)
			rx_dma_size = MTK_HW_EDMA_RX_DMA_SIZE;
		else rx_dma_size = MTK_HW_EDMA_RX_DMA_SIZE_RING1;
	} else {
		pr_info("%s: rx edma flag wrong\n", __func__);
		return -ENOMEM;
	}

	ring->frag_size = mtk_max_frag_size(rx_data_len);
	ring->buf_size = mtk_max_buf_size(ring->frag_size);

	ring->data = kcalloc(rx_dma_size, sizeof(*ring->data),
			     GFP_KERNEL);
	if (!ring->data) {
		pr_info("%s: kcalloc error \n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < rx_dma_size; i++) {
		ring->data[i] = netdev_alloc_frag(ring->frag_size);
		if (!ring->data[i]) {
			pr_info("%s: mem error (%d) %d\n", __func__, i, (int) ring->frag_size);
			return -ENOMEM;
		}
	}

	if (rx_flag == MTK_RX_FLAGS_EDMA0 || rx_flag == MTK_RX_FLAGS_EDMA1) {
		ring->edma = dma_alloc_coherent(eth->dev,
					       rx_dma_size * sizeof(*ring->edma),
					       &ring->phys,
					       GFP_ATOMIC | __GFP_ZERO);
		if (!ring->edma){
			pr_info("%s: mem error (%d) %ld\n", __func__, rx_dma_size, sizeof(*ring->edma));
			return -ENOMEM;
		}
	}

	for (i = 0; i < rx_dma_size; i++) {
		dma_addr_t dma_addr = dma_map_single(eth->dev,
				ring->data[i] + NET_SKB_PAD,
				ring->buf_size,
				DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(eth->dev, dma_addr))){
			pr_info("%s: mem error (%d) %d\n", __func__, i, ring->buf_size);
			return -ENOMEM;
		}

		if (rx_flag == MTK_RX_FLAGS_EDMA0 || rx_flag == MTK_RX_FLAGS_EDMA1) {
			ring->edma[i].rxd1 = (unsigned int)dma_addr;
			rxd2_ptr = (struct EDMA_RXD2_T*) &ring->edma[i].rxd2;
			rxd2_ptr->DDONE_bit = 0;
			rxd2_ptr->LS0 = 0;
			rxd2_ptr->PLEN0 = ring->buf_size;
		}
	}
	ring->dma_size = rx_dma_size;
	ring->calc_idx_update = false;
	ring->calc_idx = rx_dma_size - 1;
	ring->crx_idx_reg = MTK_PRX_CRX_IDX_CFG(ring_no);
	/* make sure that all changes to the dma ring are flushed before we
	 * continue
	 */
	wmb();

	mtk_w32(eth, ring->phys, MTK_PRX_BASE_PTR_CFG(ring_no) + offset);
	mtk_w32(eth, rx_dma_size, MTK_PRX_MAX_CNT_CFG(ring_no) + offset);
	mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg + offset);
	mtk_w32(eth, MTK_PST_DRX_IDX_CFG(ring_no), MTK_PDMA_RST_IDX + offset);
	return 0;
}

