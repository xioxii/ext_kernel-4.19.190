// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of_platform.h>
#include <linux/nvmem-consumer.h>

#include "mtk_spower_data.h"
#include "mtk_common_static_power.h"

#define SP_TAG     "[Power/spower] "
#define SPOWER_LOG_NONE		0
#define SPOWER_LOG_WITH_PRINTK	1

/* #define SPOWER_LOG_PRINT SPOWER_LOG_WITH_PRINTK */
#define SPOWER_LOG_PRINT SPOWER_LOG_NONE

#define SPOWER_INFO(fmt, args...)	 pr_info(SP_TAG fmt, ##args)

#if (SPOWER_LOG_PRINT == SPOWER_LOG_NONE)
#define SPOWER_DEBUG(fmt, args...)
#elif (SPOWER_LOG_PRINT == SPOWER_LOG_WITH_PRINTK)
#define SPOWER_DEBUG(fmt, args...)	 pr_debug(SP_TAG fmt, ##args)
#endif

static char static_power_buf[128];

static int static_power_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "%s", static_power_buf);
	return 0;
}

static int static_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, static_power_show, NULL);
}

static const struct file_operations static_power_operations = {
	.open = static_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static unsigned int mtSpowerInited;
int mt_spower_init(void)
{
	int devinfo = 0;
	unsigned int temp_lkg;
	int i;
	char *p_buf = static_power_buf;
	struct platform_device *pdev;
	struct nvmem_device *nvmem_dev;
	struct device_node *node;
	//unsigned int err_flag = 0;
	u32 n_domain = 0;
	const char *domain;
	int ret = 0;
	u32 value[6];

#ifdef SPOWER_NOT_READY
	/* FIX ME */
	return 0;
#endif

	if (mtSpowerInited == 1)
		return 0;

	node = of_find_node_by_name(NULL, "leakage");
	if (!node)
		return -ENODEV;

	pdev = of_device_alloc(node, NULL, NULL);
        if (!pdev)
		return -ENOMEM;

	nvmem_dev = nvmem_device_get(&pdev->dev, "mtk_efuse");
	ret = IS_ERR(nvmem_dev);
	if (ret) {
		ret = PTR_ERR(nvmem_dev);
		goto release_platform_device;
	}

	ret = of_property_read_u32(node, "n-domain", &n_domain);
	if (ret) {
		ret = -ENODEV;
		goto release_nvmem;
	}

	spower_lkg_info = kmalloc(sizeof(struct spower_leakage_info) * n_domain, GFP_KERNEL);
	if (!spower_lkg_info) {
		ret = -ENOMEM;
		goto release_nvmem;
	}

	for (i = 0; i < n_domain; i++) {
		ret = of_property_read_string_index(node, "domain", i, &domain);
		if (ret) {
			ret = -ENODEV;
			goto release_kmalloc;
		}
		ret = of_property_read_u32_array(node, domain, value, ARRAY_SIZE(value));
		if(ret) {
			ret = -ENODEV;
			goto release_kmalloc;
		}
		spower_lkg_info[i].name = domain;
		spower_lkg_info[i].v_of_fuse = value[0];
		spower_lkg_info[i].t_of_fuse = value[1];
		spower_lkg_info[i].devinfo_idx = value[2];
		spower_lkg_info[i].devinfo_offset = value[3];
		spower_lkg_info[i].value = value[4];
		spower_lkg_info[i].instance = value[5];
	}

	for (i = 0; i < n_domain; i++) {
		ret = nvmem_device_read(nvmem_dev, spower_lkg_info[i].devinfo_idx, sizeof(__u32), &devinfo);
		if (ret != sizeof(__u32)) {
			ret = -EINVAL;
			goto release_kmalloc;
		}

		temp_lkg =
			(devinfo >> spower_lkg_info[i].devinfo_offset) & 0xff;
		SPOWER_DEBUG("[Efuse] %s => 0x%x\n", spower_lkg_info[i].name,
				temp_lkg);
		if (temp_lkg != 0) {
			temp_lkg = (int) devinfo_table[temp_lkg];
			spower_lkg_info[i].value =
			(int) (temp_lkg * spower_lkg_info[i].v_of_fuse);
		}
		p_buf += sprintf(p_buf, "%d.%d/",
			DIV_ROUND_CLOSEST(spower_lkg_info[i].value,
			spower_lkg_info[i].instance * 100) / 10,
			DIV_ROUND_CLOSEST(spower_lkg_info[i].value,
			spower_lkg_info[i].instance * 100) % 10
		);
		SPOWER_DEBUG("[Efuse Leakage] %s => 0x%x\n",
			spower_lkg_info[i].name, temp_lkg);
		SPOWER_DEBUG("[Final Leakage] %s => %d\n",
			spower_lkg_info[i].name, spower_lkg_info[i].value);
	}

	p_buf += sprintf(p_buf, "\n");

	SPOWER_INFO("%s", static_power_buf);
	debugfs_create_file("static_power", 0400,
				NULL, NULL,
				&static_power_operations);
	mtSpowerInited = 1;

	ret = 0;

release_kmalloc:
	kfree(spower_lkg_info);
release_nvmem:
	nvmem_device_put(nvmem_dev);
release_platform_device:
	if (pdev != NULL) {
		of_platform_device_destroy(&pdev->dev, NULL);
		of_dev_put(pdev);
	}

	return ret;
}

module_init(mt_spower_init);
