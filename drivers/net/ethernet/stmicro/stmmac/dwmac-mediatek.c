// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */
#include <linux/bitfield.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/regmap.h>
#include <linux/stmmac.h>

#include "stmmac.h"
#include "stmmac_platform.h"

/* Peri Configuration register for mt2712 */
#define PERI_ETH_PHY_INTF_SEL	0x418
#define PHY_INTF_MII		0
#define PHY_INTF_RGMII		1
#define PHY_INTF_RMII		4
#define RMII_CLK_SRC_RXC	BIT(4)
#define RMII_CLK_SRC_INTERNAL	BIT(5)

#define PERI_ETH_DLY	0x428
#define ETH_DLY_GTXC_INV	BIT(6)
#define ETH_DLY_GTXC_ENABLE	BIT(5)
#define ETH_DLY_GTXC_STAGES	GENMASK(4, 0)
#define ETH_DLY_TXC_INV		BIT(20)
#define ETH_DLY_TXC_ENABLE	BIT(19)
#define ETH_DLY_TXC_STAGES	GENMASK(18, 14)
#define ETH_DLY_RXC_INV		BIT(13)
#define ETH_DLY_RXC_ENABLE	BIT(12)
#define ETH_DLY_RXC_STAGES	GENMASK(11, 7)

#define PERI_ETH_DLY_FINE	0x800
#define ETH_RMII_DLY_TX_INV	BIT(2)
#define ETH_FINE_DLY_GTXC	BIT(1)
#define ETH_FINE_DLY_RXC	BIT(0)

/* Peri Configuration register for mt2731 */
#define MT2731_PERI_ETH_CTRL1		0x200
#define MT2731_DLY_RXC_INV		BIT(22)
#define MT2731_DLY_RXC_ENABLE		BIT(21)
#define MT2731_DLY_RXC_STAGES		GENMASK(20, 16)
#define MT2731_DLY_TXC_INV		BIT(14)
#define MT2731_DLY_TXC_ENABLE		BIT(13)
#define MT2731_DLY_TXC_STAGES		GENMASK(12, 8)
#define MT2731_ETH_INTF_SEL		GENMASK(6, 4)
#define MT2731_RMII_CLK_SRC_SEL		GENMASK(1, 0)
#define MT2731_RMII_CLK_EXT_TXC		0
#define MT2731_RMII_CLK_EXT_RXC		1
#define MT2731_RMII_CLK_INT_RXC		2

#define MT2731_PERI_ETH_CTRL2		0x204
#define MT2731_DLY_INT_RXC_INV		BIT(28)
#define MT2731_DLY_INT_RXC_FINE_EN	BIT(27)
#define MT2731_DLY_INT_RXC_FINE_STG	GENMASK(26, 22)
#define MT2731_DLY_INT_RXC_COARSE_EN	BIT(21)
#define MT2731_DLY_INT_RXC_COARSE_STG	GENMASK(20, 16)
#define MT2731_DLY_INT_TXC_INV		BIT(12)
#define MT2731_DLY_INT_TXC_FINE_EN	BIT(11)
#define MT2731_DLY_INT_TXC_FINE_STG	GENMASK(10, 6)
#define MT2731_DLY_INT_TXC_COARSE_EN	BIT(5)
#define MT2731_DLY_INT_TXC_COARSE_STG	GENMASK(4, 0)

#define MT2731_PERI_ETH_CTRL3		0x208
#define MT2731_DLY_GTXC_INV		BIT(12)
#define MT2731_DLY_GTXC_FINE_EN		BIT(11)
#define MT2731_DLY_GTXC_FINE_STG	GENMASK(10, 6)

/* config for mt2735 colgin */
#define MT2735_PERI_ETH_CTRL1		0xfd0
#define MT2735_ETH_INTF_SEL		GENMASK(26, 24)
#define MT2735_ETH_CLK_SEL		BIT(22)
#define MT2735_ETH_PHY_SEL		BIT(21)

struct mac_delay_struct {
	u32 tx_delay;
	u32 rx_delay;
	bool tx_inv;
	bool rx_inv;
};

struct mediatek_dwmac_plat_data {
	const struct mediatek_dwmac_variant *variant;
	struct mac_delay_struct mac_delay;
	struct clk_bulk_data *clks;
	struct device_node *np;
	struct regmap *peri_regmap;
	struct device *dev;
	int phy_mode;
	bool rmii_rxc;
	bool rmii_int_clk;
	int phy_wol_gpio;
	int phy_wol_irq;
};

struct mediatek_dwmac_variant {
	int (*dwmac_set_phy_interface)(struct mediatek_dwmac_plat_data *plat);
	int (*dwmac_set_delay)(struct mediatek_dwmac_plat_data *plat);

	/* clock ids to be requested */
	const char * const *clk_list;
	int num_clks;

	u32 dma_bit_mask;
	u32 rx_delay_max;
	u32 tx_delay_max;
};

/* list of clocks required for mac */
static const char * const mt2712_dwmac_clk_l[] = {
	"axi", "apb", "mac_main", "ptp_ref"
};

/* list of clocks required for mac */
static const char * const mt2731_dwmac_clk_l[] = {
	"mac_main", "ptp_ref", "eth_cg"
};

/* list of clocks required for mac */
static const char * const mt2735_dwmac_clk_l[] = {
	"mac_main", "ptp_ref", "eth_cg", "eth_rmii", "dma"
};


#ifdef CONFIG_DEBUG_FS
static int eth_smt_result = 2;

#define MTK_ETH_FRAME_LEN (ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)

static int FRAME_PATTERN_CH[8] = {
	0x11111111,
	0x22222222,
	0x33333333,
	0x44444444,
	0x55555555,
	0x66666666,
	0x77777777,
	0x88888888,
};

static int frame_hdrs[8][4] = {
	/* for channel 0 : Non tagged header
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x800
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x00000081},

	/* for channel 1 : VLAN tagged header with priority 1
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x64200081},

	/* for channel 2 : VLAN tagged header with priority 2
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0xFFFFFFFF, 0x5500FFFF, 0xF77DB57B, 0x64400081},

	/* for channel 3 : VLAN tagged header with priority 3
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64600081},

	/* for channel 4 : VLAN tagged header with priority 4
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64800081},

	/* for channel 5 : VLAN tagged header with priority 5
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64A00081},

	/* for channel 6 : VLAN tagged header with priority 6
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64C00081},

	/* for channel 7 : VLAN tagged header with priority 7
	 * Dst addr : 0x00:0x0D:0x56:0x73:0xD0:0xF3
	 * Src addr : 0x00:0x55:0x7B:0xB5:0x7D:0xF7
	 * Type/Length : 0x8100
	 */
	{0x73560D00, 0x5500F3D0, 0xF77DB57B, 0x64E00081},
};

static int send_frame(struct stmmac_priv *priv, int num)
{
	struct sk_buff *skb = NULL;
	unsigned int *skb_data = NULL;
	int payload_cnt = 0;
	int i, j, ret, cpu;
	unsigned int frame_size = 1500;
	unsigned int tx_octetcount_gb, tx_framecount_gb;
	unsigned int rx_framecount_gb, rx_octetcount_gb, rx_octetcount_g;
	const struct net_device_ops *netdev_ops = priv->dev->netdev_ops;

	priv->mmc.mmc_tx_octetcount_gb += readl(priv->mmcaddr + 0x14);
	priv->mmc.mmc_tx_framecount_gb += readl(priv->mmcaddr + 0x18);
	priv->mmc.mmc_rx_framecount_gb += readl(priv->mmcaddr + 0x80);
	priv->mmc.mmc_rx_octetcount_gb += readl(priv->mmcaddr + 0x84);
	priv->mmc.mmc_rx_octetcount_g += readl(priv->mmcaddr + 0x88);
	usleep_range(50000, 60000);
	for (j = 0; j < priv->plat->tx_queues_to_use; j++) {
		for (i = 0; i < num; i++) {
			struct netdev_queue *txq =
					netdev_get_tx_queue(priv->dev, j);

			payload_cnt = 0;
			skb = dev_alloc_skb(MTK_ETH_FRAME_LEN);
			if (!skb) {
				pr_err("Failed to allocate tx skb\n");
				return 1;
			}

			skb_set_queue_mapping(skb, j); /*  map skb to queue0 */
			skb_data = (unsigned int *)skb->data;
			/* Add Ethernet header */
			*skb_data++ = frame_hdrs[j][0];
			*skb_data++ = frame_hdrs[j][1];
			*skb_data++ = frame_hdrs[j][2];
			*skb_data++ = frame_hdrs[j][3];
			/* Add payload */
			for (payload_cnt = 0; payload_cnt < frame_size;) {
				*skb_data++ = FRAME_PATTERN_CH[j];
				/* increment by 4 since we are writing
				 * one dword at a time
				 */
				payload_cnt += 4;
			}
			skb->len = frame_size;
			do {
				/* Disable soft irqs for various locks below.
				 * Also stops preemption for RCU.
				 * avoid dql bug-on issue.
				 */
				rcu_read_lock_bh();
				cpu = smp_processor_id();
				HARD_TX_LOCK(priv->dev, txq, cpu);
				ret = netdev_ops->ndo_start_xmit(skb,
								 priv->dev);
				if (ret == NETDEV_TX_OK)
					txq_trans_update(txq);
				HARD_TX_UNLOCK(priv->dev, txq);
				rcu_read_unlock_bh();
			} while (ret != NETDEV_TX_OK);
		}
	}

	usleep_range(50000, 60000);
	tx_octetcount_gb = readl(priv->mmcaddr + 0x14);
	tx_framecount_gb = readl(priv->mmcaddr + 0x18);
	rx_framecount_gb = readl(priv->mmcaddr + 0x80);
	rx_octetcount_gb = readl(priv->mmcaddr + 0x84);
	rx_octetcount_g = readl(priv->mmcaddr + 0x88);
	priv->mmc.mmc_tx_octetcount_gb += tx_octetcount_gb;
	priv->mmc.mmc_tx_framecount_gb += tx_framecount_gb;
	priv->mmc.mmc_rx_framecount_gb += rx_framecount_gb;
	priv->mmc.mmc_rx_octetcount_gb += rx_octetcount_gb;
	priv->mmc.mmc_rx_octetcount_g += rx_octetcount_g;

	if (tx_framecount_gb == rx_framecount_gb &&
	    tx_octetcount_gb == rx_octetcount_gb &&
	    rx_octetcount_gb == rx_octetcount_g &&
	    tx_framecount_gb == (priv->plat->tx_queues_to_use * num)) {
		pr_err("loop back success:\n");
		ret = 0;
	} else {
		pr_err("loop back fail:\n");
		ret = 1;
	}

	pr_err("tx_queues:%d\t pkt_num_per_queue:%d\n",
	       priv->plat->tx_queues_to_use, num);
	pr_err("tx_framecount_gb:%u\t tx_octetcount_gb:%u\n",
	       tx_framecount_gb, tx_octetcount_gb);
	pr_err("rx_framecount_gb:%u\t rx_octetcount_gb:%u, rx_octetcount_g:%u\n",
	       rx_framecount_gb, rx_octetcount_gb, rx_octetcount_g);

	return ret;
}

static ssize_t stmmac_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(netdev);
	int len = 0;
	int buf_len = (int)PAGE_SIZE;
	struct phy_device *phy_dev = of_phy_find_device(priv->plat->phy_node);

	len += snprintf(buf + len, buf_len - len,
			"stmmac debug commands: in hexadecimal notation\n");
	len += snprintf(buf + len, buf_len - len,
			"mac read: echo er reg_start reg_range > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"mac write: echo ew reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause22 read: echo cl22r phy_reg > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause22 write: echo cl22w phy_reg value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause45 read: echo cl45r dev_id reg_addr > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"clause45 write: echo cl45w dev_id reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"reg read: echo rr reg_addr > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"reg write: echo wr reg_addr value > stmmac\n");
	len += snprintf(buf + len, buf_len - len,
			"phy addr:%d\n", phy_dev->mdio.addr);
	len += snprintf(buf + len, buf_len - len,
			"eth_smt_result:%d\n", eth_smt_result);

	return len;
}

static ssize_t stmmac_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf,
			    size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(netdev);
	struct phy_device *phy_dev = of_phy_find_device(priv->plat->phy_node);
	void __iomem *tmp_addr;
	int reg = 0;
	int data = 0;
	int devid = 0;
	int i, origin = 0;
	int reg_addr = 0;
	int phy_addr = 0;

	if (!strncmp(buf, "er", 2) &&
	    (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		pr_err("line:%d, readback:0x%08x\n", __LINE__, readl(priv->mmcaddr));
		for (i = 0; i < data / 0x10 + 1; i++) {
			dev_info(dev,
				 "%08x:\t%08x\t%08x\t%08x\t%08x\t\n",
				 reg + i * 16,
				 readl(priv->ioaddr + reg + i * 0x10),
				 readl(priv->ioaddr + reg + i * 0x10 + 0x4),
				 readl(priv->ioaddr + reg + i * 0x10 + 0x8),
				 readl(priv->ioaddr + reg + i * 0x10 + 0xc));
		}
	} else if (!strncmp(buf, "ew", 2) &&
		   (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		origin = readl(priv->ioaddr + reg);
		writel(data, priv->ioaddr + reg);
		dev_info(dev, "mac reg%#x, value:%#x -> %#x\n",
			 reg, origin, readl(priv->ioaddr + reg));
	} else if (!strncmp(buf, "cl22r", 5) &&
		   (sscanf(buf + 5, "%x", &reg) == 1)) {
		dev_info(dev, "cl22 reg%#x, value:%#x\n",
			 reg, priv->mii->read(priv->mii,
			 phy_dev->mdio.addr, reg));
	} else if (!strncmp(buf, "rp", 2) &&
		   (sscanf(buf + 2, "%x %x", &phy_addr, &reg) == 2)) {
		dev_info(dev, "phyaddr%#x reg%#x, value:%#x\n", phy_addr,
			 reg, priv->mii->read(priv->mii, phy_addr, reg));
	} else if (!strncmp(buf, "wp", 2) &&
		   (sscanf(buf + 5, "%x %x %x", &phy_addr, &reg, &data) == 3)) {
		origin = priv->mii->read(priv->mii, phy_addr, reg);
		priv->mii->write(priv->mii, phy_addr, reg, data);
		dev_info(dev, "phy_addr%#x reg%#x, %#x -> %#x\n", phy_addr,
			 reg, origin, priv->mii->read(priv->mii, phy_addr, reg));
	} else if (!strncmp(buf, "smt", 3)) {
		eth_smt_result = 0;
		if(priv->mii->read(priv->mii, phy_dev->mdio.addr, 2) == 0x22 &&
			priv->mii->read(priv->mii, phy_dev->mdio.addr, 3) == 0x1642)
				eth_smt_result = 1;
		dev_info(dev, "eth_smt_result:%d\n", eth_smt_result);
	} else if (!strncmp(buf, "cl22w", 5) &&
		   (sscanf(buf + 5, "%x %x", &reg, &data) == 2)) {
		origin = priv->mii->read(priv->mii, phy_dev->mdio.addr, reg);
		priv->mii->write(priv->mii, phy_dev->mdio.addr, reg, data);
		dev_info(dev, "cl22 reg%#x, %#x -> %#x\n",
			 reg, origin, priv->mii->read(priv->mii,
			 phy_dev->mdio.addr, reg));
	} else if (!strncmp(buf, "cl45r", 5) &&
		   (sscanf(buf + 5, "%x %x", &devid, &reg) == 2)) {
		reg_addr = MII_ADDR_C45 |
			   ((devid & 0x1f) << 16) |
			   (reg & 0xffff);
		dev_info(dev, "cl45 reg:%#x-%#x, %#x\n", devid, reg,
			 priv->mii->read(priv->mii, phy_dev->mdio.addr,
					 reg_addr));
	} else if (!strncmp(buf, "cl45w", 5) &&
		   (sscanf(buf + 5, "%x %x %x", &devid, &reg, &data) == 3)) {
		reg_addr = MII_ADDR_C45 |
			   ((devid & 0x1f) << 16) |
			   (reg & 0xffff);
		origin = priv->mii->read(priv->mii,
					 phy_dev->mdio.addr,
					 reg_addr);
		priv->mii->write(priv->mii, phy_dev->mdio.addr,
				 reg_addr, data);
		dev_info(dev, "cl45 reg:%#x-%#x, %#x -> %#x\n",
			 devid, reg, origin,
			 priv->mii->read(priv->mii,
					 phy_dev->mdio.addr,
					 reg_addr));
	} else if (!strncmp(buf, "rr", 2) &&
		   (sscanf(buf + 2, "%x", &reg) == 1)) {
		tmp_addr = ioremap_nocache(reg, 32);
		data = readl(tmp_addr);
		dev_info(dev, "rr reg%#x, value:%#x\n", reg, data);
	} else if (!strncmp(buf, "wr", 2) &&
		   (sscanf(buf + 2, "%x %x", &reg, &data) == 2)) {
		tmp_addr = ioremap_nocache(reg, 32);
		origin = readl(tmp_addr);
		writel(data, tmp_addr);
		dev_info(dev, "reg%#x, value:%#x -> %#x\n",
			 reg, origin, readl(tmp_addr));
	} else if (!strncmp(buf, "dump_mac", 8)) {
		for (i = 0; i < 0x1300 / 0x10 + 1; i++) {
			pr_info("%08x:\t%08x\t%08x\t%08x\t%08x\t\n",
				reg + i * 16,
				readl(priv->ioaddr + i * 0x10),
				readl(priv->ioaddr + i * 0x10 + 0x4),
				readl(priv->ioaddr + i * 0x10 + 0x8),
				readl(priv->ioaddr + i * 0x10 + 0xc));
		}
	} else if (!strncmp(buf, "tx", 2) &&
		   (sscanf(buf + 2, "%d", &reg) == 1)) {
		eth_smt_result = 0;
		if (!send_frame(priv, reg))
			eth_smt_result = 1;
	} else if (!strncmp(buf, "phyloop", 7) &&
		   (sscanf(buf + 7, "%d", &reg) == 1)) {
		dev_info(dev, "####phy_loopback\n");

		netif_carrier_off(priv->dev);
		phy_loopback(phy_dev, 1);
		writel(0x8062203, priv->ioaddr);
	} else if (!strncmp(buf, "carrier_on", 10)) {
		netif_carrier_on(priv->dev);
	} else if (!strncmp(buf, "carrier_off", 11)) {
		netif_carrier_off(priv->dev);
	} else {
		dev_info(dev, "Error: command not support\n");
	}

	return count;
}

static DEVICE_ATTR_RW(stmmac);

static int eth_create_attr(struct device *dev)
{
	int err = 0;

	if (!dev)
		return -EINVAL;

	err = device_create_file(dev, &dev_attr_stmmac);
	if (err)
		pr_err("create debug file fail:%s\n", __func__);

	return err;
}

static void eth_remove_attr(struct device *dev)
{
	device_remove_file(dev, &dev_attr_stmmac);
}
#endif

static int mt2712_set_interface(struct mediatek_dwmac_plat_data *plat)
{
	int rmii_rxc = plat->rmii_rxc ? RMII_CLK_SRC_RXC : 0;
	u32 intf_val = 0;

	/* select phy interface in top control domain */
	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		intf_val |= PHY_INTF_MII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		intf_val |= (PHY_INTF_RMII | rmii_rxc);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		intf_val |= PHY_INTF_RGMII;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	regmap_write(plat->peri_regmap, PERI_ETH_PHY_INTF_SEL, intf_val);

	return 0;
}

static void mt2712_delay_ps2stage(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_RMII:
		/* 550ps per stage for MII/RMII */
		mac_delay->tx_delay /= 550;
		mac_delay->rx_delay /= 550;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		/* 170ps per stage for RGMII */
		mac_delay->tx_delay /= 170;
		mac_delay->rx_delay /= 170;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		break;
	}
}

static int mt2712_set_delay(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 delay_val = 0, fine_val = 0;

	mt2712_delay_ps2stage(plat);

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		delay_val |= FIELD_PREP(ETH_DLY_TXC_ENABLE, !!mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_TXC_STAGES, mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_TXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	case PHY_INTERFACE_MODE_RMII:
		/* the rmii reference clock is from external phy,
		 * and the property "rmii_rxc" indicates which pin(TXC/RXC)
		 * the reference clk is connected to. The reference clock is a
		 * received signal, so rx_delay/rx_inv are used to indicate
		 * the reference clock timing adjustment
		 */
		if (plat->rmii_rxc) {
			/* the rmii reference clock from outside is connected
			 * to RXC pin, the reference clock will be adjusted
			 * by RXC delay macro circuit.
			 */
			delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
		} else {
			/* the rmii reference clock from outside is connected
			 * to TXC pin, the reference clock will be adjusted
			 * by TXC delay macro circuit.
			 */
			delay_val |= FIELD_PREP(ETH_DLY_TXC_ENABLE, !!mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_TXC_STAGES, mac_delay->rx_delay);
			delay_val |= FIELD_PREP(ETH_DLY_TXC_INV, mac_delay->rx_inv);
		}
		/* tx_inv will inverse the tx clock inside mac relateive to
		 * reference clock from external phy,
		 * and this bit is located in the same register with fine-tune
		 */
		if (mac_delay->tx_inv)
			fine_val = ETH_RMII_DLY_TX_INV;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		fine_val = ETH_FINE_DLY_GTXC | ETH_FINE_DLY_RXC;

		delay_val |= FIELD_PREP(ETH_DLY_GTXC_ENABLE, !!mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_GTXC_STAGES, mac_delay->tx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_GTXC_INV, mac_delay->tx_inv);

		delay_val |= FIELD_PREP(ETH_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_STAGES, mac_delay->rx_delay);
		delay_val |= FIELD_PREP(ETH_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}
	regmap_write(plat->peri_regmap, PERI_ETH_DLY, delay_val);
	regmap_write(plat->peri_regmap, PERI_ETH_DLY_FINE, fine_val);

	return 0;
}

static const struct mediatek_dwmac_variant mt2712_gmac_variant = {
	.dwmac_set_phy_interface = mt2712_set_interface,
	.dwmac_set_delay = mt2712_set_delay,
	.clk_list = mt2712_dwmac_clk_l,
	.num_clks = ARRAY_SIZE(mt2712_dwmac_clk_l),
	.dma_bit_mask = 33,
	.rx_delay_max = 17600,
	.tx_delay_max = 17600,
};

static int mt2731_set_interface(struct mediatek_dwmac_plat_data *plat)
{
	int rmii_clk = MT2731_RMII_CLK_INT_RXC;
	u32 intf_val = 0;

	/* select phy interface in top control domain */
	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		intf_val |= FIELD_PREP(MT2731_ETH_INTF_SEL, PHY_INTF_MII);
		intf_val |= FIELD_PREP(MT2731_RMII_CLK_SRC_SEL, rmii_clk);
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (plat->rmii_int_clk)
			rmii_clk = MT2731_RMII_CLK_INT_RXC;
		else
			rmii_clk = plat->rmii_rxc ? MT2731_RMII_CLK_EXT_RXC :
						    MT2731_RMII_CLK_EXT_TXC;
		intf_val |= FIELD_PREP(MT2731_ETH_INTF_SEL, PHY_INTF_RMII);
		intf_val |= (PHY_INTF_RMII | rmii_clk);
		intf_val |= FIELD_PREP(MT2731_RMII_CLK_SRC_SEL, rmii_clk);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		intf_val |= FIELD_PREP(MT2731_ETH_INTF_SEL, PHY_INTF_RGMII);
		intf_val |= FIELD_PREP(MT2731_RMII_CLK_SRC_SEL, rmii_clk);
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	regmap_write(plat->peri_regmap, MT2731_PERI_ETH_CTRL1, intf_val);

	return 0;
}

static void mt2731_delay_ps2stage(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_RMII:
		/* 6000ps per stage for MII/RMII */
		mac_delay->tx_delay /= 600;
		mac_delay->rx_delay /= 600;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		/* 100ps per stage for RGMII tx short delay */
		mac_delay->tx_delay = (mac_delay->tx_delay - 1400) / 100;
		/* 600ps per stage for RGMII rx delay */
		mac_delay->rx_delay /= 600;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		break;
	}
}

static int mt2731_set_delay(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 ctrl1_val = 0;
	u32 ctrl2_val = 0;
	u32 ctrl3_val = 0;

	mt2731_delay_ps2stage(plat);

	regmap_read(plat->peri_regmap, MT2731_PERI_ETH_CTRL1, &ctrl1_val);

	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
		ctrl1_val |= FIELD_PREP(MT2731_DLY_TXC_ENABLE, !!mac_delay->tx_delay);
		ctrl1_val |= FIELD_PREP(MT2731_DLY_TXC_STAGES, mac_delay->tx_delay);
		ctrl1_val |= FIELD_PREP(MT2731_DLY_TXC_INV, mac_delay->tx_inv);

		ctrl1_val |= FIELD_PREP(MT2731_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		ctrl1_val |= FIELD_PREP(MT2731_DLY_RXC_STAGES, mac_delay->rx_delay);
		ctrl1_val |= FIELD_PREP(MT2731_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	case PHY_INTERFACE_MODE_RMII:
		/* the rmii reference clock is from external phy,
		 * and the property "rmii_rxc" indicates which pin(TXC/RXC)
		 * the reference clk is connected to. The reference clock is a
		 * received signal, so rx_delay/rx_inv are used to indicate
		 * the reference clock timing adjustment
		 */
		if (plat->rmii_int_clk) {
			ctrl2_val |= FIELD_PREP(MT2731_DLY_INT_TXC_COARSE_EN,
						!!mac_delay->tx_delay);
			ctrl2_val |= FIELD_PREP(MT2731_DLY_INT_TXC_COARSE_STG,
						mac_delay->tx_delay);
			ctrl2_val |= FIELD_PREP(MT2731_DLY_INT_TXC_INV,
						mac_delay->tx_inv);

			ctrl2_val |= FIELD_PREP(MT2731_DLY_INT_RXC_COARSE_EN,
						!!mac_delay->rx_delay);
			ctrl2_val |= FIELD_PREP(MT2731_DLY_INT_RXC_COARSE_STG,
						mac_delay->rx_delay);
			ctrl2_val |= FIELD_PREP(MT2731_DLY_INT_RXC_INV,
						mac_delay->rx_inv);
		} else if (plat->rmii_rxc) {
			/* the rmii reference clock from outside is connected
			 * to RXC pin, the reference clock will be adjusted
			 * by RXC delay macro circuit.
			 */
			ctrl1_val |= FIELD_PREP(MT2731_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
			ctrl1_val |= FIELD_PREP(MT2731_DLY_RXC_STAGES, mac_delay->rx_delay);
			ctrl1_val |= FIELD_PREP(MT2731_DLY_RXC_INV, mac_delay->rx_inv);
		} else {
			/* the rmii reference clock from outside is connected
			 * to TXC pin, the reference clock will be adjusted
			 * by TXC delay macro circuit.
			 */
			ctrl1_val |= FIELD_PREP(MT2731_DLY_TXC_ENABLE, !!mac_delay->rx_delay);
			ctrl1_val |= FIELD_PREP(MT2731_DLY_TXC_STAGES, mac_delay->rx_delay);
			ctrl1_val |= FIELD_PREP(MT2731_DLY_TXC_INV, mac_delay->rx_inv);
		}
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		ctrl3_val |= FIELD_PREP(MT2731_DLY_GTXC_FINE_EN, !!mac_delay->tx_delay);
		ctrl3_val |= FIELD_PREP(MT2731_DLY_GTXC_FINE_STG, mac_delay->tx_delay);
		ctrl3_val |= FIELD_PREP(MT2731_DLY_GTXC_INV, !mac_delay->tx_inv);

		ctrl3_val |= FIELD_PREP(MT2731_DLY_RXC_ENABLE, !!mac_delay->rx_delay);
		ctrl3_val |= FIELD_PREP(MT2731_DLY_RXC_STAGES, mac_delay->rx_delay);
		ctrl3_val |= FIELD_PREP(MT2731_DLY_RXC_INV, mac_delay->rx_inv);
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}
	regmap_write(plat->peri_regmap, MT2731_PERI_ETH_CTRL1, ctrl1_val);
	regmap_write(plat->peri_regmap, MT2731_PERI_ETH_CTRL2, ctrl2_val);
	regmap_write(plat->peri_regmap, MT2731_PERI_ETH_CTRL3, ctrl3_val);

	return 0;
}


static const struct mediatek_dwmac_variant mt2731_gmac_variant = {
	.dwmac_set_phy_interface = mt2731_set_interface,
	.dwmac_set_delay = mt2731_set_delay,
	.clk_list = mt2731_dwmac_clk_l,
	.num_clks = ARRAY_SIZE(mt2731_dwmac_clk_l),
	.dma_bit_mask = 32,
	.rx_delay_max = 2800,
	.tx_delay_max = 2800,
};

static int mt2735_set_interface(struct mediatek_dwmac_plat_data *plat)
{
	u32 intf_val = 0;

	regmap_read(plat->peri_regmap, MT2735_PERI_ETH_CTRL1, &intf_val);

	/* select phy interface in top control domain */
	switch (plat->phy_mode) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_RMII:
		/*MII/RMII external clock*/
		intf_val &= ~MT2735_ETH_CLK_SEL;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
		intf_val &= ~MT2735_ETH_INTF_SEL;
		intf_val |= FIELD_PREP(MT2735_ETH_INTF_SEL, PHY_INTF_RGMII);
		break;
	case PHY_INTERFACE_MODE_SGMII:
		intf_val &= ~MT2735_ETH_PHY_SEL;
		break;
	default:
		dev_err(plat->dev, "phy interface not supported\n");
		return -EINVAL;
	}

	regmap_write(plat->peri_regmap, MT2735_PERI_ETH_CTRL1, intf_val);

	return 0;
}

static int mt2735_set_delay(struct mediatek_dwmac_plat_data *plat)
{
	return 0;
}

static const struct mediatek_dwmac_variant mt2735_gmac_variant = {
	.dwmac_set_phy_interface = mt2735_set_interface,
	.dwmac_set_delay = mt2735_set_delay,
	.clk_list = mt2735_dwmac_clk_l,
	.num_clks = ARRAY_SIZE(mt2735_dwmac_clk_l),
	.dma_bit_mask = 32,
	.rx_delay_max = 2800,
	.tx_delay_max = 2800,
};

static irqreturn_t mediatek_dwmac_phy_wol_interrupt(int irq, void *dev_id)
{
	struct device *dev = (struct device *)(dev_id);

	dev_dbg(dev, "Wake up SPM by PHY WOL\n");

	return IRQ_HANDLED;
}

static int mediatek_dwmac_config_dt(struct mediatek_dwmac_plat_data *plat)
{
	struct mac_delay_struct *mac_delay = &plat->mac_delay;
	u32 tx_delay_ps, rx_delay_ps;
	int ret;

	plat->peri_regmap = syscon_regmap_lookup_by_phandle(plat->np, "mediatek,pericfg");
	if (IS_ERR(plat->peri_regmap)) {
		dev_err(plat->dev, "Failed to get pericfg syscon\n");
		return PTR_ERR(plat->peri_regmap);
	}

	plat->phy_mode = of_get_phy_mode(plat->np);
	if (plat->phy_mode < 0) {
		dev_err(plat->dev, "not find phy-mode\n");
		return -EINVAL;
	}

	if (!of_property_read_u32(plat->np, "mediatek,tx-delay-ps", &tx_delay_ps)) {
		if (tx_delay_ps < plat->variant->tx_delay_max) {
			mac_delay->tx_delay = tx_delay_ps;
		} else {
			dev_err(plat->dev, "Invalid TX clock delay: %dps\n", tx_delay_ps);
			return -EINVAL;
		}
	}

	if (!of_property_read_u32(plat->np, "mediatek,rx-delay-ps", &rx_delay_ps)) {
		if (rx_delay_ps < plat->variant->rx_delay_max) {
			mac_delay->rx_delay = rx_delay_ps;
		} else {
			dev_err(plat->dev, "Invalid RX clock delay: %dps\n", rx_delay_ps);
			return -EINVAL;
		}
	}

	mac_delay->tx_inv = of_property_read_bool(plat->np, "mediatek,txc-inverse");
	mac_delay->rx_inv = of_property_read_bool(plat->np, "mediatek,rxc-inverse");
	plat->rmii_rxc = of_property_read_bool(plat->np, "mediatek,rmii-rxc");
	plat->rmii_int_clk =
		of_property_read_bool(plat->np, "mediatek,rmii-internal-clk");
	if (plat->rmii_int_clk)
		plat->rmii_rxc = 0;

	plat->phy_wol_gpio = of_get_named_gpio(plat->np, "phy-wol-gpio", 0);
	if (plat->phy_wol_gpio > 0) {
		ret = devm_gpio_request(plat->dev, plat->phy_wol_gpio,
					"phy-wol-pin");
		plat->phy_wol_irq = gpio_to_irq(plat->phy_wol_gpio);

		if (plat->phy_wol_irq > 0) {
			ret = devm_request_irq(plat->dev, plat->phy_wol_irq,
					       mediatek_dwmac_phy_wol_interrupt,
					       IRQF_TRIGGER_RISING, "phy-wol",
					       plat->dev);
			if (unlikely(ret < 0)) {
				dev_err(plat->dev,
					"%s: ERROR: allocating PHY WoL IRQ %d (%d)\n",
					__func__, plat->phy_wol_irq, ret);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int mediatek_dwmac_clk_init(struct mediatek_dwmac_plat_data *plat)
{
	const struct mediatek_dwmac_variant *variant = plat->variant;
	int i, num = variant->num_clks;

	plat->clks = devm_kcalloc(plat->dev, num, sizeof(*plat->clks), GFP_KERNEL);
	if (!plat->clks)
		return -ENOMEM;

	for (i = 0; i < num; i++)
		plat->clks[i].id = variant->clk_list[i];

	return devm_clk_bulk_get(plat->dev, num, plat->clks);
}

static int mediatek_dwmac_init(struct platform_device *pdev, void *priv)
{
	struct mediatek_dwmac_plat_data *plat = priv;
	const struct mediatek_dwmac_variant *variant = plat->variant;
	int ret;

	ret = dma_set_mask_and_coherent(plat->dev, DMA_BIT_MASK(variant->dma_bit_mask));
	if (ret) {
		dev_err(plat->dev, "No suitable DMA available, err = %d\n", ret);
		return ret;
	}

	ret = variant->dwmac_set_phy_interface(plat);
	if (ret) {
		dev_err(plat->dev, "failed to set phy interface, err = %d\n", ret);
		return ret;
	}

	ret = variant->dwmac_set_delay(plat);
	if (ret) {
		dev_err(plat->dev, "failed to set delay value, err = %d\n", ret);
		return ret;
	}

	ret = clk_bulk_prepare_enable(variant->num_clks, plat->clks);
	if (ret) {
		dev_err(plat->dev, "failed to enable clks, err = %d\n", ret);
		return ret;
	}

	if (plat->phy_wol_irq > 0)
		disable_irq_wake(plat->phy_wol_irq);

	return 0;
}

static void mediatek_dwmac_exit(struct platform_device *pdev, void *priv)
{
	struct mediatek_dwmac_plat_data *plat = priv;
	const struct mediatek_dwmac_variant *variant = plat->variant;

	clk_bulk_disable_unprepare(variant->num_clks, plat->clks);

	if (plat->phy_wol_irq > 0)
		enable_irq_wake(plat->phy_wol_irq);
}

static int mediatek_dwmac_probe(struct platform_device *pdev)
{
	struct mediatek_dwmac_plat_data *priv_plat;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	int ret;

	priv_plat = devm_kzalloc(&pdev->dev, sizeof(*priv_plat), GFP_KERNEL);
	if (!priv_plat)
		return -ENOMEM;

	priv_plat->variant = of_device_get_match_data(&pdev->dev);
	if (!priv_plat->variant) {
		dev_err(&pdev->dev, "Missing dwmac-mediatek variant\n");
		return -EINVAL;
	}

	priv_plat->dev = &pdev->dev;
	priv_plat->np = pdev->dev.of_node;

	ret = mediatek_dwmac_config_dt(priv_plat);
	if (ret)
		return ret;

	ret = mediatek_dwmac_clk_init(priv_plat);
	if (ret)
		return ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	plat_dat->interface = priv_plat->phy_mode;
	plat_dat->has_gmac4 = 1;
	plat_dat->has_gmac = 0;
	plat_dat->pmt = 0;
	plat_dat->riwt_off = 1;
	plat_dat->maxmtu = ETH_DATA_LEN;
	plat_dat->bsp_priv = priv_plat;
	plat_dat->init = mediatek_dwmac_init;
	plat_dat->exit = mediatek_dwmac_exit;
	mediatek_dwmac_init(pdev, priv_plat);

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret) {
		stmmac_remove_config_dt(pdev, plat_dat);
		return ret;
	}

#ifdef CONFIG_DEBUG_FS
	eth_create_attr(&pdev->dev);
#endif
	return 0;
}

static int mediatek_dwmac_remove(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_DEBUG_FS
	eth_remove_attr(&pdev->dev);
#endif
	stmmac_pltfr_remove(pdev);

	ret = pm_runtime_put_sync(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "put power fail, ret=%d\n", ret);
		return ret;
	}
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id mediatek_dwmac_match[] = {
	{ .compatible = "mediatek,mt2712-gmac",
	  .data = &mt2712_gmac_variant },
	{ .compatible = "mediatek,mt2731-gmac",
	  .data = &mt2731_gmac_variant },
	{ .compatible = "mediatek,mt2735-gmac",
	  .data = &mt2735_gmac_variant },
	{ }
};

MODULE_DEVICE_TABLE(of, mediatek_dwmac_match);

static struct platform_driver mediatek_dwmac_driver = {
	.probe  = mediatek_dwmac_probe,
	.remove = mediatek_dwmac_remove,
	.driver = {
		.name           = "dwmac-mediatek",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = mediatek_dwmac_match,
	},
};
module_platform_driver(mediatek_dwmac_driver);

MODULE_AUTHOR("Biao Huang <biao.huang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek DWMAC specific glue layer");
MODULE_LICENSE("GPL v2");
