// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <dbgtop.h>
#include <linux/arm-smccc.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <mt-plat/mtk_mhccif.h>

static struct workqueue_struct *mhccif_drm_off_wq;
static void mhccif_drm_off_work_handler(struct work_struct *work);
static DECLARE_WORK(mhccif_drm_off_work, mhccif_drm_off_work_handler);

static void mhccif_drm_off_work_handler(struct work_struct *work)
{
	pr_info("[%s] set drm off\n", __func__);
	mtk_dbgtop_dram_reserved(0);
}

static irqreturn_t mhccif_drm_off_isr(int irq, void *data)
{
	struct workqueue_struct *wq;

	wq = (struct workqueue_struct *)data;
	if (wq) {
		queue_work(wq, &mhccif_drm_off_work);
	} else {
		pr_err("[%s] no drm off wq\n", __func__);
		mtk_dbgtop_dram_reserved(0);
	}

	return IRQ_HANDLED;
}

static int mhccif_drm_off_register_pcie_isr(void)
{
	u32 alloc_irq_id;
	u32 reset_irq_id = MHCCIF_H2D_INT_DRM_DISABLE_AP;
	int ret = 0;

	alloc_irq_id = mtk_mhccif_alloc_irq(reset_irq_id);
	ret = request_irq(alloc_irq_id, mhccif_drm_off_isr,
			  IRQF_SHARED, "mhccif_drm_off_irq",
			  (void *)mhccif_drm_off_wq);

	if (ret) {
		pr_err("[%s] request mhccif_drm_off_irq: %d error %d\n",
		       __func__, alloc_irq_id, ret);
	}
	return ret;
}

static struct kobject *mtk_scdk_kobject;
#define SCDK_MAGIC 	13013

static void mtk_scdk_set_dl_mode(unsigned int seconds)
{
	struct arm_smccc_res res = {0};

	seconds &= 0x3FFF;
	if (!seconds)
		seconds = 1;
	pr_info("[%s] seconds:0x%x\n", __func__, seconds);

	arm_smccc_smc(MTK_SIP_KERNEL_MTK_SCDK, seconds, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		pr_err("[%s] smc set brom fail: 0x%x\n", __func__,
		       (unsigned int)res.a0);
}

static ssize_t mtk_scdk_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned int user_input;
	unsigned int user_magic;
	int ret;

	ret = sscanf(buf, "%u %u", &user_magic, &user_input);
	if (ret != 2) {
		pr_err("[%s] fail to parse cmd: %d\n", __func__, ret);
		return -EINVAL;
	}
	if (user_magic != SCDK_MAGIC) {
		pr_err("[%s] fail to authenticate user, you shall not pass\n", __func__);
		return -EINVAL;
	}

	pr_info("[%s] user_input:%u\n", __func__, user_input);

	mtk_scdk_set_dl_mode(user_input);

	pr_debug("[%s] done\n", __func__);
	return count;
}

static struct kobj_attribute mtk_scdk_attr = {
	.attr = {.name = "mtk_scdk", .mode = 0644 },
	.store = mtk_scdk_store,
};

static int __init mtk_scdk_init(void)
{
	int error = 0;

	mtk_scdk_kobject = kobject_create_and_add("mtk_scdk", kernel_kobj);
	if (!mtk_scdk_kobject)
		return -ENOMEM;

	error = sysfs_create_file(mtk_scdk_kobject, &mtk_scdk_attr.attr);
	if (error)
		pr_err("failed to create the file in /sys/kernel/mtk_scdk\n");

	mhccif_drm_off_wq = create_singlethread_workqueue("mhccif_drm_off");
	if (!mhccif_drm_off_wq) {
		pr_err("[%s] mhccif_drm_off_wq create fail\n", __func__);
		return -EINVAL;
	}
	error |= mhccif_drm_off_register_pcie_isr();

	pr_info("[%s] done:%d\n", __func__, error);
	return error;
}

static void __exit mtk_scdk_exit(void)
{
	kobject_put(mtk_scdk_kobject);
	pr_info("[%s] done\n", __func__);
}

module_init(mtk_scdk_init);
module_exit(mtk_scdk_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mtk scdk v1.0");
