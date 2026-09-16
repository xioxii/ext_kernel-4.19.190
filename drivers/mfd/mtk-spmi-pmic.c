// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/spmi.h>

struct mtk_spmi_pmic_data {
	unsigned int max_register;
	unsigned int check_reg;
};

#define MTK_SPMI_PMIC_DATA(chip, _max_register, _check_reg)	\
static const struct mtk_spmi_pmic_data chip##_data =		\
{						\
	.max_register = _max_register,		\
	.check_reg = _check_reg,		\
}						\

MTK_SPMI_PMIC_DATA(common, 0xFFFF, 0x39E);
MTK_SPMI_PMIC_DATA(mt6315, 0x16D0, 0x3A5);
MTK_SPMI_PMIC_DATA(mt6330, 0x2300, 0x39E);

static const struct of_device_id mtk_spmi_pmic_id_table[] = {
	{ .compatible = "mtk,spmi-pmic", .data = &common_data },
	{ .compatible = "mediatek,mt6315", .data = &mt6315_data },
	{ .compatible = "mediatek,mt6330_fpga", .data = &mt6330_data },
	{ }
};

static int mtk_spmi_pmic_rw_test(struct regmap *map,
				 const struct mtk_spmi_pmic_data *data)
{
	unsigned int rdata = 0, backup;
	u8 wdata[] = {0xAB, 0xCD};
	int i, ret;

	ret = regmap_read(map, data->check_reg, &rdata);
	if (ret)
		return -EIO;
	backup = rdata;

	for (i = 0; i < ARRAY_SIZE(wdata); i++) {
		ret = regmap_write(map, data->check_reg, wdata[i]);
		ret |= regmap_read(map, data->check_reg, &rdata);
		if (ret || (rdata != wdata[i])) {
			pr_notice("%s fail, rdata(0x%x)!=wdata(0x%x)\n",
				  __func__, rdata, wdata[i]);
			return -EIO;
		}
	}
	ret = regmap_write(map, data->check_reg, backup);
	return 0;
}

static int mtk_spmi_pmic_probe(struct spmi_device *sdev)
{
	struct regmap_config spmi_regmap_config;
	struct regmap *regmap;
	const struct of_device_id *match;
	const struct mtk_spmi_pmic_data *data;
	int ret;

	match = of_match_device(mtk_spmi_pmic_id_table, &sdev->dev);
	if (!match)
		return -ENODEV;
	data = (struct mtk_spmi_pmic_data *)match->data;
	dev_notice(&sdev->dev, "%s usid:%d\n", match->compatible, sdev->usid);

	memset(&spmi_regmap_config, 0, sizeof(struct regmap_config));
	spmi_regmap_config.reg_bits = 16;
	spmi_regmap_config.val_bits = 8;
	spmi_regmap_config.fast_io = true;
	spmi_regmap_config.max_register = data->max_register;
	regmap = devm_regmap_init_spmi_ext(sdev, &spmi_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);
	ret = mtk_spmi_pmic_rw_test(regmap, data);
	if (ret)
		return ret;

	return devm_of_platform_populate(&sdev->dev);
}

MODULE_DEVICE_TABLE(of, mtk_spmi_pmic_id_table);

static struct spmi_driver mtk_spmi_pmic_driver = {
	.probe = mtk_spmi_pmic_probe,
	.driver = {
		.name = "mtk-spmi-pmic",
		.of_match_table = mtk_spmi_pmic_id_table,
	},
};
module_spmi_driver(mtk_spmi_pmic_driver);

MODULE_DESCRIPTION("Mediatek SPMI PMIC driver");
MODULE_ALIAS("spmi:spmi-pmic");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Argus Lin <argus.lin@mediatek.com>");
