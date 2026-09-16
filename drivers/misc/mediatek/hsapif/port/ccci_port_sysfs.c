// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>   /* needed by miscdevice* */

#include "ccci_port_t.h"
#include "ccci_port_base.h"
#include "ccci_debug.h"
#include "ccci_ioctrl_def.h"
#include "ccci_msg_center.h"
#include "ccci_state_mgr.h"
#include "ccci_comm_config.h"
#include "ccci_fsm_cldma.h"

#define TAG "sysfs"

/* default value of enable CLDMA log */
int cldma_log_enable;

/*
 * Showing funciton once the sys dev file has been 'cat'
 *
 */
static ssize_t cldma_log_enable_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", cldma_log_enable);
}

/*
 * Input funciton once the sys dev file has been 'echo'
 *
 */
static ssize_t cldma_log_enable_ctrl(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		cldma_log_enable = value;
		CCCI_NORMAL_LOG(-1, TAG,
		"[%s] 1:enable, 0:disable = %d\n",
		__func__, cldma_log_enable);
	}
	return n;
}

DEVICE_ATTR(cldma_log_enable, 0644, cldma_log_enable_show, cldma_log_enable_ctrl);

const struct file_operations cldma_log_ops = {
	.owner = THIS_MODULE,
	//.read = ,
	//.open =,
};

static struct miscdevice cldma_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "cldma",
	.fops = &cldma_log_ops
};

/*
 * register /dev and /sys files
 * @return:     0: success, otherwise: fail
 */
int create_cldma_files(void)
{
	int ret;

	ret = misc_register(&cldma_device);
	if (unlikely(ret != 0)) {
		pr_err("[CLDMA] misc register failed\n");
		return ret;
	}

	ret = device_create_file(cldma_device.this_device
					, &dev_attr_cldma_log_enable);

	if (unlikely(ret != 0))
		return ret;

	return 0;
}

