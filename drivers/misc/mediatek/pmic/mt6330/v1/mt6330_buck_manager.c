// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/spmi.h>
#include <linux/mfd/mt6330/core.h>
#include <linux/mfd/mt6330/registers.h>
#include <mt-plat/mtk_ccci_common.h>

struct mt6330_buck_manager_data {
	struct device *dev;
	struct regmap *regmap;
	struct spmi_device *sdev;
	u32 vmd11;
	u32 vsram_md;
	u32 vrfdig;
	bool md_init;
};

/* for MD ccci api */
static struct mt6330_buck_manager_data *g_data;


/* [Export MD CCCI API] */
void mt6330_vmd1_pmic_setting_on(void)
{
	struct mt6330_buck_manager_data *data = g_data;
	int ret;

	if (!data) {
		pr_notice("%s: data uninitialize, adjust probe order\n",
			__func__);
		return;
	}
	dev_info(data->dev, "%s\n", __func__);

	/* first time read VMODEM(VNR 2-phase)/VSRAM_MD voltage */
	if (!data->md_init) {
		ret = regmap_read(data->regmap,
				  PMIC_RG_BUCK_VMD11_VOSEL_ADDR, &data->vmd11);
		ret |= regmap_read(data->regmap,
				   PMIC_RG_BUCK_VSRAM_MD_VOSEL_ADDR, &data->vsram_md);
		ret |= regmap_read(data->regmap,
				   PMIC_RG_BUCK_VRFDIG_VOSEL_ADDR, &data->vrfdig);
		if (ret < 0) {
			dev_err(data->dev,
				"%s: read MD buck voltage fail\n", __func__);
			return;
		}
		dev_info(data->dev, "%s: [vosel]vmd11=0x%x, vsram_md=0x%x, vrfdig=0x%x\n",
			 __func__, data->vmd11, data->vsram_md, data->vrfdig);
		data->md_init = true;
		return;
	}

	/* secondary reset MD power */
	/* set buck 1/2/4 spmi control by SPMI_P */
	/* Reset VMODEM(VNR 2-phase)/VSRAM_MD default voltage */
	ret = regmap_write(data->regmap,
			   PMIC_RG_BUCK_VMD11_VOSEL_ADDR, data->vmd11);
	if (ret < 0)
		dev_err(data->dev, "%s: set VMD11 power fail\n", __func__);
	ret = regmap_write(data->regmap,
			   PMIC_RG_BUCK_VSRAM_MD_VOSEL_ADDR, data->vsram_md);
	if (ret < 0)
		dev_err(data->dev, "%s: set VSRAM_MD power fail\n", __func__);
	ret = regmap_write(data->regmap,
			   PMIC_RG_BUCK_VRFDIG_VOSEL_ADDR, data->vrfdig);
	if (ret < 0)
		dev_err(data->dev, "%s: set VRFDIG power fail\n", __func__);
	dev_info(data->dev,
		 "%s: reset vosel: vmd11=0x%x, vsram_md=0x%x, vrfdig=0x%x\n",
		 __func__, data->vmd11, data->vsram_md, data->vrfdig);
}

static int mt6330_spmi_reg_read(void *context,
				unsigned int reg, unsigned int *val)
{
	struct mt6330_buck_manager_data *data = context;
	u8 regval = 0;
	int ret;

	ret = spmi_ext_register_readl(data->sdev, reg, &regval, 1);
	if (ret < 0)
		return ret;

	*val = regval;

	return 0;
}

static int mt6330_spmi_reg_write(void *context,
				 unsigned int reg, unsigned int val)
{
	struct mt6330_buck_manager_data *data = context;
	int ret;

	ret = spmi_ext_register_writel(data->sdev, reg, (u8 *)&val, 1);
	if (ret)
		return ret;

	return 0;
}

static const struct regmap_config spmi_regmap_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0x22ff,
	.fast_io	= true,
	.use_single_rw  = true,
	.reg_read	= mt6330_spmi_reg_read,
	.reg_write	= mt6330_spmi_reg_write,
};

static int mt6330_buck_manager_probe(struct spmi_device *sdev)
{
	struct mt6330_buck_manager_data *data;
	struct regmap *regmap;
	int ret = 0;

	dev_info(&sdev->dev, "%s\n", __func__);

	data = devm_kzalloc(&sdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->sdev = sdev;
	data->dev = &sdev->dev;
	spmi_device_set_drvdata(sdev, data);

	regmap = devm_regmap_init(&sdev->dev, NULL, data, &spmi_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);
	data->regmap = regmap;

	data->vmd11 = 0;
	data->vsram_md = 0;
	data->vrfdig = 0;
	data->md_init = false;
	g_data = data;

	dev_info(&sdev->dev, "%s: successfully probe\n", __func__);

	return ret;
}

static const struct of_device_id mt6330_of_match[] = {
	{ .compatible = "mediatek,mt6330-spmi_p", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6330_of_match);

static struct spmi_driver mt6330_p_driver = {
	.driver = {
		.name = "mt6330-spmi_p",
		.of_match_table = of_match_ptr(mt6330_of_match),
	},
	.probe = mt6330_buck_manager_probe,
};
module_spmi_driver(mt6330_p_driver);

MODULE_AUTHOR("Benjamin Chao, MediaTek");
MODULE_DESCRIPTION("MediaTek MT6330 SPMI-P PMIC Driver");
MODULE_LICENSE("GPL");
