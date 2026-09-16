/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Harry Huang <harry.huang@mediatek.com>
 */

#include "raether.h"

#if defined(CONFIG_RAETH_EDMA)

static inline int get_device_seq(struct net_device *dev)
{
	if (strcmp(dev->name, DEV_NAME) == 0)
		return 0;
	else
		return 1;
}

int fe_edma_wait_dma_idle(void)
{
	unsigned int reg_val;
	unsigned int loop_cnt = 0;

	while (1) {
		if (loop_cnt++ > 1000)
			break;
		reg_val = sys_reg_read(EDMA_GLO_CFG); //todo: both edma0 and edma1 should be separate?
		if ((reg_val & RX_DMA_BUSY)) {
			pr_warn("\n  RX_DMA_BUSY !!! ");
			continue;
		}
		if ((reg_val & TX_DMA_BUSY)) {
			pr_warn("\n  TX_DMA_BUSY !!! ");
			continue;
		}
		return 0;
	}

	return -1;
}

int fe_edma_rx_dma_init(struct net_device *dev)
{
	int i,j;
	unsigned int skb_size;
	struct END_DEVICE *ei_local = netdev_priv(dev);
	dma_addr_t dma_addr;

	skb_size = SKB_DATA_ALIGN(MAX_RX_LENGTH + NET_IP_ALIGN + NET_SKB_PAD) +
		   SKB_DATA_ALIGN(sizeof(struct skb_shared_info));

	/* Initial RX Ring 0 */
	for (i = 0; i < MAX_TX_RX_RING_NUM; i++) {
		ei_local->rx_ring[i] = dma_alloc_coherent(dev->dev.parent,
							num_rx_desc *
							sizeof(struct EDMA_rxdesc),
							&ei_local->phy_rx_ring[i],
							GFP_ATOMIC | __GFP_ZERO);
		pr_debug("\nphy_rx_ring[%d] = 0x%08x, rx_ring[%d] = 0x%p\n",
			 i, (unsigned int)ei_local->phy_rx_ring[i],
			 i, (void *)ei_local->rx_ring[i]);

		for (j = 0; j < num_rx_desc; j++) {
			ei_local->netrx_skb_data[i][j] =
				raeth_alloc_skb_data(skb_size, GFP_KERNEL);
			if (!ei_local->netrx_skb_data[i][j]) {
				pr_err("rx skbuff buffer allocation failed!");
				goto no_rx_mem;
			}

			memset(&ei_local->rx_ring[i][j], 0, sizeof(struct EDMA_rxdesc));
			ei_local->rx_ring[i][j].rxd_info2.DDONE_bit = 0;
			ei_local->rx_ring[i][j].rxd_info2.LS0 = 0;
			ei_local->rx_ring[i][j].rxd_info2.PLEN0 = MAX_RX_LENGTH;
			dma_addr = dma_map_single(dev->dev.parent,
						  ei_local->netrx_skb_data[i][j] +
						  NET_SKB_PAD,
						  MAX_RX_LENGTH,
						  DMA_FROM_DEVICE);
			ei_local->rx_ring[i][j].rxd_info1.PDP0 = dma_addr;
			if (unlikely
			    (dma_mapping_error
			     (dev->dev.parent,
			      ei_local->rx_ring[i][j].rxd_info1.PDP0))) {
				pr_err("[%s]dma_map_single() failed...\n", __func__);
				goto no_rx_mem;
			}
		}
	}

	/* Tell the adapter where the RX rings are located. */
	sys_reg_write(RX_BASE_PTR0, phys_to_bus((u32)ei_local->phy_rx_ring[0]));
  	sys_reg_write(RX_BASE_PTR1, phys_to_bus((u32)ei_local->phy_rx_ring[0]));
	sys_reg_write(RX_MAX_CNT0, cpu_to_le32((u32)num_rx_desc));
  	sys_reg_write(RX_MAX_CNT1, cpu_to_le32((u32)num_rx_desc));
	sys_reg_write(RX_CALC_IDX0, cpu_to_le32((u32)(num_rx_desc - 1)));
  	sys_reg_write(RX_CALC_IDX1, cpu_to_le32((u32)(num_rx_desc - 1)));
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
	sys_reg_write(RX_BASE_PTR0_1, phys_to_bus((u32)ei_local->phy_rx_ring[1]));
  	sys_reg_write(RX_BASE_PTR1_1, phys_to_bus((u32)ei_local->phy_rx_ring[1]));
	sys_reg_write(RX_MAX_CNT0_1, cpu_to_le32((u32)num_rx_desc));
  	sys_reg_write(RX_MAX_CNT1_1, cpu_to_le32((u32)num_rx_desc));
	sys_reg_write(RX_CALC_IDX0_1, cpu_to_le32((u32)(num_rx_desc - 1)));
  	sys_reg_write(RX_CALC_IDX1_1, cpu_to_le32((u32)(num_rx_desc - 1)));
#endif

	sys_reg_write(EDMA_RST_CFG, PST_DRX_IDX0);  // todo: what this for?
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
	sys_reg_write(EDMA_RST_CFG_1, PST_DRX_IDX0);  // todo: what this for?
#endif
	return 0;

no_rx_mem:
	return -ENOMEM;
}

int fe_edma_tx_dma_init(struct net_device *dev)
{
	int i,j;
	struct END_DEVICE *ei_local = netdev_priv(dev);

	for (i = 0; i < MAX_TX_RX_RING_NUM; i++) {
		for (j = 0; j < num_tx_desc; j++)
			ei_local->skb_free[i][j] = 0;

		ei_local->tx_ring_full = 0;//todo
		RUN_EDMA_CONCURRENT(ei_local->tx_ring_full_1 = 0);//todo
		ei_local->free_idx[i] = 0;
		ei_local->tx_ring[i] =
		    dma_alloc_coherent(dev->dev.parent,
				       num_tx_desc * sizeof(struct EDMA_txdesc),
				       &ei_local->phy_tx_ring[i],
				       GFP_ATOMIC | __GFP_ZERO);
		pr_debug("\nphy_tx_ring = 0x%08x, tx_ring = 0x%p\n",
			 (unsigned int)ei_local->phy_tx_ring[i],
			 (void *)ei_local->tx_ring[i]);
		for (j = 0; j < num_tx_desc; j++) {
			memset(&ei_local->tx_ring[i][j], 0, sizeof(struct EDMA_txdesc));
			ei_local->tx_ring[i][j].txd_info2.LS0 = 1;
			ei_local->tx_ring[i][j].txd_info2.DDONE = 1;
		}
	}

	/* Tell the adapter where the TX rings are located. */
	sys_reg_write(TX_BASE_PTR0, phys_to_bus((u32)ei_local->phy_tx_ring[0]));
	sys_reg_write(TX_MAX_CNT0, cpu_to_le32((u32)num_tx_desc));
	sys_reg_write(TX_CTX_IDX0, 0);
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
    sys_reg_write(TX_BASE_PTR0_1, phys_to_bus((u32)ei_local->phy_tx_ring[1]));
	sys_reg_write(TX_MAX_CNT0_1, cpu_to_le32((u32)num_tx_desc));
	sys_reg_write(TX_CTX_IDX0_1, 0);
#endif
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
	ei_local->tx_cpu_owner_idx[0] = 0;
	ei_local->tx_cpu_owner_idx[1] = 0;
#endif
	sys_reg_write(EDMA_RST_CFG, PST_DTX_IDX0); // todo: what this for?
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
	sys_reg_write(EDMA_RST_CFG_1, PST_DTX_IDX0); // todo: what this for?
#endif

	return 0;
}

void fe_edma_rx_dma_deinit(struct net_device *dev)
{
	struct END_DEVICE *ei_local = netdev_priv(dev);
	int i,j;

	for (i = 0; i < MAX_TX_RX_RING_NUM; i++) {
		/* free RX Ring */
		dma_free_coherent(dev->dev.parent,
				  num_rx_desc * sizeof(struct EDMA_rxdesc),
				  ei_local->rx_ring[i], ei_local->phy_rx_ring[i]);

		/* free RX data */
		for (j = 0; j < num_rx_desc; j++) {
			raeth_free_skb_data(ei_local->netrx_skb_data[i][j]);
			ei_local->netrx_skb_data[i][j] = NULL;
		}
	}
}

void fe_edma_tx_dma_deinit(struct net_device *dev)
{
	struct END_DEVICE *ei_local = netdev_priv(dev);
	int i,j;

	for (i = 0; i < MAX_TX_RX_RING_NUM; i++) {
		/* free TX Ring */
		if (ei_local->tx_ring[i])
			dma_free_coherent(dev->dev.parent,
					  num_tx_desc *
					  sizeof(struct EDMA_txdesc),
					  ei_local->tx_ring[i],
					  ei_local->phy_tx_ring[i]);

		/* free TX data */
		for (j = 0; j < num_tx_desc; j++) {
			if ((ei_local->skb_free[i][j] != 0) &&
			    (ei_local->skb_free[i][j] != (struct sk_buff *)0xFFFFFFFF))
				dev_kfree_skb_any(ei_local->skb_free[i][j]);
		}
	}
}

void set_fe_edma_glo_cfg(void)
{
	unsigned int dma_glo_cfg = 0;

	dma_glo_cfg =
	    (TX_WB_DDONE | RX_DMA_EN | TX_DMA_EN | PDMA_BT_SIZE_16DWORDS | PDMA_DESC_32B_E |
	     MULTI_EN | ADMA_RX_BT_SIZE_32DWORDS);
	dma_glo_cfg |= (RX_2B_OFFSET);

	sys_reg_write(EDMA_GLO_CFG, dma_glo_cfg);
	sys_reg_write(EDMA_GLO_CFG, 0x80404C65);
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
	sys_reg_write(EDMA_GLO_CFG_1, dma_glo_cfg);
	sys_reg_write(EDMA_GLO_CFG_1, 0x80404C65);
#endif
}

/* @brief cal txd number for a page
 *
 *  @parm size
 *
 *  @return frag_txd_num
 */
static inline unsigned int edma_cal_frag_txd_num(unsigned int size)
{
	unsigned int frag_txd_num = 0;

	if (size == 0)
		return 0;
	while (size > 0) {
		if (size > MAX_PTXD_LEN) {
			frag_txd_num++;
			size -= MAX_PTXD_LEN;
		} else {
			frag_txd_num++;
			size = 0;
		}
	}
	return frag_txd_num;
}

int fe_fill_edma_tx_desc(struct net_device *dev,
		    unsigned long *tx_cpu_owner_idx,
		    struct sk_buff *skb,
		    int gmac_no)
{
	struct END_DEVICE *ei_local = netdev_priv(dev);
	struct net_device *curr_dev = dev;
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
	unsigned int ring_index = gmac_no - 1;
#else
	unsigned int ring_index = 0;
#endif
	struct EDMA_txdesc *tx_ring = &ei_local->tx_ring[ring_index][*tx_cpu_owner_idx];
	struct EDMA_TXD_INFO2_T txd_info2_tmp;
	struct EDMA_TXD_INFO5_T txd_info5_tmp;
	struct EDMA_TXD_INFO6_T txd_info6_tmp;
	struct EDMA_TXD_INFO7_T txd_info7_tmp;
	dma_addr_t mapped_addr;

	mapped_addr = dma_map_single(curr_dev->dev.parent,skb->data, skb->len, DMA_TO_DEVICE);
	//tx_ring->txd_info1.SDP0 = virt_to_phys(skb->data);
	tx_ring->txd_info1.SDP0 = mapped_addr;
	txd_info2_tmp.SDL0 = skb->len;
	txd_info5_tmp.FPORT = gmac_no;


	txd_info5_tmp.TSO = 0;

	if (ei_local->features & FE_CSUM_OFFLOAD) {
		if (skb->ip_summed == CHECKSUM_PARTIAL)
			txd_info5_tmp.TUI_CO = 7;
		else
			txd_info5_tmp.TUI_CO = 0;
	}

	if (ei_local->features & FE_HW_VLAN_TX) {
		if (skb_vlan_tag_present(skb)) {
			txd_info6_tmp.INSV_1 = 1;
			txd_info6_tmp.VLAN_TAG_1 = skb_vlan_tag_get(skb);
		} else {
			txd_info6_tmp.INSV_1 = 0;
			txd_info6_tmp.VLAN_TAG_1 = skb_vlan_tag_get(skb);
		}
	} else {
		txd_info6_tmp.VLAN_TAG_1 = 0;
	}
#if defined(CONFIG_RA_HW_NAT) || defined(CONFIG_RA_HW_NAT_MODULE)
	if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb)) {
		if (FOE_MAGIC_TAG_HEAD(skb) == FOE_MAGIC_PPE) {
			if (ppe_hook_rx_eth) {
				/* PPE */
				txd_info5_tmp.FPORT = 4;
				FOE_MAGIC_TAG(skb) = 0;
			}
		}

	} else if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb)) {
		if (FOE_MAGIC_TAG_TAIL(skb) == FOE_MAGIC_PPE) {
			if (ppe_hook_rx_eth) {
				/* PPE */
				txd_info5_tmp.FPORT = 4;
				FOE_MAGIC_TAG(skb) = 0;
			}
		}

		if (FOE_MAGIC_TAG_TAIL(skb) == FOE_MAGIC_PPE0) {
			if (ppe_hook_rx_ext) {
				//pr_info("[HS-ethernet/RAETH/TX] FOE_MAGIC_PPE0 FPORT=3\n");
				txd_info5_tmp.FPORT = 3;	/* PPE0 */
				FOE_MAGIC_TAG_TAIL(skb) = 0;
			}
		}
		if (FOE_MAGIC_TAG_TAIL(skb) == FOE_MAGIC_PPE1) {
			if (ppe_hook_rx_ext) {
				//pr_info("[HS-ethernet/RAETH/TX] FOE_MAGIC_PPE1 FPORT=4\n");
				txd_info5_tmp.FPORT = 4;
				FOE_MAGIC_TAG_TAIL(skb) = 0;
			}
		}


			


	}
#endif

	txd_info2_tmp.LS0 = 1;
	txd_info2_tmp.DDONE = 0;
	txd_info7_tmp.VLAN_TAG_0 = 0;

	tx_ring->txd_info5 = txd_info5_tmp;
	tx_ring->txd_info6 = txd_info6_tmp;
	tx_ring->txd_info7 = txd_info7_tmp;
	tx_ring->txd_info2 = txd_info2_tmp;


	return 0;
}

static int fe_fill_tx_tso_data(struct END_DEVICE *ei_local,
			       unsigned int frag_offset,
			       unsigned int frag_size,
			       unsigned long *tx_cpu_owner_idx,
			       unsigned int nr_frags,
			       int gmac_no)
{
	struct PSEUDO_ADAPTER *p_ad;
	unsigned int size;
	unsigned int frag_txd_num;
	struct EDMA_txdesc *tx_ring;
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
	unsigned int ring_index = gmac_no - 1;
#else
	unsigned int ring_index = 0;
#endif

	frag_txd_num = edma_cal_frag_txd_num(frag_size);
	tx_ring = &ei_local->tx_ring[ring_index][*tx_cpu_owner_idx];

	while (frag_txd_num > 0) {
		if (frag_size < MAX_PTXD_LEN)
			size = frag_size;
		else
			size = MAX_PTXD_LEN;

		if (ei_local->skb_txd_num[ring_index] % 2 == 0) {
			*tx_cpu_owner_idx =
			    (*tx_cpu_owner_idx + 1) % num_tx_desc;
			tx_ring = &ei_local->tx_ring[ring_index][*tx_cpu_owner_idx];

			while (tx_ring->txd_info2.DDONE == 0) {
				if (gmac_no == 2) {
					p_ad =
					    netdev_priv(ei_local->pseudo_dev);
					p_ad->stat.tx_errors++;
				} else {
					ei_local->stat.tx_errors++;
				}
			}
			tx_ring->txd_info1.SDP0 = frag_offset;
			tx_ring->txd_info2.SDL0 = size;
			if (((nr_frags == 0)) && (frag_txd_num == 1))
				tx_ring->txd_info2.LS0 = 1;
			else
				tx_ring->txd_info2.LS0 = 0;
			tx_ring->txd_info2.DDONE = 0;
			tx_ring->txd_info5.FPORT = gmac_no;
		} else {
			tx_ring->txd_info3.SDP1 = frag_offset;
			tx_ring->txd_info4.SDL1 = size;
			if (((nr_frags == 0)) && (frag_txd_num == 1))
				tx_ring->txd_info4.LS1 = 1;
			else
				tx_ring->txd_info4.LS1 = 0;
		}
		frag_offset += size;
		frag_size -= size;
		frag_txd_num--;
		ei_local->skb_txd_num[ring_index]++;
	}

	return 0;
}

static int fe_fill_tx_tso_frag(struct net_device *netdev,
			       struct sk_buff *skb,
			       unsigned long *tx_cpu_owner_idx,
			       int gmac_no)
{
	struct END_DEVICE *ei_local = netdev_priv(netdev);
	struct net_device *curr_dev = netdev;
	struct PSEUDO_ADAPTER *p_ad;
	unsigned int size;
	unsigned int frag_txd_num;
	struct skb_frag_struct *frag;
	unsigned int nr_frags;
	unsigned int frag_offset, frag_size;
	struct EDMA_txdesc *tx_ring;
	int i = 0, j = 0, unmap_idx = 0;
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
		unsigned int ring_index = gmac_no - 1;
#else
		unsigned int ring_index = 0;
#endif

#ifdef CONFIG_RAETH_EDMA_CONCURRENT
		if (gmac_no == 2)
			curr_dev = ei_local->pseudo_dev;
#endif



	nr_frags = skb_shinfo(skb)->nr_frags;
	tx_ring = &ei_local->tx_ring[ring_index][*tx_cpu_owner_idx];

	for (i = 0; i < nr_frags; i++) {
		frag = &skb_shinfo(skb)->frags[i];
		frag_offset = frag->page_offset;
		frag_size = frag->size;
		frag_txd_num = edma_cal_frag_txd_num(frag_size);

		while (frag_txd_num > 0) {
			if (frag_size < MAX_PTXD_LEN)
				size = frag_size;
			else
				size = MAX_PTXD_LEN;

			if (ei_local->skb_txd_num[ring_index] % 2 == 0) {
				*tx_cpu_owner_idx =
					(*tx_cpu_owner_idx + 1) % num_tx_desc;
				tx_ring =
					&ei_local->tx_ring[ring_index][*tx_cpu_owner_idx];

				while (tx_ring->txd_info2.DDONE == 0) {
					if (gmac_no == 2) {
						p_ad =
						    netdev_priv
						    (ei_local->pseudo_dev);
						p_ad->stat.tx_errors++;
					} else {
						ei_local->stat.tx_errors++;
					}
				}

				tx_ring->txd_info1.SDP0 =
				    dma_map_page(curr_dev->dev.parent,
						 frag->page.p, frag_offset,
						 size, DMA_TO_DEVICE);
				if (unlikely
				    (dma_mapping_error
				     (curr_dev->dev.parent,
				      tx_ring->txd_info1.SDP0))) {
					pr_err
					    ("[%s]dma_map_page() failed\n",
					     __func__);
					goto err_dma;
				}

				tx_ring->txd_info2.SDL0 = size;

				if ((frag_txd_num == 1) &&
				    (i == (nr_frags - 1)))
					tx_ring->txd_info2.LS0 = 1;
				else
					tx_ring->txd_info2.LS0 = 0;
				tx_ring->txd_info2.DDONE = 0;
				tx_ring->txd_info5.FPORT = gmac_no;
			} else {
				tx_ring->txd_info3.SDP1 =
				    dma_map_page(curr_dev->dev.parent,
						 frag->page.p, frag_offset,
						 size, DMA_TO_DEVICE);
				if (unlikely
				    (dma_mapping_error
				     (curr_dev->dev.parent,
				      tx_ring->txd_info3.SDP1))) {
					pr_err
					    ("[%s]dma_map_page() failed\n",
					     __func__);
					goto err_dma;
				}
				tx_ring->txd_info4.SDL1 = size;
				if ((frag_txd_num == 1) &&
				    (i == (nr_frags - 1)))
					tx_ring->txd_info4.LS1 = 1;
				else
					tx_ring->txd_info4.LS1 = 0;
			}
			frag_offset += size;
			frag_size -= size;
			frag_txd_num--;
			ei_local->skb_txd_num[ring_index]++;
		}
	}

	return 0;

err_dma:
	/* unmap dma */
	j = *tx_cpu_owner_idx;
	unmap_idx = i;
	for (i = 0; i < unmap_idx; i++) {
		frag = &skb_shinfo(skb)->frags[i];
		frag_offset = frag->page_offset;
		frag_size = frag->size;
		frag_txd_num = edma_cal_frag_txd_num(frag_size);

		while (frag_txd_num > 0) {
			if (frag_size < MAX_PTXD_LEN)
				size = frag_size;
			else
				size = MAX_PTXD_LEN;
			if (ei_local->skb_txd_num[ring_index] % 2 == 0) {
				j = (j + 1) % num_tx_desc;
				dma_unmap_page(netdev->dev.parent,
					       ei_local->tx_ring[ring_index][j].
					       txd_info1.SDP0,
					       ei_local->tx_ring[ring_index][j].
					       txd_info2.SDL0, DMA_TO_DEVICE);
				/* reinit txd */
				ei_local->tx_ring[ring_index][j].txd_info2.LS0 = 1;
				ei_local->tx_ring[ring_index][j].txd_info2.DDONE = 1;
			} else {
				dma_unmap_page(netdev->dev.parent,
					       ei_local->tx_ring[ring_index][j].
					       txd_info3.SDP1,
					       ei_local->tx_ring[ring_index][j].
					       txd_info4.SDL1, DMA_TO_DEVICE);
				/* reinit txd */
				ei_local->tx_ring[ring_index][j].txd_info4.LS1 = 1;
			}
			frag_offset += size;
			frag_size -= size;
			frag_txd_num--;
			ei_local->skb_txd_num[ring_index]++;
		}
	}

	return -1;
}

int fe_fill_edma_tx_desc_tso(struct net_device *dev,
			unsigned long *tx_cpu_owner_idx,
			struct sk_buff *skb,
			int gmac_no)
{
	struct END_DEVICE *ei_local = netdev_priv(dev);
	struct net_device *curr_dev = dev;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct tcphdr *th = NULL;
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags;
	unsigned int len, offset;
	int err;
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
		unsigned int ring_index = gmac_no - 1;
#else
		unsigned int ring_index = 0;
#endif
	dma_addr_t mapped_addr, mapped_addr_th;

	struct EDMA_txdesc *tx_ring = &ei_local->tx_ring[ring_index][*tx_cpu_owner_idx];

	mapped_addr = dma_map_single(curr_dev->dev.parent,skb->data, skb->len, DMA_TO_DEVICE);
	tx_ring->txd_info5.FPORT = gmac_no;
	tx_ring->txd_info5.TSO = 0;

	if (skb->ip_summed == CHECKSUM_PARTIAL)
		tx_ring->txd_info5.TUI_CO = 7;
	else
		tx_ring->txd_info5.TUI_CO = 0;

	if (ei_local->features & FE_HW_VLAN_TX) {
		if (skb_vlan_tag_present(skb)) {
			tx_ring->txd_info6.INSV_1 = 1;
			tx_ring->txd_info6.VLAN_TAG_1 =
				0x10000 | skb_vlan_tag_get(skb);
		} else {
			tx_ring->txd_info6.INSV_1 = 0;
			tx_ring->txd_info6.VLAN_TAG_1 = 0;
		}
	}
#if defined(CONFIG_RA_HW_NAT) || defined(CONFIG_RA_HW_NAT_MODULE)
	if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb)) {
		if (FOE_MAGIC_TAG_HEAD(skb) == FOE_MAGIC_PPE) {
			if (ppe_hook_rx_eth) {
				/* PPE */
				tx_ring->txd_info5.FPORT = 4;
				FOE_MAGIC_TAG(skb) = 0;
			}
		}
	} else if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb)) {
		if (FOE_MAGIC_TAG_TAIL(skb) == FOE_MAGIC_PPE) {
			if (ppe_hook_rx_eth) {
				/* PPE */
				tx_ring->txd_info5.FPORT = 4;
				FOE_MAGIC_TAG(skb) = 0;
			}
		}

		if (FOE_MAGIC_TAG_TAIL(skb) == FOE_MAGIC_PPE0) {
			if (ppe_hook_rx_ext) {
				//pr_info("[HS-ethernet/RAETH/TX] FOE_MAGIC_PPE0 FPORT=3\n");
				tx_ring->txd_info5.FPORT = 11;	/* PPE0 */
				FOE_MAGIC_TAG_TAIL(skb) = 0;
			}
		}
		if (FOE_MAGIC_TAG_TAIL(skb) == FOE_MAGIC_PPE1) {
			if (ppe_hook_rx_ext) {
				//pr_info("[HS-ethernet/RAETH/TX] FOE_MAGIC_PPE1 FPORT=4\n");
				tx_ring->txd_info5.FPORT = 12;
				FOE_MAGIC_TAG_TAIL(skb) = 0;
			}
		}
	}
#endif
	ei_local->skb_txd_num[ring_index] = 1;

	/* skb data handle */
	len = skb->len - skb->data_len;
	//offset = virt_to_phys(skb->data);
	offset = mapped_addr;
	tx_ring->txd_info1.SDP0 = offset;
	if (len < MAX_PTXD_LEN) {
		tx_ring->txd_info2.SDL0 = len;
		tx_ring->txd_info2.LS0 = nr_frags ? 0 : 1;
		len = 0;
	} else {
		tx_ring->txd_info2.SDL0 = MAX_PTXD_LEN;
		tx_ring->txd_info2.LS0 = 0;
		len -= MAX_PTXD_LEN;
		offset += MAX_PTXD_LEN;
	}

	if (len > 0)
		fe_fill_tx_tso_data(ei_local, offset, len,
				    tx_cpu_owner_idx, nr_frags, gmac_no);

	/* skb fragments handle */
	if (nr_frags > 0) {
		err = fe_fill_tx_tso_frag(dev, skb, tx_cpu_owner_idx, gmac_no);
		if (unlikely(err))
			return err;
	}

	/* fill in MSS info in tcp checksum field */
	if (skb_shinfo(skb)->gso_segs > 1) {
		/* TCP over IPv4 */
		iph = (struct iphdr *)skb_network_header(skb);
		if ((iph->version == 4) && (iph->protocol == IPPROTO_TCP)) {
			th = (struct tcphdr *)skb_transport_header(skb);
			tx_ring->txd_info5.TSO = 1;
			th->check = htons(skb_shinfo(skb)->gso_size);
			//dma_sync_single_for_device(dev->dev.parent,
						   //virt_to_phys(th),
						   //sizeof(struct tcphdr),
						   //DMA_TO_DEVICE);
			mapped_addr_th = dma_map_single(curr_dev->dev.parent, skb->data,
				     sizeof(struct tcphdr), DMA_TO_DEVICE);
		}

		/* TCP over IPv6 */
		if (ei_local->features & FE_TSO_V6) {
			ip6h = (struct ipv6hdr *)skb_network_header(skb);
			if ((ip6h->nexthdr == NEXTHDR_TCP) &&
			    (ip6h->version == 6)) {
				th = (struct tcphdr *)skb_transport_header(skb);
				tx_ring->txd_info5.TSO = 1;
				th->check = htons(skb_shinfo(skb)->gso_size);
				//dma_sync_single_for_device(dev->dev.parent,
							   //virt_to_phys(th),
							   //sizeof(struct
								  //tcphdr),
							   //DMA_TO_DEVICE);
				mapped_addr_th = dma_map_single(curr_dev->dev.parent, skb->data,
				     sizeof(struct tcphdr), DMA_TO_DEVICE);
			}
		}
	}
	tx_ring->txd_info2.DDONE = 0;

	return 0;
}

static inline int rt2880_edma_eth_send(struct net_device *dev,
				       struct sk_buff *skb, int gmac_no,
				       unsigned int num_of_frag)
{
	unsigned int length = skb->len;
#ifdef CONFIG_RAETH_EDMA_CONCURRENT
	unsigned int ring_index = gmac_no - 1;
#else
	unsigned int ring_index = 0;
#endif
	struct END_DEVICE *ei_local = netdev_priv(dev);
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
	unsigned long tx_cpu_owner_idx = ei_local->tx_cpu_owner_idx[ring_index];
#else
	if (ring_index == 0)
		unsigned long tx_cpu_owner_idx = sys_reg_read(TX_CTX_IDX0);
	else
		unsigned long tx_cpu_owner_idx = sys_reg_read(TX_CTX_IDX0_1);
#endif
	struct PSEUDO_ADAPTER *p_ad;
	int err;
	while (ei_local->tx_ring[ring_index][tx_cpu_owner_idx].txd_info2.DDONE == 0) {
		if (gmac_no == 2) {
			if (ei_local->pseudo_dev) {
				p_ad = netdev_priv(ei_local->pseudo_dev);
				p_ad->stat.tx_errors++;
			} else {
				pr_err
				    ("pseudo_dev is still not initialize ");
				pr_err
				    ("but receive packet from GMAC2\n");
			}
		} else {
			ei_local->stat.tx_errors++;
		}
	}


	if (num_of_frag > 1)
		err = fe_fill_edma_tx_desc_tso(dev, &tx_cpu_owner_idx,
					  skb, gmac_no);
	else
		err = fe_fill_edma_tx_desc(dev, &tx_cpu_owner_idx, skb, gmac_no);
	if (err)
		return err;

	tx_cpu_owner_idx = (tx_cpu_owner_idx + 1) % num_tx_desc;
	while (ei_local->tx_ring[ring_index][tx_cpu_owner_idx].txd_info2.DDONE == 0) {
		if (gmac_no == 2) {
			p_ad = netdev_priv(ei_local->pseudo_dev);
			p_ad->stat.tx_errors++;
		} else {
			ei_local->stat.tx_errors++;
		}
	}
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
	ei_local->tx_cpu_owner_idx[ring_index] = tx_cpu_owner_idx;
#endif
	/* make sure that all changes to the dma ring are flushed before we
	 * continue
	 */
	wmb();

	if (ring_index == 0)
		sys_reg_write(TX_CTX_IDX0, cpu_to_le32((u32)tx_cpu_owner_idx));
	else
		sys_reg_write(TX_CTX_IDX0_1, cpu_to_le32((u32)tx_cpu_owner_idx));

	if (gmac_no == 2) {
		p_ad = netdev_priv(ei_local->pseudo_dev);
		p_ad->stat.tx_packets++;
		p_ad->stat.tx_bytes += length;
	} else {
		ei_local->stat.tx_packets++;
		ei_local->stat.tx_bytes += length;
	}

	return length;
}

int ei_edma_start_xmit(struct sk_buff *skb, struct net_device *dev, int gmac_no)
{
	struct END_DEVICE *ei_local = netdev_priv(dev);
	unsigned long tx_cpu_owner_idx;
	unsigned int tx_cpu_owner_idx_next, tx_cpu_owner_idx_next2;
	unsigned int num_of_txd, num_of_frag;
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags, i;
	struct skb_frag_struct *frag;
	struct PSEUDO_ADAPTER *p_ad;
	unsigned int tx_cpu_cal_idx;
	unsigned int ring_index;

	if(ei_local->features & FE_EDMA_CONCURRENT)
		ring_index = gmac_no - 1;
	else
		ring_index = 0;

#if defined(CONFIG_RA_HW_NAT)  || defined(CONFIG_RA_HW_NAT_MODULE) //todo
	if (ppe_hook_tx_eth) {
		if (FOE_MAGIC_TAG(skb) != FOE_MAGIC_PPE) {
			if (ppe_hook_tx_eth(skb, gmac_no) != 1) {
				dev_kfree_skb_any(skb);
				return 0;
			}
		}
	}
#endif

#ifdef CONFIG_RAETH_EDMA_CONCURRENT
	if (gmac_no == 1)
		netif_trans_update(dev);
	else
		netif_trans_update(ei_local->pseudo_dev);
#else
	netif_trans_update(dev);
#endif
//	dev->trans_start = jiffies;	/* save the timestamp */
	spin_lock(&ei_local->page_lock); //todo

	//dma_sync_single_for_device(dev->dev.parent, virt_to_phys(skb->data),
				   //skb->len, DMA_TO_DEVICE);
	//mapped_addr = dma_map_single(dev->dev.parent,, skb->data, skb->len, DMA_TO_DEVICE);
#ifdef CONFIG_RAETH_RW_PDMAPTR_FROM_VAR
	tx_cpu_owner_idx = ei_local->tx_cpu_owner_idx[ring_index];
#else
	if (ring_index == 0)
		tx_cpu_owner_idx = sys_reg_read(TX_CTX_IDX0);
	else
		tx_cpu_owner_idx = sys_reg_read(TX_CTX_IDX0_1);
#endif

	if (ei_local->features & FE_TSO) {
		num_of_txd = edma_cal_frag_txd_num(skb->len - skb->data_len);
		if (nr_frags != 0) {
			for (i = 0; i < nr_frags; i++) {
				frag = &skb_shinfo(skb)->frags[i];
				num_of_txd += edma_cal_frag_txd_num(frag->size);
			}
		}
		num_of_frag = num_of_txd;
		num_of_txd = (num_of_txd + 1) >> 1;
	} else {
		num_of_frag = 1;
		num_of_txd = 1;
	}

	tx_cpu_owner_idx_next = (tx_cpu_owner_idx + num_of_txd) % num_tx_desc;

	if ((ei_local->skb_free[ring_index][tx_cpu_owner_idx_next] == 0) &&
	    (ei_local->skb_free[ring_index][tx_cpu_owner_idx] == 0)) {
		if (rt2880_edma_eth_send(dev, skb, gmac_no, num_of_frag) < 0) {
			dev_kfree_skb_any(skb);
			if (gmac_no == 2) {
				p_ad = netdev_priv(ei_local->pseudo_dev);
				p_ad->stat.tx_dropped++;
			} else {
				ei_local->stat.tx_dropped++;
			}
			goto tx_err;
		}

		tx_cpu_owner_idx_next2 =
		    (tx_cpu_owner_idx_next + 1) % num_tx_desc;

		if (ei_local->skb_free[ring_index][tx_cpu_owner_idx_next2] != 0)
		{
			if (ring_index == 0)
				ei_local->tx_ring_full = 1; //todo
			else
				RUN_EDMA_CONCURRENT(ei_local->tx_ring_full_1 = 1);
		}
	} else {
		if (gmac_no == 2) {
			p_ad = netdev_priv(ei_local->pseudo_dev);
			p_ad->stat.tx_dropped++;
		} else {
			ei_local->stat.tx_dropped++;
		}

		dev_kfree_skb_any(skb);
		spin_unlock(&ei_local->page_lock); //todo
		return NETDEV_TX_OK;
	}

	/* SG: use multiple TXD to send the packet (only have one skb) */
	tx_cpu_cal_idx = (tx_cpu_owner_idx + num_of_txd - 1) % num_tx_desc;
	ei_local->skb_free[ring_index][tx_cpu_cal_idx] = skb;
	while (--num_of_txd)
		/* MAGIC ID */
		ei_local->skb_free[ring_index][(--tx_cpu_cal_idx) % num_tx_desc] =
			(struct sk_buff *)0xFFFFFFFF;

tx_err:
	spin_unlock(&ei_local->page_lock);  //todo
	return NETDEV_TX_OK;
}

int ei_edma_xmit_housekeeping(struct net_device *netdev, int budget)
{
	struct END_DEVICE *ei_local = netdev_priv(netdev);
	struct EDMA_txdesc *tx_desc;
	unsigned long skb_free_idx;
	int tx_processed = 0;
	unsigned int ring_index;

	if(ei_local->features & FE_EDMA_CONCURRENT)
	{
		ring_index = get_device_seq(netdev);  //todo: need to revise here, ei_local & pseudo dev, pseudo in should revise
	}else {
		ring_index = 0;
	}

	tx_desc = ei_local->tx_ring[ring_index];
	skb_free_idx = ei_local->free_idx[ring_index];

	while (budget &&
	       (ei_local->skb_free[ring_index][skb_free_idx] != 0) &&
	       (tx_desc[skb_free_idx].txd_info2.DDONE == 1)) {
		if (ei_local->skb_free[ring_index][skb_free_idx] !=
		    (struct sk_buff *)0xFFFFFFFF)
			dev_kfree_skb_any(ei_local->skb_free[ring_index][skb_free_idx]);

		ei_local->skb_free[ring_index][skb_free_idx] = 0;
		skb_free_idx = (skb_free_idx + 1) % num_tx_desc;
		budget--;
		tx_processed++;
	}

	ei_local->tx_ring_full = 0;
	ei_local->free_idx[ring_index] = skb_free_idx;

	return tx_processed;
}

#ifdef CONFIG_RAETH_EDMA_CONCURRENT
int ei_edma1_xmit_housekeeping(struct net_device *netdev, int budget)
{
	struct END_DEVICE *ei_local = netdev_priv(netdev);
	struct EDMA_txdesc *tx_desc;
	unsigned long skb_free_idx;
	int tx_processed = 0;
	unsigned int ring_index = 1;

	tx_desc = ei_local->tx_ring[ring_index];
	skb_free_idx = ei_local->free_idx[ring_index];

	while (budget &&
	       (ei_local->skb_free[ring_index][skb_free_idx] != 0) &&
	       (tx_desc[skb_free_idx].txd_info2.DDONE == 1)) {
		if (ei_local->skb_free[ring_index][skb_free_idx] !=
		    (struct sk_buff *)0xFFFFFFFF)
			dev_kfree_skb_any(ei_local->skb_free[ring_index][skb_free_idx]);

		ei_local->skb_free[ring_index][skb_free_idx] = 0;
		skb_free_idx = (skb_free_idx + 1) % num_tx_desc;
		budget--;
		tx_processed++;
	}

	ei_local->tx_ring_full_1 = 0;
	ei_local->free_idx[ring_index] = skb_free_idx;

	return tx_processed;
}
#endif
#endif
