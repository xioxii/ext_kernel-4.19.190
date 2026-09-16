// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>


#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <iio/mt635x-auxadc.h>
#include <linux/iio/consumer.h>
#include <linux/nvmem-consumer.h>
/*=============================================================
 *Local variable definition
 *=============================================================
 */

/*diff part in pmic, vcore, vproc, vgpu*/
struct pmic_cali_data {
	int cali_factor;
	int slope1;
	int slope2;
	int intercept;
	int iio_chan_id;
	int o_vts;
};

/*common part in pmic, vcore, vproc, vgpu*/
struct pmic_efuse_data {
	int degc_cali;
	int adc_cali_en;
	int o_slope;
	int o_slope_sign;
	int id;
	u16 *efuse_buff;
	int num_efuse;
	int (*pmic_get_efuse)(struct device *dev,
				struct pmic_efuse_data *temp_info,
				struct pmic_cali_data *cali_data);
};

struct pmic_temp_info {
	struct device *dev;
	struct pmic_cali_data *cali_data;
	struct pmic_efuse_data *efuse_data;
	struct iio_channel *chan_temp;
	struct iio_channel *vcore_temp;
	struct iio_channel *vproc_temp;
	struct iio_channel *vgpu_temp;
};


/*=============================================================*/

static int pmic_raw_to_temp(struct pmic_cali_data *cali_data,
						int val)
{
	int t_current;

	int y_curr = val;

	t_current = cali_data->intercept +
		((cali_data->slope1 * y_curr) / (cali_data->slope2));

	return t_current;
}

static int mt6330_read_efuse(struct device *dev, struct pmic_efuse_data *efuse,
	struct pmic_cali_data *cali_data)
{
	int val_l = 0;
	int val_h = 0;
	size_t len;
	struct nvmem_cell *cell_1;
	u16 *buf;
	int i;

/*read efuse once*/
	if (cali_data->iio_chan_id == 0) {
		efuse->efuse_buff = devm_kcalloc(dev, efuse->num_efuse,
				  sizeof(*efuse->efuse_buff), GFP_KERNEL);
		if (!efuse->efuse_buff)
			return -ENOMEM;
		cell_1 = devm_nvmem_cell_get(dev, "t_e_data1@6c");
		if (IS_ERR(cell_1)) {
			dev_err(dev, "Error: Failed to get nvmem cell %s\n",
				"t_e_data1@6c");
			return PTR_ERR(cell_1);
		}
		buf = (u16 *)nvmem_cell_read(cell_1, &len);
		nvmem_cell_put(cell_1);

		if (IS_ERR(buf))
			return PTR_ERR(buf);
		for (i = 0; i < efuse->num_efuse; i++) {
			efuse->efuse_buff[i] = buf[i];
		}
		kfree(buf);		
	}

	if (cali_data->iio_chan_id == 0) {
		val_l = ((efuse->efuse_buff[1] & GENMASK(15, 8)) >> 8);
		val_h = ((efuse->efuse_buff[2] & GENMASK(4, 0)) << 8);
		cali_data->o_vts = (val_l | val_h);
	} else if (cali_data->iio_chan_id == 1) {
		val_l = ((efuse->efuse_buff[2] & GENMASK(15, 8)) >> 8);
		val_h = ((efuse->efuse_buff[3] & GENMASK(4, 0)) << 8);
		cali_data->o_vts = (val_l | val_h);
	}else if (cali_data->iio_chan_id == 2) {
		val_l = ((efuse->efuse_buff[3] & GENMASK(15, 8)) >> 8);
		val_h = ((efuse->efuse_buff[4] & GENMASK(4, 0)) << 8);
		cali_data->o_vts = (val_l | val_h);
	} else if (cali_data->iio_chan_id == 3) {
		val_l = ((efuse->efuse_buff[4] & GENMASK(15, 8)) >> 8);
		val_h = ((efuse->efuse_buff[5] & GENMASK(4, 0)) << 8);
		cali_data->o_vts = (val_l | val_h);
	} else {
		dev_info(dev, "unsupported iio_chan_id%d\n", cali_data->iio_chan_id);
	}
	efuse->degc_cali = ((efuse->efuse_buff[0] & GENMASK(13, 8)) >> 8);
	efuse->adc_cali_en = ((efuse->efuse_buff[0] & BIT(14)) >> 14);
	efuse->o_slope_sign = ((efuse->efuse_buff[1] & BIT(7)) >> 7);
	efuse->o_slope = ((efuse->efuse_buff[1] & GENMASK(6, 1)) >> 1);
	efuse->id = (efuse->efuse_buff[1] & BIT(0));

	return 0;
}

int mtktspmic_get_cali_data(struct device *dev,
				struct pmic_temp_info *pmic_info)
{
	struct pmic_cali_data *cali = pmic_info->cali_data;
	struct pmic_efuse_data *efuse= pmic_info->efuse_data;

	if (efuse->pmic_get_efuse(dev, efuse, cali) < 0)
		return -1;

	if (efuse->id == 0)
		efuse->o_slope = 0;

	/* adc_cali_en=0;//FIX ME */

	if (efuse->adc_cali_en == 0) {	/* no calibration */
		//mtktspmic_info("[pmic_debug]  It isn't calibration values\n");
		cali->o_vts = 1600;
		efuse->degc_cali = 50;
		efuse->o_slope_sign = 0;
		efuse->o_slope = 0;
	}

	if (efuse->degc_cali < 38 || efuse->degc_cali > 60)
		efuse->degc_cali = 53;

	dev_info(dev, "[pmic_debug] o_vts    = 0x%x\n", cali->o_vts);
	dev_info(dev, "[pmic_debug] degc_cali= 0x%x\n", efuse->degc_cali);
	dev_info(dev, "[pmic_debug] adc_cali_en        = 0x%x\n", efuse->adc_cali_en);
	dev_info(dev, "[pmic_debug] o_slope        = 0x%x\n", efuse->o_slope);
	dev_info(dev, "[pmic_debug] o_slope_sign        = 0x%x\n", efuse->o_slope_sign);
	dev_info(dev, "[pmic_debug] id        = 0x%x\n", efuse->id);
	return 0;
}

void mtktspmic_get_temp_convert_params(struct pmic_temp_info *data)
{
	int vbe_t;
	int factor;
	struct pmic_temp_info *temp_info = data;
	struct pmic_cali_data *cali = temp_info->cali_data;
	struct pmic_efuse_data *efuse= temp_info->efuse_data;

	factor = cali->cali_factor;

	cali->slope1 = (100 * 1000 * 10);	/* 1000 is for 0.001 degree */

	if (efuse->o_slope_sign == 0)
		cali->slope2 = -(factor + efuse->o_slope);
	else
		cali->slope2 = -(factor - efuse->o_slope);

	vbe_t = (-1) * ((((cali->o_vts) * 1800)) / 4096) * 1000;


	if (efuse->o_slope_sign == 0)
		cali->intercept = (vbe_t * 1000) / (-(factor + efuse->o_slope * 10));
	/*0.001 degree */
	else
		cali->intercept = (vbe_t * 1000) / (-(factor - efuse->o_slope * 10));
	/*0.001 degree */

	cali->intercept = cali->intercept + (efuse->degc_cali * (1000 / 2));
	/* 1000 is for 0.1 degree */
}
/*==================================================
 * Support chips
 *==================================================
 */

static struct pmic_efuse_data mt6330_pmic_efuse_data = {
	.degc_cali = 50,
	.adc_cali_en = 0,
	.o_slope = 0,
	.o_slope_sign = 0,
	.id = 0,
	.num_efuse = 6,
	.pmic_get_efuse = mt6330_read_efuse,
};

static const struct of_device_id pmic_temp_of_match[] = {
	{
		.compatible = "mediatek,mt6330-pmic-temp",
		.data = (void *)&mt6330_pmic_efuse_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, pmic_temp_of_match);

static int pmic_get_temp(void *data, int *temp)
{
	int val = 0;
	int ret;
	struct pmic_temp_info *temp_info = (struct pmic_temp_info *)data;
	struct pmic_cali_data *cali_data = temp_info->cali_data;
	int iio_chan_id = cali_data->iio_chan_id;

	*temp = -127000;
	if ((iio_chan_id == 0) && (!IS_ERR(temp_info->chan_temp)))
		ret = iio_read_channel_processed(temp_info->chan_temp, &val);
	else if ((iio_chan_id == 1) && (!IS_ERR(temp_info->vcore_temp)))
		ret = iio_read_channel_processed(temp_info->vcore_temp, &val);
	else if ((iio_chan_id == 2) && (!IS_ERR(temp_info->vproc_temp)))
		ret = iio_read_channel_processed(temp_info->vproc_temp, &val);
	else if ((iio_chan_id == 3) && (!IS_ERR(temp_info->vgpu_temp)))
		ret = iio_read_channel_processed(temp_info->vgpu_temp, &val);
	else {
		pr_notice("unsupport iio channel\n");
		*temp = -127000;
		return 0;
	}
	if (ret < 0) {
		pr_notice("pmic_chip_temp read fail, ret=%d\n", ret);
		*temp = -127000;
		dev_info(temp_info->dev, "temp = %d\n", *temp);
		return 0;
	}

	*temp = pmic_raw_to_temp(cali_data, val);
	return 0;
}

static const struct thermal_zone_of_device_ops pmic_temp_ops = {
	.get_temp = pmic_get_temp,
};

static int pmic_temp_parse_cali_data(struct device *dev,
					struct pmic_temp_info *pmic_info)
{
	struct device_node *np = dev->of_node;
	int ret;
	struct pmic_cali_data *cali_data;

	pmic_info->cali_data = devm_kzalloc(dev,
					 sizeof(*pmic_info->cali_data),
					 GFP_KERNEL);
	if (!pmic_info->cali_data)
		return -ENOMEM;
	cali_data= pmic_info->cali_data;

	ret = of_property_read_u32(np, "pmic_temp,cali_factor",
					(unsigned int *)&(cali_data->cali_factor));
	if (ret < 0) {
		dev_info(dev, "Failed to read cali_factor: %d\n",
			ret);
		return ret;
	}
	return 0;
}

static int pmic_temp_parse_iio_channel(struct device *dev,
					struct pmic_temp_info *pmic_info)
{
	struct device_node *np = dev->of_node;
	int ret;
	struct pmic_temp_info *temp_info = pmic_info;
	struct pmic_cali_data *cali_data= temp_info->cali_data;

	ret = of_property_read_u32(np, "pmic_temp,iio_chan",
					&(cali_data->iio_chan_id));
	if (ret < 0) {
		dev_info(dev, "Failed to read iio_channel_index: %d\n",
			ret);
		return ret;
	}

	if (cali_data->iio_chan_id == 0) {
		pmic_info->chan_temp = devm_iio_channel_get(dev, "pmic_chip_temp");
		ret = PTR_ERR_OR_ZERO(pmic_info->chan_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_err("pmic_chip_temp auxadc get fail, ret=%d\n", ret);
				return ret;
		}
	} else if (cali_data->iio_chan_id == 1) {
		pmic_info->vcore_temp = devm_iio_channel_get(dev, "pmic_buck1_temp");
		ret = PTR_ERR_OR_ZERO(pmic_info->vcore_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_err("pmic_chip_temp auxadc get fail, ret=%d\n", ret);
				return ret;
		}
	} else if (cali_data->iio_chan_id == 2) {
		pmic_info->vproc_temp = devm_iio_channel_get(dev, "pmic_buck2_temp");
		ret = PTR_ERR_OR_ZERO(pmic_info->vproc_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_err("pmic_chip_temp auxadc get fail, ret=%d\n", ret);
					return ret;
		}
	} else if (cali_data->iio_chan_id == 3) {
		pmic_info->vgpu_temp = devm_iio_channel_get(dev, "pmic_buck3_temp");
		ret = PTR_ERR_OR_ZERO(pmic_info->vgpu_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_err("pmic_chip_temp auxadc get fail, ret=%d\n", ret);
					return ret;
		}
	} else {
		dev_info(dev, "unsupported iio_channel_index\n");
		return -1;
	}
	return 0;
}

static int pmic_temp_probe(struct platform_device *pdev)
{
	struct pmic_temp_info *pmic_info;
	struct thermal_zone_device *tz_dev;
	int ret;

	if (!pdev->dev.of_node) {
		dev_info(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	pmic_info = devm_kzalloc(&pdev->dev, sizeof(*pmic_info), GFP_KERNEL);
	if (!pmic_info)
		return -ENOMEM;

	pmic_info->dev = &pdev->dev;
	pmic_info->efuse_data= (struct pmic_efuse_data *)
		of_device_get_match_data(&pdev->dev);

	ret = pmic_temp_parse_cali_data(&pdev->dev, pmic_info);
	if (ret < 0)
		return ret;

	ret = pmic_temp_parse_iio_channel(&pdev->dev, pmic_info);
	if (ret < 0)
		return ret;

	ret = mtktspmic_get_cali_data(&pdev->dev, pmic_info);
	if (ret < 0)
		return ret;

	mtktspmic_get_temp_convert_params(pmic_info);

	platform_set_drvdata(pdev, pmic_info);

	tz_dev = devm_thermal_zone_of_sensor_register(
			&pdev->dev, 0, pmic_info, &pmic_temp_ops);
	if (IS_ERR(tz_dev)) {
		ret = PTR_ERR(tz_dev);
		dev_info(&pdev->dev, "Thermal zone sensor register fail:%d\n",
			ret);
		return ret;
	}
	return 0;
}

static struct platform_driver pmic_temp_driver = {
	.probe = pmic_temp_probe,
	.driver = {
		.name = "mtk-pmic-temp",
		.of_match_table = pmic_temp_of_match,
	},
};

module_platform_driver(pmic_temp_driver);

MODULE_AUTHOR("Henry Huang <henry.huang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek pmic temp sensor driver");
MODULE_LICENSE("GPL v2");
