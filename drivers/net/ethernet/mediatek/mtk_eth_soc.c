// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 *   Copyright (C) 2009-2016 John Crispin <blogic@openwrt.org>
 *   Copyright (C) 2009-2016 Felix Fietkau <nbd@openwrt.org>
 *   Copyright (C) 2013-2016 Michael Lee <igvtee@gmail.com>
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

static int mtk_msg_level = -1;
module_param_named(msg_level, mtk_msg_level, int, 0);
MODULE_PARM_DESC(msg_level, "Message level (-1=defaults,0=none,...,16=all)");
static struct pinctrl *eth_gpio;
/*static struct pinctrl_state *eth_gpio_default;*/
static struct pinctrl_state *eth_smi_mdio_pinctl;
static struct pinctrl_state *eth_smi_mdc_pinctl;
#define MTK_ETHTOOL_STAT(x) { #x, \
			      offsetof(struct mtk_hw_stats, x) / sizeof(u64) }

/* strings used by ethtool */
static const struct mtk_ethtool_stats {
	char str[ETH_GSTRING_LEN];
	u32 offset;
} mtk_ethtool_stats[] = {
	MTK_ETHTOOL_STAT(tx_bytes),
	MTK_ETHTOOL_STAT(tx_packets),
	MTK_ETHTOOL_STAT(tx_skip),
	MTK_ETHTOOL_STAT(tx_collisions),
	MTK_ETHTOOL_STAT(rx_bytes),
	MTK_ETHTOOL_STAT(rx_packets),
	MTK_ETHTOOL_STAT(rx_overflow),
	MTK_ETHTOOL_STAT(rx_fcs_errors),
	MTK_ETHTOOL_STAT(rx_short_errors),
	MTK_ETHTOOL_STAT(rx_long_errors),
	MTK_ETHTOOL_STAT(rx_checksum_errors),
	MTK_ETHTOOL_STAT(rx_flow_control_packets),
};

static const char * const mtk_clks_source_name[] = {
	"ethif", "sgmiitop", "esw", "gp0", "gp1", "gp2", "fe", "trgpll",
	"sgmii_tx250m", "sgmii_rx250m", "sgmii_cdr_ref", "sgmii_cdr_fb",
	"sgmii2_tx250m", "sgmii2_rx250m", "sgmii2_cdr_ref", "sgmii2_cdr_fb",
	"sgmii_ck", "eth2pll", "net_sel", "med_sel", "net_500_sel", "med_mcu_sel",
	"wed_mcu_sel", "net_2x_sel", "sgmii_sel", "sgmii_sbus_sel",
};

void mtk_w32(struct mtk_eth *eth, u32 val, unsigned reg)
{
	__raw_writel(val, eth->base + reg);
}

u32 mtk_r32(struct mtk_eth *eth, unsigned reg)
{
	return __raw_readl(eth->base + reg);
}

static int mtk_mdio_busy_wait(struct mtk_eth *eth)
{
	unsigned long t_start = jiffies;

	while (1) {
		if (!(mtk_r32(eth, MTK_PHY_IAC) & PHY_IAC_ACCESS))
			return 0;
		if (time_after(jiffies, t_start + PHY_IAC_TIMEOUT))
			break;
		usleep_range(10, 20);
	}

	dev_err(eth->dev, "mdio: MDIO timeout\n");
	return -1;
}

u32 _mtk_mdio_write(struct mtk_eth *eth, u32 phy_addr,
			   u32 phy_register, u32 write_data)
{
	if (mtk_mdio_busy_wait(eth))
		return -1;

	write_data &= 0xffff;

	mtk_w32(eth, PHY_IAC_ACCESS | PHY_IAC_START | PHY_IAC_WRITE |
		(phy_register << PHY_IAC_REG_SHIFT) |
		(phy_addr << PHY_IAC_ADDR_SHIFT) | write_data,
		MTK_PHY_IAC);

	if (mtk_mdio_busy_wait(eth))
		return -1;

	return 0;
}

u32 mii_mgr_cl45_set_address(struct mtk_eth *eth, u32 port_num, u32 devad, u32 reg_addr)
{
	u32 data;

	data =
	    (devad << 25) | (port_num << 20) | (0x00 << 18) | (0x00 << 16) |
	    reg_addr;

	mtk_w32(eth, data, MTK_PHY_IAC);
	mtk_w32(eth, data | (1<<31) , MTK_PHY_IAC );

	return 0;
}
u32 _mtk_mdio_read_C45(struct mtk_eth *eth, u32 phy_addr,
			   u32 phy_register, u32 devad,u32 debug_flag)
{
	u32 data;
	u32 d;

	if (mtk_mdio_busy_wait(eth))
		return -1;

	/* set address first */
	mii_mgr_cl45_set_address(eth, phy_addr, devad, phy_register);
	/* udelay(10); */

	if (mtk_mdio_busy_wait(eth))
		return -1;

	data = (devad << PHY_IAC_REG_SHIFT) | (phy_addr << PHY_IAC_ADDR_SHIFT) |
	       (0x03 << MDIO_CMD) | (0x00 << NMDIO_ST) | phy_register;

	mtk_w32(eth, data, MTK_PHY_IAC);
	mtk_w32(eth, data | (1<<31) , MTK_PHY_IAC);

	if (mtk_mdio_busy_wait(eth))
		return -1;

	d = mtk_r32(eth, MTK_PHY_IAC) & 0xffff;

    if(1 == debug_flag){
        dev_info(eth->dev,"%s()->%d phy_addr=0x%08X,phy_register=0x%08X,read_data=0x%08X,devad=0x%08X\n",
                __func__,
                __LINE__,
                phy_addr,
                phy_register,
                d,
                devad);
    }
	return d;

}
u32 _mtk_mdio_write_C45(struct mtk_eth *eth, u32 phy_addr,
			   u32 phy_register, u32 write_data, u32 devad,u32 debug_flag)
{
	u32 data;

	if (mtk_mdio_busy_wait(eth))
		return -1;

	write_data &= 0xffff;

	/* set address first */
	mii_mgr_cl45_set_address(eth, phy_addr, devad, phy_register);
	/* udelay(10); */

	if (mtk_mdio_busy_wait(eth))
		return -1;
	data =
	    (devad << PHY_IAC_REG_SHIFT) | (phy_addr << PHY_IAC_ADDR_SHIFT) |
	    PHY_IAC_WRITE | (0x00 << NMDIO_ST) |
	    write_data;

	mtk_w32(eth, data, MTK_PHY_IAC);
	mtk_w32(eth, data | (1<<31) , MTK_PHY_IAC);

    if(1 == debug_flag){
        dev_info(eth->dev,"%s()->%d phy_addr=0x%08X,phy_register=0x%08X,write_data=0x%08X,devad=0x%08X\n",
                __func__,
                __LINE__,
                phy_addr,
                phy_register,
                write_data,
                devad);
    }

	if (mtk_mdio_busy_wait(eth))
		return -1;

	return 0;
}

u32 _mtk_mdio_read(struct mtk_eth *eth, int phy_addr, int phy_reg)
{
	u32 d;

	if (mtk_mdio_busy_wait(eth))
		return 0xffff;

	mtk_w32(eth, PHY_IAC_ACCESS | PHY_IAC_START | PHY_IAC_READ |
		(phy_reg << PHY_IAC_REG_SHIFT) |
		(phy_addr << PHY_IAC_ADDR_SHIFT),
		MTK_PHY_IAC);

	if (mtk_mdio_busy_wait(eth))
		return 0xffff;

	d = mtk_r32(eth, MTK_PHY_IAC) & 0xffff;

	return d;
}

u32 mtk_cl45_ind_read(struct mtk_eth *eth, u32 port, u32 devad, u32 reg, u32 *data)
{
	mutex_lock(&eth->mii_bus->mdio_lock);
	*data = _mtk_mdio_read_C45(eth, port, reg, devad,0);
	mutex_unlock(&eth->mii_bus->mdio_lock);
	return 0;
}

u32 mtk_cl45_ind_write(struct mtk_eth *eth, u32 port, u32 devad, u32 reg, u32 data)
{
	mutex_lock(&eth->mii_bus->mdio_lock);
	_mtk_mdio_write_C45(eth, port, reg, data, devad,0);
	mutex_unlock(&eth->mii_bus->mdio_lock);
	return 0;
}

/*BEGIN support mdio c45 cmd add by haowei.cheng*/
static int mtk_mdio_write(struct mii_bus *bus, int phy_addr,
			  int phy_reg, u16 val)
{
	struct mtk_eth *eth = bus->priv;
	
	int devad=0;
	//u32 addr = MII_ADDR_C45 | (devad << 16) | (regnum & 0xffff);
	//ret = __mdiobus_write(phydev->mdio.bus, phydev->mdio.addr,addr, val);
	if(phy_reg&MII_ADDR_C45){
		devad = ((phy_reg&0xFFFF0000)>>16)&0xFFFF;
		phy_reg = phy_reg&0xFFFF;
		return _mtk_mdio_write_C45(eth,phy_addr,phy_reg,val,devad,0);
	}else{
		return _mtk_mdio_write(eth, phy_addr, phy_reg, val);
	}
}

static int mtk_mdio_read(struct mii_bus *bus, int phy_addr, int phy_reg)
{
	struct mtk_eth *eth = bus->priv;
	int devad=0;
	//u32 addr = MII_ADDR_C45 | (devad << 16) | (regnum & 0xffff);
    //val = __mdiobus_read(phydev->mdio.bus, phydev->mdio.addr, addr);
    if(phy_reg&MII_ADDR_C45){
		devad = ((phy_reg&0xFFFF0000)>>16)&0xFFFF;
		phy_reg = phy_reg&0xFFFF;
		return _mtk_mdio_read_C45(eth,phy_addr,phy_reg,devad,0);
	}else{
		return _mtk_mdio_read(eth, phy_addr, phy_reg);
	}
}
/*END support mdio c45 cmd add by haowei.cheng*/

static void mtk_gmac0_rgmii_adjust(struct mtk_eth *eth, int speed)
{
	u32 val;
	int ret;

	val = (speed == SPEED_1000) ?
		INTF_MODE_RGMII_1000 : INTF_MODE_RGMII_10_100;
	mtk_w32(eth, val, INTF_MODE);

	regmap_update_bits(eth->ethsys, ETHSYS_CLKCFG0,
			   ETHSYS_TRGMII_CLK_SEL362_5,
			   ETHSYS_TRGMII_CLK_SEL362_5);

	val = (speed == SPEED_1000) ? 250000000 : 500000000;
	ret = clk_set_rate(eth->clks[MTK_CLK_TRGPLL], val);
	if (ret)
		dev_err(eth->dev, "Failed to set trgmii pll: %d\n", ret);

	val = (speed == SPEED_1000) ?
		RCK_CTRL_RGMII_1000 : RCK_CTRL_RGMII_10_100;
	mtk_w32(eth, val, TRGMII_RCK_CTRL);

	val = (speed == SPEED_1000) ?
		TCK_CTRL_RGMII_1000 : TCK_CTRL_RGMII_10_100;
	mtk_w32(eth, val, TRGMII_TCK_CTRL);
}

/*BEGIN automatically adjust sgmii mode according to UTP rate,add by haowei.cheng*/
void dump_sgmii_reg_info(struct mtk_sgmii *mtk_sgmii,int sgmii_id)
{
    int i=0;
    unsigned int val=0;
    regmap_read(mtk_sgmii->regmap[(sgmii_id)%2], SGMSYS_SGMII_MODE, &val);
}

static unsigned int* get_sgmii_mode(int sgmii_id)
{
    static unsigned int sgmii_mode[2] = {0x31120001,0x31120001};
    pr_info("get sgmii_id %d SGMII_MODE=0x%08x\n",sgmii_id,sgmii_mode[sgmii_id%2]);
    return &sgmii_mode[sgmii_id%2];
}

static int mtk_sgmii_mode_adjust(void* sgmii,int mac_id)
{
    struct mtk_sgmii *mtk_sgmii;
    unsigned int* sgmii_mode = get_sgmii_mode(mac_id);
    int val=0;
    
    mtk_sgmii = (struct mtk_sgmii *)sgmii;

    //test default value
    //*sgmii_mode = 0x31120009;
    //val |= (*sgmii_mode)&0x0F;
    //pr_info("set mac %d SGMII_MODE=0x%08x\n",mac->id,val);
    //regmap_write(eth->sgmii->regmap[(mac->id)%2], SGMSYS_SGMII_MODE, val);

    regmap_read(mtk_sgmii->regmap[(mac_id)%2], SGMSYS_SGMII_MODE, &val);
    pr_info("get gmac %d sgmii mode=0x%08x\n",mac_id,val);

    val = 0x31120009;
    val &= (~0x1F);
    val |= (*sgmii_mode)&0x0F;
    pr_info("set gmac %d sgmii mode=0x%08x\n",mac_id,val);
    regmap_write(mtk_sgmii->regmap[(mac_id)%2], SGMSYS_SGMII_MODE, val);
    return 0;
}


static void mtk_phy_link_adjust(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
    struct mtk_eth *eth;
    unsigned int* sgmii_mode;
	u16 lcl_adv = 0, rmt_adv = 0;
	u8 flowctrl;
    int err=0;
    int val=0;
    
    
    eth = mac->hw;
    sgmii_mode = get_sgmii_mode(mac->id);
    *sgmii_mode = 0x31120001;
    
	u32 mcr = MAC_MCR_MAX_RX_1536 | MAC_MCR_IPG_CFG |
		  MAC_MCR_FORCE_MODE | MAC_MCR_TX_EN |
		  MAC_MCR_RX_EN | MAC_MCR_BACKOFF_EN |
		  MAC_MCR_BACKPR_EN;

	if (mac->id == 0)
		mcr = mcr | MCA_MCR_RXFIFO_CLR;

	if (unlikely(test_bit(MTK_RESETTING, &mac->hw->state)))
		return;

    switch (dev->phydev->speed) {
        case SPEED_1000:
            *sgmii_mode |= BIT(3);
            mcr |= MAC_MCR_SPEED_1000;
        break;
        case SPEED_100:
            *sgmii_mode |= BIT(2);
            mcr |= MAC_MCR_SPEED_100;
        break;
    };


	if (MTK_HAS_CAPS(mac->hw->soc->caps, MTK_GMAC1_TRGMII) &&
		!mac->id && !mac->trgmii)
		mtk_gmac0_rgmii_adjust(mac->hw, dev->phydev->speed);

	if (dev->phydev->link)
		mcr |= MAC_MCR_FORCE_LINK;


	if (dev->phydev->duplex) {
		mcr |= MAC_MCR_FORCE_DPX;

		if (dev->phydev->pause)
			rmt_adv = LPA_PAUSE_CAP;
		if (dev->phydev->asym_pause)
			rmt_adv |= LPA_PAUSE_ASYM;

		if (dev->phydev->advertising & ADVERTISED_Pause)
			lcl_adv |= ADVERTISE_PAUSE_CAP;
		if (dev->phydev->advertising & ADVERTISED_Asym_Pause)
			lcl_adv |= ADVERTISE_PAUSE_ASYM;

		flowctrl = mii_resolve_flowctrl_fdx(lcl_adv, rmt_adv);

		if (flowctrl & FLOW_CTRL_TX)
			mcr |= MAC_MCR_FORCE_TX_FC;
		if (flowctrl & FLOW_CTRL_RX)
			mcr |= MAC_MCR_FORCE_RX_FC;

		netif_dbg(mac->hw, link, dev, "rx pause %s, tx pause %s\n",
			  flowctrl & FLOW_CTRL_RX ? "enabled" : "disabled",
			  flowctrl & FLOW_CTRL_TX ? "enabled" : "disabled");
	}



    if(1 == mac->id){
        if(get_gmac1_mode()){
            //mcr = mcr | MCA_MCR_RXFIFO_CLR;

            pr_info("disable netsys sgmii ");
            regmap_read(eth->ethsys, ETHSYS_SYSCFG0, &val);
            regmap_update_bits(eth->ethsys, ETHSYS_SYSCFG0,BIT(8), ~(u32)BIT(8));
            
            if(dev->phydev->link){
                pr_info("link up set sgmii modem");
            }else{
                *sgmii_mode = 0x31120009;
                pr_info("link down set sgmii mode");
            }

            pr_info("call mtk_sgmii_setup_mode_force\n");
            mtk_sgmii_setup_mode_force(eth->sgmii, mac->id, eth);

            pr_info("enable netsys sgmii ");
            regmap_update_bits(eth->ethsys, ETHSYS_SYSCFG0,BIT(8),BIT(8));
            pr_info("mac->id=%d,mac reg=0x%08X\n",mac->id,mcr);
            mtk_w32(mac->hw, mcr, MTK_MAC_MCR(mac->id));
            
            pr_info("skip set default mac");
            goto update_phy_state;
        }
    }

    if(0 == mac->id){
        pr_info("set default mac0 config 0x0104f33b");
        mtk_w32(mac->hw, 0x0104f33b, MTK_MAC_MCR(mac->id));
    }else if(1 == mac->id){
        pr_info("set default mac1 config 0x0104e33b");
        mtk_w32(mac->hw, 0x0104e33b, MTK_MAC_MCR(mac->id));
    }
update_phy_state:


	if (dev->phydev->link)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);

	if (!of_phy_is_fixed_link(mac->of_node))
		phy_print_status(dev->phydev);
}
/*END automatically adjust sgmii mode according to UTP rate,add by haowei.cheng*/

static int mtk_phy_connect_node(struct mtk_eth *eth, struct mtk_mac *mac,
				struct device_node *phy_node)
{
	struct phy_device *phydev;
	int phy_mode;

	phy_mode = of_get_phy_mode(phy_node);
	if (phy_mode < 0) {
		dev_err(eth->dev, "incorrect phy-mode %d\n", phy_mode);
		return -EINVAL;
	}

	phydev = of_phy_connect(eth->netdev[mac->id], phy_node,
				mtk_phy_link_adjust, 0, phy_mode);
	if (!phydev) {
		dev_err(eth->dev, "could not connect to PHY\n");
		return -ENODEV;
	}

	dev_info(eth->dev,
		 "connected mac %d to PHY at %s [uid=%08x, driver=%s]\n",
		 mac->id, phydev_name(phydev), phydev->phy_id,
		 phydev->drv->name);

	return 0;
}

static int mtk_phy_connect(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth;
	struct device_node *np;
	u32 val;
	int err;

	eth = mac->hw;
	np = of_parse_phandle(mac->of_node, "phy-handle", 0);
	if (!np && of_phy_is_fixed_link(mac->of_node))
		if (!of_phy_register_fixed_link(mac->of_node))
			np = of_node_get(mac->of_node);
	if (!np)
		return -ENODEV;

	err = mtk_setup_hw_path(eth, mac->id, of_get_phy_mode(np));
	if (err)
		goto err_phy;

	mac->ge_mode = 0;
	switch (of_get_phy_mode(np)) {
	case PHY_INTERFACE_MODE_TRGMII:
		mac->trgmii = true;
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_SGMII:
		break;
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_GMII:
		mac->ge_mode = 1;
		break;
	case PHY_INTERFACE_MODE_REVMII:
		mac->ge_mode = 2;
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (!mac->id)
			goto err_phy;
		mac->ge_mode = 3;
		break;
	default:
		goto err_phy;
	}

	/* put the gmac into the right mode */
	regmap_read(eth->ethsys, ETHSYS_SYSCFG0, &val);
	val &= ~SYSCFG0_GE_MODE(SYSCFG0_GE_MASK, mac->id);
	val |= SYSCFG0_GE_MODE(mac->ge_mode, mac->id);
	regmap_write(eth->ethsys, ETHSYS_SYSCFG0, val);

	/* couple phydev to net_device */
	if (mtk_phy_connect_node(eth, mac, np))
		goto err_phy;

	dev->phydev->autoneg = AUTONEG_ENABLE;
	dev->phydev->speed = 0;
	dev->phydev->duplex = 0;

	if (of_phy_is_fixed_link(mac->of_node))
		dev->phydev->supported |=
		SUPPORTED_Pause | SUPPORTED_Asym_Pause;

	dev->phydev->supported &= PHY_GBIT_FEATURES | SUPPORTED_Pause |
				   SUPPORTED_Asym_Pause;
	dev->phydev->advertising = dev->phydev->supported |
				    ADVERTISED_Autoneg;
	phy_start_aneg(dev->phydev);

	of_node_put(np);

	return 0;

err_phy:
	if (of_phy_is_fixed_link(mac->of_node))
		of_phy_deregister_fixed_link(mac->of_node);
	of_node_put(np);
	dev_err(eth->dev, "%s: invalid phy\n", __func__);
	return -EINVAL;
}

/*BEGIN config gmac mode add by haowei.cheng*/

static int get_phy_id(struct mtk_eth *eth, int addr, u32 *phy_id,int is_c45)
{
	int phy_reg;

	if(is_c45)
		phy_reg = _mtk_mdio_read_C45(eth, addr, MII_PHYSID1,1,0);
	else
		phy_reg = _mtk_mdio_read(eth, addr, MII_PHYSID1);
	
	if (phy_reg < 0) {
		if (phy_reg == -EIO || phy_reg == -ENODEV) {
			*phy_id = 0xffffffff;
			return 0;
		}

		return -EIO;
	}

	*phy_id = (phy_reg & 0xffff) << 16;

	if(is_c45)
		phy_reg = _mtk_mdio_read_C45(eth, addr, MII_PHYSID2,1,0);
	else
		phy_reg = _mtk_mdio_read(eth, addr, MII_PHYSID2);
	
	if (phy_reg < 0) {
		return (phy_reg == -EIO || phy_reg == -ENODEV) ? -ENODEV : -EIO;
	}

	*phy_id |= (phy_reg & 0xffff);

	return 0;
}


struct fg360_phy_mapping {
	char	name[20];
	u16		mac_number;
	u16		phy_exist;
	u32		mac_gen;
	u32		phy_id;
};



static struct fg360_phy_mapping fg360_def_phy_mapping[] = {
	{
		.name 		= "aqr112c",
		.mac_number = 0,
		.phy_exist 	= 0,
		.mac_gen	= 0,
		.phy_id		= 0x03a1b792,
	}, {
		.name = "mt7531AE",
		.mac_number = 1,
		.phy_exist  = 0,
		.mac_gen	= 0,
		.phy_id		= 0x0000181D,
	}, {
		.name = "rtl8221b",
		.mac_number = 0,
		.phy_exist  = 0,
		.mac_gen	= 0,
		.phy_id		= 0x001CC849,
	}, {
		.name = "rtl8211fs",
		.mac_number = 1,
		.phy_exist 	= 0,
		.mac_gen	= 1,
		.phy_id		= 0x001cc916,
	},
};

static struct fg360_phy_mapping fg360_mac_phy_mapping[2]={0};


static int fg360_phy_find_exist(struct device_node *np)
{
	const char *map;
	int i;
	if (of_property_read_string(np, "fg360,phy_name", &map))
		return 0;

	for (i = 0; i < ARRAY_SIZE(fg360_def_phy_mapping); i++)
		if (!strcmp(map, fg360_def_phy_mapping[i].name)){
			pr_info("map=%s exit=%d\n",map,fg360_def_phy_mapping[i].phy_exist);
			if(fg360_def_phy_mapping[i].phy_exist){
				int mac = fg360_def_phy_mapping[i].mac_number;
				pr_info("mac=%d,gen=%d\n",mac,fg360_mac_phy_mapping[0].mac_gen);
				memcpy(&fg360_mac_phy_mapping[mac&1],&fg360_def_phy_mapping[i],sizeof(struct fg360_phy_mapping));
			}
			return fg360_def_phy_mapping[i].phy_exist;
		}
	
	return 0;
}

static int get_node_mac_id(struct mtk_eth *eth, struct device_node *np)
{
	struct mtk_mac *mac;
	const __be32 *_id = of_get_property(np, "reg", NULL);
	int id, err;

	if (!_id) {
		dev_err(eth->dev, "missing mac id\n");
		return -EINVAL;
	}

	id = be32_to_cpup(_id);
	if (id >= MTK_MAC_COUNT) {
		dev_err(eth->dev, "%d is not a valid mac id\n", id);
		return -EINVAL;
	}
	return id;
}


int get_gmac1_mode()
{
	pr_info("_gmac_gen=%d\n",fg360_mac_phy_mapping[1].mac_gen);
	return fg360_mac_phy_mapping[1].mac_gen;
}

int get_gmac_phy_status(int mac_number)
{
	pr_info("mac[%d] phy_exist=%d\n",mac_number,fg360_mac_phy_mapping[mac_number&1].phy_exist);
	return fg360_mac_phy_mapping[mac_number&1].phy_exist;
}


static void scan_phy_id(struct mtk_eth *eth,int is_c45)
{
	int i=0,j=0;
	u32 phy_id=0; 

	for(i=0;i<32;i++){
		get_phy_id(eth,i,&phy_id,is_c45);
		dev_info(eth->dev,"%s addr=%d,phyID=0x%08X\n",__func__,i,phy_id);

		for (j = 0; j < ARRAY_SIZE(fg360_def_phy_mapping); j++){
			if (phy_id == fg360_def_phy_mapping[j].phy_id){
				dev_info(eth->dev,"%s found %s phyID=0x%08X\n",__func__,fg360_def_phy_mapping[j].name,phy_id);
				fg360_def_phy_mapping[j].phy_exist = 1;
			}
		}
	}
}



static int set_reset_pin(struct mtk_eth *eth,struct device_node *np)
{
	int reset_pin=0;
	int ret=0;
	dev_err(eth->dev, "%s np=%p\n", __func__,np);
	reset_pin = of_get_named_gpio(np, "phy1reset-gpios", 0);
	if (reset_pin < 0) {
		dev_err(eth->dev, "Missing reset pin of phy1\n");
		return ret;
	}

	ret = devm_gpio_request(eth->dev, reset_pin, "phy1-reset");
	if (ret) {
		dev_info(eth->dev, "Failed to request gpio %d\n",reset_pin);
		return ret;
	}


	gpio_direction_output(reset_pin, 1);
	msleep(30);
	gpio_set_value(reset_pin, 1);
	msleep(500);
	return 0;
}

static int release_reset_pin(struct mtk_eth *eth,struct device_node *np)
{
	int reset_pin=0;
	int ret=0;
	dev_err(eth->dev, "%s np=%p\n", __func__,np);
	reset_pin = of_get_named_gpio(np, "phy1reset-gpios", 0);
	if (reset_pin < 0) {
		dev_err(eth->dev, "Missing reset pin of phy1\n");
		return ret;
	}

	devm_gpio_free(eth->dev, reset_pin);

	return 0;
}


int auto_scan_phy(struct mtk_eth *eth)
{
	struct device_node *mii_np;
	int ret;

	mii_np = of_get_child_by_name(eth->dev->of_node, "mdio-bus");
	if (!mii_np) {
		dev_err(eth->dev, "no %s child node found", "mdio-bus");
		return -ENODEV;
	}

	set_reset_pin(eth,mii_np);

	scan_phy_id(eth,0);
	scan_phy_id(eth,1);
	release_reset_pin(eth,mii_np);
	

	of_node_put(mii_np);
	return ret;

}

/*END auto scan fg360 phy device add by haowei.cheng*/

static int mtk_mdio_init(struct mtk_eth *eth)
{
	struct device_node *mii_np;
	int ret;

	mii_np = of_get_child_by_name(eth->dev->of_node, "mdio-bus");
	if (!mii_np) {
		dev_err(eth->dev, "no %s child node found", "mdio-bus");
		return -ENODEV;
	}

	if (!of_device_is_available(mii_np)) {
		ret = -ENODEV;
		goto err_put_node;
	}
	

	eth->mii_bus = devm_mdiobus_alloc(eth->dev);
	if (!eth->mii_bus) {
		ret = -ENOMEM;
		goto err_put_node;
	}

	eth->mii_bus->name = "mdio";
	eth->mii_bus->read = mtk_mdio_read;
	eth->mii_bus->write = mtk_mdio_write;
	eth->mii_bus->priv = eth;
	eth->mii_bus->parent = eth->dev;
	snprintf(eth->mii_bus->id, MII_BUS_ID_SIZE, "%s", mii_np->name);
	
	ret = of_mdiobus_register(eth->mii_bus, mii_np);

err_put_node:
	of_node_put(mii_np);
	return ret;
}

static void mtk_mdio_cleanup(struct mtk_eth *eth)
{
	if (!eth->mii_bus)
		return;

	mdiobus_unregister(eth->mii_bus);
}

static inline void mtk_tx_irq_disable(struct mtk_eth *eth, u32 mask)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&eth->tx_irq_lock, flags);
	val = mtk_r32(eth, MTK_QDMA_INT_MASK);
	mtk_w32(eth, val & ~mask, MTK_QDMA_INT_MASK);
	spin_unlock_irqrestore(&eth->tx_irq_lock, flags);
}

static inline void mtk_tx_irq_enable(struct mtk_eth *eth, u32 mask)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&eth->tx_irq_lock, flags);
	val = mtk_r32(eth, MTK_QDMA_INT_MASK);
	mtk_w32(eth, val | mask, MTK_QDMA_INT_MASK);
	spin_unlock_irqrestore(&eth->tx_irq_lock, flags);
}

static inline void mtk_rx_irq_disable(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK0);
	mtk_w32(eth, val & ~mask, MTK_PDMA_INT_MASK0);
}

static inline void mtk_rx_irq_enable(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK0);
	mtk_w32(eth, val | mask, MTK_PDMA_INT_MASK0);
}

static inline void mtk_rx_irq_disable0(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK0);
	mtk_w32(eth, val & ~mask, MTK_PDMA_INT_MASK0);

}

static inline void mtk_rx_irq_enable0(struct mtk_eth *eth, u32 mask)
{

	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK0);
	mtk_w32(eth, val | mask, MTK_PDMA_INT_MASK0);
}

static inline void mtk_rx_irq_disable1(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK1);
	mtk_w32(eth, val & ~mask, MTK_PDMA_INT_MASK1);
}

static inline void mtk_rx_irq_enable1(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK1);
	mtk_w32(eth, val | mask, MTK_PDMA_INT_MASK1);

}

static inline void mtk_rx_irq_disable2(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK2);
	mtk_w32(eth, val & ~mask, MTK_PDMA_INT_MASK2);

}

static inline void mtk_rx_irq_enable2(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK2);
	mtk_w32(eth, val | mask, MTK_PDMA_INT_MASK2);
}

static inline void mtk_rx_irq_disable3(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK3);
	mtk_w32(eth, val & ~mask, MTK_PDMA_INT_MASK3);
}

static inline void mtk_rx_irq_enable3(struct mtk_eth *eth, u32 mask)
{
	u32 val;

	val = mtk_r32(eth, MTK_PDMA_INT_MASK3);
	mtk_w32(eth, val | mask, MTK_PDMA_INT_MASK3);
}

static int mtk_set_mac_address(struct net_device *dev, void *p)
{
	int ret = eth_mac_addr(dev, p);
	struct mtk_mac *mac = netdev_priv(dev);
	const char *macaddr = dev->dev_addr;

	if (ret)
		return ret;

	if (unlikely(test_bit(MTK_RESETTING, &mac->hw->state)))
		return -EBUSY;

	spin_lock_bh(&mac->hw->page_lock);
	mtk_w32(mac->hw, (macaddr[0] << 8) | macaddr[1],
		MTK_GDMA_MAC_ADRH(mac->id));
	mtk_w32(mac->hw, (macaddr[2] << 24) | (macaddr[3] << 16) |
		(macaddr[4] << 8) | macaddr[5],
		MTK_GDMA_MAC_ADRL(mac->id));
	spin_unlock_bh(&mac->hw->page_lock);

	return 0;
}

void mtk_stats_update_mac(struct mtk_mac *mac)
{
	struct mtk_hw_stats *hw_stats = mac->hw_stats;
	unsigned int base = MTK_GDM1_TX_GBCNT;
	u64 stats;

	base += hw_stats->reg_offset;

	u64_stats_update_begin(&hw_stats->syncp);

	hw_stats->rx_bytes += mtk_r32(mac->hw, base);
	stats =  mtk_r32(mac->hw, base + 0x04);
	if (stats)
		hw_stats->rx_bytes += (stats << 32);
	hw_stats->rx_packets += mtk_r32(mac->hw, base + 0x08);
	hw_stats->rx_overflow += mtk_r32(mac->hw, base + 0x10);
	hw_stats->rx_fcs_errors += mtk_r32(mac->hw, base + 0x14);
	hw_stats->rx_short_errors += mtk_r32(mac->hw, base + 0x18);
	hw_stats->rx_long_errors += mtk_r32(mac->hw, base + 0x1c);
	hw_stats->rx_checksum_errors += mtk_r32(mac->hw, base + 0x20);
	hw_stats->rx_flow_control_packets +=
					mtk_r32(mac->hw, base + 0x24);
	hw_stats->tx_skip += mtk_r32(mac->hw, base + 0x28);
	hw_stats->tx_collisions += mtk_r32(mac->hw, base + 0x2c);
	hw_stats->tx_bytes += mtk_r32(mac->hw, base + 0x30);
	stats =  mtk_r32(mac->hw, base + 0x34);
	if (stats)
		hw_stats->tx_bytes += (stats << 32);
	hw_stats->tx_packets += mtk_r32(mac->hw, base + 0x38);
	u64_stats_update_end(&hw_stats->syncp);

}

static void mtk_stats_update(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->mac[i] || !eth->mac[i]->hw_stats)
			continue;
		if (spin_trylock(&eth->mac[i]->hw_stats->stats_lock)) {
			mtk_stats_update_mac(eth->mac[i]);
			spin_unlock(&eth->mac[i]->hw_stats->stats_lock);
		}
	}
}

static void mtk_get_stats64(struct net_device *dev,
			    struct rtnl_link_stats64 *storage)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_hw_stats *hw_stats = mac->hw_stats;
	unsigned int start;

	if (netif_running(dev) && netif_device_present(dev)) {
		if (spin_trylock_bh(&hw_stats->stats_lock)) {
			mtk_stats_update_mac(mac);
			spin_unlock_bh(&hw_stats->stats_lock);
		}
	}

	do {
		start = u64_stats_fetch_begin_irq(&hw_stats->syncp);
		storage->rx_packets = hw_stats->rx_packets;
		storage->tx_packets = hw_stats->tx_packets;
		storage->rx_bytes = hw_stats->rx_bytes;
		storage->tx_bytes = hw_stats->tx_bytes;
		storage->collisions = hw_stats->tx_collisions;
		storage->rx_length_errors = hw_stats->rx_short_errors +
			hw_stats->rx_long_errors;
		storage->rx_over_errors = hw_stats->rx_overflow;
		storage->rx_crc_errors = hw_stats->rx_fcs_errors;
		storage->rx_errors = hw_stats->rx_checksum_errors;
		storage->tx_aborted_errors = hw_stats->tx_skip;
	} while (u64_stats_fetch_retry_irq(&hw_stats->syncp, start));

	storage->tx_errors = dev->stats.tx_errors;
	storage->rx_dropped = dev->stats.rx_dropped;
	storage->tx_dropped = dev->stats.tx_dropped;
}

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

static inline void mtk_rx_get_desc(struct mtk_rx_dma *rxd,
				   struct mtk_rx_dma *dma_rxd)
{
	rxd->rxd1 = READ_ONCE(dma_rxd->rxd1);
	rxd->rxd2 = READ_ONCE(dma_rxd->rxd2);
	rxd->rxd3 = READ_ONCE(dma_rxd->rxd3);
	rxd->rxd4 = READ_ONCE(dma_rxd->rxd4);
}

/* the qdma core needs scratch memory to be setup */
static int mtk_init_fq_dma(struct mtk_eth *eth)
{
	dma_addr_t phy_ring_tail;
	int cnt = MTK_DMA_SIZE;
	dma_addr_t dma_addr;
	int i;

	eth->scratch_ring = dma_alloc_coherent(eth->dev,
					       cnt * sizeof(struct mtk_tx_dma),
					       &eth->phy_scratch_ring,
					       GFP_ATOMIC | __GFP_ZERO);
	if (unlikely(!eth->scratch_ring))
		return -ENOMEM;

	eth->scratch_head = kcalloc(cnt, MTK_QDMA_PAGE_SIZE,
				    GFP_KERNEL);
	if (unlikely(!eth->scratch_head))
		return -ENOMEM;

	dma_addr = dma_map_single(eth->dev,
				  eth->scratch_head, cnt * MTK_QDMA_PAGE_SIZE,
				  DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(eth->dev, dma_addr)))
		return -ENOMEM;

	memset(eth->scratch_ring, 0x0, sizeof(struct mtk_tx_dma) * cnt);
	phy_ring_tail = eth->phy_scratch_ring +
			(sizeof(struct mtk_tx_dma) * (cnt - 1));

	for (i = 0; i < cnt; i++) {
		eth->scratch_ring[i].txd1 =
					(dma_addr + (i * MTK_QDMA_PAGE_SIZE));
		if (i < cnt - 1)
			eth->scratch_ring[i].txd2 = (eth->phy_scratch_ring +
				((i + 1) * sizeof(struct mtk_tx_dma)));
		eth->scratch_ring[i].txd3 = TX_DMA_SDL(MTK_QDMA_PAGE_SIZE);
	}

	mtk_w32(eth, eth->phy_scratch_ring, MTK_QDMA_FQ_HEAD);
	mtk_w32(eth, phy_ring_tail, MTK_QDMA_FQ_TAIL);
	mtk_w32(eth, (cnt << 16) | cnt, MTK_QDMA_FQ_CNT);
	mtk_w32(eth, MTK_QDMA_PAGE_SIZE << 16, MTK_QDMA_FQ_BLEN);

	return 0;
}

static inline void *mtk_qdma_phys_to_virt(struct mtk_tx_ring *ring, u32 desc)
{
	void *ret = ring->dma;

	return ret + (desc - ring->phys);
}

static inline struct mtk_tx_buf *mtk_desc_to_tx_buf(struct mtk_tx_ring *ring,
						    struct mtk_tx_dma *txd)
{
	int idx = txd - ring->dma;

	return &ring->buf[idx];
}

static void mtk_tx_unmap(struct mtk_eth *eth, struct mtk_tx_buf *tx_buf)
{
	if (tx_buf->flags & MTK_TX_FLAGS_SINGLE0) {
		dma_unmap_single(eth->dev,
				 dma_unmap_addr(tx_buf, dma_addr0),
				 dma_unmap_len(tx_buf, dma_len0),
				 DMA_TO_DEVICE);
	} else if (tx_buf->flags & MTK_TX_FLAGS_PAGE0) {
		dma_unmap_page(eth->dev,
			       dma_unmap_addr(tx_buf, dma_addr0),
			       dma_unmap_len(tx_buf, dma_len0),
			       DMA_TO_DEVICE);
	}
	tx_buf->flags = 0;
	if (tx_buf->skb &&
	    (tx_buf->skb != (struct sk_buff *)MTK_DMA_DUMMY_DESC))
		dev_kfree_skb_any(tx_buf->skb);
	tx_buf->skb = NULL;
}

static int mtk_tx_map(struct sk_buff *skb, struct net_device *dev,
		      int tx_num, struct mtk_tx_ring *ring, bool gso)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	struct mtk_tx_dma *itxd, *txd;
	struct mtk_tx_buf *itx_buf, *tx_buf;
	dma_addr_t mapped_addr;
	unsigned int nr_frags;
	int i, n_desc = 1;
	u32 txd4 = 0, txd5 = 0, fport;

	itxd = ring->next_free;

	if (itxd == ring->last_free) {
		return -ENOMEM;
	}

	/* set the forward port */
	fport = (mac->id + 1) << TX_DMA_FPORT_SHIFT;
#if defined(CONFIG_HW_NAT)
	if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb)) {
		if (FOE_MAGIC_TAG_HEAD(skb) == FOE_MAGIC_PPE) {
			if (ppe_hook_rx_eth) {
				fport = (3 << TX_DMA_FPORT_SHIFT);
				FOE_MAGIC_TAG(skb) = 0;
			}
		}
	} else if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb)) {
		if (FOE_MAGIC_TAG_TAIL(skb) == FOE_MAGIC_PPE) {
			if (ppe_hook_rx_eth) {
				fport = (3 << TX_DMA_FPORT_SHIFT);
				FOE_MAGIC_TAG(skb) = 0;
			}
		}
	}
#endif
	txd4 |= fport;

	itx_buf = mtk_desc_to_tx_buf(ring, itxd);
	memset(itx_buf, 0, sizeof(*itx_buf));

	if (gso)
		txd5 |= TX_DMA_TSO;

	/* TX Checksum offload */
	if (skb->ip_summed == CHECKSUM_PARTIAL)
		txd5 |= TX_DMA_CHKSUM;

#if defined(CONFIG_NET_MEDIATEK_HW_QOS)
	qid = skb->mark & (MTK_QDMA_TX_MASK);
/* Disable Qos CPU Path
	if ((qid != 0) && (skb->mark < 64)) {
		txd4 = txd4 | ((M2Q_table[skb->mark] & 0x3f) << 16);
	}
*/
#endif

	mapped_addr = dma_map_single(eth->dev, skb->data,
				     skb_headlen(skb), DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(eth->dev, mapped_addr)))
		return -ENOMEM;

	WRITE_ONCE(itxd->txd1, mapped_addr);
	itx_buf->flags |= MTK_TX_FLAGS_SINGLE0;
	itx_buf->flags |= (!mac->id) ? MTK_TX_FLAGS_FPORT0 :
			  MTK_TX_FLAGS_FPORT1;
	dma_unmap_addr_set(itx_buf, dma_addr0, mapped_addr);
	dma_unmap_len_set(itx_buf, dma_len0, skb_headlen(skb));

	/* TX SG offload */
	txd = itxd;
	nr_frags = skb_shinfo(skb)->nr_frags;
	for (i = 0; i < nr_frags; i++) {
		struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];
		unsigned int offset = 0;
		int frag_size = skb_frag_size(frag);

		while (frag_size) {
			bool last_frag = false;
			unsigned int frag_map_size;

			txd = mtk_qdma_phys_to_virt(ring, txd->txd2);
			if (txd == ring->last_free)
				goto err_dma;

			n_desc++;
			frag_map_size = min(frag_size, MTK_TX_DMA_BUF_LEN);
			mapped_addr = skb_frag_dma_map(eth->dev, frag, offset,
						       frag_map_size,
						       DMA_TO_DEVICE);
			if (unlikely(dma_mapping_error(eth->dev, mapped_addr)))
				goto err_dma;

			if (i == nr_frags - 1 &&
			    (frag_size - frag_map_size) == 0)
				last_frag = true;

			WRITE_ONCE(txd->txd1, mapped_addr);
			WRITE_ONCE(txd->txd3, (TX_DMA_PLEN0(frag_map_size) |
					       last_frag * TX_DMA_LS0));
			txd4 = TX_DMA_SWC | fport;
			WRITE_ONCE(txd->txd4, txd4);

			tx_buf = mtk_desc_to_tx_buf(ring, txd);
			memset(tx_buf, 0, sizeof(*tx_buf));
			tx_buf->skb = (struct sk_buff *)MTK_DMA_DUMMY_DESC;
			tx_buf->flags |= MTK_TX_FLAGS_PAGE0;
			tx_buf->flags |= (!mac->id) ? MTK_TX_FLAGS_FPORT0 :
					 MTK_TX_FLAGS_FPORT1;

			dma_unmap_addr_set(tx_buf, dma_addr0, mapped_addr);
			dma_unmap_len_set(tx_buf, dma_len0, frag_map_size);
			frag_size -= frag_map_size;
			offset += frag_map_size;
		}
	}

	/* store skb to cleanup */
	itx_buf->skb = skb;
	WRITE_ONCE(itxd->txd5, txd5);
	WRITE_ONCE(itxd->txd6, 0);
	WRITE_ONCE(itxd->txd7, 0);
	WRITE_ONCE(itxd->txd8, 0);
	WRITE_ONCE(itxd->txd3, (TX_DMA_PLEN0(skb_headlen(skb)) |
				(!nr_frags * TX_DMA_LS0)));

	txd4 = txd4 | TX_DMA_SWC;
	WRITE_ONCE(itxd->txd4, txd4);

	netdev_sent_queue(dev, skb->len);
	skb_tx_timestamp(skb);

	ring->next_free = mtk_qdma_phys_to_virt(ring, txd->txd2);
	atomic_sub(n_desc, &ring->free_count);

	/* make sure that all changes to the dma ring are flushed before we
	 * continue
	 */
	wmb();

	mtk_w32(eth, txd->txd2, MTK_QTX_CTX_PTR);

	return 0;

err_dma:
	do {
		tx_buf = mtk_desc_to_tx_buf(ring, itxd);

		/* unmap dma */
		mtk_tx_unmap(eth, tx_buf);

		itxd->txd3 = TX_DMA_LS0 | TX_DMA_OWNER_CPU;
		itxd = mtk_qdma_phys_to_virt(ring, itxd->txd2);
	} while (itxd != txd);

	return -ENOMEM;
}

static inline int mtk_cal_txd_req(struct sk_buff *skb)
{
	int i, nfrags;
	struct skb_frag_struct *frag;

	nfrags = 1;
	if (skb_is_gso(skb)) {
		for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
			frag = &skb_shinfo(skb)->frags[i];
			nfrags += DIV_ROUND_UP(frag->size, MTK_TX_DMA_BUF_LEN);
		}
	} else {
		nfrags += skb_shinfo(skb)->nr_frags;
	}

	return nfrags;
}

static int mtk_queue_stopped(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->netdev[i])
			continue;
		if (netif_queue_stopped(eth->netdev[i]))
			return 1;
	}

	return 0;
}

static void mtk_wake_queue(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->netdev[i])
			continue;
		netif_wake_queue(eth->netdev[i]);
	}
}

static void mtk_stop_queue(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->netdev[i])
			continue;
		netif_stop_queue(eth->netdev[i]);
	}
}

static int mtk_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	struct mtk_tx_ring *ring = &eth->tx_ring;
	struct net_device_stats *stats = &dev->stats;
	bool gso = false;
	int tx_num;

	/* normally we can rely on the stack not calling this more than once,
	 * however we have 2 queues running on the same ring so we need to lock
	 * the ring access
	 */
	spin_lock(&eth->page_lock);

	if (unlikely(test_bit(MTK_RESETTING, &eth->state)))
		goto drop;

#if defined(CONFIG_HW_NAT)
	if (ppe_hook_tx_eth) {
		if (ppe_hook_tx_eth(skb, mac->id + 1) != 1)
			goto drop;
	}
#endif

	tx_num = mtk_cal_txd_req(skb);
	if (unlikely(atomic_read(&ring->free_count) <= tx_num)) {
		mtk_stop_queue(eth);
		netif_err(eth, tx_queued, dev,
			  "Tx Ring full when queue awake!\n");
		spin_unlock(&eth->page_lock);
		return NETDEV_TX_BUSY;
	}

	/* TSO: fill MSS info in tcp checksum field */
	if (skb_is_gso(skb)) {
		if (skb_cow_head(skb, 0)) {
			netif_warn(eth, tx_err, dev,
				   "GSO expand head fail.\n");
			goto drop;
		}

		if (skb_shinfo(skb)->gso_type &
				(SKB_GSO_TCPV4 | SKB_GSO_TCPV6)) {
			gso = true;
			tcp_hdr(skb)->check = htons(skb_shinfo(skb)->gso_size);
		}
	}

	if (mtk_tx_map(skb, dev, tx_num, ring, gso) < 0)
		goto drop;

	if (unlikely(atomic_read(&ring->free_count) <= ring->thresh))
		mtk_stop_queue(eth);

	spin_unlock(&eth->page_lock);

	return NETDEV_TX_OK;

drop:
	spin_unlock(&eth->page_lock);
	stats->tx_dropped++;
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

static struct mtk_rx_ring *mtk_get_rx_ring(struct mtk_eth *eth)
{
	int i;
	struct mtk_rx_ring *ring;
	int idx;

	if (!eth->hwlro)
		return &eth->rx_ring[0];

	for (i = 0; i < MTK_MAX_RX_RING_NUM; i++) {
		ring = &eth->rx_ring[i];
		idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		if (ring->dma[idx].rxd2 & RX_DMA_DONE) {
			ring->calc_idx_update = true;
			return ring;
		}
	}

	return NULL;
}

static void mtk_update_rx_cpu_idx(struct mtk_eth *eth)
{
	struct mtk_rx_ring *ring;
	int i;

	if (eth->hwlro) {
		for (i = 0; i < MTK_MAX_RX_RING_NUM; i++) {
			ring = &eth->rx_ring[i];
			if (ring->calc_idx_update) {
				ring->calc_idx_update = false;
				mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg);
			}
		}
	} else {
		ring = &eth->rx_ring[0];
		mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg);
	}
}

static int mtk_poll_rx(struct napi_struct *napi, int budget,
		       struct mtk_eth *eth)
{
	struct mtk_rx_ring *ring;
	int idx;
	struct sk_buff *skb;
	u8 *data, *new_data;
	struct mtk_rx_dma *rxd, trxd;
	int done = 0;

	while (done < budget) {
		struct net_device *netdev;
		unsigned int pktlen;
		dma_addr_t dma_addr;
		int mac = 0;

		ring = mtk_get_rx_ring(eth);
		if (unlikely(!ring))
			goto rx_done;

		idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		rxd = &ring->dma[idx];
		data = ring->data[idx];

		mtk_rx_get_desc(&trxd, rxd);
		if (!(trxd.rxd2 & RX_DMA_DONE))
			break;

		/* find out which mac the packet come from. values start at 1 */
		mac = (trxd.rxd4 >> RX_DMA_FPORT_SHIFT) &
		      RX_DMA_FPORT_MASK;
		mac--;

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
		pktlen = RX_DMA_GET_PLEN0(trxd.rxd2);
		skb->dev = netdev;
		skb_put(skb, pktlen);
		if (trxd.rxd4 & RX_DMA_L4_VALID)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb_checksum_none_assert(skb);
#if defined(CONFIG_HW_NAT)
		*(u32 *)(skb->head) = trxd.rxd4;

		if (ppe_hook_rx_eth) {
			if (IS_SPACE_AVAILABLE_HEAD(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_HEAD(skb)) =
					trxd.rxd4;
				FOE_ALG_HEAD(skb) = 0;
				FOE_MAGIC_TAG_HEAD(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_HEAD(skb) = TAG_PROTECT;
			}
			if (IS_SPACE_AVAILABLE_TAIL(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_TAIL(skb) + 2) =
					trxd.rxd4;
				FOE_ALG_TAIL(skb) = 0;
				FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
			}
		}
#endif
		skb->protocol = eth_type_trans(skb, netdev);

		if (netdev->features & NETIF_F_HW_VLAN_CTAG_RX &&
		    RX_DMA_VID(trxd.rxd3))
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
					       RX_DMA_VID(trxd.rxd3));

/* ppe_hook_rx_eth return 1 --> continue
 * ppe_hook_rx_eth return 0 --> FWD & without netif_rx
 */
#if defined(CONFIG_HW_NAT)
		if ((!ppe_hook_rx_eth) || (ppe_hook_rx_eth && ppe_hook_rx_eth(skb))) {
#endif
			skb_record_rx_queue(skb, 0);
			napi_gro_receive(napi, skb);
#if defined(CONFIG_HW_NAT)  || defined(CONFIG_HW_NAT_MODULE)
		}
#endif

		ring->data[idx] = new_data;
		rxd->rxd1 = (unsigned int)dma_addr;

release_desc:
		rxd->rxd2 = RX_DMA_PLEN0(ring->buf_size);

		ring->calc_idx = idx;

		done++;
	}

rx_done:
	if (done) {
		/* make sure that all changes to the dma ring are flushed before
		 * we continue
		 */
		wmb();
		mtk_update_rx_cpu_idx(eth);
	}

	return done;
}

static int mtk_poll_rx0(struct napi_struct *napi, int budget,
		       struct mtk_eth *eth)
{
	struct mtk_rx_ring *ring;
	int idx;
	struct sk_buff *skb;
	u8 *data, *new_data;
	struct mtk_rx_dma *rxd, trxd;
	int done = 0;

	while (done < budget) {
		struct net_device *netdev;
		unsigned int pktlen;
		dma_addr_t dma_addr;
		int mac = 0;

		//ring = mtk_get_rx_ring(eth);
		ring = &eth->rx_ring[0];
		//idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		idx = (ring->calc_idx + 1) % (ring->dma_size);
		//pr_notice("%s true true(idx = %x)   ring->calc_idx =%x, ring->dma_size = %x\n", __func__, idx, ring->calc_idx, ring->dma_size);
		if (ring->dma[idx].rxd2 & RX_DMA_DONE) {
			//pr_notice("%s true true(idx = %x)\n", __func__, idx);
			ring->calc_idx_update = true;
		} else {
			//pr_notice("%s NULL NULL\n", __func__);
			ring = NULL;
		}
		if (unlikely(!ring))
			goto rx_done;
		//idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		//pr_notice("%s !!!(idx = %x)\n", __func__, idx);
		rxd = &ring->dma[idx];
		data = ring->data[idx];
		mtk_rx_get_desc(&trxd, rxd);
		if (!(trxd.rxd2 & RX_DMA_DONE))
			break;

		/* find out which mac the packet come from. values start at 1 */
		mac = (trxd.rxd4 >> RX_DMA_FPORT_SHIFT) &
		      RX_DMA_FPORT_MASK;
		mac--;

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
		pktlen = RX_DMA_GET_PLEN0(trxd.rxd2);
		skb->dev = netdev;
		skb_put(skb, pktlen);
		if (trxd.rxd4 & RX_DMA_L4_VALID)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb_checksum_none_assert(skb);

#if defined(CONFIG_HW_NAT)
		*(u32 *)(skb->head) = trxd.rxd4;

		if (ppe_hook_rx_eth) {
			if (IS_SPACE_AVAILABLE_HEAD(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_HEAD(skb)) =
					trxd.rxd4;
				FOE_ALG_HEAD(skb) = 0;
				FOE_MAGIC_TAG_HEAD(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_HEAD(skb) = TAG_PROTECT;
			}
			if (IS_SPACE_AVAILABLE_TAIL(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_TAIL(skb) + 2) =
					trxd.rxd4;
				FOE_ALG_TAIL(skb) = 0;
				FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
			}
		}
#endif

		skb->protocol = eth_type_trans(skb, netdev);

		if (netdev->features & NETIF_F_HW_VLAN_CTAG_RX &&
		    RX_DMA_VID(trxd.rxd3))
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
					       RX_DMA_VID(trxd.rxd3));

/* ppe_hook_rx_eth return 1 --> continue
 * ppe_hook_rx_eth return 0 --> FWD & without netif_rx
 */
#if defined(CONFIG_HW_NAT)
		if ((!ppe_hook_rx_eth) || (ppe_hook_rx_eth && ppe_hook_rx_eth(skb))) {
#endif
			skb_record_rx_queue(skb, 0);
			napi_gro_receive(napi, skb);
#if defined(CONFIG_HW_NAT)  || defined(CONFIG_HW_NAT_MODULE)
		}
#endif
		ring->data[idx] = new_data;
		rxd->rxd1 = (unsigned int)dma_addr;

release_desc:
		//pr_notice("%s !!!release_desc\n", __func__);
		rxd->rxd2 = RX_DMA_PLEN0(ring->buf_size);

		ring->calc_idx = idx;
		//pr_notice("%s !!!release_desc ring->calc = %x\n", __func__, ring->calc_idx);
		done++;
	}

rx_done:
	if (done) {
		/* make sure that all changes to the dma ring are flushed before
		 * we continue
		 */
		wmb();
		ring = &eth->rx_ring[0];
		//pr_notice("%s rx_done!!!\n", __func__);
		if (ring->calc_idx_update) {
			//pr_notice("%s rx_done calc_idx_update calc_idx = %x\n", __func__, ring->calc_idx);
			ring->calc_idx_update = false;
			mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg);
		}
	}

	return done;
}

static int mtk_poll_rx1(struct napi_struct *napi, int budget,
		       struct mtk_eth *eth)
{
	struct mtk_rx_ring *ring;
	int idx;
	struct sk_buff *skb;
	u8 *data, *new_data;
	struct mtk_rx_dma *rxd, trxd;
	int done = 0;

	while (done < budget) {
		struct net_device *netdev;
		unsigned int pktlen;
		dma_addr_t dma_addr;
		int mac = 0;

		//ring = mtk_get_rx_ring(eth);
		ring = &eth->rx_ring[1];
		//idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		idx = (ring->calc_idx + 1) % (ring->dma_size);
		if (ring->dma[idx].rxd2 & RX_DMA_DONE)
			ring->calc_idx_update = true;
		else
			ring = NULL;

		if (unlikely(!ring))
			goto rx_done;

		//idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		rxd = &ring->dma[idx];
		data = ring->data[idx];

		mtk_rx_get_desc(&trxd, rxd);
		if (!(trxd.rxd2 & RX_DMA_DONE))
			break;

		/* find out which mac the packet come from. values start at 1 */
		mac = (trxd.rxd4 >> RX_DMA_FPORT_SHIFT) &
		      RX_DMA_FPORT_MASK;
		mac--;

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
		pktlen = RX_DMA_GET_PLEN0(trxd.rxd2);
		skb->dev = netdev;

		skb_put(skb, pktlen);
		if (trxd.rxd4 & RX_DMA_L4_VALID)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb_checksum_none_assert(skb);

#if defined(CONFIG_HW_NAT)
		*(u32 *)(skb->head) = trxd.rxd4;

		if (ppe_hook_rx_eth) {
			if (IS_SPACE_AVAILABLE_HEAD(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_HEAD(skb)) =
					trxd.rxd4;
				FOE_ALG_HEAD(skb) = 0;
				FOE_MAGIC_TAG_HEAD(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_HEAD(skb) = TAG_PROTECT;
			}
			if (IS_SPACE_AVAILABLE_TAIL(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_TAIL(skb) + 2) =
					trxd.rxd4;
				FOE_ALG_TAIL(skb) = 0;
				FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
			}
		}
#endif
		skb->protocol = eth_type_trans(skb, netdev);


		if (netdev->features & NETIF_F_HW_VLAN_CTAG_RX &&
		    RX_DMA_VID(trxd.rxd3))
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
					       RX_DMA_VID(trxd.rxd3));

/* ppe_hook_rx_eth return 1 --> continue
 * ppe_hook_rx_eth return 0 --> FWD & without netif_rx
 */
#if defined(CONFIG_HW_NAT)
		if ((!ppe_hook_rx_eth) || (ppe_hook_rx_eth && ppe_hook_rx_eth(skb))) {
#endif
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			skb_record_rx_queue(skb, 0);
			napi_gro_receive(napi, skb);
#if defined(CONFIG_HW_NAT)  || defined(CONFIG_HW_NAT_MODULE)
		}
#endif

		ring->data[idx] = new_data;
		rxd->rxd1 = (unsigned int)dma_addr;

release_desc:
		rxd->rxd2 = RX_DMA_PLEN0(ring->buf_size);

		ring->calc_idx = idx;

		done++;
	}

rx_done:
	if (done) {
		/* make sure that all changes to the dma ring are flushed before
		 * we continue
		 */
		wmb();
		ring = &eth->rx_ring[1];
		if (ring->calc_idx_update) {
			ring->calc_idx_update = false;
			mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg);
		}

	}

	return done;
}

static int mtk_poll_rx2(struct napi_struct *napi, int budget,
		       struct mtk_eth *eth)
{
	struct mtk_rx_ring *ring;
	int idx;
	struct sk_buff *skb;
	u8 *data, *new_data;
	struct mtk_rx_dma *rxd, trxd;
	int done = 0;

	while (done < budget) {
		struct net_device *netdev;
		unsigned int pktlen;
		dma_addr_t dma_addr;
		int mac = 0;

		//ring = mtk_get_rx_ring(eth);
		ring = &eth->rx_ring[2];
		//idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		idx = (ring->calc_idx + 1) % (ring->dma_size);
		if (ring->dma[idx].rxd2 & RX_DMA_DONE)
			ring->calc_idx_update = true;
		else
			ring = NULL;

		if (unlikely(!ring))
			goto rx_done;

		//idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		rxd = &ring->dma[idx];
		data = ring->data[idx];

		mtk_rx_get_desc(&trxd, rxd);
		if (!(trxd.rxd2 & RX_DMA_DONE))
			break;

		/* find out which mac the packet come from. values start at 1 */
		mac = (trxd.rxd4 >> RX_DMA_FPORT_SHIFT) &
		      RX_DMA_FPORT_MASK;
		mac--;

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
		pktlen = RX_DMA_GET_PLEN0(trxd.rxd2);
		skb->dev = netdev;
		skb_put(skb, pktlen);
		if (trxd.rxd4 & RX_DMA_L4_VALID)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb_checksum_none_assert(skb);

#if defined(CONFIG_HW_NAT)
		*(u32 *)(skb->head) = trxd.rxd4;

		if (ppe_hook_rx_eth) {
			if (IS_SPACE_AVAILABLE_HEAD(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_HEAD(skb)) =
					trxd.rxd4;
				FOE_ALG_HEAD(skb) = 0;
				FOE_MAGIC_TAG_HEAD(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_HEAD(skb) = TAG_PROTECT;
			}
			if (IS_SPACE_AVAILABLE_TAIL(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_TAIL(skb) + 2) =
					trxd.rxd4;
				FOE_ALG_TAIL(skb) = 0;
				FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
			}
		}
#endif
		skb->protocol = eth_type_trans(skb, netdev);

		if (netdev->features & NETIF_F_HW_VLAN_CTAG_RX &&
		    RX_DMA_VID(trxd.rxd3))
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
					       RX_DMA_VID(trxd.rxd3));
/* ppe_hook_rx_eth return 1 --> continue
 * ppe_hook_rx_eth return 0 --> FWD & without netif_rx
 */
#if defined(CONFIG_HW_NAT)
		if ((!ppe_hook_rx_eth) || (ppe_hook_rx_eth && ppe_hook_rx_eth(skb))) {
#endif
			skb_record_rx_queue(skb, 0);
			napi_gro_receive(napi, skb);
#if defined(CONFIG_HW_NAT)  || defined(CONFIG_HW_NAT_MODULE)
		}
#endif

		ring->data[idx] = new_data;
		rxd->rxd1 = (unsigned int)dma_addr;

release_desc:
		rxd->rxd2 = RX_DMA_PLEN0(ring->buf_size);

		ring->calc_idx = idx;

		done++;
	}

rx_done:
	if (done) {
		/* make sure that all changes to the dma ring are flushed before
		 * we continue
		 */
		wmb();
		ring = &eth->rx_ring[2];
		if (ring->calc_idx_update) {
			ring->calc_idx_update = false;
			mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg);
		}

	}

	return done;
}

static int mtk_poll_rx3(struct napi_struct *napi, int budget,
		       struct mtk_eth *eth)
{
	struct mtk_rx_ring *ring;
	int idx;
	struct sk_buff *skb;
	u8 *data, *new_data;
	struct mtk_rx_dma *rxd, trxd;
	int done = 0;

	while (done < budget) {
		struct net_device *netdev;
		unsigned int pktlen;
		dma_addr_t dma_addr;
		int mac = 0;

		//ring = mtk_get_rx_ring(eth);
		ring = &eth->rx_ring[3];
		//idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);
		idx = (ring->calc_idx + 1) % (ring->dma_size);
		if (ring->dma[idx].rxd2 & RX_DMA_DONE) {
			ring->calc_idx_update = true;
		} else {
			ring = NULL;
		}
		if (unlikely(!ring))
			goto rx_done;
		//idx = NEXT_RX_DESP_IDX(ring->calc_idx, ring->dma_size);

		rxd = &ring->dma[idx];
		data = ring->data[idx];
		mtk_rx_get_desc(&trxd, rxd);
		if (!(trxd.rxd2 & RX_DMA_DONE))
			break;

		/* find out which mac the packet come from. values start at 1 */
		mac = (trxd.rxd4 >> RX_DMA_FPORT_SHIFT) &
		      RX_DMA_FPORT_MASK;
		mac--;

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
		pktlen = RX_DMA_GET_PLEN0(trxd.rxd2);
		skb->dev = netdev;
		skb_put(skb, pktlen);
		if (trxd.rxd4 & RX_DMA_L4_VALID)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb_checksum_none_assert(skb);

#if defined(CONFIG_HW_NAT)
		*(u32 *)(skb->head) = trxd.rxd4;

		if (ppe_hook_rx_eth) {
			if (IS_SPACE_AVAILABLE_HEAD(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_HEAD(skb)) =
					trxd.rxd4;
				FOE_ALG_HEAD(skb) = 0;
				FOE_MAGIC_TAG_HEAD(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_HEAD(skb) = TAG_PROTECT;
			}
			if (IS_SPACE_AVAILABLE_TAIL(skb)) {
				*(u32 *)(FOE_INFO_START_ADDR_TAIL(skb) + 2) =
					trxd.rxd4;
				FOE_ALG_TAIL(skb) = 0;
				FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_GE;
				FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
			}
		}
#endif

		skb->protocol = eth_type_trans(skb, netdev);

		if (netdev->features & NETIF_F_HW_VLAN_CTAG_RX &&
		    RX_DMA_VID(trxd.rxd3))
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
					       RX_DMA_VID(trxd.rxd3));
/* ppe_hook_rx_eth return 1 --> continue
 * ppe_hook_rx_eth return 0 --> FWD & without netif_rx
 */
#if defined(CONFIG_HW_NAT)
		if ((!ppe_hook_rx_eth) || (ppe_hook_rx_eth && ppe_hook_rx_eth(skb))) {
#endif
			skb_record_rx_queue(skb, 0);
			napi_gro_receive(napi, skb);
#if defined(CONFIG_HW_NAT)  || defined(CONFIG_HW_NAT_MODULE)
		}
#endif
		ring->data[idx] = new_data;
		rxd->rxd1 = (unsigned int)dma_addr;

release_desc:

		rxd->rxd2 = RX_DMA_PLEN0(ring->buf_size);
		ring->calc_idx = idx;
		done++;
	}

rx_done:
	if (done) {
		/* make sure that all changes to the dma ring are flushed before
		 * we continue
		 */
		wmb();
		ring = &eth->rx_ring[3];
		if (ring->calc_idx_update) {
			ring->calc_idx_update = false;
			mtk_w32(eth, ring->calc_idx, ring->crx_idx_reg);
		}
	}
	return done;
}

static int mtk_poll_tx(struct mtk_eth *eth, int budget)
{
	struct mtk_tx_ring *ring = &eth->tx_ring;
	struct mtk_tx_dma *desc;
	struct sk_buff *skb;
	struct mtk_tx_buf *tx_buf;
	unsigned int done[MTK_MAX_DEVS];
	unsigned int bytes[MTK_MAX_DEVS];
	u32 cpu, dma;
	int total = 0, i;

	memset(done, 0, sizeof(done));
	memset(bytes, 0, sizeof(bytes));

	cpu = mtk_r32(eth, MTK_QTX_CRX_PTR);
	dma = mtk_r32(eth, MTK_QTX_DRX_PTR);

	desc = mtk_qdma_phys_to_virt(ring, cpu);

	while ((cpu != dma) && budget) {
		u32 next_cpu = desc->txd2;
		int mac = 0;

		if ((desc->txd3 & TX_DMA_OWNER_CPU) == 0)
			break;

		desc = mtk_qdma_phys_to_virt(ring, desc->txd2);
		tx_buf = mtk_desc_to_tx_buf(ring, desc);
		if (tx_buf->flags & MTK_TX_FLAGS_FPORT1)
			mac = 1;

		skb = tx_buf->skb;
		if (!skb)
			break;

		if (skb != (struct sk_buff *)MTK_DMA_DUMMY_DESC) {
			bytes[mac] += skb->len;
			done[mac]++;
			budget--;
		}
		mtk_tx_unmap(eth, tx_buf);

		ring->last_free = desc;
		atomic_inc(&ring->free_count);

		cpu = next_cpu;
	}

	mtk_w32(eth, cpu, MTK_QTX_CRX_PTR);

	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->netdev[i] || !done[i])
			continue;
		netdev_completed_queue(eth->netdev[i], done[i], bytes[i]);
		total += done[i];
	}

	if (mtk_queue_stopped(eth) &&
	    (atomic_read(&ring->free_count) > ring->thresh))
		mtk_wake_queue(eth);

	return total;
}

void mtk_handle_status_irq(struct mtk_eth *eth)
{
	u32 status2 = mtk_r32(eth, MTK_INT_STATUS2);

	if (unlikely(status2 & (MTK_GDM1_AF | MTK_GDM2_AF))) {
		mtk_stats_update(eth);
		mtk_w32(eth, (MTK_GDM1_AF | MTK_GDM2_AF),
			MTK_INT_STATUS2);
	}
}

static int mtk_napi_tx(struct napi_struct *napi, int budget)
{
	struct mtk_eth *eth = container_of(napi, struct mtk_eth, tx_napi);
	u32 status, mask;
	int tx_done = 0;

	mtk_handle_status_irq(eth);
	mtk_w32(eth, MTK_TX_DONE_INT | MTK_TX_DONE_INT0, MTK_QMTK_INT_STATUS);
	tx_done = mtk_poll_tx(eth, budget);

	if (unlikely(netif_msg_intr(eth))) {
		status = mtk_r32(eth, MTK_QMTK_INT_STATUS);
		mask = mtk_r32(eth, MTK_QDMA_INT_MASK);
		dev_info(eth->dev,
			 "done tx %d, intr 0x%08x/0x%x\n",
			 tx_done, status, mask);
	}

	if (tx_done == budget)
		return budget;

	status = mtk_r32(eth, MTK_QMTK_INT_STATUS);
	if (status & MTK_TX_DONE_INT)
		return budget;

	napi_complete(napi);
	mtk_tx_irq_enable(eth, MTK_TX_DONE_INT);

	return tx_done;
}

static int mtk_napi_rx(struct napi_struct *napi, int budget)
{
	struct mtk_eth *eth = container_of(napi, struct mtk_eth, rx_napi);
	u32 status, mask;
	int rx_done = 0;
	int remain_budget = budget;

	mtk_handle_status_irq(eth);

poll_again:
	mtk_w32(eth, MTK_RX_DONE_INT, MTK_PDMA_INT_STATUS0);
	rx_done = mtk_poll_rx(napi, remain_budget, eth);

	if (unlikely(netif_msg_intr(eth))) {
		status = mtk_r32(eth, MTK_PDMA_INT_STATUS0);
		mask = mtk_r32(eth, MTK_PDMA_INT_MASK0);
		dev_info(eth->dev,
			 "done rx %d, intr 0x%08x/0x%x\n",
			 rx_done, status, mask);
	}
	if (rx_done == remain_budget)
		return budget;

	status = mtk_r32(eth, MTK_PDMA_INT_STATUS0);
	if (status & MTK_RX_DONE_INT) {
		remain_budget -= rx_done;
		goto poll_again;
	}
	napi_complete(napi);
	mtk_rx_irq_enable(eth, MTK_RX_DONE_INT);

	return rx_done + budget - remain_budget;
}

static int mtk_napi_rx0(struct napi_struct *napi, int budget)
{
	struct mtk_eth *eth = container_of(napi, struct mtk_eth, rx_napi0);
	u32 status, mask;
	int rx_done = 0;
	int remain_budget = budget;

	////pr_notice("%s\n", __func__);
	mtk_handle_status_irq(eth);

poll_again:
	mtk_w32(eth, MTK_RX_DONE_DLY, MTK_PDMA_INT_STATUS0);
	rx_done = mtk_poll_rx0(napi, remain_budget, eth);
	if (unlikely(netif_msg_intr(eth))) {
		status = mtk_r32(eth, MTK_PDMA_INT_STATUS0);
		mask = mtk_r32(eth, MTK_PDMA_INT_MASK0);
		dev_info(eth->dev,
			 "done rx %d, intr 0x%08x/0x%x\n",
			 rx_done, status, mask);
	}
	if (rx_done == remain_budget)
		return budget;

	status = mtk_r32(eth, MTK_PDMA_INT_STATUS0);
	if (status & (MTK_RX_DONE_DLY)) {
		remain_budget -= rx_done;
		goto poll_again;
	}
	napi_complete(napi);
	mtk_rx_irq_enable0(eth, MTK_RX_DONE_DLY);

	return rx_done + budget - remain_budget;
}

static int mtk_napi_rx1(struct napi_struct *napi, int budget)
{
	struct mtk_eth *eth = container_of(napi, struct mtk_eth, rx_napi1);
	u32 status, mask;
	int rx_done = 0;
	int remain_budget = budget;

	////pr_notice("%s\n", __func__);
	mtk_handle_status_irq(eth);

poll_again:
	mtk_w32(eth, MTK_RX_DONE_DLY1 | MTK_RX_DONE_INT1, MTK_PDMA_INT_STATUS1);
	rx_done = mtk_poll_rx1(napi, remain_budget, eth);

	if (unlikely(netif_msg_intr(eth))) {
		status = mtk_r32(eth, MTK_PDMA_INT_STATUS1);
		mask = mtk_r32(eth, MTK_PDMA_INT_MASK1);
		dev_info(eth->dev,
			 "done rx %d, intr 0x%08x/0x%x\n",
			 rx_done, status, mask);
	}
	if (rx_done == remain_budget)
		return budget;

	status = mtk_r32(eth, MTK_PDMA_INT_STATUS1);
	if (status & (MTK_RX_DONE_DLY1 | MTK_RX_DONE_INT1)) {
		remain_budget -= rx_done;
		goto poll_again;
	}
	napi_complete(napi);
	mtk_rx_irq_enable1(eth, MTK_RX_DONE_DLY1);

	return rx_done + budget - remain_budget;
}

static int mtk_napi_rx2(struct napi_struct *napi, int budget)
{
	struct mtk_eth *eth = container_of(napi, struct mtk_eth, rx_napi2);
	u32 status, mask;
	int rx_done = 0;
	int remain_budget = budget;

	////pr_notice("%s\n", __func__);
	mtk_handle_status_irq(eth);

poll_again:
	////pr_notice("%s status0 = %x\n", __func__, mtk_r32(eth, MTK_PDMA_INT_STATUS2));
	mtk_w32(eth, MTK_RX_DONE_DLY2 | MTK_RX_DONE_INT2, MTK_PDMA_INT_STATUS2);
	////pr_notice("%s status = %x\n", __func__, mtk_r32(eth, MTK_PDMA_INT_STATUS2));
	rx_done = mtk_poll_rx2(napi, remain_budget, eth);

	if (unlikely(netif_msg_intr(eth))) {
		status = mtk_r32(eth, MTK_PDMA_INT_STATUS2);
		mask = mtk_r32(eth, MTK_PDMA_INT_MASK2);
		dev_info(eth->dev,
			 "done rx %d, intr 0x%08x/0x%x\n",
			 rx_done, status, mask);
	}
	if (rx_done == remain_budget)
		return budget;

	status = mtk_r32(eth, MTK_PDMA_INT_STATUS2);
	if (status & (MTK_RX_DONE_DLY2 | MTK_RX_DONE_INT2)) {
		remain_budget -= rx_done;
		goto poll_again;
	}
	napi_complete(napi);
	mtk_rx_irq_enable2(eth, MTK_RX_DONE_DLY2);

	return rx_done + budget - remain_budget;
}

static int mtk_napi_rx3(struct napi_struct *napi, int budget)
{
	struct mtk_eth *eth = container_of(napi, struct mtk_eth, rx_napi3);
	u32 status, mask;
	int rx_done = 0;
	int remain_budget = budget;

	////pr_notice("%s\n", __func__);
	mtk_handle_status_irq(eth);

poll_again:
	//////pr_notice("%s status0 = %x\n", __func__, mtk_r32(eth, MTK_PDMA_INT_STATUS2));
	mtk_w32(eth, MTK_RX_DONE_DLY3 | MTK_RX_DONE_INT3, MTK_PDMA_INT_STATUS3);
	//////pr_notice("%s status = %x\n", __func__, mtk_r32(eth, MTK_PDMA_INT_STATUS3));
	rx_done = mtk_poll_rx3(napi, remain_budget, eth);

	if (unlikely(netif_msg_intr(eth))) {
		status = mtk_r32(eth, MTK_PDMA_INT_STATUS3);
		mask = mtk_r32(eth, MTK_PDMA_INT_MASK3);
		dev_info(eth->dev,
			 "done rx %d, intr 0x%08x/0x%x\n",
			 rx_done, status, mask);
	}
	//////pr_notice("%s333333333333333333\n", __func__);
	if (rx_done == remain_budget)
		return budget;

	status = mtk_r32(eth, MTK_PDMA_INT_STATUS3);
	if (status & (MTK_RX_DONE_DLY3 | MTK_RX_DONE_INT3)) {
		remain_budget -= rx_done;
		goto poll_again;
	}
	//////pr_notice("%s1111111111111\n", __func__);
	napi_complete(napi);
	mtk_rx_irq_enable3(eth, MTK_RX_DONE_DLY3);
	//////pr_notice("%s22222222222222\n", __func__);
	return rx_done + budget - remain_budget;
}

static int mtk_tx_alloc(struct mtk_eth *eth)
{
	struct mtk_tx_ring *ring = &eth->tx_ring;
	int i, sz = sizeof(*ring->dma);

	ring->buf = kcalloc(MTK_DMA_SIZE, sizeof(*ring->buf),
			       GFP_KERNEL);
	if (!ring->buf)
		goto no_tx_mem;

	ring->dma = dma_alloc_coherent(eth->dev,
					  MTK_DMA_SIZE * sz,
					  &ring->phys,
					  GFP_ATOMIC | __GFP_ZERO);
	if (!ring->dma)
		goto no_tx_mem;

	memset(ring->dma, 0, MTK_DMA_SIZE * sz);
	for (i = 0; i < MTK_DMA_SIZE; i++) {
		int next = (i + 1) % MTK_DMA_SIZE;
		u32 next_ptr = ring->phys + next * sz;

		ring->dma[i].txd2 = next_ptr;
		ring->dma[i].txd3 = TX_DMA_LS0 | TX_DMA_OWNER_CPU;
	}

	atomic_set(&ring->free_count, MTK_DMA_SIZE - 2);
	ring->next_free = &ring->dma[0];
	ring->last_free = &ring->dma[MTK_DMA_SIZE - 1];
	ring->thresh = MAX_SKB_FRAGS;

	/* make sure that all changes to the dma ring are flushed before we
	 * continue
	 */
	wmb();

	mtk_w32(eth, ring->phys, MTK_QTX_CTX_PTR);
	mtk_w32(eth, ring->phys, MTK_QTX_DTX_PTR);
	mtk_w32(eth,
		ring->phys + ((MTK_DMA_SIZE - 1) * sz),
		MTK_QTX_CRX_PTR);
	mtk_w32(eth,
		ring->phys + ((MTK_DMA_SIZE - 1) * sz),
		MTK_QTX_DRX_PTR);
	mtk_w32(eth, (QDMA_RES_THRES << 8) | QDMA_RES_THRES, MTK_QTX_CFG(0));

	return 0;

no_tx_mem:
	return -ENOMEM;
}

static void mtk_tx_clean(struct mtk_eth *eth)
{
	struct mtk_tx_ring *ring = &eth->tx_ring;
	int i;

	if (ring->buf) {
		for (i = 0; i < MTK_DMA_SIZE; i++)
			mtk_tx_unmap(eth, &ring->buf[i]);
		kfree(ring->buf);
		ring->buf = NULL;
	}

	if (ring->dma) {
		dma_free_coherent(eth->dev,
				  MTK_DMA_SIZE * sizeof(*ring->dma),
				  ring->dma,
				  ring->phys);
		ring->dma = NULL;
	}
}

static int mtk_rx_alloc(struct mtk_eth *eth, int ring_no, int rx_flag)
{
	struct mtk_rx_ring *ring;
	int rx_data_len, rx_dma_size;
	int i;
	u32 offset = 0;

	if (rx_flag == MTK_RX_FLAGS_QDMA) {
		if (ring_no)
			return -EINVAL;
		ring = &eth->rx_ring_qdma;
		offset = 0x1000;
	} else {
		ring = &eth->rx_ring[ring_no];
	}

	if (rx_flag == MTK_RX_FLAGS_HWLRO) {
		rx_data_len = MTK_MAX_LRO_RX_LENGTH;
		rx_dma_size = MTK_HW_LRO_DMA_SIZE;
	} else if (rx_flag == MTK_RX_FLAGS_QDMA) {
		rx_data_len = ETH_DATA_LEN;
		rx_dma_size = 16;
	} else {
		rx_data_len = ETH_DATA_LEN;
		rx_dma_size = MTK_DMA_SIZE;
	}

	ring->frag_size = mtk_max_frag_size(rx_data_len);
	ring->buf_size = mtk_max_buf_size(ring->frag_size);
	ring->data = kcalloc(rx_dma_size, sizeof(*ring->data),
			     GFP_KERNEL);
	if (!ring->data) {
		//pr_notice(" data no mem\n");
		return -ENOMEM;
	}

	for (i = 0; i < rx_dma_size; i++) {
		ring->data[i] = netdev_alloc_frag(ring->frag_size);
		if (!ring->data[i]) {
			//pr_notice(" data no mem 11\n");
			return -ENOMEM;
		}
	}

	ring->dma = dma_alloc_coherent(eth->dev,
				       rx_dma_size * sizeof(*ring->dma),
				       &ring->phys,
				       GFP_ATOMIC | __GFP_ZERO);
	if (!ring->dma){
		//pr_notice(" dma no mem\n");
		return -ENOMEM;
	}

	for (i = 0; i < rx_dma_size; i++) {
		dma_addr_t dma_addr = dma_map_single(eth->dev,
				ring->data[i] + NET_SKB_PAD,
				ring->buf_size,
				DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(eth->dev, dma_addr)))
			return -ENOMEM;
		ring->dma[i].rxd1 = (unsigned int)dma_addr;

		ring->dma[i].rxd2 = RX_DMA_PLEN0(ring->buf_size);
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

static void mtk_rx_clean(struct mtk_eth *eth, struct mtk_rx_ring *ring)
{
	int i;

	if (ring->data && ring->dma) {
		for (i = 0; i < ring->dma_size; i++) {
			if (!ring->data[i])
				continue;
			if (!ring->dma[i].rxd1)
				continue;
			dma_unmap_single(eth->dev,
					 ring->dma[i].rxd1,
					 ring->buf_size,
					 DMA_FROM_DEVICE);
			skb_free_frag(ring->data[i]);
		}
		kfree(ring->data);
		ring->data = NULL;
	}

	if (ring->dma) {
		dma_free_coherent(eth->dev,
				  ring->dma_size * sizeof(*ring->dma),
				  ring->dma,
				  ring->phys);
		ring->dma = NULL;
	}
}

static int mtk_hwlro_rx_init(struct mtk_eth *eth)
{
	int i;
	u32 ring_ctrl_dw1 = 0, ring_ctrl_dw2 = 0, ring_ctrl_dw3 = 0;
	u32 lro_ctrl_dw0 = 0, lro_ctrl_dw3 = 0;

	/* set LRO rings to auto-learn modes */
	ring_ctrl_dw2 |= MTK_RING_AUTO_LERAN_MODE;

	/* validate LRO ring */
	ring_ctrl_dw2 |= MTK_RING_VLD;

	/* set AGE timer (unit: 20us) */
	ring_ctrl_dw2 |= MTK_RING_AGE_TIME_H;
	ring_ctrl_dw1 |= MTK_RING_AGE_TIME_L;

	/* set max AGG timer (unit: 20us) */
	ring_ctrl_dw2 |= MTK_RING_MAX_AGG_TIME;

	/* set max LRO AGG count */
	ring_ctrl_dw2 |= MTK_RING_MAX_AGG_CNT_L;
	ring_ctrl_dw3 |= MTK_RING_MAX_AGG_CNT_H;

	for (i = 1; i < MTK_MAX_RX_RING_NUM; i++) {
		mtk_w32(eth, ring_ctrl_dw1, MTK_LRO_CTRL_DW1_CFG(i));
		mtk_w32(eth, ring_ctrl_dw2, MTK_LRO_CTRL_DW2_CFG(i));
		mtk_w32(eth, ring_ctrl_dw3, MTK_LRO_CTRL_DW3_CFG(i));
	}

	/* IPv4 checksum update enable */
	lro_ctrl_dw0 |= MTK_L3_CKS_UPD_EN;

	/* switch priority comparison to packet count mode */
	lro_ctrl_dw0 |= MTK_LRO_ALT_PKT_CNT_MODE;

	/* bandwidth threshold setting */
	mtk_w32(eth, MTK_HW_LRO_BW_THRE, MTK_PDMA_LRO_CTRL_DW2);

	/* auto-learn score delta setting */
	mtk_w32(eth, MTK_HW_LRO_REPLACE_DELTA, MTK_PDMA_LRO_ALT_SCORE_DELTA);

	/* set refresh timer for altering flows to 1 sec. (unit: 20us) */
	mtk_w32(eth, (MTK_HW_LRO_TIMER_UNIT << 16) | MTK_HW_LRO_REFRESH_TIME,
		MTK_PDMA_LRO_ALT_REFRESH_TIMER);

	/* set HW LRO mode & the max aggregation count for rx packets */
	lro_ctrl_dw3 |= MTK_ADMA_MODE | (MTK_HW_LRO_MAX_AGG_CNT & 0xff);

	/* the minimal remaining room of SDL0 in RXD for lro aggregation */
	lro_ctrl_dw3 |= MTK_LRO_MIN_RXD_SDL;

	/* enable HW LRO */
	lro_ctrl_dw0 |= MTK_LRO_EN;

	mtk_w32(eth, lro_ctrl_dw3, MTK_PDMA_LRO_CTRL_DW3);
	mtk_w32(eth, lro_ctrl_dw0, MTK_PDMA_LRO_CTRL_DW0);

	return 0;
}

static int mtk_hwrss_rx_init(struct mtk_eth *eth)
{
	u32 reg_ctrl = 0;
	/* 1. Set RX ring1~3 to pse modes */
	//SET_PDMA_RXRING_MODE(ADMA_RX_RING1, PDMA_RX_PSE_MODE);
	//SET_PDMA_RXRING_MODE(ADMA_RX_RING2, PDMA_RX_PSE_MODE);
	//SET_PDMA_RXRING_MODE(ADMA_RX_RING3, PDMA_RX_PSE_MODE);
	reg_ctrl = mtk_r32(eth, LRO_RX_RING1_CTRL_DW2);
	reg_ctrl &= ~(0x3 << MTK_RX_MODE_OFFSET);
	mtk_w32(eth, reg_ctrl | MTK_RING_PSE_MODE, LRO_RX_RING1_CTRL_DW2 );
	mtk_w32(eth, reg_ctrl | MTK_RING_PSE_MODE, LRO_RX_RING2_CTRL_DW2 );
	mtk_w32(eth, reg_ctrl | MTK_RING_PSE_MODE, LRO_RX_RING3_CTRL_DW2 );

	/* 2. Enable non-lro multiple rx */
	//SET_PDMA_NON_LRO_MULTI_EN(1);  /* MRX EN */
	reg_ctrl = mtk_r32(eth, MTK_PDMA_LRO_CTRL_DW0);
	mtk_w32(eth, reg_ctrl | MTK_NON_LRO_MULTI_EN | MTK_LRO_DLY_INT_EN , MTK_PDMA_LRO_CTRL_DW0);

	/*Hash Type*/
	//SET_PDMA_RSS_IPV4_TYPE(7);
	//SET_PDMA_RSS_IPV6_TYPE(7);

	reg_ctrl = mtk_r32(eth, MTK_RSS_GLO_CFG);
	reg_ctrl &= ~(MTK_RSS_IPV4_TYPE);
	reg_ctrl &= ~(MTK_RSS_IPV6_TYPE);
	reg_ctrl |= ((7) & 0x7) << MTK_RSS_IPV4_TYPE_OFFSET;
	reg_ctrl |= ((7) & 0x7) << MTK_RSS_IPV6_TYPE_OFFSET;
	mtk_w32(eth, reg_ctrl, MTK_RSS_GLO_CFG);

	/* 3. Select the size of indirection table */
	mtk_w32(eth, 0x39393939, MTK_RSS_INDR_TABLE_DW0);
	mtk_w32(eth, 0x39393939, MTK_RSS_INDR_TABLE_DW1);
	mtk_w32(eth, 0x39393939, MTK_RSS_INDR_TABLE_DW2);
	mtk_w32(eth, 0x39393939, MTK_RSS_INDR_TABLE_DW3);
	mtk_w32(eth, 0x39393939, MTK_RSS_INDR_TABLE_DW4);
	mtk_w32(eth, 0x39393939, MTK_RSS_INDR_TABLE_DW5);
	mtk_w32(eth, 0x39393939, MTK_RSS_INDR_TABLE_DW6);
	mtk_w32(eth, 0x39393939, MTK_RSS_INDR_TABLE_DW7);
#if(0)
	mtk_w32(eth, 0x44444444, MTK_RSS_INDR_TABLE_DW0);
	mtk_w32(eth, 0x44444444, MTK_RSS_INDR_TABLE_DW1);
	mtk_w32(eth, 0x44444444, MTK_RSS_INDR_TABLE_DW2);
	mtk_w32(eth, 0x44444444, MTK_RSS_INDR_TABLE_DW3);
	mtk_w32(eth, 0x44444444, MTK_RSS_INDR_TABLE_DW4);
	mtk_w32(eth, 0x44444444, MTK_RSS_INDR_TABLE_DW5);
	mtk_w32(eth, 0x44444444, MTK_RSS_INDR_TABLE_DW6);
	mtk_w32(eth, 0x44444444, MTK_RSS_INDR_TABLE_DW7);
#endif
#if(1)
	/* 4. Pause */
	//SET_PDMA_RSS_CFG_REQ(1);
	reg_ctrl = mtk_r32(eth, MTK_RSS_GLO_CFG);
	reg_ctrl &= ~(MTK_RSS_CFG_REQ);
	reg_ctrl |= ((1) & 0x1) << MTK_RSS_CFG_REQ_OFFSET;
	mtk_w32(eth, reg_ctrl, MTK_RSS_GLO_CFG);

	/* 5. Enable RSS */
	//SET_PDMA_RSS_EN(1);

	reg_ctrl = mtk_r32(eth, MTK_RSS_GLO_CFG);
	reg_ctrl &= ~(MTK_RSS_EN);
	reg_ctrl |= ((1) & 0x1) << MTK_RSS_EN_OFFSET;
	mtk_w32(eth, reg_ctrl, MTK_RSS_GLO_CFG);

	/* 6. Release pause */
	//SET_PDMA_RSS_CFG_REQ(0);
	reg_ctrl = mtk_r32(eth, MTK_RSS_GLO_CFG);
	reg_ctrl &= ~(MTK_RSS_CFG_REQ);
	reg_ctrl |= ((0) & 0x1) << MTK_RSS_CFG_REQ_OFFSET;
	mtk_w32(eth, reg_ctrl, MTK_RSS_GLO_CFG);
#endif
	return 0;
}

static void mtk_hwlro_rx_uninit(struct mtk_eth *eth)
{
	int i;
	u32 val;

	/* relinquish lro rings, flush aggregated packets */
	mtk_w32(eth, MTK_LRO_RING_RELINQUISH_REQ, MTK_PDMA_LRO_CTRL_DW0);

	/* wait for relinquishments done */
	for (i = 0; i < 10; i++) {
		val = mtk_r32(eth, MTK_PDMA_LRO_CTRL_DW0);
		if (val & MTK_LRO_RING_RELINQUISH_DONE) {
			msleep(20);
			continue;
		}
		break;
	}

	/* invalidate lro rings */
	for (i = 1; i < MTK_MAX_RX_RING_NUM; i++)
		mtk_w32(eth, 0, MTK_LRO_CTRL_DW2_CFG(i));

	/* disable HW LRO */
	mtk_w32(eth, 0, MTK_PDMA_LRO_CTRL_DW0);
}

static void mtk_hwlro_val_ipaddr(struct mtk_eth *eth, int idx, __be32 ip)
{
	u32 reg_val;

	reg_val = mtk_r32(eth, MTK_LRO_CTRL_DW2_CFG(idx));

	/* invalidate the IP setting */
	mtk_w32(eth, (reg_val & ~MTK_RING_MYIP_VLD), MTK_LRO_CTRL_DW2_CFG(idx));

	mtk_w32(eth, ip, MTK_LRO_DIP_DW0_CFG(idx));

	/* validate the IP setting */
	mtk_w32(eth, (reg_val | MTK_RING_MYIP_VLD), MTK_LRO_CTRL_DW2_CFG(idx));
}

static void mtk_hwlro_inval_ipaddr(struct mtk_eth *eth, int idx)
{
	u32 reg_val;

	reg_val = mtk_r32(eth, MTK_LRO_CTRL_DW2_CFG(idx));

	/* invalidate the IP setting */
	mtk_w32(eth, (reg_val & ~MTK_RING_MYIP_VLD), MTK_LRO_CTRL_DW2_CFG(idx));

	mtk_w32(eth, 0, MTK_LRO_DIP_DW0_CFG(idx));
}

static int mtk_hwlro_get_ip_cnt(struct mtk_mac *mac)
{
	int cnt = 0;
	int i;

	for (i = 0; i < MTK_MAX_LRO_IP_CNT; i++) {
		if (mac->hwlro_ip[i])
			cnt++;
	}

	return cnt;
}

static int mtk_hwlro_add_ipaddr(struct net_device *dev,
				struct ethtool_rxnfc *cmd)
{
	struct ethtool_rx_flow_spec *fsp =
		(struct ethtool_rx_flow_spec *)&cmd->fs;
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	int hwlro_idx;

	if ((fsp->flow_type != TCP_V4_FLOW) ||
	    (!fsp->h_u.tcp_ip4_spec.ip4dst) ||
	    (fsp->location > 1))
		return -EINVAL;

	mac->hwlro_ip[fsp->location] = htonl(fsp->h_u.tcp_ip4_spec.ip4dst);
	hwlro_idx = (mac->id * MTK_MAX_LRO_IP_CNT) + fsp->location;

	mac->hwlro_ip_cnt = mtk_hwlro_get_ip_cnt(mac);

	mtk_hwlro_val_ipaddr(eth, hwlro_idx, mac->hwlro_ip[fsp->location]);

	return 0;
}

static int mtk_hwlro_del_ipaddr(struct net_device *dev,
				struct ethtool_rxnfc *cmd)
{
	struct ethtool_rx_flow_spec *fsp =
		(struct ethtool_rx_flow_spec *)&cmd->fs;
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	int hwlro_idx;

	if (fsp->location > 1)
		return -EINVAL;

	mac->hwlro_ip[fsp->location] = 0;
	hwlro_idx = (mac->id * MTK_MAX_LRO_IP_CNT) + fsp->location;

	mac->hwlro_ip_cnt = mtk_hwlro_get_ip_cnt(mac);

	mtk_hwlro_inval_ipaddr(eth, hwlro_idx);

	return 0;
}

static void mtk_hwlro_netdev_disable(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	int i, hwlro_idx;

	for (i = 0; i < MTK_MAX_LRO_IP_CNT; i++) {
		mac->hwlro_ip[i] = 0;
		hwlro_idx = (mac->id * MTK_MAX_LRO_IP_CNT) + i;

		mtk_hwlro_inval_ipaddr(eth, hwlro_idx);
	}

	mac->hwlro_ip_cnt = 0;
}

static int mtk_hwlro_get_fdir_entry(struct net_device *dev,
				    struct ethtool_rxnfc *cmd)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct ethtool_rx_flow_spec *fsp =
		(struct ethtool_rx_flow_spec *)&cmd->fs;

	/* only tcp dst ipv4 is meaningful, others are meaningless */
	fsp->flow_type = TCP_V4_FLOW;
	fsp->h_u.tcp_ip4_spec.ip4dst = ntohl(mac->hwlro_ip[fsp->location]);
	fsp->m_u.tcp_ip4_spec.ip4dst = 0;

	fsp->h_u.tcp_ip4_spec.ip4src = 0;
	fsp->m_u.tcp_ip4_spec.ip4src = 0xffffffff;
	fsp->h_u.tcp_ip4_spec.psrc = 0;
	fsp->m_u.tcp_ip4_spec.psrc = 0xffff;
	fsp->h_u.tcp_ip4_spec.pdst = 0;
	fsp->m_u.tcp_ip4_spec.pdst = 0xffff;
	fsp->h_u.tcp_ip4_spec.tos = 0;
	fsp->m_u.tcp_ip4_spec.tos = 0xff;

	return 0;
}

static int mtk_hwlro_get_fdir_all(struct net_device *dev,
				  struct ethtool_rxnfc *cmd,
				  u32 *rule_locs)
{
	struct mtk_mac *mac = netdev_priv(dev);
	int cnt = 0;
	int i;

	for (i = 0; i < MTK_MAX_LRO_IP_CNT; i++) {
		if (mac->hwlro_ip[i]) {
			rule_locs[cnt] = i;
			cnt++;
		}
	}

	cmd->rule_cnt = cnt;

	return 0;
}

static netdev_features_t mtk_fix_features(struct net_device *dev,
					  netdev_features_t features)
{
	if (!(features & NETIF_F_LRO)) {
		struct mtk_mac *mac = netdev_priv(dev);
		int ip_cnt = mtk_hwlro_get_ip_cnt(mac);

		if (ip_cnt) {
			netdev_info(dev, "RX flow is programmed, LRO should keep on\n");

			features |= NETIF_F_LRO;
		}
	}

	return features;
}

static int mtk_set_features(struct net_device *dev, netdev_features_t features)
{
	int err = 0;

	if (!((dev->features ^ features) & NETIF_F_LRO))
		return 0;

	if (!(features & NETIF_F_LRO))
		mtk_hwlro_netdev_disable(dev);

	return err;
}

/* wait for DMA to finish whatever it is doing before we start using it again */
static int mtk_dma_busy_wait(struct mtk_eth *eth)
{
	unsigned long t_start = jiffies;

	while (1) {
		if (!(mtk_r32(eth, MTK_QDMA_GLO_CFG) &
		      (MTK_RX_DMA_BUSY | MTK_TX_DMA_BUSY)))
			return 0;
		if (time_after(jiffies, t_start + MTK_DMA_BUSY_TIMEOUT))
			break;
	}

	if (eth->hwedmarx) {
		while (1) {
			if (!(mtk_r32(eth, MTK_EDMA0_GLO_CFG) &
				  (MTK_RX_DMA_BUSY)) && !(mtk_r32(eth, MTK_EDMA1_GLO_CFG) &
				  (MTK_RX_DMA_BUSY)))
				return 0;
			if (time_after(jiffies, t_start + MTK_DMA_BUSY_TIMEOUT))
				break;
		}
	}

	dev_err(eth->dev, "DMA init timeout\n");
	return -1;
}

static int mtk_dma_init(struct mtk_eth *eth)
{
	int err;
	u32 i;

	if (mtk_dma_busy_wait(eth))
		return -EBUSY;

	/* QDMA needs scratch memory for internal reordering of the
	 * descriptors
	 */
	err = mtk_init_fq_dma(eth);
	if (err)
		return err;

	err = mtk_tx_alloc(eth);
	if (err)
		return err;

	err = mtk_rx_alloc(eth, 0, MTK_RX_FLAGS_QDMA);
	if (err)
		return err;

	err = mtk_rx_alloc(eth, 0, MTK_RX_FLAGS_NORMAL);
	if (err)
		return err;

	if (eth->hwlro) {
		for (i = 1; i < MTK_MAX_RX_RING_NUM; i++) {
			err = mtk_rx_alloc(eth, i, MTK_RX_FLAGS_HWLRO);
			if (err)
				return err;
		}
		err = mtk_hwlro_rx_init(eth);
		if (err)
			return err;
	}


	if (eth->hwrss) {
		for (i = 1; i < MTK_MAX_RX_RING_NUM; i++) {
			err = mtk_rx_alloc(eth, i, MTK_RX_FLAGS_HWRSS);
			if (err)
				return err;
		}
		err = mtk_hwrss_rx_init(eth);
		if (err)
			return err;
	}


	if (eth->hwedmarx) {
		for (i = 0; i < MTK_MAX_EDMA_RX_RING_NUM; i++) {
			err = mtk_rx_alloc_edma(eth, i, MTK_RX_FLAGS_EDMA0);
			if (err)
				return err;
		}
		for (i = 0; i < MTK_MAX_EDMA_RX_RING_NUM; i++) {
			err = mtk_rx_alloc_edma(eth, i, MTK_RX_FLAGS_EDMA1);
			if (err)
				return err;
		}
	}

	/* Enable random early drop and set drop threshold automatically */
	mtk_w32(eth, FC_THRES_HW_DROP_EN | FC_THRES_SW_DROP_EN | FC_THRES_MIN,
		MTK_QDMA_FC_THRES);
	for (i = 0; i < 64; i++) {
		if (i <= 15) {
			mtk_w32(eth, 0, MTK_QDMA_SEL);
			mtk_w32(eth, (QDMA_RES_THRES << 8) | QDMA_RES_THRES, MTK_QTX_CFG(i));
			mtk_w32(eth, (TX_SCH_SEL0 << 30) , MTK_QTX_SCH(i));
		} else if (i > 15 && i <= 31) {
			mtk_w32(eth, 1, MTK_QDMA_SEL);
			mtk_w32(eth, (QDMA_RES_THRES << 8) | QDMA_RES_THRES, MTK_QTX_CFG((i-16)));
			mtk_w32(eth, (TX_SCH_SEL1 << 30) , MTK_QTX_SCH((i-16)));
		} else if (i > 31 && i <= 47) {
			mtk_w32(eth, 2, MTK_QDMA_SEL);
			mtk_w32(eth, (QDMA_RES_THRES << 8) | QDMA_RES_THRES, MTK_QTX_CFG((i-32)));
			mtk_w32(eth, (TX_SCH_SEL2 << 30) , MTK_QTX_SCH((i-32)));
		} else if (i > 47 && i <= 63) {
			mtk_w32(eth, 3, MTK_QDMA_SEL);
			mtk_w32(eth, (QDMA_RES_THRES << 8) | QDMA_RES_THRES, MTK_QTX_CFG((i-48)));
			mtk_w32(eth, (TX_SCH_SEL3 << 30) , MTK_QTX_SCH((i-48)));
		}
	}
	mtk_w32(eth, 0, MTK_QDMA_SEL);
	mtk_w32(eth, 0x0, MTK_QDMA_HRED2);

	return 0;
}

static void mtk_dma_free(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_MAC_COUNT; i++)
		if (eth->netdev[i])
			netdev_reset_queue(eth->netdev[i]);
	if (eth->scratch_ring) {
		dma_free_coherent(eth->dev,
				  MTK_DMA_SIZE * sizeof(struct mtk_tx_dma),
				  eth->scratch_ring,
				  eth->phy_scratch_ring);
		eth->scratch_ring = NULL;
		eth->phy_scratch_ring = 0;
	}
	mtk_tx_clean(eth);
	mtk_rx_clean(eth, &eth->rx_ring[0]);
	mtk_rx_clean(eth, &eth->rx_ring_qdma);

	if (eth->hwlro) {
		mtk_hwlro_rx_uninit(eth);
		for (i = 1; i < MTK_MAX_RX_RING_NUM; i++)
			mtk_rx_clean(eth, &eth->rx_ring[i]);
	}

	if (eth->hwrss) {
		for (i = 1; i < MTK_MAX_RX_RING_NUM; i++)
			mtk_rx_clean(eth, &eth->rx_ring[i]);
	}
	kfree(eth->scratch_head);


	if(eth->hwedmarx) {
		for (i = 0; i < MTK_MAX_EDMA_RX_RING_NUM; i++){
			mtk_rx_clean_edma(eth, &eth->rx_ring_edma0[i]);
			mtk_rx_clean_edma(eth, &eth->rx_ring_edma1[i]);
		}
	}
}
#if(0)
static void mtk_tx_timeout(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;

	eth->netdev[mac->id]->stats.tx_errors++;
	netif_err(eth, tx_err, dev,
		  "transmit timed out\n");
	//schedule_work(&eth->pending_work);
}
#endif
static irqreturn_t mtk_handle_irq_rx(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (likely(napi_schedule_prep(&eth->rx_napi))) {
		__napi_schedule(&eth->rx_napi);
		mtk_rx_irq_disable(eth, MTK_RX_DONE_INT);
	}

	return IRQ_HANDLED;
}

static irqreturn_t mtk_handle_irq_rx0(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (likely(napi_schedule_prep(&eth->rx_napi0))) {
		__napi_schedule(&eth->rx_napi0);
		mtk_rx_irq_disable0(eth, MTK_RX_DONE_DLY);
	}

	return IRQ_HANDLED;
}

static irqreturn_t mtk_handle_irq_rx1(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (likely(napi_schedule_prep(&eth->rx_napi1))) {
		__napi_schedule(&eth->rx_napi1);
		mtk_rx_irq_disable1(eth, MTK_RX_DONE_DLY1);
	}

	return IRQ_HANDLED;
}

static irqreturn_t mtk_handle_irq_rx2(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (likely(napi_schedule_prep(&eth->rx_napi2))) {
		__napi_schedule(&eth->rx_napi2);
		mtk_rx_irq_disable2(eth, MTK_RX_DONE_DLY2);
	}

	return IRQ_HANDLED;
}

static irqreturn_t mtk_handle_irq_rx3(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (likely(napi_schedule_prep(&eth->rx_napi3))) {
		__napi_schedule(&eth->rx_napi3);
		mtk_rx_irq_disable3(eth, MTK_RX_DONE_DLY3);
	}

	return IRQ_HANDLED;
}

/*Notice: The following modification is for Single PHY chip and MT6890 flow control
status Sync. This Solution is only for AQR112C Chip. If Single PHY chip is not
AQR112C, the solution is not avaliable.
If Single PHY chip is not AQR112C, and you encounter the flow control mismatch issue,
the following step you should do:
1. Contact your Single PHY chip vendor and get the details of
    - which Interrupt should listen when link state change
    - how to check link status, and flow control status
2. Modify the code for the customization.
*/

/*
Handle Interrupt for AQR112C PHY Link State Change
*/
static irqreturn_t mtk_handle_irq_ext(int irq, void *_eth){

	struct mtk_eth *eth = _eth;

	disable_irq_nosync(eth->irq[8]);
	schedule_work(&eth->link_work);

	return IRQ_HANDLED;
}

/*
Schedule Task To Check AQR112C PHY Flow Control Status
*/

void mtk_handle_ext_worker(struct work_struct *work){

	struct mtk_eth *eth;
	u32 lp_pause, speed_bits, link;
	const char *speed;
	eth = container_of(work, struct mtk_eth, link_work);


	/*1. Check Single PHY Flow Control Status*/
	_mtk_mdio_read_C45(eth, 8, 0xfc00, 0x1e,0);
	_mtk_mdio_read_C45(eth, 8, 0xFC01, 0x1e,0);
	_mtk_mdio_read_C45(eth, 8, 0xFC00, 0x7,0);
	_mtk_mdio_read_C45(eth, 8, 0xCC01, 0x7,0);
	lp_pause = (_mtk_mdio_read_C45(eth, 8, 0x13, 0x7,0) & BIT(10)) >> 10;
	if(lp_pause) {
		dev_info(eth->dev, "lp pause enable\n");
		/*if PHY Flow control enable - enable MAC flow control*/
		mtk_w32(eth, 0x0104f33b, MTK_MAC_MCR(0));
	} else {
		dev_info(eth->dev, "lp pause disaable\n");
		/*if PHY Flow control disable - disable MAC flow control*/
		mtk_w32(eth, 0x0104f30b, MTK_MAC_MCR(0));
	}

	/*2. Check Single PHY Link Status*/
	speed = "link_down";
	link = (_mtk_mdio_read_C45(eth, 8, 0xe800, 0x1,0)) & BIT(0);
	speed_bits = ((_mtk_mdio_read_C45(eth, 8, 0xc800, 0x7,0)) & 0xe) >> 1;
	if(link) {
		switch (speed_bits) {
			case 0:
				speed = "down";
			break;
			case 1:
				speed = "100Mbps";
				break;
			case 2:
				speed = "1Gbps";
			break;
			case 3:
				speed = "10Gbps";
				break;
			case 4:
				speed = "2.5Gbps";
				break;
			case 5:
				speed = "5Gbps";
				break;
			}
	} else {
		/*if PHY Link down - reset MAC flow control to default*/
		mtk_w32(eth, 0x0104f33b, MTK_MAC_MCR(0));
	}
	dev_info(eth->dev, "PHY Link is - %s\n", speed);
	enable_irq(eth->irq[8]);
}

static irqreturn_t mtk_handle_irq_tx(int irq, void *_eth)
{
	struct mtk_eth *eth = _eth;

	if (likely(napi_schedule_prep(&eth->tx_napi))) {
		__napi_schedule(&eth->tx_napi);
		mtk_tx_irq_disable(eth, MTK_TX_DONE_INT);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void mtk_poll_controller(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;

	mtk_tx_irq_disable(eth, MTK_TX_DONE_INT);

	if (eth->hwrss) {
		mtk_rx_irq_disable0(eth, MTK_RX_DONE_DLY);
		mtk_rx_irq_disable1(eth, MTK_RX_DONE_DLY1);
		mtk_rx_irq_disable2(eth, MTK_RX_DONE_DLY2);
		mtk_rx_irq_disable3(eth, MTK_RX_DONE_DLY3);
		mtk_handle_irq_rx0(eth->irq[2], dev);
		mtk_handle_irq_rx1(eth->irq[3], dev);
		mtk_handle_irq_rx2(eth->irq[4], dev);
		mtk_handle_irq_rx3(eth->irq[5], dev);
		mtk_rx_irq_enable0(eth, MTK_RX_DONE_DLY);
		mtk_rx_irq_enable1(eth, MTK_RX_DONE_DLY1);
		mtk_rx_irq_enable2(eth, MTK_RX_DONE_DLY2);
		mtk_rx_irq_enable3(eth, MTK_RX_DONE_DLY3);
	} else {
		mtk_rx_irq_disable(eth, MTK_RX_DONE_INT);
		mtk_handle_irq_rx(eth->irq[2], dev);
		mtk_rx_irq_enable(eth, MTK_RX_DONE_INT);
	}

	if(eth->hwedmarx) {
		mtk_poll_edma(eth, dev);
	}

	mtk_tx_irq_enable(eth, MTK_TX_DONE_INT);
}
#endif

static int mtk_start_dma(struct mtk_eth *eth)
{
	int err;
	u32 rx_2b_offet = (NET_IP_ALIGN == 2) ? MTK_RX_2B_OFFSET : 0;

	err = mtk_dma_init(eth);
	if (err) {
		mtk_dma_free(eth);
		return err;
	}
#if(0)
	mtk_w32(eth, MTK_TX_DMA_EN | MTK_RX_DMA_EN | MTK_NDP_CO_PRO |
		MTK_MULTI_DMA | MTK_WCOMP_EN | MTK_DMAD_WR_WDONE |
		MTK_CHK_DDONE_EN | MTK_RX_DMA_EN | MTK_RX_2B_OFFSET,
		MTK_QDMA_GLO_CFG);
#endif
	//0x9560F537
	mtk_w32(eth, 0x95404535, MTK_QDMA_GLO_CFG);
	mtk_w32(eth,
		MTK_RX_DMA_EN | rx_2b_offet |
		MTK_RX_BT_32DWORDS | MTK_MULTI_EN,
		MTK_PDMA_GLO_CFG);

	if (eth->hwedmarx) {
		mtk_start_dma_edma(eth);
	}
	return 0;
}

static void mtk_gdma_config(struct mtk_eth *eth, u32 config)
{
	int i;

	for (i = 0; i < 2; i++) {
		u32 val = mtk_r32(eth, MTK_GDMA_FWD_CFG(i));

		/* default setup the forward port to send frame to PDMA */
		val &= ~0xffff;
		if (ppe_hook_tx_eth)
			val |= MTK_GDMA_PPE;
		val |= config;

		mtk_w32(eth, val, MTK_GDMA_FWD_CFG(i));
	}
	/*Reset and enable PSE*/
	mtk_w32(eth, RST_GL_PSE, MTK_RST_GL);
	mtk_w32(eth, 0, MTK_RST_GL);
}

static int mtk_open(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	int err;

	pr_info("1!!!!! RX desc = %x\n", cpu_to_le32((u32)(4200 - 1)));

	regulator_set_voltage(eth->dvfsrc_vcore_power, SGMII_VCORE_OPS, INT_MAX);
	/* we run 2 netdevs on the same dma ring so we only bring it up once */
	if (!refcount_read(&eth->dma_refcnt)) {
		err = mtk_start_dma(eth);
		if (err)
			return err;
		mtk_gdma_config(eth, MTK_GDMA_ICS_EN | MTK_GDMA_TCS_EN | MTK_GDMA_UCS_EN);
		napi_enable(&eth->tx_napi);

		if(eth->hwrss) {
			napi_enable(&eth->rx_napi0);
			napi_enable(&eth->rx_napi1);
			napi_enable(&eth->rx_napi2);
			napi_enable(&eth->rx_napi3);
		} else {
			napi_enable(&eth->rx_napi);
		}
		mtk_tx_irq_enable(eth, MTK_TX_DONE_INT);
		if(eth->hwrss) {
			mtk_rx_irq_enable0(eth, MTK_RX_DONE_DLY);
			mtk_rx_irq_enable1(eth, MTK_RX_DONE_DLY1);
			mtk_rx_irq_enable2(eth, MTK_RX_DONE_DLY2);
			mtk_rx_irq_enable3(eth, MTK_RX_DONE_DLY3);
		} else {
			mtk_rx_irq_enable(eth, MTK_RX_DONE_INT);
		}

		if(eth->hwedmarx) {
			napi_enable_edma(eth);
			mtk_rx_irq_enable_edma_all(eth);
		}

		refcount_set(&eth->dma_refcnt, 1);
	}
	else
		refcount_inc(&eth->dma_refcnt);

	phy_start(dev->phydev);
	netif_start_queue(dev);

	return 0;
}

static void mtk_stop_dma(struct mtk_eth *eth, u32 glo_cfg)
{
	u32 val;
	int i;

	/* stop the dma engine */
	spin_lock_bh(&eth->page_lock);
	val = mtk_r32(eth, glo_cfg);
	mtk_w32(eth, val & ~(MTK_TX_WB_DDONE | MTK_RX_DMA_EN | MTK_TX_DMA_EN),
		glo_cfg);
	spin_unlock_bh(&eth->page_lock);

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

static int mtk_stop(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;

	netif_tx_disable(dev);
	phy_stop(dev->phydev);

	/* only shutdown DMA if this is the last user */
	if (!refcount_dec_and_test(&eth->dma_refcnt))
		return 0;
	mtk_gdma_config(eth, MTK_GDMA_DROP_ALL);

	mtk_tx_irq_disable(eth, MTK_TX_DONE_INT);


	if(eth->hwrss) {
		mtk_rx_irq_disable0(eth, MTK_RX_DONE_DLY);
		mtk_rx_irq_disable1(eth, MTK_RX_DONE_DLY1);
		mtk_rx_irq_disable2(eth, MTK_RX_DONE_DLY2);
		mtk_rx_irq_disable3(eth, MTK_RX_DONE_DLY3);
	} else {
		mtk_rx_irq_disable(eth, MTK_RX_DONE_INT);
	}

	if(eth->hwedmarx) {
		mtk_rx_irq_disable_edma_all(eth);
		napi_disable(&eth->rx_napi_edma0);
		napi_disable(&eth->rx_napi_edma1);
	}
	napi_disable(&eth->tx_napi);

	if(eth->hwrss) {
		napi_disable(&eth->rx_napi0);
		napi_disable(&eth->rx_napi1);
		napi_disable(&eth->rx_napi2);
		napi_disable(&eth->rx_napi3);
	} else {
		napi_disable(&eth->rx_napi);
	}

	mtk_stop_dma(eth, MTK_QDMA_GLO_CFG);
	mtk_stop_dma(eth, MTK_PDMA_GLO_CFG);

	if(eth->hwedmarx) {
		mtk_stop_dma_edma(eth, MTK_EDMA0_GLO_CFG);
		mtk_stop_dma_edma(eth, MTK_EDMA1_GLO_CFG);
	}

	mtk_dma_free(eth);
	regulator_set_voltage(eth->dvfsrc_vcore_power, SGMII_VCORE_NON_OPS, INT_MAX);

	return 0;
}

static void ethsys_reset(struct mtk_eth *eth, u32 reset_bits)
{
	regmap_update_bits(eth->ethsys, ETHSYS_RSTCTRL,
			   reset_bits,
			   reset_bits);

	usleep_range(1000, 1100);
	regmap_update_bits(eth->ethsys, ETHSYS_RSTCTRL,
			   reset_bits,
			   ~reset_bits);
	mdelay(10);
}

static void mtk_clk_disable(struct mtk_eth *eth)
{
	int clk;

	for (clk = MTK_CLK_MAX - 1; clk >= 0; clk--) {
		if (eth->soc->required_clks & BIT(clk)) {
			if (eth->clks[clk] != NULL)
				clk_disable_unprepare(eth->clks[clk]);
		}
	}
}

static int mtk_clk_enable(struct mtk_eth *eth)
{
	int clk, ret;

	for (clk = 0; clk < MTK_CLK_MAX ; clk++) {
		if (eth->soc->required_clks & BIT(clk)) {
			if (eth->clks[clk] != NULL) {
				ret = clk_prepare_enable(eth->clks[clk]);
				if (ret)
					goto err_disable_clks;
                        }
		}
	}

	//////pr_notice("open clkc success\n");
	return 0;

err_disable_clks:
	while (--clk >= 0)
		clk_disable_unprepare(eth->clks[clk]);

	return ret;
}

static int mtk_hw_init(struct mtk_eth *eth)
{
	int i, val, ret;
	u32 reg_val = 0;
	u32 tmp;

	if (test_and_set_bit(MTK_HW_INIT, &eth->state)) {
		//////pr_notice("#######return\n");
		return 0;
	}

	if (!(MTK_HAS_CAPS(eth->soc->caps, MTK_FPGA_CLK))) {
		pm_runtime_enable(eth->dev);
		pm_runtime_get_sync(eth->dev);
		if (eth->soc->sgmii_pm) {
			pm_runtime_enable(eth->sgmii_phy_dev0);
			pm_runtime_get_sync(eth->sgmii_phy_dev0);
			pm_runtime_enable(eth->sgmii_phy_dev1);
			pm_runtime_get_sync(eth->sgmii_phy_dev1);
			pm_runtime_enable(eth->sgmii_dev0);
			pm_runtime_get_sync(eth->sgmii_dev0);
			pm_runtime_enable(eth->sgmii_dev1);
			pm_runtime_get_sync(eth->sgmii_dev1);
		}
		ret = mtk_clk_enable(eth);
		if (ret)
			goto err_disable_pm;
	}

	//ethsys_reset(eth, RSTCTRL_FE);
	ethsys_reset(eth, RSTCTRL_PPE);

	regmap_read(eth->ethsys, ETHSYS_SYSCFG0, &val);
	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->mac[i])
			continue;
		val &= ~SYSCFG0_GE_MODE(SYSCFG0_GE_MASK, eth->mac[i]->id);
		val |= SYSCFG0_GE_MODE(eth->mac[i]->ge_mode, eth->mac[i]->id);
	}
	regmap_write(eth->ethsys, ETHSYS_SYSCFG0, val);

	if (eth->pctl) {
		/* Set GE2 driving and slew rate */
		regmap_write(eth->pctl, GPIO_DRV_SEL10, 0xa00);

		/* set GE2 TDSEL */
		regmap_write(eth->pctl, GPIO_OD33_CTRL8, 0x5);

		/* set GE2 TUNE */
		regmap_write(eth->pctl, GPIO_BIAS_CTRL, 0x0);
	}

	/* Set linkdown as the default for each GMAC. Its own MCR would be set
	 * up with the more appropriate value when mtk_phy_link_adjust call is
	 * being invoked.
	 */
	for (i = 0; i < MTK_MAC_COUNT; i++)
		mtk_w32(eth, 0, MTK_MAC_MCR(i));

	/* Indicates CDM to parse the MTK special tag from CPU
	 * which also is working out for untag packets.
	val = mtk_r32(eth, MTK_CDMQ_IG_CTRL);
	mtk_w32(eth, val | MTK_CDMQ_STAG_EN, MTK_CDMQ_IG_CTRL);
	*/
	/* Disable RX VLan Offloading */
	mtk_w32(eth, 0, MTK_CDMP_EG_CTRL);

	mtk_w32(eth, 0x8f0f8f0f, MTK_PDMA_DELAY_INT);
	if (eth->hwrss) {
		mtk_w32(eth, 0x8f0f8f0f, MTK_PDMA1_DELAY_INT);
		mtk_w32(eth, 0x8f0f8f0f, MTK_PDMA2_DELAY_INT);
		mtk_w32(eth, 0x8f0f8f0f, MTK_PDMA3_DELAY_INT);
	}
	mtk_w32(eth, 0x8f0f8f0f, MTK_QDMA_DELAY_INT);

	mtk_tx_irq_disable(eth, ~0);

	if (eth->hwrss) {
		mtk_rx_irq_disable0(eth, MTK_RX_DONE_DLY);
		mtk_rx_irq_disable1(eth, MTK_RX_DONE_DLY1);
		mtk_rx_irq_disable2(eth, MTK_RX_DONE_DLY2);
		mtk_rx_irq_disable3(eth, MTK_RX_DONE_DLY3);
	} else {
		mtk_rx_irq_disable(eth, ~0);
	}
	if(eth->hwedmarx) {
		mtk_rx_irq_disable_edma_all(eth);
	}

	/* FE int grouping */
	/* Enable multipe rx ring delay interrupt */
	if (eth->hwrss) {
		reg_val = mtk_r32(eth, MTK_PDMA_LRO_CTRL_DW0);
		reg_val |= MTK_LRO_DLY_INT_EN;
		mtk_w32(eth, reg_val, MTK_PDMA_LRO_CTRL_DW0);
		mtk_w32(eth, 0x2020000, MTK_PDMA_INT_GRP1);
		mtk_w32(eth, 0x4040000, MTK_PDMA_INT_GRP2);
		mtk_w32(eth, 0x8080000, MTK_PDMA_INT_GRP3);
		mtk_w32(eth, 0x01000000, MTK_FE_INT_GRP);
	} else {
		//RX use MTK_PDMA_INT_GRP0
		mtk_w32(eth, 0x0, MTK_PDMA_INT_GRP1);
		mtk_w32(eth, 0x0, MTK_PDMA_INT_GRP2);
		mtk_w32(eth, 0x0, MTK_PDMA_INT_GRP3);

		//mtk_w32(eth, MTK_RX_DONE_INT, MTK_PDMA_INT_GRP2);
		mtk_w32(eth, 0x01000000, MTK_FE_INT_GRP); //set FE_INT no RX ADMA INT
	}
	mtk_w32(eth, MTK_TX_DONE_INT, MTK_QDMA_INT_GRP1);
	//mtk_w32(eth, MTK_RX_DONE_INT, MTK_QDMA_INT_GRP2); ///maybe no need to set

	if(eth->hwedmarx) {
		mtk_w32(eth, 0x8f0f8f0f, MTK_EDMA0_DELAY_INT);
		mtk_w32(eth, 0x8f0f8f0f, MTK_EDMA1_DELAY_INT);

	}
//echo reg_write 0x15100104 0x01fa01f4 > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100140 0x001a000e > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100144 0x01ff001a > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100148 0x000e01ff > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x1510014C 0x000e000e > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100150 0x000e000e > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100154 0x000e000e > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100158 0x000e000e > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x1510015C 0x000e000e > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100160 0x000f000a > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100164 0x001a000f > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100168 0x000f001a > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x1510016C 0x01ff000f > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100170 0x000f000f > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100174 0x0006000f > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x15100178 0x00060006 > /proc/clkdbg ; cat /proc/clkdbg"
//echo reg_write 0x1510017C 0x00060006 > /proc/clkdbg ; cat /proc/clkdbg"

//#Global FC threshold
	mtk_w32(eth, 0x01fa01f4, 0x104);

//#Per CDM Input threshold update

	mtk_w32(eth, 0x001a000e, 0x140);
	mtk_w32(eth, 0x01ff001a, 0x144);
	mtk_w32(eth, 0x000e01ff, 0x148);
	mtk_w32(eth, 0x000e000e, 0x14c);
	mtk_w32(eth, 0x000e000e, 0x150);
	mtk_w32(eth, 0x000e000e, 0x154);
	mtk_w32(eth, 0x000e000e, 0x158);
	mtk_w32(eth, 0x000e000e, 0x15c);
//#Per port Output threshold update
	mtk_w32(eth, 0x000f000a, 0x160);
	mtk_w32(eth, 0x001a000f, 0x164);
	mtk_w32(eth, 0x000f001a, 0x168);
	mtk_w32(eth, 0x01ff000f, 0x16c);
	mtk_w32(eth, 0x000f000f, 0x170);
	mtk_w32(eth, 0x0006000f, 0x174);
	mtk_w32(eth, 0x00060006, 0x178);
	mtk_w32(eth, 0x00060006, 0x17c);

	mtk_w32(eth, 0x00000004, 0x1530);
	mtk_w32(eth, 0x00000004, 0x164c);
	mtk_w32(eth, 0x00000004, 0x1650);
	mtk_w32(eth, 0x00000004, 0x1654);
	mtk_w32(eth, 0x00000004, 0x1658);
	mtk_w32(eth, 0x00000004, 0x165c);

#if 0
	_mtk_mdio_write_C45(eth, 8, 0x10, 0x9d81, 7,0);
	_mtk_mdio_write_C45(eth, 8, 0, 0x3200, 7,0);
#else

	/*Notice: The following modification is to set Single PHY chip
	This Solution is only for AQR112C Chip. If Single PHY chip is not
	AQR112C, the solution is not avaliable.
	If Single PHY chip is not AQR112C, the following step you should do:
	1. Contact your Single PHY chip vendor and get the details of
	    - how to enable flow control
		- how to Enables link status change alarm
		- how to Enables vendor Auto-Neg alarm
		- how to Enables Chip-wide vendor alarm
		- restart AN
	2. Modify the code for the customization.
	*/
	_mtk_mdio_write_C45(eth, 8, 0x10, 0x9d81, 7,0);

	tmp = _mtk_mdio_read_C45(eth, 8, 0xd401, 7,0);
	_mtk_mdio_write_C45(eth, 8, 0xd401, (tmp | BIT(0)), 7,0);
	tmp = _mtk_mdio_read_C45(eth, 8, 0xFF01, 0x1e,0);
	_mtk_mdio_write_C45(eth, 8, 0xFF01, (tmp | BIT(12)), 0x1e,0);
	tmp = _mtk_mdio_read_C45(eth, 8, 0xFF00, 0x1e,0);
	_mtk_mdio_write_C45(eth, 8, 0xFF00, (tmp | BIT(0)), 0x1e,0);

	_mtk_mdio_write_C45(eth, 8, 0, 0x3200, 7,0);
	/*Modify Emd*/
#endif
	return 0;

err_disable_pm:
	pm_runtime_put_sync(eth->dev);
	pm_runtime_disable(eth->dev);
	if (eth->soc->sgmii_pm) {
		pm_runtime_put_sync(eth->sgmii_dev0);
		pm_runtime_disable(eth->sgmii_dev0);
		pm_runtime_put_sync(eth->sgmii_dev1);
		pm_runtime_disable(eth->sgmii_dev1);
		pm_runtime_put_sync(eth->sgmii_phy_dev0);
		pm_runtime_disable(eth->sgmii_phy_dev0);
		pm_runtime_put_sync(eth->sgmii_phy_dev1);
		pm_runtime_disable(eth->sgmii_phy_dev1);
	}
	return ret;
}

static int mtk_hw_deinit(struct mtk_eth *eth)
{
	if (!test_and_clear_bit(MTK_HW_INIT, &eth->state))
		return 0;

	if (!MTK_HAS_CAPS(eth->soc->caps, MTK_FPGA_CLK)) {
		mtk_clk_disable(eth);
		pm_runtime_put_sync(eth->dev);
		pm_runtime_disable(eth->dev);
		if (eth->soc->sgmii_pm) {
			pm_runtime_put_sync(eth->sgmii_dev0);
			pm_runtime_disable(eth->sgmii_dev0);
			pm_runtime_put_sync(eth->sgmii_dev1);
			pm_runtime_disable(eth->sgmii_dev1);
			pm_runtime_put_sync(eth->sgmii_phy_dev0);
			pm_runtime_disable(eth->sgmii_phy_dev0);
			pm_runtime_put_sync(eth->sgmii_phy_dev1);
			pm_runtime_disable(eth->sgmii_phy_dev1);
		}
	}

	return 0;
}

/*BEGIN add at cmd to write eth0 eth1 mac addr, by haowei.cheng*/
#define MAC_ADDR_LEN (6) 

static void hex_dump(char* str,char* buf, size_t len)
{
    int i=0;
    if((NULL == buf)||(NULL == str)||(len<0)){
        pr_info("buf is NULL or len invalid\n");
        return;
    }
    pr_info("\r\n %s HEX dump:\n",str);
    for(i=0;i<len;i++){
        pr_info("%02X ",buf[i]);
    }
    pr_info("\r\n");
}

static int str2mac(char* str,char* mac)
{
    int err = 0;
    int i=0;
    char tmp_mac[20];
    memset(tmp_mac,0,sizeof(tmp_mac));
    if((NULL == str)||(NULL == mac)){
        pr_info("ERROR:str2mac param invalid\n");
        return -1;
    }
    
    err = sscanf(str,"%02X:%02X:%02X:%02X:%02X:%02X",
                &tmp_mac[0],
                &tmp_mac[1],
                &tmp_mac[2],
                &tmp_mac[3],
                &tmp_mac[4],
                &tmp_mac[5]);
    if(MAC_ADDR_LEN != err){
        pr_info("ERROR:str2mac err=%d\n",err);
        return -1;
    }
    memcpy(mac,tmp_mac,MAC_ADDR_LEN);
    return 0;
    
}

static char* get_fg360_mac_addr(char* key)
{
    struct device_node *node;
    const char *value;
    #define MAC_ADDR_STR_LEN (20) 
    
    static char mac_addr[MAC_ADDR_LEN]={0};
    char* result = NULL;
    int ret = 0;
    
    if(NULL == key){
        return NULL;
    }

    node = of_find_node_by_path("/chosen");
    
    if (node) {
        
        if (of_property_read_string(node,key, &value) == 0) {
            int len = strlen(value);
            if (len > 0) {
                pr_info("value=%s,len=%d\n",value,len);
                if (len < MAC_ADDR_STR_LEN) {
                    
                    if (strnstr(value, "NULL", 4)) {
                        result = NULL;
                    } else {
                        memset(mac_addr,0,sizeof(mac_addr));
                        ret = str2mac(value,mac_addr);
                        if (0 == ret) {
                            result = mac_addr;
                        } else {
                            pr_err("%s: Failed to parse MAC address, ret=%d\n", __func__,ret);
                        }
                    }
                    
                } else {
                    pr_err("%s: %s node value invalid\n", __func__,key);
                    
                }
            }

        }
        of_node_put(node);
    } else {
        pr_err("%s: Can't find chosen node\n", __func__);
    }

    return result;
}
/*END add at cmd to write eth0 eth1 mac addr, by haowei.cheng*/



/*BEGIN add at cmd to write eth0 eth1 mac addr, by haowei.cheng*/
static int __init mtk_init(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;
	const char *mac_addr;
    

	mac_addr = of_get_mac_address(mac->of_node);
	if (mac_addr)
		ether_addr_copy(dev->dev_addr, mac_addr);

    if (!is_valid_ether_addr(dev->dev_addr)) {
        if(0 == mac->id){
            mac_addr = get_fg360_mac_addr("mac1");
        }else if(1 == mac->id){
            mac_addr = get_fg360_mac_addr("mac0");
        }else{
            dev_err(eth->dev, "error eth id = %d\n",mac->id);
        }
        if (mac_addr){
            ether_addr_copy(dev->dev_addr, mac_addr);
            dev_err(eth->dev, "load MAC address %pM\n",dev->dev_addr);
        }

    }

	/* If the mac address is invalid, use random mac address  */
	if (!is_valid_ether_addr(dev->dev_addr)) {
		eth_hw_addr_random(dev);
		dev_err(eth->dev, "generated random MAC address %pM\n",dev->dev_addr);
	}

	return mtk_phy_connect(dev);
}
/*END add at cmd to write eth0 eth1 mac addr, by haowei.cheng*/

static void mtk_uninit(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_eth *eth = mac->hw;

	phy_disconnect(dev->phydev);
	if (of_phy_is_fixed_link(mac->of_node))
		of_phy_deregister_fixed_link(mac->of_node);
	mtk_tx_irq_disable(eth, ~0);
	mtk_rx_irq_disable(eth, ~0);
}

static int mtk_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		return phy_mii_ioctl(dev->phydev, ifr, cmd);
	default:
		/* default invoke the mtk_eth_dbg handler */
		return mtk_do_priv_ioctl(dev, ifr, cmd);
		break;
	}

	return -EOPNOTSUPP;
}
#if(0)
static void mtk_pending_work(struct work_struct *work)
{
	struct mtk_eth *eth = container_of(work, struct mtk_eth, pending_work);
	int err, i;
	unsigned long restart = 0;

	rtnl_lock();

	dev_dbg(eth->dev, "[%s][%d] reset\n", __func__, __LINE__);

	while (test_and_set_bit_lock(MTK_RESETTING, &eth->state))
		cpu_relax();

	dev_dbg(eth->dev, "[%s][%d] mtk_stop starts\n", __func__, __LINE__);
	/* stop all devices to make sure that dma is properly shut down */
	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->netdev[i])
			continue;
		mtk_stop(eth->netdev[i]);
		__set_bit(i, &restart);
	}
	dev_dbg(eth->dev, "[%s][%d] mtk_stop ends\n", __func__, __LINE__);

	/* restart underlying hardware such as power, clock, pin mux
	 * and the connected phy
	 */
	mtk_hw_deinit(eth);

	if (eth->dev->pins)
		pinctrl_select_state(eth->dev->pins->p,
				     eth->dev->pins->default_state);
	mtk_hw_init(eth);

	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->mac[i] ||
		    of_phy_is_fixed_link(eth->mac[i]->of_node))
			continue;
		err = phy_init_hw(eth->netdev[i]->phydev);
		if (err)
			dev_err(eth->dev, "%s: PHY init failed.\n",
				eth->netdev[i]->name);
	}

	/* restart DMA and enable IRQs */
	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!test_bit(i, &restart))
			continue;
		err = mtk_open(eth->netdev[i]);
		if (err) {
			netif_alert(eth, ifup, eth->netdev[i],
			      "Driver up/down cycle failed, closing device.\n");
			dev_close(eth->netdev[i]);
		}
	}

	dev_dbg(eth->dev, "[%s][%d] reset done\n", __func__, __LINE__);

	clear_bit_unlock(MTK_RESETTING, &eth->state);

	rtnl_unlock();
}
#endif
static int mtk_free_dev(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->netdev[i])
			continue;
		free_netdev(eth->netdev[i]);
	}

	return 0;
}

static int mtk_unreg_dev(struct mtk_eth *eth)
{
	int i;

	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->netdev[i])
			continue;
		unregister_netdev(eth->netdev[i]);
	}

	return 0;
}

static int mtk_cleanup(struct mtk_eth *eth)
{
	mtk_unreg_dev(eth);
	mtk_free_dev(eth);
	cancel_work_sync(&eth->pending_work);

	return 0;
}

static int mtk_get_link_ksettings(struct net_device *ndev,
				  struct ethtool_link_ksettings *cmd)
{
	struct mtk_mac *mac = netdev_priv(ndev);

	if (unlikely(test_bit(MTK_RESETTING, &mac->hw->state)))
		return -EBUSY;

	phy_ethtool_ksettings_get(ndev->phydev, cmd);

	return 0;
}

static int mtk_set_link_ksettings(struct net_device *ndev,
				  const struct ethtool_link_ksettings *cmd)
{
	struct mtk_mac *mac = netdev_priv(ndev);

	if (unlikely(test_bit(MTK_RESETTING, &mac->hw->state)))
		return -EBUSY;

	return phy_ethtool_ksettings_set(ndev->phydev, cmd);
}

static void mtk_get_drvinfo(struct net_device *dev,
			    struct ethtool_drvinfo *info)
{
	struct mtk_mac *mac = netdev_priv(dev);

	strlcpy(info->driver, mac->hw->dev->driver->name, sizeof(info->driver));
	strlcpy(info->bus_info, dev_name(mac->hw->dev), sizeof(info->bus_info));
	info->n_stats = ARRAY_SIZE(mtk_ethtool_stats);
}

static u32 mtk_get_msglevel(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);

	return mac->hw->msg_enable;
}

static void mtk_set_msglevel(struct net_device *dev, u32 value)
{
	struct mtk_mac *mac = netdev_priv(dev);

	mac->hw->msg_enable = value;
}

static int mtk_nway_reset(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);

	if (unlikely(test_bit(MTK_RESETTING, &mac->hw->state)))
		return -EBUSY;

	return genphy_restart_aneg(dev->phydev);
}

static u32 mtk_get_link(struct net_device *dev)
{
	struct mtk_mac *mac = netdev_priv(dev);
	int err;

	if (unlikely(test_bit(MTK_RESETTING, &mac->hw->state)))
		return -EBUSY;

	err = genphy_update_link(dev->phydev);
	if (err)
		return ethtool_op_get_link(dev);

	return dev->phydev->link;
}

static void mtk_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	int i;

	switch (stringset) {
	case ETH_SS_STATS:
		for (i = 0; i < ARRAY_SIZE(mtk_ethtool_stats); i++) {
			memcpy(data, mtk_ethtool_stats[i].str, ETH_GSTRING_LEN);
			data += ETH_GSTRING_LEN;
		}
		break;
	}
}

static int mtk_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(mtk_ethtool_stats);
	default:
		return -EOPNOTSUPP;
	}
}

static void mtk_get_ethtool_stats(struct net_device *dev,
				  struct ethtool_stats *stats, u64 *data)
{
	struct mtk_mac *mac = netdev_priv(dev);
	struct mtk_hw_stats *hwstats = mac->hw_stats;
	u64 *data_src, *data_dst;
	unsigned int start;
	int i;

	if (unlikely(test_bit(MTK_RESETTING, &mac->hw->state)))
		return;

	if (netif_running(dev) && netif_device_present(dev)) {
		if (spin_trylock_bh(&hwstats->stats_lock)) {
			mtk_stats_update_mac(mac);
			spin_unlock_bh(&hwstats->stats_lock);
		}
	}

	data_src = (u64 *)hwstats;

	do {
		data_dst = data;
		start = u64_stats_fetch_begin_irq(&hwstats->syncp);

		for (i = 0; i < ARRAY_SIZE(mtk_ethtool_stats); i++)
			*data_dst++ = *(data_src + mtk_ethtool_stats[i].offset);
	} while (u64_stats_fetch_retry_irq(&hwstats->syncp, start));
}

static int mtk_get_rxnfc(struct net_device *dev, struct ethtool_rxnfc *cmd,
			 u32 *rule_locs)
{
	int ret = -EOPNOTSUPP;

	switch (cmd->cmd) {
	case ETHTOOL_GRXRINGS:
		if (dev->hw_features & NETIF_F_LRO) {
			cmd->data = MTK_MAX_RX_RING_NUM;
			ret = 0;
		}
		break;
	case ETHTOOL_GRXCLSRLCNT:
		if (dev->hw_features & NETIF_F_LRO) {
			struct mtk_mac *mac = netdev_priv(dev);

			cmd->rule_cnt = mac->hwlro_ip_cnt;
			ret = 0;
		}
		break;
	case ETHTOOL_GRXCLSRULE:
		if (dev->hw_features & NETIF_F_LRO)
			ret = mtk_hwlro_get_fdir_entry(dev, cmd);
		break;
	case ETHTOOL_GRXCLSRLALL:
		if (dev->hw_features & NETIF_F_LRO)
			ret = mtk_hwlro_get_fdir_all(dev, cmd,
						     rule_locs);
		break;
	default:
		break;
	}

	return ret;
}

static int mtk_set_rxnfc(struct net_device *dev, struct ethtool_rxnfc *cmd)
{
	int ret = -EOPNOTSUPP;

	switch (cmd->cmd) {
	case ETHTOOL_SRXCLSRLINS:
		if (dev->hw_features & NETIF_F_LRO)
			ret = mtk_hwlro_add_ipaddr(dev, cmd);
		break;
	case ETHTOOL_SRXCLSRLDEL:
		if (dev->hw_features & NETIF_F_LRO)
			ret = mtk_hwlro_del_ipaddr(dev, cmd);
		break;
	default:
		break;
	}

	return ret;
}

static const struct ethtool_ops mtk_ethtool_ops = {
	.get_link_ksettings	= mtk_get_link_ksettings,
	.set_link_ksettings	= mtk_set_link_ksettings,
	.get_drvinfo		= mtk_get_drvinfo,
	.get_msglevel		= mtk_get_msglevel,
	.set_msglevel		= mtk_set_msglevel,
	.nway_reset		= mtk_nway_reset,
	.get_link		= mtk_get_link,
	.get_strings		= mtk_get_strings,
	.get_sset_count		= mtk_get_sset_count,
	.get_ethtool_stats	= mtk_get_ethtool_stats,
	.get_rxnfc		= mtk_get_rxnfc,
	.set_rxnfc              = mtk_set_rxnfc,
};

static const struct net_device_ops mtk_netdev_ops = {
	.ndo_init		= mtk_init,
	.ndo_uninit		= mtk_uninit,
	.ndo_open		= mtk_open,
	.ndo_stop		= mtk_stop,
	.ndo_start_xmit		= mtk_start_xmit,
	.ndo_set_mac_address	= mtk_set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_do_ioctl		= mtk_do_ioctl,
	//.ndo_tx_timeout		= mtk_tx_timeout,
	.ndo_get_stats64        = mtk_get_stats64,
	.ndo_fix_features	= mtk_fix_features,
	.ndo_set_features	= mtk_set_features,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= mtk_poll_controller,
#endif
};

static int mtk_add_mac(struct mtk_eth *eth, struct device_node *np)
{
	struct mtk_mac *mac;
	const __be32 *_id = of_get_property(np, "reg", NULL);
	int id, err;

	if (!_id) {
		dev_err(eth->dev, "missing mac id\n");
		return -EINVAL;
	}

	id = be32_to_cpup(_id);
	if (id >= MTK_MAC_COUNT) {
		dev_err(eth->dev, "%d is not a valid mac id\n", id);
		return -EINVAL;
	}

	if (eth->netdev[id]) {
		dev_err(eth->dev, "duplicate mac id found: %d\n", id);
		return -EINVAL;
	}

	eth->netdev[id] = alloc_etherdev(sizeof(*mac));
	if (!eth->netdev[id]) {
		dev_err(eth->dev, "alloc_etherdev failed\n");
		return -ENOMEM;
	}
	mac = netdev_priv(eth->netdev[id]);
	eth->mac[id] = mac;
	mac->id = id;
	mac->hw = eth;
	mac->of_node = np;

	memset(mac->hwlro_ip, 0, sizeof(mac->hwlro_ip));
	mac->hwlro_ip_cnt = 0;

	mac->hw_stats = devm_kzalloc(eth->dev,
				     sizeof(*mac->hw_stats),
				     GFP_KERNEL);
	if (!mac->hw_stats) {
		dev_err(eth->dev, "failed to allocate counter memory\n");
		err = -ENOMEM;
		goto free_netdev;
	}
	spin_lock_init(&mac->hw_stats->stats_lock);
	u64_stats_init(&mac->hw_stats->syncp);
	mac->hw_stats->reg_offset = id * MTK_STAT_OFFSET;

	SET_NETDEV_DEV(eth->netdev[id], eth->dev);
	eth->netdev[id]->watchdog_timeo = 5 * HZ;
	eth->netdev[id]->netdev_ops = &mtk_netdev_ops;
	eth->netdev[id]->base_addr = (unsigned long)eth->base;

	eth->netdev[id]->hw_features = MTK_HW_FEATURES;
	if (eth->hwlro)
		eth->netdev[id]->hw_features |= NETIF_F_LRO;

	eth->netdev[id]->vlan_features = MTK_HW_FEATURES &
		~(NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_HW_VLAN_CTAG_RX);
	eth->netdev[id]->features |= MTK_HW_FEATURES;
	eth->netdev[id]->ethtool_ops = &mtk_ethtool_ops;

	eth->netdev[id]->irq = eth->irq[0];
	eth->netdev[id]->dev.of_node = np;

	eth->netdev[id]->max_mtu = MTK_MAX_RX_LENGTH - MTK_RX_ETH_HLEN;

	return 0;

free_netdev:
	free_netdev(eth->netdev[id]);
	return err;
}


/*BEGIN Add MDIO access node. add by haowei.cheng*/
static void dump_mii_info(struct mtk_eth *eth,struct fibo_mii_data *mii,char* str)
{
    if(NULL == eth){
        pr_info("### error: eth is NULL\n");
        return;
    }
    dev_info(eth->dev,"\n=============   BEGIN %s   ==============\n",str);
    dev_info(eth->dev,"%s()->%d\n",__func__,__LINE__);
    if(NULL == mii){
        dev_info(eth->dev,"mmi is NULL\n");
        return;
    }
    dev_info(eth->dev,"mii->phy_id=0x%08x\n",mii->phy_id);
    dev_info(eth->dev,"mii->reg_num=0x%08x\n",mii->reg_num);
    dev_info(eth->dev,"mii->val_in=0x%08x\n",mii->val_in);
    dev_info(eth->dev,"mii->val_out=0x%08x\n",mii->val_out);
    dev_info(eth->dev,"mii->port_num=0x%08x\n",mii->port_num);
    dev_info(eth->dev,"mii->dev_addr=0x%08x\n",mii->dev_addr);
    dev_info(eth->dev,"mii->reg_addr=0x%08x\n",mii->reg_addr);
    dev_info(eth->dev,"mii->mode=0x%08x\n",mii->mode);
    dev_info(eth->dev,"mii->debug_flag=0x%08x\n",mii->debug_flag);
    dev_info(eth->dev,"\n=============   END %s   ==============\n",str);
}

struct mtk_eth **get_ethnet_dev(void)
{
    static struct mtk_eth *s_eth = NULL;
    return &s_eth;
}

void set_ethnet_dev(struct mtk_eth *eth)
{
    struct mtk_eth **s_eth = NULL;
    s_eth = get_ethnet_dev();
	*s_eth = eth;
}


static ssize_t mdio_ops_write(struct file *filp,
						const char __user *buf,
						size_t count,
						loff_t *pos)
{
    struct fibo_mii_data mii;
    struct mtk_eth *eth = NULL;
    struct mtk_eth **peth = NULL;
    peth = get_ethnet_dev();
    eth = *peth;

    if (copy_from_user(&mii, buf, sizeof(mii)))
        goto err_copy;

    if(1 == mii.debug_flag){
        dev_info(eth->dev,"%s()->%d,s_eth = %p app[%s]\n",__func__,__LINE__,eth,current->comm);
        dev_info(eth->dev,"%s()->%d,count=%ld\n",__func__,__LINE__,count);
    }
    

    if(45 == mii.mode){
        mutex_lock(&eth->mii_bus->mdio_lock);
        mii.val_out = _mtk_mdio_write_C45(eth, mii.port_num, mii.reg_addr, mii.val_in, mii.dev_addr,mii.debug_flag);
        mutex_unlock(&eth->mii_bus->mdio_lock);
    }else if(22 == mii.mode){
        mii.val_out = mtk_mdio_write(eth->mii_bus,mii.port_num, mii.reg_addr,mii.val_in);
    }else{
        dev_info(eth->dev,"error mii->mode=%d\n",mii.mode);
    }

    if(1 == mii.debug_flag){
        dump_mii_info(eth,&mii,"write");
    }

    return 0;
err_copy:
    dev_info(eth->dev,"%s()->%d,ERROR\n",__func__,__LINE__);
    return -1;

}

static ssize_t mdio_ops_read(struct file *file,char *buf, size_t len,loff_t *offset)
{
    struct fibo_mii_data mii;
    struct mtk_eth *eth = NULL;
    struct mtk_eth **peth = NULL;
    peth = get_ethnet_dev();
    eth = *peth;

    if (copy_from_user(&mii, buf, sizeof(mii)))
    	goto err_copy;

    if(1 == mii.debug_flag){
        dev_info(eth->dev,"%s()->%d,s_eth = %p app[%s]\n",__func__,__LINE__,eth,current->comm);
        dev_info(eth->dev,"%s()->%d,len=%ld\n",__func__,__LINE__,len);
        dump_mii_info(eth,&mii,"read org");
    }
    


    if(45 == mii.mode){
        mutex_lock(&eth->mii_bus->mdio_lock);
        mii.val_out = _mtk_mdio_read_C45(eth, mii.port_num, mii.reg_addr, mii.dev_addr,mii.debug_flag);
        mutex_unlock(&eth->mii_bus->mdio_lock);
    }else if(22 == mii.mode){
        mii.val_out = mtk_mdio_read(eth->mii_bus,mii.port_num, mii.reg_addr);
    }else{
        dev_info(eth->dev,"error mii->mode=%d\n",mii.mode);
    }

    if (copy_to_user(buf, &mii, sizeof(mii)))
        goto err_copy;

    if(1 == mii.debug_flag){
        dump_mii_info(eth,&mii,"read data");
    }
    
    return 0;
	
err_copy:
	dev_info(eth->dev,"%s()->%d,ERROR\n",__func__,__LINE__);
	return -1;


}

atomic_t usage_cnt;

static int mdio_ops_open(struct inode *inode, struct file *filp)
{
    struct mtk_eth *eth = NULL;
    struct mtk_eth **peth = NULL;
    peth = get_ethnet_dev();
    eth = *peth;
    //dev_info(eth->dev,"%s()->%d,s_eth = %p app[%s]\n",__func__,__LINE__,eth,current->comm);
    
    if( 0 == strcmp(current->comm,APP_NAME)){
        dev_info(eth->dev,"app = [%s] pass",current->comm);
        if(atomic_read(&usage_cnt)){
            dev_info(eth->dev,"%s open fail with EBUSY\n",current->comm);
            return -EBUSY;
        }
        atomic_inc(&usage_cnt);
        return 0;
    }
    return -2;
}

static int mdio_ops_close(struct inode *inode, struct file *filp)
{
    struct mtk_eth *eth = NULL;
    struct mtk_eth **peth = NULL;
    peth = get_ethnet_dev();
    eth = *peth;
    dev_info(eth->dev,"%s()->%d,s_eth = %p app[%s]\n",__func__,__LINE__,eth,current->comm);
    atomic_dec(&usage_cnt);
    return 0;
}


static const struct file_operations mdio_pos_fops = {
    .owner      = THIS_MODULE,
    .open       = mdio_ops_open,
    .read       = mdio_ops_read,
    .release    = mdio_ops_close,
    .write      = mdio_ops_write,
};


#define MDIO_DEVICE_NAME			("mdio_ops")


static int creat_mdio_dev_node(struct platform_device *pdev,struct mtk_eth *eth)
{
    int major=0, minor=0;
    struct class *mdio_class = NULL;
    struct device *mdio_device = NULL;

    set_ethnet_dev(eth);
    major = register_chrdev(0, MDIO_DEVICE_NAME, &mdio_pos_fops);
    if(major < 0){
        dev_err(eth->dev,"register_chrdev fail[%d]\n", major);
        return -ENODEV;
    }

    mdio_class = class_create(THIS_MODULE, MDIO_DEVICE_NAME);
    if(IS_ERR(mdio_class)){
        dev_err(eth->dev,"class_create fail [%s]\n", MDIO_DEVICE_NAME);
        unregister_chrdev(major, MDIO_DEVICE_NAME);
        return -ENODEV;
    }

    mdio_device = device_create(mdio_class, NULL, MKDEV(major, minor), NULL, MDIO_DEVICE_NAME);
    if(IS_ERR(mdio_device)){
        dev_err(eth->dev,"device_create fail [%s]\n", MDIO_DEVICE_NAME);
        class_destroy(mdio_class);
        unregister_chrdev(major, MDIO_DEVICE_NAME);
        return -ENODEV;
    }
    return 0;
}
/*END Add MDIO access node. add by haowei.cheng*/


int mtk_ext_int_init(struct mtk_eth *eth) {

	int err;

	INIT_WORK(&eth->link_work, mtk_handle_ext_worker);
	err = devm_request_irq(eth->dev, eth->irq[8], mtk_handle_irq_ext, 0,
		"ext-handler", eth);
	if (err) {
		pr_info("mtk_ext_int_init fail\n");
		return err;
	}
	eth->int_ext = true;
	return 0;
}


static int mtk_probe(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct device_node *mac_np;
	struct mtk_eth *eth;
	struct platform_device *sgmii_pdev;
	int err;
	int i;

	eth = devm_kzalloc(&pdev->dev, sizeof(*eth), GFP_KERNEL);
	if (!eth)
		return -ENOMEM;

	eth->soc = of_device_get_match_data(&pdev->dev);

	eth->dev = &pdev->dev;
	eth->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(eth->base))
		return PTR_ERR(eth->base);

	spin_lock_init(&eth->page_lock);
	spin_lock_init(&eth->tx_irq_lock);
	spin_lock_init(&eth->rx_irq_lock);

	eth->ethsys = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						      "mediatek,ethsys");
	if (IS_ERR(eth->ethsys)) {
		dev_err(&pdev->dev, "no ethsys regmap found\n");
		return PTR_ERR(eth->ethsys);
	}

	eth->wo = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "mediatek,wo");
	if (IS_ERR(eth->wo)) {
		dev_err(&pdev->dev, "no wo regmap found\n");
		return PTR_ERR(eth->wo);
	}
	regmap_write(eth->wo, 0x0070, 0xb);
	regmap_write(eth->wo, 0x0074, 0xb);


	if (MTK_HAS_CAPS(eth->soc->caps, MTK_INFRA)) {
		eth->infra = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
							"mediatek,infracfg");
		if (IS_ERR(eth->infra)) {
			dev_info(&pdev->dev, "no infracfg regmap found\n");
			//return PTR_ERR(eth->infra);
		}
	}

	if (eth->soc->sgmii_pm) {
		eth->dvfsrc_vcore_power = regulator_get(&pdev->dev, "dvfsrc-vcore");
		regulator_set_voltage(eth->dvfsrc_vcore_power, SGMII_VCORE_OPS, INT_MAX);

		eth->sgmii_node0 = of_find_compatible_node(NULL, NULL,
						"mediatek,colgin-sgmiisys_0");

		if (IS_ERR(eth->sgmii_node0)) {
			dev_err(&pdev->dev, "no eth->sgmii_node0 regmap found\n");
			return PTR_ERR(eth->sgmii_node0);
		}
		sgmii_pdev = of_find_device_by_node(eth->sgmii_node0);
		eth->sgmii_dev0 = &sgmii_pdev->dev;

		eth->sgmii_node1 = of_find_compatible_node(NULL, NULL,
						"mediatek,colgin-sgmiisys_1");
		sgmii_pdev = of_find_device_by_node(eth->sgmii_node1);
		eth->sgmii_dev1 = &sgmii_pdev->dev;

		if (IS_ERR(eth->sgmii_node1)) {
			dev_err(&pdev->dev, "no eth->sgmii_node1 regmap found\n");
			return PTR_ERR(eth->sgmii_node1);
		}

		eth->sgmii_phy_node0 = of_find_compatible_node(NULL, NULL,
						"mediatek,colgin-sgmiisys_phy_0");
		sgmii_pdev = of_find_device_by_node(eth->sgmii_phy_node0);
		eth->sgmii_phy_dev0 = &sgmii_pdev->dev;
		if (IS_ERR(eth->sgmii_phy_node0)) {
			dev_err(&pdev->dev, "no eth->sgmii_phy_node0 regmap found\n");
			return PTR_ERR(eth->sgmii_phy_node0);
		}


		eth->sgmii_phy_node1 = of_find_compatible_node(NULL, NULL,
						"mediatek,colgin-sgmiisys_phy_1");
		sgmii_pdev = of_find_device_by_node(eth->sgmii_phy_node1);
		eth->sgmii_phy_dev1 = &sgmii_pdev->dev;
		if (IS_ERR(eth->sgmii_phy_node1)) {
			dev_err(&pdev->dev, "no eth->sgmii_phy_node1 regmap found\n");
			return PTR_ERR(eth->sgmii_phy_node1);
		}
	}

	if (eth->soc->pinctrl) {
		eth_gpio = devm_pinctrl_get(&pdev->dev);
		if (!IS_ERR(eth_gpio)) {
			eth_smi_mdio_pinctl = pinctrl_lookup_state(eth_gpio, "eth_smi_mdio_pinctl");
			if (!IS_ERR(eth_smi_mdio_pinctl))
				pinctrl_select_state(eth_gpio, eth_smi_mdio_pinctl);
			else
				dev_info(&pdev->dev, "[ETH][ERROR] Cannot ETH_MDIO!\n");

			eth_smi_mdc_pinctl = pinctrl_lookup_state(eth_gpio, "eth_smi_mdc_pinctl");
			if (!IS_ERR(eth_smi_mdc_pinctl))
				pinctrl_select_state(eth_gpio, eth_smi_mdc_pinctl);
			else
				dev_info(&pdev->dev, "[ETH][ERROR] Cannot ETH_MDC!\n");

		} else {
			dev_info(&pdev->dev, "[ETH][ERROR] Cannot find eth_gpio!\n");

		}
	}

	if (MTK_HAS_CAPS(eth->soc->caps, MTK_SGMII)) {
		eth->sgmii = devm_kzalloc(eth->dev, sizeof(*eth->sgmii),
								GFP_KERNEL);
		
		if (!eth->sgmii)
			return -ENOMEM;
		/*BEGIN automatically adjust sgmii mode according to UTP rate,add by haowei.cheng*/
		eth->sgmii->sgmii_link_adjust = mtk_sgmii_mode_adjust;
		/*END automatically adjust sgmii mode according to UTP rate,add by haowei.cheng*/
		err = mtk_sgmii_init(eth->sgmii, pdev->dev.of_node,
							eth->soc->ana_rgc3);

		if (err)
			return err;
	}

	if (eth->soc->required_pctl) {
		eth->pctl = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
							    "mediatek,pctl");
		if (IS_ERR(eth->pctl)) {
			dev_err(&pdev->dev, "no pctl regmap found\n");
			return PTR_ERR(eth->pctl);
		}
	}

	for (i = 0; i < eth->soc->irq_num; i++) {
		eth->irq[i] = platform_get_irq(pdev, i);
		if (eth->irq[i] < 0) {
			dev_err(&pdev->dev, "no IRQ%d resource found\n", i);
			return -ENXIO;
		}
	}

	if (!(MTK_HAS_CAPS(eth->soc->caps, MTK_FPGA_CLK))) {
		for (i = 0; i < ARRAY_SIZE(eth->clks); i++) {
			eth->clks[i] = NULL;
			if (eth->soc->required_clks & BIT(i)) {
				//pr_notice("!!! mtk_clks_source_name[%d] = %s\n", i, mtk_clks_source_name[i]);
				eth->clks[i] = devm_clk_get(eth->dev, mtk_clks_source_name[i]);
				if (IS_ERR(eth->clks[i])) {
					dev_err(&pdev->dev, "clock %s not found\n",
							    mtk_clks_source_name[i]);
					/*tmp mark*/
					//return -EINVAL;
				}
			}
		}
	}

	eth->msg_enable = netif_msg_init(mtk_msg_level, MTK_DEFAULT_MSG_ENABLE);
	//INIT_WORK(&eth->pending_work, mtk_pending_work);
	eth->hwlro = MTK_HAS_CAPS(eth->soc->caps, MTK_HWLRO);
	eth->hwrss = MTK_HAS_CAPS(eth->soc->caps, MTK_HWRSS);
	eth->hwedmarx = MTK_HAS_CAPS(eth->soc->caps, MTK_EDMA_RX);

	err = mtk_hw_init(eth);
	if (err)
		return err;

	/*BEGIN auto scan fg360 phy device add by haowei.cheng*/
	auto_scan_phy(eth);

	for_each_child_of_node(pdev->dev.of_node, mac_np) {
		if (!of_device_is_compatible(mac_np, "mediatek,eth-mac"))
			continue;

		if(!fg360_phy_find_exist(mac_np)){
			continue;
		}
		
		if (!of_device_is_available(mac_np))
			continue;

		err = mtk_add_mac(eth, mac_np);
		if (err)
			goto err_deinit_hw;
	}
	/*If no physical PHY is matched, the default virtual PHY is used*/
	
	for_each_child_of_node(pdev->dev.of_node, mac_np) {
		int id;
		if (!of_device_is_compatible(mac_np, "mediatek,eth-mac_def"))
			continue;
		if (!of_device_is_available(mac_np))
			continue;

		id = get_node_mac_id(eth,mac_np);
		
		if(!get_gmac_phy_status(id)){	
			pr_info("id=%d not found\n",id);
			err = mtk_add_mac(eth, mac_np);
			if (err)
				goto err_deinit_hw;
		}
	}
	/*END auto scan fg360 phy device add by haowei.cheng*/

	err = devm_request_irq(eth->dev, eth->irq[1], mtk_handle_irq_tx, IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
			       "eth-tx", eth);

	if(eth->hwrss) {
		err = devm_request_irq(eth->dev, eth->irq[2], mtk_handle_irq_rx0,
				       IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
				       "eth-rx0", eth);
		err = devm_request_irq(eth->dev, eth->irq[3], mtk_handle_irq_rx1,
				       IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
				       "eth-rx1", eth);
		err = devm_request_irq(eth->dev, eth->irq[4], mtk_handle_irq_rx2,
				       IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
				       "eth-rx2", eth);
		err = devm_request_irq(eth->dev, eth->irq[5], mtk_handle_irq_rx3,
				       IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
				       "eth-rx3", eth);
	} else {
		err = devm_request_irq(eth->dev, eth->irq[2], mtk_handle_irq_rx,
				       IRQF_NO_SUSPEND | IRQF_TRIGGER_NONE,
				       "eth-rx", eth);
	}

	if(eth->hwedmarx) {
		err = mtk_edma_request_irq(eth);
	}

	if (err)
		goto err_free_dev;

	/*Notice: it is for AQR112C Flow Control handle, Internal default disable*/
/*	if (mtk_ext_int_init(eth))
		goto err_free_dev;
*/
	err = mtk_mdio_init(eth);

	if (err)
		goto err_free_dev;

	/*BEGIN Add MDIO access node. add by haowei.cheng*/
	err = creat_mdio_dev_node(pdev,eth);
	if (err){
		dev_err(eth->dev, "creat_mdio_dev_node fail %d\n",err);
	}
	/*END Add MDIO access node. add by haowei.cheng*/


	for (i = (MTK_MAX_DEVS-1); i >= 0; i--) {
		if (!eth->netdev[i])
			continue;
		err = register_netdev(eth->netdev[i]);

		if (err) {
			dev_err(eth->dev, "error bringing up device\n");
			goto err_deinit_mdio;
		} else
			netif_info(eth, probe, eth->netdev[i],
				   "mediatek frame engine at 0x%08lx, irq %d\n",
				   eth->netdev[i]->base_addr, eth->irq[0]);
	}

	/* we run 2 devices on the same DMA ring so we need a dummy device
	 * for NAPI to work
	 */
	init_dummy_netdev(&eth->dummy_dev);

	netif_napi_add(&eth->dummy_dev, &eth->tx_napi, mtk_napi_tx,
		       MTK_NAPI_WEIGHT);
	if(eth->hwrss) {
		netif_napi_add(&eth->dummy_dev, &eth->rx_napi0, mtk_napi_rx0,
			       MTK_NAPI_WEIGHT);
		netif_napi_add(&eth->dummy_dev, &eth->rx_napi1, mtk_napi_rx1,
			       MTK_NAPI_WEIGHT);
		netif_napi_add(&eth->dummy_dev, &eth->rx_napi2, mtk_napi_rx2,
			       MTK_NAPI_WEIGHT);
		netif_napi_add(&eth->dummy_dev, &eth->rx_napi3, mtk_napi_rx3,
			       MTK_NAPI_WEIGHT);
	} else {
		netif_napi_add(&eth->dummy_dev, &eth->rx_napi, mtk_napi_rx,
			       MTK_NAPI_WEIGHT);
	}

	if(eth->hwedmarx) {
		netif_napi_add_edma(eth);
	}

	mtketh_debugfs_init(eth);
	debug_proc_init(eth);
	platform_set_drvdata(pdev, eth);

	return 0;

err_deinit_mdio:
	mtk_mdio_cleanup(eth);
err_free_dev:
	mtk_free_dev(eth);
err_deinit_hw:
	mtk_hw_deinit(eth);

	return err;
}

static int mtk_remove(struct platform_device *pdev)
{
	struct mtk_eth *eth = platform_get_drvdata(pdev);
	int i;

	/* stop all devices to make sure that dma is properly shut down */
	for (i = 0; i < MTK_MAC_COUNT; i++) {
		if (!eth->netdev[i])
			continue;
		mtk_stop(eth->netdev[i]);
	}

	mtk_hw_deinit(eth);

	debug_proc_exit();
	mtketh_debugfs_exit(eth);

	netif_napi_del(&eth->tx_napi);
	if (eth->hwrss) {
		netif_napi_del(&eth->rx_napi0);
		netif_napi_del(&eth->rx_napi1);
		netif_napi_del(&eth->rx_napi2);
		netif_napi_del(&eth->rx_napi3);
	} else {
		netif_napi_del(&eth->rx_napi);
	}
	mtk_cleanup(eth);
	mtk_mdio_cleanup(eth);

	return 0;
}

int netsys_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct mtk_eth *eth;
	u32 val;
	int ret;
	static int enable_hw_idle = 0;

	if (pdev == NULL) {
		////pr_notice("%s pdev == NULL\n", __func__);
		return -1;
	}

	eth = platform_get_drvdata(pdev);
        netif_stop_queue(eth->netdev[0]);
        netif_stop_queue(eth->netdev[1]);
	phy_stop(eth->netdev[0]->phydev);
	phy_stop(eth->netdev[1]->phydev);
#if 0
	regmap_update_bits(eth->ethsys, MTK_PPE_TB_CFG,
			   SCAN_MODE_MASK,
			   0x0 <<  16);
	regmap_update_bits(eth->ethsys, MTK_PPE1_TB_CFG,
			   SCAN_MODE_MASK,
			   0x0 <<  16);
#endif
	mtk_w32(eth, 0x7fb5, MTK_PPE_TB_CFG);
	mtk_w32(eth, 0x7fb5, MTK_PPE1_TB_CFG);
	regulator_set_voltage(eth->dvfsrc_vcore_power, SGMII_VCORE_NON_OPS, INT_MAX);
	mtk_gdma_config(eth, MTK_GDMA_DROP_ALL);
	mtk_tx_irq_disable(eth, MTK_TX_DONE_INT);
	if (eth->hwrss) {
		mtk_rx_irq_disable0(eth, MTK_RX_DONE_DLY);
		mtk_rx_irq_disable1(eth, MTK_RX_DONE_DLY1);
		mtk_rx_irq_disable2(eth, MTK_RX_DONE_DLY2);
		mtk_rx_irq_disable3(eth, MTK_RX_DONE_DLY3);
	} else {
		mtk_rx_irq_disable(eth, MTK_RX_DONE_INT);
	}

	if (eth->int_ext) {
		disable_irq_nosync(eth->irq[8]);
	}

	if(eth->hwedmarx) {
		mtk_rx_irq_disable_edma_all(eth);
		mtk_stop_dma_edma(eth, MTK_EDMA0_GLO_CFG);
		mtk_stop_dma_edma(eth, MTK_EDMA1_GLO_CFG);
	}

	//napi_disable(&eth->tx_napi);
	//napi_disable(&eth->rx_napi);
	/*disable ADMA*/
	mtk_w32(eth, 0x00200080, MTK_PDMA_LRO_CTRL_DW0);
	mtk_w32(eth, 0x80001c00, MTK_PDMA_GLO_CFG);
	mtk_w32(eth, 0x810001b0, 0);
	/*check ADMA IDLE*/
	if (enable_hw_idle == 0) {
		regmap_write(eth->ethsys, 0xe8, 0x00008202);
		regmap_write(eth->ethsys, 0xe4, 0x7c000000);
		regmap_write(eth->ethsys, 0x10, 0xb6000000);
		regmap_write(eth->ethsys, 0x58, 0x20000000);
		regmap_write(eth->ethsys, 0xf0, 0x007f0000);
		regmap_write(eth->ethsys, 0xfc, 0x00001080);
		enable_hw_idle = 1;
	}
	ret = regmap_read(eth->ethsys, 0x54, &val);
	if (ret)
		return ret;

	pr_notice("0x15000054 is 0x%x\n", val);
	val = val & 0x0C000002;
	if (val == 0x0C000002) {
		mtk_w32(eth, 0x05f2c040, 0x98c);
		msleep(1);
		mtk_w32(eth, 0x05f28040, 0x98c);
		regmap_read(eth->ethsys, 0x54, &val);
		pr_notice("NETSYS hw idle status: 0x%x\n", val);
		msleep(3);
	} else
		pr_notice("ADMA related bus not idle!!!!\n");
	mtk_clk_disable(eth);
	pr_notice("netsys suspend_noirq done\n");

	return 0;
}

int netsys_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct device_node *np;
	struct mtk_eth *eth;
	int err;
	const char *str;

//	if (pdev == NULL) {
		////pr_notice("%s pdev == NULL\n", __func__);
//		return -1;
//	}

	eth = platform_get_drvdata(pdev);
#if 0
	regmap_update_bits(eth->ethsys, MTK_PPE_TB_CFG,
			   SCAN_MODE_MASK,
			   SCAN_MODE);
	regmap_update_bits(eth->ethsys, MTK_PPE1_TB_CFG,
			   SCAN_MODE_MASK,
			   SCAN_MODE);
#endif
	mtk_clk_enable(eth);
	mtk_w32(eth, 0x27fb5, MTK_PPE_TB_CFG);
	mtk_w32(eth, 0x27fb5, MTK_PPE1_TB_CFG);

	np = of_parse_phandle(pdev->dev.of_node, "mediatek,sgmiisys", 0);
	err = of_property_read_string(np, "mediatek,physpeed", &str);
	if (err)
		return err;

	if (refcount_read(&eth->dma_refcnt)) {
		if (!strcmp(str, "auto"))
			regulator_set_voltage(eth->dvfsrc_vcore_power, SGMII_VCORE_1G, INT_MAX);
		 else
			regulator_set_voltage(eth->dvfsrc_vcore_power, SGMII_VCORE_OPS, INT_MAX);
	}
	//mtk_clk_enable(eth);
	mtk_sgmii_setup_mode_force(eth->sgmii, 0, eth);
	mtk_sgmii_setup_mode_force(eth->sgmii, 1, eth);
	//napi_enable(&eth->tx_napi);
	//napi_enable(&eth->rx_napi);
	mtk_tx_irq_enable(eth, MTK_TX_DONE_INT);

	if (eth->hwrss) {
		mtk_rx_irq_enable0(eth, MTK_RX_DONE_DLY);
		mtk_rx_irq_enable1(eth, MTK_RX_DONE_DLY1);
		mtk_rx_irq_enable2(eth, MTK_RX_DONE_DLY2);
		mtk_rx_irq_enable3(eth, MTK_RX_DONE_DLY3);
	} else {
		mtk_rx_irq_enable(eth, MTK_RX_DONE_INT);
	}

	if(eth->hwedmarx) {
		mtk_rx_irq_enable_edma_all(eth);
		mtk_start_dma_edma(eth);
	}

	if (eth->int_ext) {
		enable_irq(eth->irq[8]);
	}

	mtk_gdma_config(eth, MTK_GDMA_ICS_EN | MTK_GDMA_TCS_EN | MTK_GDMA_UCS_EN);
	mtk_w32(eth, 0x810000b0, 0);
	//TBD check harry
	if (eth->hwrss)
		mtk_w32(eth, 0x002000a4, MTK_PDMA_LRO_CTRL_DW0);
	else
		mtk_w32(eth, 0x00200081, MTK_PDMA_LRO_CTRL_DW0);
	mtk_w32(eth, 0x80001c04, MTK_PDMA_GLO_CFG);
	phy_start(eth->netdev[0]->phydev);
	phy_start(eth->netdev[1]->phydev);
	netif_start_queue(eth->netdev[0]);
	netif_start_queue(eth->netdev[1]);
	//pr_notice("netsys resume_noirq done\n");

	return 0;
}
static const struct dev_pm_ops netsys_pm_ops = {
	.suspend_noirq = netsys_pm_suspend,
	.resume_noirq = netsys_pm_resume,
};

static const struct mtk_soc_data mt2701_data = {
	.caps = MT7623_CAPS | MTK_HWLRO,
	.required_clks = MT7623_CLKS_BITMAP,
	.required_pctl = true,
};

static const struct mtk_soc_data mt7622_data = {
	.ana_rgc3 = 0x2028,
	.caps = MT7622_CAPS | MTK_HWLRO,
	.required_clks = MT7622_CLKS_BITMAP,
	.required_pctl = false,
};

static const struct mtk_soc_data mt7623_data = {
	.caps = MT7623_CAPS | MTK_HWLRO,
	.required_clks = MT7623_CLKS_BITMAP,
	.required_pctl = true,
};

static const struct mtk_soc_data mt7629_data = {
	.ana_rgc3 = 0x128,
	.caps = LEOPARD_CAPS | MTK_HWLRO,
	.required_clks = LEOPARD_CLKS_BITMAP,
	.required_pctl = false,
};

static const struct mtk_soc_data fpga_data = {
	.irq_num = 3,
	.caps = COLGIN_FPGA_CAPS | MTK_HWLRO,
	.required_pctl = false,
};

static const struct mtk_soc_data mt6890_data = {
	.ana_rgc3 = 0x128,
	.irq_num = 9,
#if defined(CONFIG_EDMA_RX)
	.caps = MT6890_CAPS | MTK_HWLRO | MTK_EDMA_RX,
#else
	.caps = MT6890_CAPS | MTK_HWLRO,
#endif
//	.caps = MT6890_CAPS | MTK_HWRSS,
	.required_clks = MT6890_CLKS_BITMAP,
	.required_pctl = false,
	.pinctrl = true,
	.sgmii_pm = true,
};

const struct of_device_id of_mtk_match[] = {
	{ .compatible = "mediatek,mt2701-eth", .data = &mt2701_data},
	{ .compatible = "mediatek,mt7622-eth", .data = &mt7622_data},
	{ .compatible = "mediatek,mt7623-eth", .data = &mt7623_data},
	{ .compatible = "mediatek,mt7629-eth", .data = &mt7629_data},
	{ .compatible = "mediatek,mt6890-eth", .data = &mt6890_data},
	{ .compatible = "mediatek,fpga-eth", .data = &fpga_data},
	{},
};

MODULE_DEVICE_TABLE(of, of_mtk_match);

static struct platform_driver mtk_driver = {
	.probe = mtk_probe,
	.remove = mtk_remove,
	.driver = {
		.name = "mtk_soc_eth",
		.pm = &netsys_pm_ops,
		.of_match_table = of_mtk_match,
	},
};

module_platform_driver(mtk_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Crispin <blogic@openwrt.org>");
MODULE_DESCRIPTION("Ethernet driver for MediaTek SoC");
