// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yuesheng Wang <yuesheng.wang@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>

#include "mtk_disp_aal.h"

#define AAL_MODULE_NUM 2

struct aal_debug_info {
	struct dentry *dfs_root;
	struct dentry *dfs_aal;
};

static struct aal_debug_info aal_d_info[AAL_MODULE_NUM];

static int aal_debufs_open(struct inode *inode, struct file *filp)
{
	pr_info("%s\n", __func__);
	filp->private_data = inode->i_private;
	return 0;
}

static long aal_unlocked_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct dentry *dent = filp->f_path.dentry;
	int aal_index = 0;
	int ret = 0;

	for (aal_index = 0; aal_index < AAL_MODULE_NUM; aal_index++) {
		if (dent == aal_d_info[aal_index].dfs_aal)
			break;
	}

	if (aal_index == AAL_MODULE_NUM)
		return -1;

	switch (cmd) {
	case DISP_IOCTL_AAL_GET_RESOLUTION:
		{
			pr_info("DISP_IOCTL_AAL_GET_RESOLUTION\n");
			mtk_aal_get_resolution(aal_index, (__user void *)arg);
		}
		break;
	case DISP_IOCTL_AAL_EVENTCTL:
		{
			mtk_aal_eventctl(aal_index, (__user void *)arg);
		}
		break;
	case DISP_IOCTL_AAL_GET_HIST:
		{
			ret = mtk_aal_get_max_hist(aal_index,
					(__user void *)arg);
		}
		break;
	case DISP_IOCTL_AAL_SET_PARAM:
		{
			mtk_aal_set_param(aal_index, (__user void *)arg);
		}
		break;
	case DISP_IOCTL_AAL_INIT_REG:
		{
			pr_info("DISP_IOCTL_AAL_INIT_REG\n");
			mtk_aal_init_reg(aal_index, (__user void *)arg);
		}
		break;
	case DISP_IOCTL_AAL_GET_TUNING_PARAM:
		{
			mtk_aal_get_tuning_parameters(aal_index,
					(__user void *)arg);
		}
		break;
	case DISP_IOCTL_AAL_SET_TUNING_PARAM:
		{
			mtk_aal_set_tuning_parameters(aal_index,
					(__user void *)arg);
		}
		break;
	default:
		pr_info("Unsupport ioctl.\n");
		ret = -1;
		break;
	}

	return ret;
}

static const struct file_operations debufs_fops = {
	.owner = THIS_MODULE,
	.open = aal_debufs_open,
	.unlocked_ioctl = aal_unlocked_ioctl,
};


// static int __init mtk_drm_aal_debugfs_dir_init(void)
// {
// 	int i;
// 	int aal_idx;
// 	char dname[8];
// 	const char *fname;
// 	struct dentry *dent;
// 	struct aal_debug_info *d;
// 	int ret;

// 	pr_info("%s start", __func__);

// 	for (i = 0; i < AAL_MODULE_NUM; i++) {
// 		d = &aal_d_info[i];
// 		aal_idx = i;

// 		sprintf(dname, "aal%d", i);
// 		dent = debugfs_create_dir(dname, NULL);
// 		if (IS_ERR_OR_NULL(dent))
// 			return -ENOENT;
// 		d->dfs_root = dent;
// 		fname = "aal";
// 		dent = debugfs_create_file(fname, 0600,
// 				d->dfs_root, NULL, &debufs_fops);
// 		if (IS_ERR_OR_NULL(dent))
// 			goto error;
// 		d->dfs_aal = dent;

// 		ret = mtk_aal_request_irq(aal_idx);
// 		if (ret != 0) {
// 			pr_info("mtk_aal_request_irq(%d) fail.\n", aal_idx);
// 			goto error;
// 		}
// 	}

// 	return 0;

// error:
// 	debugfs_remove_recursive(d->dfs_root);
// 	d->dfs_root = NULL;

// 	return -ENOENT;
// }

// static void __exit mtk_drm_aal_debugfs_dir_exit(void)
// {
// 	int i;

// 	pr_info("%s start", __func__);
// 	for (i = 0; i < AAL_MODULE_NUM; i++)
// 		debugfs_remove_recursive(aal_d_info[i].dfs_root);
// }


// mt6890
// module_init(mtk_drm_aal_debugfs_dir_init);
// module_exit(mtk_drm_aal_debugfs_dir_exit);
