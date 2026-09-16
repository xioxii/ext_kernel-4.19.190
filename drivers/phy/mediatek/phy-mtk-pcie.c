// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Jianjun Wang <jianjun.wang@mediatek.com>
 *
 */

#include <dt-bindings/phy/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/pm_domain.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#define PCIE_PHY_0_NUM			0
#define PCIE_PHY_1_NUM			1
#define PCIE_PHY_2_NUM			2
#define PCIE_PHY_3_NUM			3
#define PCIE_PHY_0_EFUSE_CNT		5
#define PCIE_PHY_CKM_REG04		0x04
#define PCIE_EFSUE_P1_INDEX		0x108
#define PCIE_EFSUE_P2_INDEX		0x10C
#define PCIE_EFSUE_P3_INDEX		0x110
#define CKM_PT0_CKTX_IMPSEL(val)	((val) & GENMASK(3, 0))
#define CKM_PT0_CKTX_R50_EN		BIT(5)
#define CKM_PT0_CKTX_SLEW(val)		((val << 8) & GENMASK(9, 8))
#define CKM_PT0_CKTX_OFFSET(val)	((val << 10) & GENMASK(11, 10))
#define CKM_PT0_CKTX_AMP(val)		((val << 12) & GENMASK(15, 12))

#define PEXTP_GLB_00_RG			0x9000
#define PEXTP_04_RG			0xA004
#define PEXTP_3C_RG			0xA03C
#define PEXTP_104_RG			0xA104
#define PEXTP_13C_RG			0xA13c

#define EFUSE_FIELD_SIZE		4
#define EFUSE_SHIFT(val, right, left, left_phy) \
		((((val) & GENMASK(right, left)) >> (left)) << (left_phy))

struct mtk_pcie_phy {
	struct device *dev;
	struct phy *phy;
	void __iomem *sif_base;
	void __iomem *ckm_base;
	struct clk *pipe_clk;	/* pipe clock to mac */
	enum phy_mode mode;
};

static const unsigned int efuse_field_offset[PCIE_PHY_0_EFUSE_CNT] = {
	0x100, 0x104, 0x1B4, 0x1BC, 0x1C0
};

/*
 * MT6890 RC portA is unlike the other ports, needs to get
 * impedance select values from the efuse's multiple registers.
 */
static int pcie_fetch_efuse_scatter(struct mtk_pcie_phy *pcie_phy)
{
	struct nvmem_device *nvmem_dev;
	u32 efuse_addr[PCIE_PHY_0_EFUSE_CNT] = {0}, val;
	int ret;
	int i;

	nvmem_dev = nvmem_device_get(pcie_phy->dev, "mtk_efuse");
	if (IS_ERR(nvmem_dev)) {
		if (PTR_ERR(nvmem_dev) == -EPROBE_DEFER)
			return PTR_ERR(nvmem_dev);
		return -EINVAL;
	}

	for (i = 0; i < PCIE_PHY_0_EFUSE_CNT; i++) {
		ret = nvmem_device_read(nvmem_dev, efuse_field_offset[i],
					   EFUSE_FIELD_SIZE, &efuse_addr[i]);
		if (ret != EFUSE_FIELD_SIZE) {
			dev_warn(pcie_phy->dev, "pcie efuse val is error\n");
			return -EINVAL;
		}
	}

	/* PEXTP_GLB_00_RG[28:24] Internal RG Selection of TX Bias Current */
	val = readl(pcie_phy->sif_base + PEXTP_GLB_00_RG);
	val = (val & ~GENMASK(28, 24)) | EFUSE_SHIFT(efuse_addr[0], 27, 23, 24);
	writel(val, pcie_phy->sif_base + PEXTP_GLB_00_RG);

	/* PCIE_PHY_CKM_REG04[3:0] Internal R50 impedance select */
	val = readl(pcie_phy->ckm_base + PCIE_PHY_CKM_REG04);
	val = (val & ~GENMASK(3, 0)) | EFUSE_SHIFT(efuse_addr[1], 8, 5, 0);
	writel(val, pcie_phy->ckm_base + PCIE_PHY_CKM_REG04);

	/* PEXTP_04_RG[5:2] LN0 TX PMOS impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_04_RG);
	val = (val & ~GENMASK(5, 2)) | EFUSE_SHIFT(efuse_addr[2], 4, 1, 2);
	writel(val, pcie_phy->sif_base + PEXTP_04_RG);

	/* PEXTP_104_RG[5:2] LN1 TX PMOS impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_104_RG);
	val = (val & ~GENMASK(5, 2)) | EFUSE_SHIFT(efuse_addr[2], 8, 5, 2);
	writel(val, pcie_phy->sif_base + PEXTP_104_RG);

	/* PEXTP_04_RG[11:8] LN0 TX NMOS impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_04_RG);
	val = (val & ~GENMASK(11, 8)) | EFUSE_SHIFT(efuse_addr[2], 20, 17, 8);
	writel(val, pcie_phy->sif_base + PEXTP_04_RG);

	/* PEXTP_104_RG[11:8] LN1 TX NMOS impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_104_RG);
	val = (val & ~GENMASK(11, 8)) | EFUSE_SHIFT(efuse_addr[2], 24, 21, 8);
	writel(val, pcie_phy->sif_base + PEXTP_104_RG);

	/* PEXTP_3C_RG[3:0] LN0 RX impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_3C_RG);
	val = (val & ~GENMASK(3, 0)) | EFUSE_SHIFT(efuse_addr[3], 31, 28, 0);
	writel(val, pcie_phy->sif_base + PEXTP_3C_RG);

	/* PEXTP_13C_RG[3:0] LN1 RX impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_13C_RG);
	val = (val & ~GENMASK(3, 0)) | EFUSE_SHIFT(efuse_addr[4], 3, 0, 0);
	writel(val, pcie_phy->sif_base + PEXTP_13C_RG);

	nvmem_device_put(nvmem_dev);

	return 0;
}

static int pcie_fetch_efuse_unify(struct mtk_pcie_phy *pcie_phy)
{
	struct nvmem_device *nvmem_dev;
	u32 efuse_addr = 0, val;
	unsigned int efuse_index_offset = 0;
	int ret;

	switch (pcie_phy->phy->id) {
	case PCIE_PHY_1_NUM:
		efuse_index_offset = PCIE_EFSUE_P1_INDEX;
		break;
	case PCIE_PHY_2_NUM:
		efuse_index_offset = PCIE_EFSUE_P2_INDEX;
		break;
	case PCIE_PHY_3_NUM:
		efuse_index_offset = PCIE_EFSUE_P3_INDEX;
		break;
	default:
		dev_info(pcie_phy->dev, "pcie phy num for efuse is error\n");
	}

	nvmem_dev = nvmem_device_get(pcie_phy->dev, "mtk_efuse");
	if (IS_ERR(nvmem_dev)) {
		if (PTR_ERR(nvmem_dev) == -EPROBE_DEFER)
			return PTR_ERR(nvmem_dev);
		return -EINVAL;
	}

	ret = nvmem_device_read(nvmem_dev, efuse_index_offset,
				 EFUSE_FIELD_SIZE, &efuse_addr);
	if (ret != EFUSE_FIELD_SIZE) {
		dev_warn(pcie_phy->dev, "pcie efuse val is error\n");
		return -EINVAL;
	}

	/* PEXTP_3C_RG[3:0] LN0 RX impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_3C_RG);
	val = (val & ~GENMASK(3, 0)) | EFUSE_SHIFT(efuse_addr, 3, 0, 0);
	writel(val, pcie_phy->sif_base + PEXTP_3C_RG);

	/* PEXTP_04_RG[5:2] LN0 TX PMOS impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_04_RG);
	val = (val & ~GENMASK(5, 2)) | EFUSE_SHIFT(efuse_addr, 7, 4, 2);
	writel(val, pcie_phy->sif_base + PEXTP_04_RG);

	/* PEXTP_04_RG[11:8] LN0 TX NMOS impedance selection */
	val = readl(pcie_phy->sif_base + PEXTP_04_RG);
	val = (val & ~GENMASK(11, 8)) | EFUSE_SHIFT(efuse_addr, 11, 8, 8);
	writel(val, pcie_phy->sif_base + PEXTP_04_RG);

	/* PEXTP_GLB_00_RG[28:24] Internal RG Selection of TX Bias Current */
	val = readl(pcie_phy->sif_base + PEXTP_GLB_00_RG);
	val = (val & ~GENMASK(28, 24)) | EFUSE_SHIFT(efuse_addr, 16, 12, 24);
	writel(val, pcie_phy->sif_base + PEXTP_GLB_00_RG);

	/* portB is two lanes */
	if (pcie_phy->phy->id == 1) {
		/* PEXTP_13C_RG[3:0] LN1 RX impedance selection */
		val = readl(pcie_phy->sif_base + PEXTP_13C_RG);
		val = (val & ~GENMASK(3, 0)) |
					EFUSE_SHIFT(efuse_addr, 20, 17, 0);
		writel(val, pcie_phy->sif_base + PEXTP_13C_RG);

		/* PEXTP_104_RG[5:2] LN1 TX PMOS impedance selection */
		val = readl(pcie_phy->sif_base + PEXTP_104_RG);
		val = (val & ~GENMASK(5, 2)) |
					EFUSE_SHIFT(efuse_addr, 24, 21, 2);
		writel(val, pcie_phy->sif_base + PEXTP_104_RG);

		/* PEXTP_104_RG[11:8] LN1 TX NMOS impedance selection */
		val = readl(pcie_phy->sif_base + PEXTP_104_RG);
		val = (val & ~GENMASK(11, 8)) |
					EFUSE_SHIFT(efuse_addr, 28, 25, 8);
		writel(val, pcie_phy->sif_base + PEXTP_104_RG);
	}

	nvmem_device_put(nvmem_dev);

	return 0;
}

static int mtk_pcie_phy_init(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	struct device *dev = pcie_phy->dev;
	struct generic_pm_domain *pcie_pd;
	int err;

	if (pcie_phy->mode == PHY_MODE_PCIE_EP) {
		dev_info(pcie_phy->dev, "set phy to EP mode\n");
		if (dev->pm_domain) {
			pcie_pd = pd_to_genpd(dev->pm_domain);
			pcie_pd->flags |= GENPD_FLAG_ALWAYS_ON;
		}
	}

	if (pcie_phy->mode == PHY_MODE_PCIE_RC) {
		if (pcie_phy->phy->id == PCIE_PHY_0_NUM) {
			err = pcie_fetch_efuse_scatter(pcie_phy);
			if (err)
				return -EINVAL;
		}

		if (pcie_phy->phy->id > PCIE_PHY_0_NUM) {
			err = pcie_fetch_efuse_unify(pcie_phy);
			if (err)
				return -EINVAL;
		}
	}

	return 0;
}

static int mtk_pcie_phy_power_on(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int val, ret;

	pm_runtime_get_sync(pcie_phy->dev);

	if (pcie_phy->mode == PHY_MODE_PCIE_RC) {
		val = readl(pcie_phy->ckm_base + PCIE_PHY_CKM_REG04);
		val &= ~GENMASK(11, 10);
		val |= CKM_PT0_CKTX_OFFSET(0x2);
		writel(val, pcie_phy->ckm_base + PCIE_PHY_CKM_REG04);
	}

	/* provide pipe clock to pcie mac */
	if (pcie_phy->pipe_clk) {
		ret = clk_prepare_enable(pcie_phy->pipe_clk);
		if (ret) {
			dev_err(pcie_phy->dev, "failed to enable pipe_clk\n");
			return ret;
		}
	}

	return 0;
}

static int mtk_pcie_phy_power_off(struct phy *phy)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);

	if (pcie_phy->pipe_clk)
		clk_disable_unprepare(pcie_phy->pipe_clk);

	pm_runtime_put_sync(pcie_phy->dev);

	return 0;
}

static int mtk_pcie_phy_exit(struct phy *phy)
{
	return 0;
}

static int mtk_pcie_phy_set_mode(struct phy *phy, enum phy_mode mode)
{
	struct mtk_pcie_phy *pcie_phy = phy_get_drvdata(phy);

	pcie_phy->mode = mode;

	return 0;
}

static const struct phy_ops mtk_pcie_phy_ops = {
	.init		= mtk_pcie_phy_init,
	.exit		= mtk_pcie_phy_exit,
	.power_on	= mtk_pcie_phy_power_on,
	.power_off	= mtk_pcie_phy_power_off,
	.set_mode	= mtk_pcie_phy_set_mode,
	.owner		= THIS_MODULE,
};

static struct phy *mtk_pcie_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct mtk_pcie_phy *pcie_phy = dev_get_drvdata(dev);

	if (!pcie_phy)
		return NULL;

	return pcie_phy->phy;
}

static int mtk_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *provider;
	struct resource *reg_res;
	struct mtk_pcie_phy *pcie_phy;

	pcie_phy = devm_kzalloc(dev, sizeof(*pcie_phy), GFP_KERNEL);
	if (!pcie_phy)
		return -ENOMEM;

	pcie_phy->dev = dev;
	pcie_phy->pipe_clk = devm_clk_get(pcie_phy->dev, "pipe_clk");
	if (IS_ERR(pcie_phy->pipe_clk)) {
		dev_warn(dev, "pcie pipe_clk not found\n");
		pcie_phy->pipe_clk = NULL;
	}

	pm_runtime_enable(pcie_phy->dev);

	reg_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy-sif");
	pcie_phy->sif_base = devm_ioremap_resource(dev, reg_res);
	if (IS_ERR(pcie_phy->sif_base)) {
		dev_warn(dev, "failed to map pcie phy base\n");
		return PTR_ERR(pcie_phy->sif_base);
	}

	reg_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy-ckm");
	pcie_phy->ckm_base = devm_ioremap_resource(dev, reg_res);
	if (IS_ERR(pcie_phy->ckm_base)) {
		dev_warn(dev, "failed to map pcie phy base\n");
		return PTR_ERR(pcie_phy->ckm_base);
	}

	platform_set_drvdata(pdev, pcie_phy);

	pcie_phy->phy = devm_phy_create(dev, dev->of_node, &mtk_pcie_phy_ops);
	if (IS_ERR(pcie_phy->phy)) {
		dev_err(dev, "failed to create pcie phy\n");
		return PTR_ERR(pcie_phy->phy);
	}

	phy_set_drvdata(pcie_phy->phy, pcie_phy);

	provider = devm_of_phy_provider_register(dev, mtk_pcie_phy_xlate);

	return PTR_ERR_OR_ZERO(provider);
}

static const struct of_device_id mtk_pcie_phy_id_table[] = {
	{ .compatible = "mediatek,mt6880-pcie-phy" },
	{ },
};

static struct platform_driver mtk_pcie_phy_driver = {
	.probe	= mtk_pcie_phy_probe,
	.driver	= {
		.name	= "mtk-pcie-phy",
		.of_match_table = mtk_pcie_phy_id_table,
	},
};

module_platform_driver(mtk_pcie_phy_driver);
MODULE_LICENSE("GPL v2");
