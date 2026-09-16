/*
 * MediaTek display pulse-width-modulation controller driver.
 * Copyright (c) 2015 MediaTek Inc.
 * Author: YH Huang <yh.huang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/delay.h>
/* BEGIN: Support PWM output, modify by fuhua.huang */
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
/* END: Support PWM output, modify by fuhua.huang */

#define DISP_PWM_EN		0x00

#define PWM_CLKDIV_SHIFT	16
#define PWM_CLKDIV_MAX		0x3ff
#define PWM_CLKDIV_MASK		(PWM_CLKDIV_MAX << PWM_CLKDIV_SHIFT)

#define PWM_PERIOD_BIT_WIDTH	12
#define PWM_PERIOD_MASK		((1 << PWM_PERIOD_BIT_WIDTH) - 1)

#define PWM_HIGH_WIDTH_SHIFT	16
#define PWM_HIGH_WIDTH_MASK	(0x1fff << PWM_HIGH_WIDTH_SHIFT)

#undef pr_fmt
#define pr_fmt(fmt) "[mtk_disp_pwm]" fmt

/* BEGIN: Support PWM output, modify by fuhua.huang */
#define DISP_PWM_DEV_NAME "mediatek-disp-pwm"
/* END: Support PWM output, modify by fuhua.huang */

struct mtk_pwm_data {
	u32 enable_mask;
	unsigned int con0;
	u32 con0_sel;
	unsigned int con1;

	bool has_commit;
	unsigned int commit;
	unsigned int commit_mask;

	unsigned int bls_debug;
	u32 bls_debug_mask;
};

struct mtk_disp_pwm {
	struct pwm_chip chip;
	const struct mtk_pwm_data *data;
	struct clk *clk_main;
	struct clk *clk_mm;
	struct clk *clk_source;
	void __iomem *base;
	void __iomem *pmw_src_addr;
	bool pwm_src_enabled;
	bool pwm_src_set;
};

/* BEGIN: Support PWM output, modify by fuhua.huang */
typedef struct {
	dev_t dev_id;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct pwm_chip *chip;
} disp_pwm_dev_t;

static disp_pwm_dev_t disp_pwm_dev;
/* END: Support PWM output, modify by fuhua.huang */

static inline struct mtk_disp_pwm *to_mtk_disp_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct mtk_disp_pwm, chip);
}

static void mtk_disp_pwm_update_bits(struct mtk_disp_pwm *mdp, u32 offset,
				     u32 mask, u32 data)
{
	void __iomem *address = mdp->base + offset;
	u32 value;

	value = readl(address);
	value &= ~mask;
	value |= data;
	writel(value, address);
}

static int get_pwm_src_base(struct device *dev, struct mtk_disp_pwm *mdp)
{
	int ret = 0;
	struct device_node *node;
	void __iomem *pmw_src_base;
	u32 addr_offset = 0;

	node = of_parse_phandle(dev->of_node, "pwm_src_base", 0);
	if (!node) {
		dev_info(dev, "find pwm_src node failed\n");
		return -1;
	}
	pmw_src_base = of_iomap(node, 0);
	if (!pmw_src_base) {
		dev_info(dev, "find pwm_src address failed\n");
		of_node_put(node);
		return -1;
	}
	ret = of_property_read_u32(dev->of_node, "pwm_src_addr", &addr_offset);
	if (ret >= 0)
		mdp->pmw_src_addr = pmw_src_base + addr_offset;

	dev_info(dev, "get pwm_src_addr=%x\n", addr_offset);
	of_node_put(node);
	return ret;
}

static int pwm_src_power_on(struct mtk_disp_pwm *mdp)
{
	u32 regosc;

	if (!mdp->pmw_src_addr || mdp->pwm_src_enabled)
		return 0;

	mdp->pwm_src_enabled = true;
	regosc = readl(mdp->pmw_src_addr);

	regosc = regosc | 0x1;
	writel(regosc, mdp->pmw_src_addr);
	udelay(150);

	regosc = readl(mdp->pmw_src_addr);
	regosc = regosc | 0x4;
	writel(regosc, mdp->pmw_src_addr);
	regosc = readl(mdp->pmw_src_addr);

	return 0;
}

static int pwm_src_power_off(struct mtk_disp_pwm *mdp)
{
	u32 regosc;

	if (!mdp->pmw_src_addr || !mdp->pwm_src_enabled)
		return 0;

	mdp->pwm_src_enabled = false;
	regosc = readl(mdp->pmw_src_addr);

	regosc = regosc & (~0x4);
	writel(regosc, mdp->pmw_src_addr);

	udelay(150);
	regosc = readl(mdp->pmw_src_addr);

	regosc = regosc & (~0x1);
	writel(regosc, mdp->pmw_src_addr);
	regosc = readl(mdp->pmw_src_addr);

	return 0;
}

static int mtk_disp_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			       int duty_ns, int period_ns)
{
	struct mtk_disp_pwm *mdp = to_mtk_disp_pwm(chip);
	u32 clk_div, period, high_width, value;
	u64 div, rate;
	int err;

	/*
	 * Find period, high_width and clk_div to suit duty_ns and period_ns.
	 * Calculate proper div value to keep period value in the bound.
	 *
	 * period_ns = 10^9 * (clk_div + 1) * (period + 1) / PWM_CLK_RATE
	 * duty_ns = 10^9 * (clk_div + 1) * high_width / PWM_CLK_RATE
	 *
	 * period = (PWM_CLK_RATE * period_ns) / (10^9 * (clk_div + 1)) - 1
	 * high_width = (PWM_CLK_RATE * duty_ns) / (10^9 * (clk_div + 1))
	 */
	dev_info(mdp->chip.dev, "duty=%d period=%d\n", duty_ns, period_ns);
	rate = clk_get_rate(mdp->clk_main);
	clk_div = div_u64(rate * period_ns, NSEC_PER_SEC) >>
			  PWM_PERIOD_BIT_WIDTH;
	if (clk_div > PWM_CLKDIV_MAX)
		return -EINVAL;
	dev_info(mdp->chip.dev, "rate=%lld clk_div=%d\n", rate, clk_div);

	div = NSEC_PER_SEC * (clk_div + 1);
	period = div64_u64(rate * period_ns, div);
	if (period > 0)
		period--;

	high_width = div64_u64(rate * duty_ns, div);
	value = period | (high_width << PWM_HIGH_WIDTH_SHIFT);
	dev_info(mdp->chip.dev, "high_width=%d period=%d\n",
		high_width, period);

	pwm_src_power_on(mdp);

	err = clk_prepare_enable(mdp->clk_main);
	if (err < 0)
		return err;

	err = clk_prepare_enable(mdp->clk_mm);
	if (err < 0) {
		clk_disable_unprepare(mdp->clk_main);
		return err;
	}

	mtk_disp_pwm_update_bits(mdp, mdp->data->con0,
				 PWM_CLKDIV_MASK,
				 clk_div << PWM_CLKDIV_SHIFT);
	mtk_disp_pwm_update_bits(mdp, mdp->data->con1,
				 PWM_PERIOD_MASK | PWM_HIGH_WIDTH_MASK,
				 value);

	if (mdp->data->has_commit) {
		mtk_disp_pwm_update_bits(mdp, mdp->data->commit,
					 mdp->data->commit_mask,
					 mdp->data->commit_mask);
		mtk_disp_pwm_update_bits(mdp, mdp->data->commit,
					 mdp->data->commit_mask,
					 0x0);
	}

	clk_disable_unprepare(mdp->clk_mm);
	clk_disable_unprepare(mdp->clk_main);

	return 0;
}

static int mtk_disp_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_disp_pwm *mdp = to_mtk_disp_pwm(chip);
	int err;

	dev_info(mdp->chip.dev, "%s\n", __func__);
	err = clk_prepare_enable(mdp->clk_main);
	if (err < 0)
		return err;

	err = clk_prepare_enable(mdp->clk_mm);
	if (err < 0) {
		clk_disable_unprepare(mdp->clk_main);
		return err;
	}

	pwm_src_power_on(mdp);
	if (mdp->pwm_src_set != true) {
		if (!IS_ERR(mdp->clk_source)) {
			err = clk_set_parent(mdp->clk_mm, mdp->clk_source);
				if (err < 0) {
					dev_info(mdp->chip.dev, "no pwm_src\n");
				} else {
					dev_info(mdp->chip.dev, "select clk_mm with pwm_src\n");
				}
		}
		mdp->pwm_src_set = true;
	}

	mtk_disp_pwm_update_bits(mdp, DISP_PWM_EN, mdp->data->enable_mask,
				 mdp->data->enable_mask);

	return 0;
}

static void mtk_disp_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_disp_pwm *mdp = to_mtk_disp_pwm(chip);

	dev_info(mdp->chip.dev, "%s\n", __func__);
	mtk_disp_pwm_update_bits(mdp, DISP_PWM_EN, mdp->data->enable_mask,
				 0x0);

	clk_disable_unprepare(mdp->clk_mm);
	clk_disable_unprepare(mdp->clk_main);

	pwm_src_power_off(mdp);
}

static const struct pwm_ops mtk_disp_pwm_ops = {
	.config = mtk_disp_pwm_config,
	.enable = mtk_disp_pwm_enable,
	.disable = mtk_disp_pwm_disable,
	.owner = THIS_MODULE,
};

/* BEGIN: Support PWM output, modify by fuhua.huang */
static int disp_pwm_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t disp_pwm_write(struct file *file, const char __user *buf, size_t cnt, loff_t *off)
{
	int ret;
	typedef struct {
		unsigned int duty_ns;
		unsigned int period_ns;
	} disp_pwm_config_t;
	disp_pwm_config_t cfg = {0};
	if (cnt > sizeof(disp_pwm_config_t)) {
		dev_err(disp_pwm_dev.chip->dev, "Overflow when data length exceeds %ld\n", sizeof(disp_pwm_config_t));
		return -EFAULT;
	}

	if (copy_from_user((void *)&cfg, buf, cnt)) {
		dev_err(disp_pwm_dev.chip->dev, "copy_from_user failed\n");
		return -EFAULT;
	}

	mtk_disp_pwm_ops.disable(disp_pwm_dev.chip, NULL);

	ret = mtk_disp_pwm_ops.config(disp_pwm_dev.chip, NULL, cfg.duty_ns, cfg.period_ns);
	if (ret != 0) {
		dev_err(disp_pwm_dev.chip->dev, "mtk_disp_pwm_config error,ret:%d\n", ret);
		return ret;
	}

	if (cfg.duty_ns != 0) {
		ret = mtk_disp_pwm_ops.enable(disp_pwm_dev.chip, NULL);
		if (ret != 0) {
			dev_err(disp_pwm_dev.chip->dev, "mtk_disp_pwm_enable error,ret:%d\n", ret);
		}
	} else {
		mtk_disp_pwm_ops.disable(disp_pwm_dev.chip, NULL);
	}

	return ret;
}

static struct file_operations disp_pwm_fops = {
	.owner = THIS_MODULE,
	.open = disp_pwm_open,
	.write = disp_pwm_write,
};
/* END: Support PWM output, modify by fuhua.huang */

static int mtk_disp_pwm_probe(struct platform_device *pdev)
{
	struct mtk_disp_pwm *mdp;
	struct resource *r;
	int ret;

	mdp = devm_kzalloc(&pdev->dev, sizeof(*mdp), GFP_KERNEL);
	if (!mdp)
		return -ENOMEM;

	mdp->data = of_device_get_match_data(&pdev->dev);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mdp->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(mdp->base))
		return PTR_ERR(mdp->base);

	mdp->clk_main = devm_clk_get(&pdev->dev, "main");
	if (IS_ERR(mdp->clk_main))
		return PTR_ERR(mdp->clk_main);

	mdp->clk_mm = devm_clk_get(&pdev->dev, "mm");
	if (IS_ERR(mdp->clk_mm))
		return PTR_ERR(mdp->clk_mm);

	mdp->clk_source = devm_clk_get(&pdev->dev, "pwm_src");
	get_pwm_src_base(&pdev->dev, mdp);
	if (!IS_ERR(mdp->clk_source)) {
		dev_info(&pdev->dev, "get clk_mm with pwm_src\n");
	}

	mdp->chip.dev = &pdev->dev;
	mdp->chip.ops = &mtk_disp_pwm_ops;
	mdp->chip.base = -1;
	mdp->chip.npwm = 1;
	mdp->pwm_src_set = false;

	ret = pwmchip_add(&mdp->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, mdp);

	/*
	 * For MT2701, disable double buffer before writing register
	 * and select manual mode and use PWM_PERIOD/PWM_HIGH_WIDTH.
	 */
	if (!mdp->data->has_commit) {
		mtk_disp_pwm_update_bits(mdp, mdp->data->bls_debug,
					 mdp->data->bls_debug_mask,
					 mdp->data->bls_debug_mask);
		mtk_disp_pwm_update_bits(mdp, mdp->data->con0,
					 mdp->data->con0_sel,
					 mdp->data->con0_sel);
	}
/* BEGIN: Support PWM output, modify by fuhua.huang */
	disp_pwm_dev.chip = &mdp->chip;

	ret = alloc_chrdev_region(&disp_pwm_dev.dev_id, 0, 1, DISP_PWM_DEV_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to allocate char dev region,ret:%d\n", ret);
		return ret;
	}
	disp_pwm_dev.cdev.owner = THIS_MODULE;
	cdev_init(&disp_pwm_dev.cdev, &disp_pwm_fops);

	cdev_add(&disp_pwm_dev.cdev, disp_pwm_dev.dev_id, 1);

	disp_pwm_dev.class = class_create(THIS_MODULE, DISP_PWM_DEV_NAME);
	if (disp_pwm_dev.class == NULL) {
		dev_err(&pdev->dev, "create class error\n");
		goto dev_destroy;
	}

	disp_pwm_dev.device = device_create(disp_pwm_dev.class, NULL, disp_pwm_dev.dev_id, NULL, DISP_PWM_DEV_NAME);
	if (disp_pwm_dev.device == NULL) {
		dev_err(&pdev->dev, "create device error\n");
		goto class_destroy;
	}
	return 0;
class_destroy:
	class_destroy(disp_pwm_dev.class);
dev_destroy:
	cdev_del(&disp_pwm_dev.cdev);
	unregister_chrdev_region(disp_pwm_dev.dev_id, 1);
	return -1;
}

static int mtk_disp_pwm_remove(struct platform_device *pdev)
{
	device_destroy(disp_pwm_dev.class, disp_pwm_dev.dev_id);
	class_destroy(disp_pwm_dev.class);
	cdev_del(&disp_pwm_dev.cdev);
	unregister_chrdev_region(disp_pwm_dev.dev_id, 1);
/* END: Support PWM output, modify by fuhua.huang */

	struct mtk_disp_pwm *mdp = platform_get_drvdata(pdev);
	int ret;

	ret = pwmchip_remove(&mdp->chip);

	return ret;
}

static const struct mtk_pwm_data mt2701_pwm_data = {
	.enable_mask = BIT(16),
	.con0 = 0xa8,
	.con0_sel = 0x2,
	.con1 = 0xac,
	.has_commit = false,
	.bls_debug = 0xb0,
	.bls_debug_mask = 0x3,
};

static const struct mtk_pwm_data mt8173_pwm_data = {
	.enable_mask = BIT(0),
	.con0 = 0x10,
	.con0_sel = 0x0,
	.con1 = 0x14,
	.has_commit = true,
	.commit = 0x8,
	.commit_mask = 0x1,
};

static const struct mtk_pwm_data mt6799_pwm_data = {
	.enable_mask = BIT(0),
	.con0 = 0x18,
	.con0_sel = 0x0,
	.con1 = 0x1C,
	.has_commit = true,
	.commit = 0xC,
	.commit_mask = 0x1,
};

static const struct of_device_id mtk_disp_pwm_of_match[] = {
	{.compatible = "mediatek,mt2701-disp-pwm", .data = &mt2701_pwm_data},
	{.compatible = "mediatek,mt6595-disp-pwm", .data = &mt8173_pwm_data},
	{.compatible = "mediatek,mt6779-disp-pwm", .data = &mt6799_pwm_data},
	{.compatible = "mediatek,mt8173-disp-pwm", .data = &mt8173_pwm_data},
	{.compatible = "mediatek,mt6880-disp-pwm", .data = &mt6799_pwm_data},
	{.compatible = "mediatek,mt6890-disp-pwm", .data = &mt6799_pwm_data},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_disp_pwm_of_match);

static struct platform_driver mtk_disp_pwm_driver = {
	.driver = {
		.name = "mediatek-disp-pwm",
		.of_match_table = mtk_disp_pwm_of_match,
	},
	.probe = mtk_disp_pwm_probe,
	.remove = mtk_disp_pwm_remove,
};
module_platform_driver(mtk_disp_pwm_driver);

MODULE_AUTHOR("YH Huang <yh.huang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display PWM driver");
MODULE_LICENSE("GPL v2");
