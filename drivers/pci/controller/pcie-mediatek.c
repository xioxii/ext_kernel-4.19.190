// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek PCIe host controller driver.
 *
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Ryder Lee <ryder.lee@mediatek.com>
 *	   Honghui Zhang <honghui.zhang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/msi.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_clk.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include "../pci.h"

/* PCIe shared registers */
#define PCIE_SYS_CFG		0x00
#define PCIE_INT_ENABLE		0x0c
#define PCIE_CFG_ADDR		0x20
#define PCIE_CFG_DATA		0x24

/* PCIe per port registers */
#define PCIE_BAR0_SETUP		0x10
#define PCIE_CLASS		0x34
#define PCIE_LINK_STATUS	0x50

#define PCIE_PORT_INT_EN(x)	BIT(20 + (x))
#define PCIE_PORT_PERST(x)	BIT(1 + (x))
#define PCIE_PORT_LINKUP	BIT(0)
#define PCIE_BAR_MAP_MAX	GENMASK(31, 16)

#define PCIE_BAR_ENABLE		BIT(0)
#define PCIE_REVISION_ID	BIT(0)
#define PCIE_CLASS_CODE		(0x60400 << 8)
#define PCIE_CONF_REG(regn)	(((regn) & GENMASK(7, 2)) | \
				((((regn) >> 8) & GENMASK(3, 0)) << 24))
#define PCIE_CONF_FUN(fun)	(((fun) << 8) & GENMASK(10, 8))
#define PCIE_CONF_DEV(dev)	(((dev) << 11) & GENMASK(15, 11))
#define PCIE_CONF_BUS(bus)	(((bus) << 16) & GENMASK(23, 16))
#define PCIE_CONF_ADDR(regn, fun, dev, bus) \
	(PCIE_CONF_REG(regn) | PCIE_CONF_FUN(fun) | \
	 PCIE_CONF_DEV(dev) | PCIE_CONF_BUS(bus))

/* MediaTek specific configuration registers */
#define PCIE_FTS_NUM		0x70c
#define PCIE_FTS_NUM_MASK	GENMASK(15, 8)
#define PCIE_FTS_NUM_L0(x)	((x) & 0xff << 8)

#define PCIE_FC_CREDIT		0x73c
#define PCIE_FC_CREDIT_MASK	(GENMASK(31, 31) | GENMASK(28, 16))
#define PCIE_FC_CREDIT_VAL(x)	((x) << 16)

/* PCIe V2 share registers */
#define PCIE_SYS_CFG_V2		0x0
#define PCIE_CSR_LTSSM_EN(x)	BIT(0 + (x) * 8)
#define PCIE_CSR_ASPM_L1_EN(x)	BIT(1 + (x) * 8)

/* PCIe V2 per-port registers */
#define PCIE_MSI_VECTOR		0x0c0

#define PCIE_CONF_VEND_ID	0x100
#define PCIE_CONF_DEVICE_ID	0x102
#define PCIE_CONF_CLASS_ID	0x106

#define PCIE_INT_MASK		0x420
#define INTX_MASK		GENMASK(19, 16)
#define INTX_SHIFT		16
#define INTX_NUM		4
#define PCIE_INT_STATUS		0x424
#define MSI_STATUS		BIT(23)
#define PCIE_IMSI_STATUS	0x42c
#define PCIE_IMSI_ADDR		0x430
#define MSI_MASK		BIT(23)
#define MTK_MSI_IRQS_NUM	32

#define PCIE_AHB_TRANS_BASE0_L	0x438
#define PCIE_AHB_TRANS_BASE0_H	0x43c
#define AHB2PCIE_SIZE(x)	((x) & GENMASK(4, 0))
#define PCIE_AXI_WINDOW0	0x448
#define WIN_ENABLE		BIT(7)

/* PCIe V2 configuration transaction header */
#define PCIE_CFG_HEADER0	0x460
#define PCIE_CFG_HEADER1	0x464
#define PCIE_CFG_HEADER2	0x468
#define PCIE_CFG_WDATA		0x470
#define PCIE_APP_TLP_REQ	0x488
#define PCIE_CFG_RDATA		0x48c
#define APP_CFG_REQ		BIT(0)
#define APP_CPL_STATUS		GENMASK(7, 5)

#define CFG_WRRD_TYPE_0		4
#define CFG_WR_FMT		2
#define CFG_RD_FMT		0

#define CFG_DW0_LENGTH(length)	((length) & GENMASK(9, 0))
#define CFG_DW0_TYPE(type)	(((type) << 24) & GENMASK(28, 24))
#define CFG_DW0_FMT(fmt)	(((fmt) << 29) & GENMASK(31, 29))
#define CFG_DW2_REGN(regn)	((regn) & GENMASK(11, 2))
#define CFG_DW2_FUN(fun)	(((fun) << 16) & GENMASK(18, 16))
#define CFG_DW2_DEV(dev)	(((dev) << 19) & GENMASK(23, 19))
#define CFG_DW2_BUS(bus)	(((bus) << 24) & GENMASK(31, 24))
#define CFG_HEADER_DW0(type, fmt) \
	(CFG_DW0_LENGTH(1) | CFG_DW0_TYPE(type) | CFG_DW0_FMT(fmt))
#define CFG_HEADER_DW1(where, size) \
	(GENMASK(((size) - 1), 0) << ((where) & 0x3))
#define CFG_HEADER_DW2(regn, fun, dev, bus) \
	(CFG_DW2_REGN(regn) | CFG_DW2_FUN(fun) | \
	CFG_DW2_DEV(dev) | CFG_DW2_BUS(bus))

#define PCIE_RST_CTRL		0x510
#define PCIE_PHY_RSTB		BIT(0)
#define PCIE_PIPE_SRSTB		BIT(1)
#define PCIE_MAC_SRSTB		BIT(2)
#define PCIE_CRSTB		BIT(3)
#define PCIE_PERSTB		BIT(8)
#define PCIE_LINKDOWN_RST_EN	GENMASK(15, 13)
#define PCIE_LINK_STATUS_V2	0x804
#define PCIE_PORT_LINKUP_V2	BIT(10)


/* PCIe V3 CfgWr/CfgRd registers */
#define PCIE_CFGNUM		0x140
#define CFG_DEVFN(devfn)        ((devfn) & GENMASK(7, 0))
#define CFG_BUS(busno)          (((busno) << 8) & GENMASK(15, 8))
#define CFG_BYTE_EN             GENMASK(19, 16)
#define CFG_FORCE_BYTE_EN       0

#define CFG_HEADER(devfn, busno)        \
	(CFG_DEVFN(devfn) | CFG_BUS(busno) | CFG_BYTE_EN | CFG_FORCE_BYTE_EN)

/* PCI Interrupt registers */
#define PCIE_INT_MASK_V3	0x180
#define MSI_MASK_V3		BIT(8)
#define L2_ENTRY_WAKE_MASK_V3	BIT(23)
#define INTA_MASK_V3		BIT(24)
#define INTB_MASK_V3		BIT(25)
#define INTC_MASK_V3		BIT(26)
#define	INTD_MASK_V3		BIT(27)
#define INTX_MASK_V3		(INTA_MASK_V3 | \
				INTB_MASK_V3 | INTC_MASK_V3 | INTD_MASK_V3)
#define MTK_PCIE_INTX_SHIFT_V3	24
#define LTR_HP_EVENT_MASK_V3	BIT(28)
#define PM_EVENT_MASK_V3	BIT(30)

#define PCIE_INT_STATUS_V3	0x184
#define MSI_STATUS_V3		BIT(8)
#define L2_ENTRY_WAKE_STATUS_V3	BIT(23)
#define INTA_STATUS_V3		BIT(24)
#define INTB_STATUS_V3		BIT(25)
#define INTC_STATUS_V3		BIT(26)
#define INTD_STATUS_V3		BIT(27)
#define LTR_HP_EVENT_STATUS_V3	BIT(28)
#define AER_EVENT_STATUS_V3	BIT(29)
#define PM_EVENT_STATUS_V3	BIT(30)

/* PCI settings */
#define PCIE_IF_TIMEOUT		0x344
#define PTX_TIMEOUT_DISABLE	BIT(7)
#define PCIE_MISC_CTRL          0x348
#define PCIE_SETTING		0x80
#define PCIE_RC_MODE		BIT(0)

/* PCI MAC registers */
#define PCIE_IDS2		0x9c
#define PCI_CLASS(class)	(class << 8)
#define PCIE_IREG_PEX_SPC	0xd4
#define SLOT_REG_IMPL		BIT(12)
#define PCIE_PEX_LINK		0xc8
#define ASPM_L1_TIMER_RECOUNT	BIT(21)
#define PCIE_RST_CTRL_V3	0x148
#define PCIE_MAC_RSTB_V3	BIT(0)
#define PCIE_PHY_RSTB_V3	BIT(1)
#define PCIE_BRG_RSTB_V3	BIT(2)
#define PCIE_PE_RSTB_V3		BIT(3)

#define PCIE_ICMD_PM		0x198
#define Turn_Off_Link		BIT(4)

#define PCIE_LINK_STATUS_V3	0x150
#define PCIE_PORT_LINKUP_V3	BIT(4)

#define PCIE_DATA_LINK_STATUS_V3	0x154
#define PCIE_DATA_LINKUP_V3	BIT(8)

#define PCIE_INT_STATUS_V3      0x184
#define MSI_GRP_STATUS(x)       BIT(8 + (x))

#define PCIE_MSI_GRP_EN		0x190
#define MSI_GRP_ENABLE(x)       BIT(x)

#define PCIE_MSI_ADDR_GRP(x)    (0xC00 + 0x10 * (x))
#define PCIE_MSI_STATUS_GRP(x)  (0xC04 + 0x10 * (x))
#define PCIE_MSI_ENABLE_GRP(x)  (0xC08 + 0x10 * (x))

#define MSI_ENABLE		0x190
#define MSI_VECTOR		0xC00
#define MSI_VECTOR_MASK		(~0x3fff)
#define IMSI_STATUS		0xC04
#define MSI_INT_MASK		0xC08
#define MSI_IRQS		32
#define PCIE_PORT_MSI_BIT	32
#define MAX_MSI_IRQS		(MSI_IRQS + 1)
#define INTX_IRQ_NUM		5

#define AXI_SLV0_T0_BASE	0x800
#define AXI_SLV0_T0_PAR_SRC_LSB	(AXI_SLV0_T0_BASE + 0x00)
#define AXI_SLV0_T0_SRC_MSB	(AXI_SLV0_T0_BASE + 0x04)
#define AXI_SLV0_T0_TRSL_LSB	(AXI_SLV0_T0_BASE + 0x08)
#define AXI_SLV0_T0_TRSL_MSB	(AXI_SLV0_T0_BASE + 0x0c)
#define AXI_SLV0_T0_TRSL_PAR	(AXI_SLV0_T0_BASE + 0x10)
#define ATR_IMPL		BIT(0)

#define AXI_SLV0_T1_BASE	0x820
#define AXI_SLV0_T1_PAR_SRC_LSB	(AXI_SLV0_T1_BASE + 0x00)
#define AXI_SLV0_T1_SRC_MSB	(AXI_SLV0_T1_BASE + 0x04)
#define AXI_SLV0_T1_TRSL_LSB	(AXI_SLV0_T1_BASE + 0x08)
#define AXI_SLV0_T1_TRSL_MSB	(AXI_SLV0_T1_BASE + 0x0c)
#define AXI_SLV0_T1_TRSL_PAR	(AXI_SLV0_T1_BASE + 0x10)

#define ATR_SIZE(size)		((size & 0x3f) << 1)
#define ATR_SRC_ADDR_L(base)	(base & GENMASK(31, 12))
#define ATR_ID(id)		(id & 0xf)
#define ATR_PARAM(param)	((param & 0xfff) << 16)

#define CFG_OFFSET_ADDR                 0x1000

#define PCI_VENDOR_ID_MEDIATEK	0x14c3

struct mtk_pcie_port;

/**
 * struct mtk_pcie_irq_info - interrupts related register information
 * @int_status: interrupt status register
 * @int_mask: interrupt mask register
 * @msi_status: MSI status register
 * @msi_addr: MSI address register
 * @intx_shift: INTx offset bit on interrupt status register
 * @msi_mask_bit: MSI mask bit on interrupt mask register
 * @intx_mask_bit: INTx mask bit on interrupt mask register
 * @enable_msi_group: pointer to MSI group select functions
 */
struct mtk_pcie_irq_info {
	u32 int_status;
	u32 int_mask;
	u32 msi_status;
	u32 msi_addr;
	int intx_shift;
	int msi_mask_bit;
	int intx_mask_bit;
	void (*enable_msi_group)(struct mtk_pcie_port *port, int group);
};

/**
 * struct mtk_pcie_soc - differentiate between host generations
 * @need_fix_class_id: whether this host's class ID needed to be fixed or not
 * @pm_support: whether the host's MTCMOS will be off when suspend
 * @need_fix_device_id: whether this host's device ID needed to be fixed or not
 * @device_id: device ID which this host need to be fixed
 * @ops: pointer to configuration access functions
 * @startup: pointer to controller setting functions
 * @setup_irq: pointer to initialize IRQ functions
 */
struct mtk_pcie_soc {
	bool need_fix_class_id;
	bool pm_support;
	bool need_fix_device_id;
	unsigned int device_id;
	struct pci_ops *ops;
	int (*startup)(struct mtk_pcie_port *port);
	int (*setup_irq)(struct mtk_pcie_port *port, struct device_node *node);
	struct mtk_pcie_irq_info *irq_info;
};

/**
 * struct mtk_pcie_port - PCIe port information
 * @base: IO mapped register base
 * @list: port list
 * @pcie: pointer to PCIe host info
 * @reset: pointer to port reset control
 * @clks: pointer to PCIe clocks
 * @num_clks: PCIe clock count
 * @phy: pointer to PHY control block
 * @lane: lane count
 * @slot: port slot
 * @irq_domain: legacy INTx IRQ domain
 * @inner_domain: inner IRQ domain
 * @msi_domain: MSI IRQ domain
 * @lock: protect the msi_irq_in_use bitmap
 * @msi_irq_in_use: bit map for assigned MSI IRQ
 */
struct mtk_pcie_port {
	void __iomem *base;
	struct list_head list;
	struct mtk_pcie *pcie;
	struct reset_control *reset;
	struct clk **clks;
	int num_clks;
	struct phy *phy;
	u32 lane;
	u32 slot;
	int irq;
	struct irq_domain *irq_domain;
	struct irq_domain *inner_domain;
	struct irq_domain *msi_domain;
	struct mutex lock;
	DECLARE_BITMAP(msi_irq_in_use, MTK_MSI_IRQS_NUM);
};

/**
 * struct mtk_pcie - PCIe host information
 * @dev: pointer to PCIe device
 * @base: IO mapped register base
 * @io: IO resource
 * @pio: PIO resource
 * @mem: non-prefetchable memory resource
 * @busn: bus range
 * @offset: IO / Memory offset
 * @ports: pointer to PCIe port information
 * @soc: pointer to SoC-dependent operations
 */
struct mtk_pcie {
	struct device *dev;
	void __iomem *base;

	struct resource io;
	struct resource pio;
	struct resource mem;
	struct resource busn;
	struct {
		resource_size_t mem;
		resource_size_t io;
	} offset;
	struct list_head ports;
	const struct mtk_pcie_soc *soc;
};

static void mtk_pcie_subsys_powerdown(struct mtk_pcie *pcie)
{
	struct device *dev = pcie->dev;

	if (dev->pm_domain) {
		pm_runtime_put_sync(dev);
		pm_runtime_disable(dev);
	}
}

static void mtk_pcie_port_free(struct mtk_pcie_port *port)
{
	struct mtk_pcie *pcie = port->pcie;
	struct device *dev = pcie->dev;

	devm_iounmap(dev, port->base);
	list_del(&port->list);
	devm_kfree(dev, port);
}

static int mtk_pcie_disable_clk(struct mtk_pcie_port *port)
{
	int i;

	for (i = 0; i < port->num_clks; i ++) {
		clk_disable_unprepare(port->clks[i]);
		clk_put(port->clks[i]);
	}
	port->num_clks = 0;

	return 0;
}

static void mtk_pcie_put_resources(struct mtk_pcie *pcie)
{
	struct mtk_pcie_port *port, *tmp;

	list_for_each_entry_safe(port, tmp, &pcie->ports, list) {
		phy_power_off(port->phy);
		phy_exit(port->phy);
		mtk_pcie_disable_clk(port);
		mtk_pcie_port_free(port);
	}

	mtk_pcie_subsys_powerdown(pcie);
}

static int mtk_pcie_check_cfg_cpld(struct mtk_pcie_port *port)
{
	u32 val;
	int err;

	err = readl_poll_timeout_atomic(port->base + PCIE_APP_TLP_REQ, val,
					!(val & APP_CFG_REQ), 10,
					100 * USEC_PER_MSEC);
	if (err)
		return PCIBIOS_SET_FAILED;

	if (readl(port->base + PCIE_APP_TLP_REQ) & APP_CPL_STATUS)
		return PCIBIOS_SET_FAILED;

	return PCIBIOS_SUCCESSFUL;
}

static int mtk_pcie_hw_rd_cfg(struct mtk_pcie_port *port, u32 bus, u32 devfn,
			      int where, int size, u32 *val)
{
	u32 tmp;

	/* Write PCIe configuration transaction header for Cfgrd */
	writel(CFG_HEADER_DW0(CFG_WRRD_TYPE_0, CFG_RD_FMT),
	       port->base + PCIE_CFG_HEADER0);
	writel(CFG_HEADER_DW1(where, size), port->base + PCIE_CFG_HEADER1);
	writel(CFG_HEADER_DW2(where, PCI_FUNC(devfn), PCI_SLOT(devfn), bus),
	       port->base + PCIE_CFG_HEADER2);

	/* Trigger h/w to transmit Cfgrd TLP */
	tmp = readl(port->base + PCIE_APP_TLP_REQ);
	tmp |= APP_CFG_REQ;
	writel(tmp, port->base + PCIE_APP_TLP_REQ);

	/* Check completion status */
	if (mtk_pcie_check_cfg_cpld(port))
		return PCIBIOS_SET_FAILED;

	/* Read cpld payload of Cfgrd */
	*val = readl(port->base + PCIE_CFG_RDATA);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int mtk_pcie_hw_wr_cfg(struct mtk_pcie_port *port, u32 bus, u32 devfn,
			      int where, int size, u32 val)
{
	/* Write PCIe configuration transaction header for Cfgwr */
	writel(CFG_HEADER_DW0(CFG_WRRD_TYPE_0, CFG_WR_FMT),
	       port->base + PCIE_CFG_HEADER0);
	writel(CFG_HEADER_DW1(where, size), port->base + PCIE_CFG_HEADER1);
	writel(CFG_HEADER_DW2(where, PCI_FUNC(devfn), PCI_SLOT(devfn), bus),
	       port->base + PCIE_CFG_HEADER2);

	/* Write Cfgwr data */
	val = val << 8 * (where & 3);
	writel(val, port->base + PCIE_CFG_WDATA);

	/* Trigger h/w to transmit Cfgwr TLP */
	val = readl(port->base + PCIE_APP_TLP_REQ);
	val |= APP_CFG_REQ;
	writel(val, port->base + PCIE_APP_TLP_REQ);

	/* Check completion status */
	return mtk_pcie_check_cfg_cpld(port);
}

static struct mtk_pcie_port *mtk_pcie_find_port(struct pci_bus *bus,
						unsigned int devfn)
{
	struct mtk_pcie *pcie = bus->sysdata;
	struct mtk_pcie_port *port;
	struct pci_dev *dev;
	struct pci_bus *pbus;

	list_for_each_entry(port, &pcie->ports, list) {
		if (bus->number == 0 && port->slot == PCI_SLOT(devfn)) {
			return port;
		} else if (bus->number != 0) {
			pbus = bus;
			do {
				dev = pbus->self;
				if (port->slot == PCI_SLOT(dev->devfn))
					return port;

				pbus = dev->bus;
			} while (dev->bus->number != 0);
		}
	}

	return NULL;
}

static int mtk_pcie_config_read(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *val)
{
	struct mtk_pcie_port *port;
	u32 bn = bus->number;
	int ret;

	port = mtk_pcie_find_port(bus, devfn);
	if (!port) {
		*val = ~0;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	ret = mtk_pcie_hw_rd_cfg(port, bn, devfn, where, size, val);
	if (ret)
		*val = ~0;

	return ret;
}

static int mtk_pcie_config_write(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 val)
{
	struct mtk_pcie_port *port;
	u32 bn = bus->number;

	port = mtk_pcie_find_port(bus, devfn);
	if (!port)
		return PCIBIOS_DEVICE_NOT_FOUND;

	return mtk_pcie_hw_wr_cfg(port, bn, devfn, where, size, val);
}

static struct pci_ops mtk_pcie_ops_v2 = {
	.read  = mtk_pcie_config_read,
	.write = mtk_pcie_config_write,
};

static void mtk_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_irq_info *irq_info = port->pcie->soc->irq_info;
	phys_addr_t addr;

	/* MT2712/MT7622 only support 32-bit MSI addresses */
	addr = readl(port->base + irq_info->msi_addr);
	msg->address_hi = 0;
	msg->address_lo = lower_32_bits(addr);

	msg->data = data->hwirq;

	dev_dbg(port->pcie->dev, "msi#%d address_hi %#x address_lo %#x\n",
		(int)data->hwirq, msg->address_hi, msg->address_lo);
}

static int mtk_msi_set_affinity(struct irq_data *irq_data,
				const struct cpumask *mask, bool force)
{
	return -EINVAL;
}

static void mtk_msi_ack_irq(struct irq_data *data)
{
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(data);
	struct mtk_pcie_irq_info *irq_info = port->pcie->soc->irq_info;
	u32 hwirq = data->hwirq;

	writel(1 << hwirq, port->base + irq_info->msi_status);
}

static struct irq_chip mtk_msi_bottom_irq_chip = {
	.name			= "MTK MSI",
	.irq_compose_msi_msg	= mtk_compose_msi_msg,
	.irq_set_affinity	= mtk_msi_set_affinity,
	.irq_ack		= mtk_msi_ack_irq,
};

static int mtk_pcie_irq_domain_alloc(struct irq_domain *domain,
				     unsigned int virq, unsigned int nr_irqs,
				     void *args)
{
	struct mtk_pcie_port *port = domain->host_data;
	unsigned long bit;

	WARN_ON(nr_irqs != 1);
	mutex_lock(&port->lock);

	bit = find_first_zero_bit(port->msi_irq_in_use, MTK_MSI_IRQS_NUM);
	if (bit >= MTK_MSI_IRQS_NUM) {
		mutex_unlock(&port->lock);
		return -ENOSPC;
	}

	__set_bit(bit, port->msi_irq_in_use);

	mutex_unlock(&port->lock);

	irq_domain_set_info(domain, virq, bit, &mtk_msi_bottom_irq_chip,
			    domain->host_data, handle_edge_irq,
			    NULL, NULL);

	return 0;
}

static void mtk_pcie_irq_domain_free(struct irq_domain *domain,
				     unsigned int virq, unsigned int nr_irqs)
{
	struct irq_data *d = irq_domain_get_irq_data(domain, virq);
	struct mtk_pcie_port *port = irq_data_get_irq_chip_data(d);

	mutex_lock(&port->lock);

	if (!test_bit(d->hwirq, port->msi_irq_in_use))
		dev_err(port->pcie->dev, "trying to free unused MSI#%lu\n",
			d->hwirq);
	else
		__clear_bit(d->hwirq, port->msi_irq_in_use);

	mutex_unlock(&port->lock);

	irq_domain_free_irqs_parent(domain, virq, nr_irqs);
}

static const struct irq_domain_ops msi_domain_ops = {
	.alloc	= mtk_pcie_irq_domain_alloc,
	.free	= mtk_pcie_irq_domain_free,
};

static struct irq_chip mtk_msi_irq_chip = {
	.name		= "MTK PCIe MSI",
	.irq_ack	= irq_chip_ack_parent,
	.irq_mask	= pci_msi_mask_irq,
	.irq_unmask	= pci_msi_unmask_irq,
};

static struct msi_domain_info mtk_msi_domain_info = {
	.flags	= (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		   MSI_FLAG_PCI_MSIX),
	.chip	= &mtk_msi_irq_chip,
};

static int mtk_pcie_allocate_msi_domains(struct mtk_pcie_port *port)
{
	struct fwnode_handle *fwnode;

	mutex_init(&port->lock);

	fwnode = of_node_to_fwnode(port->pcie->dev->of_node);
	port->inner_domain = irq_domain_create_linear(fwnode, MTK_MSI_IRQS_NUM,
						      &msi_domain_ops, port);
	if (!port->inner_domain) {
		dev_err(port->pcie->dev, "failed to create IRQ domain\n");
		return -ENOMEM;
	}

	port->msi_domain = pci_msi_create_irq_domain(fwnode,
						     &mtk_msi_domain_info,
						     port->inner_domain);
	if (!port->msi_domain) {
		dev_err(port->pcie->dev, "failed to create MSI domain\n");
		irq_domain_remove(port->inner_domain);
		return -ENOMEM;
	}

	return 0;
}

static void mtk_pcie_enable_msi(struct mtk_pcie_port *port)
{
	u32 val;
	phys_addr_t msg_addr;
	struct mtk_pcie_irq_info *irq_info = port->pcie->soc->irq_info;

	msg_addr = virt_to_phys(port->base);
	val = lower_32_bits(msg_addr) & MSI_VECTOR_MASK;
	writel(val, port->base + irq_info->msi_addr);

	if (irq_info->enable_msi_group) {
		irq_info->enable_msi_group(port, 0);
	} else {
		val = readl(port->base + irq_info->int_mask);
		val |= irq_info->msi_mask_bit;
		writel(val, port->base + irq_info->int_mask);
	}
}

static void mtk_pcie_irq_teardown(struct mtk_pcie *pcie)
{
	struct mtk_pcie_port *port, *tmp;

	if (list_empty(&pcie->ports))
		return;

	list_for_each_entry_safe(port, tmp, &pcie->ports, list) {
		irq_set_chained_handler_and_data(port->irq, NULL, NULL);

		if (port->irq_domain)
			irq_domain_remove(port->irq_domain);
		if (port->msi_domain)
			irq_domain_remove(port->msi_domain);
		if (port->inner_domain)
			irq_domain_remove(port->inner_domain);

		irq_dispose_mapping(port->irq);
	}
}

static int mtk_pcie_intx_map(struct irq_domain *domain, unsigned int irq,
			     irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &dummy_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);

	return 0;
}

static const struct irq_domain_ops intx_domain_ops = {
	.map = mtk_pcie_intx_map,
};

static int mtk_pcie_init_irq_domain(struct mtk_pcie_port *port,
				    struct device_node *node)
{
	struct device *dev = port->pcie->dev;
	struct device_node *pcie_intc_node;
	int ret;

	/* Setup INTx */
	pcie_intc_node = of_get_next_child(node, NULL);
	if (!pcie_intc_node) {
		dev_err(dev, "no PCIe Intc node found\n");
		return -ENODEV;
	}

	port->irq_domain = irq_domain_add_linear(pcie_intc_node, INTX_NUM,
						 &intx_domain_ops, port);
	if (!port->irq_domain) {
		dev_err(dev, "failed to get INTx IRQ domain\n");
		return -ENODEV;
	}

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		ret = mtk_pcie_allocate_msi_domains(port);
		if (ret)
			return ret;
	}

	return 0;
}

static void mtk_pcie_intr_handler(struct irq_desc *desc)
{
	struct mtk_pcie_port *port = irq_desc_get_handler_data(desc);
	struct mtk_pcie_irq_info *irq_info = port->pcie->soc->irq_info;
	struct irq_chip *irqchip = irq_desc_get_chip(desc);
	unsigned long status;
	u32 virq, val, mask;
	u32 bit = irq_info->intx_shift;

	chained_irq_enter(irqchip, desc);

	val = readl(port->base + irq_info->int_status);
	mask = readl(port->base + irq_info->int_mask);
	status = val & mask;

	if (status & irq_info->intx_mask_bit) {
		for_each_set_bit_from(bit, &status, PCI_NUM_INTX +
				      irq_info->intx_shift) {
			virq = irq_find_mapping(port->irq_domain,
					bit - irq_info->intx_shift);
			generic_handle_irq(virq);
			/* Clear the INTx */
			writel(1 << bit, port->base + irq_info->int_status);
		}
	}

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		if (status & irq_info->msi_mask_bit) {
			unsigned long imsi_status;

			while ((imsi_status = readl(port->base +
						    irq_info->msi_status))) {
				for_each_set_bit(bit, &imsi_status,
						 MTK_MSI_IRQS_NUM) {
					virq = irq_find_mapping(
						port->inner_domain, bit);
					generic_handle_irq(virq);
				}
			}
			/* Clear MSI interrupt status */
			writel(irq_info->msi_mask_bit, port->base +
			       irq_info->int_status);
		}
	}

	chained_irq_exit(irqchip, desc);
}

struct mtk_pcie_irq_info irq_info_v2 = {
	.int_status = PCIE_INT_STATUS,
	.msi_status = PCIE_IMSI_STATUS,
	.msi_addr = PCIE_IMSI_ADDR,
	.int_mask = PCIE_INT_MASK,
	.intx_shift = INTX_SHIFT,
	.intx_mask_bit = INTX_MASK,
	.msi_mask_bit = MSI_MASK,
};

static void mtk_enable_msi_group(struct mtk_pcie_port *port, int group)
{
	int val;

	writel(MSI_GRP_ENABLE(group), port->base + PCIE_MSI_GRP_EN);

	val = readl(port->base + PCIE_INT_MASK_V3);
	writel(MSI_GRP_STATUS(group) | val, port->base + PCIE_INT_MASK_V3);
	writel(~0, port->base + PCIE_MSI_ENABLE_GRP(group));
}

struct mtk_pcie_irq_info irq_info_v3 = {
	.int_status = PCIE_INT_STATUS_V3,
	.msi_status = IMSI_STATUS,
	.msi_addr = MSI_VECTOR,
	.int_mask = PCIE_INT_MASK_V3,
	.intx_shift = MTK_PCIE_INTX_SHIFT_V3,
	.intx_mask_bit = INTX_MASK_V3,
	.msi_mask_bit = MSI_MASK_V3,
	.enable_msi_group = mtk_enable_msi_group,
};

static int mtk_pcie_setup_irq(struct mtk_pcie_port *port,
			      struct device_node *node)
{
	struct mtk_pcie *pcie = port->pcie;
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(dev);
	int err;

	err = mtk_pcie_init_irq_domain(port, node);
	if (err) {
		dev_err(dev, "failed to init PCIe IRQ domain\n");
		return err;
	}

	port->irq = platform_get_irq(pdev, 0);
	irq_set_chained_handler_and_data(port->irq,
					 mtk_pcie_intr_handler, port);

	return 0;
}

static int mtk_pcie_startup_port_v2(struct mtk_pcie_port *port)
{
	struct mtk_pcie *pcie = port->pcie;
	struct resource *mem = &pcie->mem;
	const struct mtk_pcie_soc *soc = port->pcie->soc;
	u32 val;
	size_t size;
	int err;

	/* MT7622 platforms need to enable LTSSM and ASPM from PCIe subsys */
	if (pcie->base) {
		val = readl(pcie->base + PCIE_SYS_CFG_V2);
		val |= PCIE_CSR_LTSSM_EN(port->slot) |
		       PCIE_CSR_ASPM_L1_EN(port->slot);
		writel(val, pcie->base + PCIE_SYS_CFG_V2);
	}

	/* Assert all reset signals */
	writel(0, port->base + PCIE_RST_CTRL);

	/*
	 * Enable PCIe link down reset, if link status changed from link up to
	 * link down, this will reset MAC control registers and configuration
	 * space.
	 */
	writel(PCIE_LINKDOWN_RST_EN, port->base + PCIE_RST_CTRL);

	/* De-assert PHY, PE, PIPE, MAC and configuration reset	*/
	val = readl(port->base + PCIE_RST_CTRL);
	val |= PCIE_PHY_RSTB | PCIE_PERSTB | PCIE_PIPE_SRSTB |
	       PCIE_MAC_SRSTB | PCIE_CRSTB;
	writel(val, port->base + PCIE_RST_CTRL);

	/* Set up vendor ID and class code */
	if (soc->need_fix_class_id) {
		val = PCI_VENDOR_ID_MEDIATEK;
		writew(val, port->base + PCIE_CONF_VEND_ID);

		val = PCI_CLASS_BRIDGE_HOST;
		writew(val, port->base + PCIE_CONF_CLASS_ID);
	}

	if (soc->need_fix_device_id)
		writew(soc->device_id, port->base + PCIE_CONF_DEVICE_ID);

	/* 100ms timeout value should be enough for Gen1/2 training */
	err = readl_poll_timeout(port->base + PCIE_LINK_STATUS_V2, val,
				 !!(val & PCIE_PORT_LINKUP_V2), 20,
				 100 * USEC_PER_MSEC);
	if (err)
		return -ETIMEDOUT;

	/* Set INTx mask */
	val = readl(port->base + PCIE_INT_MASK);
	val &= ~INTX_MASK;
	writel(val, port->base + PCIE_INT_MASK);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		mtk_pcie_enable_msi(port);

	/* Set AHB to PCIe translation windows */
	size = mem->end - mem->start;
	val = lower_32_bits(mem->start) | AHB2PCIE_SIZE(fls(size));
	writel(val, port->base + PCIE_AHB_TRANS_BASE0_L);

	val = upper_32_bits(mem->start);
	writel(val, port->base + PCIE_AHB_TRANS_BASE0_H);

	/* Set PCIe to AXI translation memory space.*/
	val = fls(0xffffffff) | WIN_ENABLE;
	writel(val, port->base + PCIE_AXI_WINDOW0);

	return 0;
}

static void __iomem *mtk_pcie_map_bus_v3(struct pci_bus *bus,
				      unsigned int devfn, int where)
{
	struct mtk_pcie_port *port;

	port = mtk_pcie_find_port(bus, devfn);
	if (!port)
		return NULL;

	writel(CFG_HEADER(devfn, bus->number), port->base + PCIE_CFGNUM);

	return port->base + CFG_OFFSET_ADDR + where;
}

static struct pci_ops mtk_pcie_ops_v3 = {
	.map_bus = mtk_pcie_map_bus_v3,
	.read	 = pci_generic_config_read,
	.write	 = pci_generic_config_write,
};

static int mtk_pcie_port_enable(void)
{
	int val = 0;
	void __iomem *infra_base, *remap_base, *phy_base;

	infra_base = ioremap(0x10001000, 0x1000);
	remap_base = ioremap(0x10041000, 0x100);
	phy_base = ioremap(0x10005000, 0x1000);

	/* Set phy mode to RC for port 0 */
	val = readl(phy_base + 0x600);
	val |= 1 << 14;
	writel(val, phy_base + 0x600);

	/* Enable AXI clock for port1*/
	val = readl(infra_base + 0xe0);
	//val |= BIT(14);
	val |= 0x1c000; //For ECO version
	writel(val, infra_base + 0xe0);

	/* Enable PCIe LTSSM */
	val = readl(infra_base + 0xc80);
	val |= 0x1;
	writel(val, infra_base + 0xc80);

	/* Remap address of AP/MD to PCIe */
	writel(0x3, remap_base + 0x28);
	writel(0x0, remap_base + 0x2c);

	iounmap(infra_base);
	iounmap(remap_base);
	iounmap(phy_base);

	return 0;
}

static int mtk_pcie_startup_port_v3(struct mtk_pcie_port *port)
{
	int size, val, err;
	struct resource *mem = &port->pcie->mem;
	const struct mtk_pcie_soc *soc = port->pcie->soc;

	mtk_pcie_port_enable();

	/* disable hw trapping and set as RC mode */
	writel(BIT(31), port->base + PCIE_MISC_CTRL);
	val = readl(port->base + PCIE_SETTING);
	writel(val | PCIE_RC_MODE, port->base + PCIE_SETTING);

	/* Setup class code */
	writel(0x06040000, port->base + 0x9c);

	val = readl(port->base + 0x74);
	writel(val | BIT(19), port->base + 0x74);

	/* bypass dvfsrc, only used on emulation */
	val = readl(port->base + 0x348);
	writel(val | BIT(1), port->base + 0x348);

	/* assert all reset signals */
	val = readl(port->base + PCIE_RST_CTRL_V3);
	val |= PCIE_MAC_RSTB_V3 | PCIE_PHY_RSTB_V3 | PCIE_BRG_RSTB_V3|
	       PCIE_PE_RSTB_V3;
	writel(val, port->base + PCIE_RST_CTRL_V3);
	usleep_range(500, 1000);

	/* de-assert reset signals*/
	val &= ~(PCIE_MAC_RSTB_V3 | PCIE_PHY_RSTB_V3 | PCIE_BRG_RSTB_V3);
	writel(val, port->base + PCIE_RST_CTRL_V3);

	usleep_range(100 * 1000, 120 * 1000);

	/* de-assert pe reset signals*/
	val &= ~PCIE_PE_RSTB_V3;
	writel(val, port->base + PCIE_RST_CTRL_V3);

	/* Set up vendor ID and class code */
	if (soc->need_fix_class_id) {
		val = readl(port->base + PCIE_IDS2) & 0xff;
		writel(val | PCI_CLASS(PCI_CLASS_BRIDGE_PCI << 8),
		       port->base + PCIE_IDS2);
	}

	/* 100ms timeout value should be enough for Gen1/2 training */
	err = readl_poll_timeout(port->base + PCIE_DATA_LINK_STATUS_V3, val,
			!!(val & PCIE_DATA_LINKUP_V3), 20,
			200 * USEC_PER_MSEC);
	if (err)
		return -ETIMEDOUT;

	/* set INT mask */
	writel(INTX_MASK_V3, port->base + PCIE_INT_MASK_V3);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		mtk_pcie_enable_msi(port);

	/* Set AHB to PCIe translation windows */
	size = fls(mem->end - mem->start) - 1;
	writel(ATR_SRC_ADDR_L(mem->start) | ATR_SIZE(size) | ATR_IMPL,
	       port->base + AXI_SLV0_T0_PAR_SRC_LSB);
	writel(upper_32_bits(mem->start), port->base + AXI_SLV0_T0_SRC_MSB);

	writel(ATR_SRC_ADDR_L(mem->start), port->base + AXI_SLV0_T0_TRSL_LSB);
	writel(upper_32_bits(mem->start), port->base + AXI_SLV0_T0_TRSL_MSB);

	writel(ATR_ID(0) | ATR_PARAM(0), port->base + AXI_SLV0_T0_TRSL_PAR);

	return 0;
}

static void __iomem *mtk_pcie_map_bus(struct pci_bus *bus,
				      unsigned int devfn, int where)
{
	struct mtk_pcie *pcie = bus->sysdata;

	writel(PCIE_CONF_ADDR(where, PCI_FUNC(devfn), PCI_SLOT(devfn),
			      bus->number), pcie->base + PCIE_CFG_ADDR);

	return pcie->base + PCIE_CFG_DATA + (where & 3);
}

static struct pci_ops mtk_pcie_ops = {
	.map_bus = mtk_pcie_map_bus,
	.read  = pci_generic_config_read,
	.write = pci_generic_config_write,
};

static int mtk_pcie_startup_port(struct mtk_pcie_port *port)
{
	struct mtk_pcie *pcie = port->pcie;
	u32 func = PCI_FUNC(port->slot << 3);
	u32 slot = PCI_SLOT(port->slot << 3);
	u32 val;
	int err;

	/* assert port PERST_N */
	val = readl(pcie->base + PCIE_SYS_CFG);
	val |= PCIE_PORT_PERST(port->slot);
	writel(val, pcie->base + PCIE_SYS_CFG);

	/* de-assert port PERST_N */
	val = readl(pcie->base + PCIE_SYS_CFG);
	val &= ~PCIE_PORT_PERST(port->slot);
	writel(val, pcie->base + PCIE_SYS_CFG);

	/* 100ms timeout value should be enough for Gen1/2 training */
	err = readl_poll_timeout(port->base + PCIE_LINK_STATUS, val,
				 !!(val & PCIE_PORT_LINKUP), 20,
				 100 * USEC_PER_MSEC);
	if (err)
		return -ETIMEDOUT;

	/* enable interrupt */
	val = readl(pcie->base + PCIE_INT_ENABLE);
	val |= PCIE_PORT_INT_EN(port->slot);
	writel(val, pcie->base + PCIE_INT_ENABLE);

	/* map to all DDR region. We need to set it before cfg operation. */
	writel(PCIE_BAR_MAP_MAX | PCIE_BAR_ENABLE,
	       port->base + PCIE_BAR0_SETUP);

	/* configure class code and revision ID */
	writel(PCIE_CLASS_CODE | PCIE_REVISION_ID, port->base + PCIE_CLASS);

	/* configure FC credit */
	writel(PCIE_CONF_ADDR(PCIE_FC_CREDIT, func, slot, 0),
	       pcie->base + PCIE_CFG_ADDR);
	val = readl(pcie->base + PCIE_CFG_DATA);
	val &= ~PCIE_FC_CREDIT_MASK;
	val |= PCIE_FC_CREDIT_VAL(0x806c);
	writel(PCIE_CONF_ADDR(PCIE_FC_CREDIT, func, slot, 0),
	       pcie->base + PCIE_CFG_ADDR);
	writel(val, pcie->base + PCIE_CFG_DATA);

	/* configure RC FTS number to 250 when it leaves L0s */
	writel(PCIE_CONF_ADDR(PCIE_FTS_NUM, func, slot, 0),
	       pcie->base + PCIE_CFG_ADDR);
	val = readl(pcie->base + PCIE_CFG_DATA);
	val &= ~PCIE_FTS_NUM_MASK;
	val |= PCIE_FTS_NUM_L0(0x50);
	writel(PCIE_CONF_ADDR(PCIE_FTS_NUM, func, slot, 0),
	       pcie->base + PCIE_CFG_ADDR);
	writel(val, pcie->base + PCIE_CFG_DATA);

	return 0;
}

static int mtk_pcie_clk_init(struct mtk_pcie_port *port)
{
	struct mtk_pcie *pcie = port->pcie;
	struct device *dev = pcie->dev;
	struct device_node *np = dev->of_node;
	int i;

	port->num_clks = of_clk_get_parent_count(np);
	if (port->num_clks == 0) {
		dev_warn(dev, "pcie clock is not found\n");
		return 0;
	}

	port->clks = devm_kzalloc(dev, port->num_clks, GFP_KERNEL);
	if (!port->clks)
		return -ENOMEM;

	for (i = 0; i < port->num_clks; i++) {
		struct clk      *clk;
		int             ret;

		clk = of_clk_get(np, i);
		if (IS_ERR(clk)) {
			while (--i >= 0)
				clk_put(port->clks[i]);
			return PTR_ERR(clk);
		}

		ret = clk_prepare_enable(clk);
		if (ret < 0) {
			while (--i >= 0) {
				clk_disable_unprepare(port->clks[i]);
				clk_put(port->clks[i]);
			}
			clk_put(clk);

			return ret;
		}

		port->clks[i] = clk;
	}

	return 0;
}

static void mtk_pcie_enable_port(struct mtk_pcie_port *port)
{
	struct mtk_pcie *pcie = port->pcie;
	struct device *dev = pcie->dev;
	int err;

	err = phy_init(port->phy);
	if (err) {
		dev_err(dev, "failed to initialize port%d phy\n", port->slot);
		goto err_clk;
	}

	err = phy_power_on(port->phy);
	if (err) {
		dev_err(dev, "failed to power on port%d phy\n", port->slot);
		goto err_phy_on;
	}

	err = mtk_pcie_clk_init(port);
	if (err) {
		dev_err(dev, "failed to enable clocks\n");
		goto err_clk;
	}

	if (!pcie->soc->startup(port)) {
		dev_info(dev, "Port%d link up success!\n", port->slot);
		/* For independent port, the slot number is always zero */
		port->slot = 0;
		return;
	}

	dev_info(dev, "Port%d link down\n", port->slot);

	phy_power_off(port->phy);
err_phy_on:
	phy_exit(port->phy);
	mtk_pcie_disable_clk(port);
err_clk:
	mtk_pcie_port_free(port);
}

static int mtk_pcie_parse_port(struct mtk_pcie *pcie,
			       struct device_node *node,
			       int slot)
{
	struct mtk_pcie_port *port;
	struct resource *regs;
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(dev);
	char name[10];
	int err;

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	err = of_property_read_u32(node, "num-lanes", &port->lane);
	if (err) {
		dev_err(dev, "missing num-lanes property\n");
		return err;
	}

	snprintf(name, sizeof(name), "port%d", slot);
	regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	port->base = devm_ioremap_resource(dev, regs);
	if (IS_ERR(port->base)) {
		dev_err(dev, "failed to map port%d base\n", slot);
		return PTR_ERR(port->base);
	}

	/* some platforms may use default PHY setting */
	port->phy = devm_phy_optional_get(dev, "pcie-phy");
	if (IS_ERR(port->phy))
		port->phy = NULL;

	port->slot = slot;
	port->pcie = pcie;

	if (pcie->soc->setup_irq) {
		err = pcie->soc->setup_irq(port, node);
		if (err)
			return err;
	}

	INIT_LIST_HEAD(&port->list);
	list_add_tail(&port->list, &pcie->ports);

	return 0;
}

static int mtk_pcie_subsys_powerup(struct mtk_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *regs;

	/* get shared registers, which are optional */
	regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "subsys");
	if (regs) {
		pcie->base = devm_ioremap_resource(dev, regs);
		if (IS_ERR(pcie->base)) {
			dev_err(dev, "failed to map shared register\n");
			return PTR_ERR(pcie->base);
		}
	}

	if (dev->pm_domain) {
		pm_runtime_enable(dev);
		pm_runtime_get_sync(dev);
	}

	return 0;
}

static int mtk_pcie_setup(struct mtk_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct device_node *node = dev->of_node, *child;
	struct of_pci_range_parser parser;
	struct of_pci_range range;
	struct resource res;
	struct mtk_pcie_port *port, *tmp;
	int err;

	if (of_pci_range_parser_init(&parser, node)) {
		dev_err(dev, "missing \"ranges\" property\n");
		return -EINVAL;
	}

	for_each_of_pci_range(&parser, &range) {
		err = of_pci_range_to_resource(&range, node, &res);
		if (err < 0)
			return err;

		switch (res.flags & IORESOURCE_TYPE_BITS) {
		case IORESOURCE_IO:
			pcie->offset.io = res.start - range.pci_addr;

			memcpy(&pcie->pio, &res, sizeof(res));
			pcie->pio.name = node->full_name;

			pcie->io.start = range.cpu_addr;
			pcie->io.end = range.cpu_addr + range.size - 1;
			pcie->io.flags = IORESOURCE_MEM;
			pcie->io.name = "I/O";

			memcpy(&res, &pcie->io, sizeof(res));
			break;

		case IORESOURCE_MEM:
			pcie->offset.mem = res.start - range.pci_addr;

			memcpy(&pcie->mem, &res, sizeof(res));
			pcie->mem.name = "non-prefetchable";
			break;
		}
	}

	err = of_pci_parse_bus_range(node, &pcie->busn);
	if (err < 0) {
		dev_err(dev, "failed to parse bus ranges property: %d\n", err);
		pcie->busn.name = node->name;
		pcie->busn.start = 0;
		pcie->busn.end = 0xff;
		pcie->busn.flags = IORESOURCE_BUS;
	}

	for_each_available_child_of_node(node, child) {
		int slot;

		err = of_pci_get_devfn(child);
		if (err < 0) {
			dev_err(dev, "failed to parse devfn: %d\n", err);
			goto error_put_node;
		}

		slot = PCI_SLOT(err);

		err = mtk_pcie_parse_port(pcie, child, slot);
		if (err)
			goto error_put_node;
	}

	err = mtk_pcie_subsys_powerup(pcie);
	if (err)
		return err;

	/* enable each port, and then check link status */
	list_for_each_entry_safe(port, tmp, &pcie->ports, list)
		mtk_pcie_enable_port(port);

	/* power down PCIe subsys if slots are all empty (link down) */
	if (list_empty(&pcie->ports))
		mtk_pcie_subsys_powerdown(pcie);

	return 0;
error_put_node:
	of_node_put(child);
	return err;
}

static int mtk_pcie_request_resources(struct mtk_pcie *pcie)
{
	struct pci_host_bridge *host = pci_host_bridge_from_priv(pcie);
	struct list_head *windows = &host->windows;
	struct device *dev = pcie->dev;
	int err;

	pci_add_resource_offset(windows, &pcie->pio, pcie->offset.io);
	pci_add_resource_offset(windows, &pcie->mem, pcie->offset.mem);
	pci_add_resource(windows, &pcie->busn);

	err = devm_request_pci_bus_resources(dev, windows);
	if (err < 0)
		return err;

	pci_remap_iospace(&pcie->pio, pcie->io.start);

	return 0;
}

static int mtk_pcie_register_host(struct pci_host_bridge *host)
{
	struct mtk_pcie *pcie = pci_host_bridge_priv(host);
	struct pci_bus *child;
	int err;

	host->busnr = pcie->busn.start;
	host->dev.parent = pcie->dev;
	host->ops = pcie->soc->ops;
	host->map_irq = of_irq_parse_and_map_pci;
	host->swizzle_irq = pci_common_swizzle;
	host->sysdata = pcie;

	err = pci_scan_root_bus_bridge(host);
	if (err < 0)
		return err;

	pci_bus_size_bridges(host->bus);
	pci_bus_assign_resources(host->bus);

	list_for_each_entry(child, &host->bus->children, node)
		pcie_bus_configure_settings(child);

	pci_bus_add_devices(host->bus);

	return 0;
}

static int mtk_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_pcie *pcie;
	struct pci_host_bridge *host;
	int err;

	host = devm_pci_alloc_host_bridge(dev, sizeof(*pcie));
	if (!host)
		return -ENOMEM;

	pcie = pci_host_bridge_priv(host);

	pcie->dev = dev;
	pcie->soc = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, pcie);
	INIT_LIST_HEAD(&pcie->ports);

	err = mtk_pcie_setup(pcie);
	if (err)
		return err;

	err = mtk_pcie_request_resources(pcie);
	if (err)
		goto put_resources;

	err = mtk_pcie_register_host(host);
	if (err)
		goto put_resources;

	return 0;

put_resources:
	if (!list_empty(&pcie->ports))
		mtk_pcie_put_resources(pcie);

	return err;
}

static void mtk_pcie_free_resources(struct mtk_pcie *pcie)
{
	struct pci_host_bridge *host = pci_host_bridge_from_priv(pcie);
	struct list_head *windows = &host->windows;

	pci_unmap_iospace(&pcie->pio);
	pci_free_resource_list(windows);
}

static int mtk_pcie_remove(struct platform_device *pdev)
{
	struct mtk_pcie *pcie = platform_get_drvdata(pdev);
	struct pci_host_bridge *host = pci_host_bridge_from_priv(pcie);

	pci_stop_root_bus(host->bus);
	pci_remove_root_bus(host->bus);
	mtk_pcie_free_resources(pcie);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		mtk_pcie_irq_teardown(pcie);

	if (!list_empty(&pcie->ports))
		mtk_pcie_put_resources(pcie);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_pcie_suspend_noirq(struct device *dev)
{
	struct mtk_pcie *pcie = dev_get_drvdata(dev);
	const struct mtk_pcie_soc *soc = pcie->soc;
	struct mtk_pcie_port *port;
	int i;

	if (!soc->pm_support)
		return 0;

	list_for_each_entry(port, &pcie->ports, list) {
		phy_power_off(port->phy);

		for (i = 0; i < port->num_clks; i ++)
			clk_disable_unprepare(port->clks[i]);
	}

	return 0;
}

static int mtk_pcie_resume_noirq(struct device *dev)
{
	struct mtk_pcie *pcie = dev_get_drvdata(dev);
	const struct mtk_pcie_soc *soc = pcie->soc;
	struct mtk_pcie_port *port;
	int ret, i;

	if (!soc->pm_support)
		return 0;

	list_for_each_entry(port, &pcie->ports, list) {
		phy_power_on(port->phy);
		for (i = 0; i < port->num_clks; i ++)
			clk_prepare_enable(port->clks[i]);

		ret = soc->startup(port);
		if (ret) {
			dev_err(dev, "Port%d link down\n", port->slot);
			phy_power_off(port->phy);
			for (i = 0; i < port->num_clks; i ++)
				clk_disable_unprepare(port->clks[i]);
			return ret;
		}

		if (IS_ENABLED(CONFIG_PCI_MSI))
			mtk_pcie_enable_msi(port);
	}

	return 0;
}
#endif

static const struct dev_pm_ops mtk_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mtk_pcie_suspend_noirq,
				      mtk_pcie_resume_noirq)
};

static const struct mtk_pcie_soc mtk_pcie_soc_v1 = {
	.ops = &mtk_pcie_ops,
	.startup = mtk_pcie_startup_port,
};

static const struct mtk_pcie_soc mtk_pcie_soc_mt2712 = {
	.pm_support = true,
	.ops = &mtk_pcie_ops_v2,
	.startup = mtk_pcie_startup_port_v2,
	.setup_irq = mtk_pcie_setup_irq,
	.irq_info = &irq_info_v2,
};

static const struct mtk_pcie_soc mtk_pcie_soc_mt7622 = {
	.need_fix_class_id = true,
	.ops = &mtk_pcie_ops_v2,
	.startup = mtk_pcie_startup_port_v2,
	.setup_irq = mtk_pcie_setup_irq,
	.irq_info = &irq_info_v2,
};

static const struct mtk_pcie_soc mtk_pcie_soc_v3 = {
	.need_fix_class_id = false,
	.ops = &mtk_pcie_ops_v3,
	.startup = mtk_pcie_startup_port_v3,
	.setup_irq = mtk_pcie_setup_irq,
	.irq_info = &irq_info_v3,
};

static const struct mtk_pcie_soc mtk_pcie_soc_mt7629 = {
	.need_fix_class_id = true,
	.need_fix_device_id = true,
	.device_id = PCI_DEVICE_ID_MEDIATEK_7629,
	.ops = &mtk_pcie_ops_v2,
	.startup = mtk_pcie_startup_port_v2,
	.setup_irq = mtk_pcie_setup_irq,
};

static const struct of_device_id mtk_pcie_ids[] = {
	{ .compatible = "mediatek,mt2701-pcie", .data = &mtk_pcie_soc_v1 },
	{ .compatible = "mediatek,mt7623-pcie", .data = &mtk_pcie_soc_v1 },
	{ .compatible = "mediatek,mt2712-pcie", .data = &mtk_pcie_soc_mt2712 },
	{ .compatible = "mediatek,mt7622-pcie", .data = &mtk_pcie_soc_mt7622 },
	{ .compatible = "mediatek,mt5895-pcie", .data = &mtk_pcie_soc_v3 },
	{ .compatible = "mediatek,mt7629-pcie", .data = &mtk_pcie_soc_mt7629 },
	{},
};

static struct platform_driver mtk_pcie_driver = {
	.probe = mtk_pcie_probe,
	.remove = mtk_pcie_remove,
	.driver = {
		.name = "mtk-pcie",
		.of_match_table = mtk_pcie_ids,
		.suppress_bind_attrs = true,
		.pm = &mtk_pcie_pm_ops,
	},
};

static int __init mtk_pcie_init(void)
{
	return platform_driver_register(&mtk_pcie_driver);
}

late_initcall(mtk_pcie_init);
MODULE_LICENSE("GPL");
