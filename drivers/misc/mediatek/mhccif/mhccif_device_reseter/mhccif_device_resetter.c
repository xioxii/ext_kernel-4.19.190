// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <mt-plat/mtk_mhccif.h>

static struct workqueue_struct *mhccif_dev_rst_wq;
static void mhccif_dev_rst_work_handler(struct work_struct *work);
static DECLARE_WORK(mhccif_dev_rst_work, mhccif_dev_rst_work_handler);

static void mhccif_dev_rst_work_handler(struct work_struct *work)
{
	pr_info("[%s] trigger kernel restart\n", __func__);
	kernel_restart("pcie");
}

static irqreturn_t mhccif_device_resetter_isr(int irq, void *data)
{
	struct workqueue_struct *wq;

	wq = (struct workqueue_struct *)data;
	if (wq) {
		queue_work(wq, &mhccif_dev_rst_work);
	} else {
		pr_err("[%s] no wq\n", __func__);
		kernel_restart("pcie");
	}

	return IRQ_HANDLED;
}

static int mhccif_device_resetter_register_pcie_isr(void)
{
	u32 alloc_irq_id;
	u32 reset_irq_id = MHCCIF_H2D_INT_DEVICE_RESET;
	int ret = 0;

	alloc_irq_id = mtk_mhccif_alloc_irq(reset_irq_id);
	ret = request_irq(alloc_irq_id, mhccif_device_resetter_isr,
			  IRQF_SHARED, "mhccif_reset_irq",
			  (void *)mhccif_dev_rst_wq);

	if (ret) {
		pr_err("[%s] request mhccif_reset_irq: %d error %d\n",
		       __func__, alloc_irq_id, ret);
	}
	return ret;
}

static int __init mhccif_device_resetter_init(void)
{
	int ret = 0;

	mhccif_dev_rst_wq = create_singlethread_workqueue("mhccif_dev_rst");
	if (!mhccif_dev_rst_wq) {
		pr_err("[%s] workqueue create fail\n", __func__);
		return -EINVAL;
	}
	ret = mhccif_device_resetter_register_pcie_isr();

	pr_info("[%s] done:%d\n", __func__, ret);
	return ret;
}

static void __exit mhccif_device_resetter_exit(void)
{
	if (mhccif_dev_rst_wq) {
		flush_workqueue(mhccif_dev_rst_wq);
		destroy_workqueue(mhccif_dev_rst_wq);
	}
}

module_init(mhccif_device_resetter_init);
module_exit(mhccif_device_resetter_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mhccif device resetter v1.0");
MODULE_AUTHOR("Jerry-ch Chen <jerry-ch.chen@mediatek.com>");
