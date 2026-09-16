// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <drm/drmP.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_debugfs.h"
#ifdef CONFIG_MTK_DISPLAY_COLOR
#include "mtk_drm_color.h"
#endif

#include "mtk_disp_aal.h"

struct mtk_drm_debugfs_table {
	enum mtk_ddp_comp_id id;
	char name[8];
	unsigned int offset[2];
	unsigned int length[2];
	unsigned long reg_base;
};

/* ------------------------------------------------------------------------- */
/* External variable declarations */
/* ------------------------------------------------------------------------- */
static void __iomem *gdrm_disp1_base[10];
static void __iomem *gdrm_disp2_base[10];
static void __iomem *gdrm_disp3_base[4];
static struct mtk_drm_debugfs_table gdrm_disp1_reg_range[10] = {
	{ DDP_COMPONENT_OVL0, "OVL0 ", {0, 0xf40}, {0x260, 0x80}, 0},
	{ DDP_COMPONENT_COLOR0, "COLOR0 ", {0x400, 0xc00}, {0x100, 0x100}, 0},
	{ DDP_COMPONENT_AAL0, "AAL0 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_OD0, "OD0 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_RDMA0, "RDMA0 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_DPI0, "DPI0 ", {0, 0xf00}, {0x100, 0x10}, 0},
	{ DDP_COMPONENT_DSI0, "DSI0 ", {0, 0}, {0, 0}, 0},
	{ DDP_COMPONENT_LVDS0, "LVDS0 ", {0xa10, 0}, {0x10, 0}, 0},
	{ DDP_COMPONENT_CONFIG, "CONFIG ", {0, 0}, {0x120, 0}, 0},
	{ DDP_COMPONENT_MUTEX, "MUTEX ", {0, 0}, {0x100, 0}, 0}
};

static struct mtk_drm_debugfs_table gdrm_disp2_reg_range[10] = {
	{ DDP_COMPONENT_OVL1, "OVL1 ", {0, 0xf40}, {0x260, 0x80}, 0},
	{ DDP_COMPONENT_COLOR1, "COLOR1 ", {0x400, 0xc00}, {0x100, 0x100}, 0},
	{ DDP_COMPONENT_AAL1, "AAL1 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_OD1, "OD1 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_RDMA1, "RDMA1 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_DPI1, "DPI1 ", {0, 0}, {0x1000, 0}, 0},
	{ DDP_COMPONENT_DSI2, "DSI2 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_LVDS1, "LVDS1 ", {0xa10, 0}, {0x10, 0}, 0},
	{ DDP_COMPONENT_CONFIG, "CONFIG ", {0, 0}, {0x120, 0}, 0},
	{ DDP_COMPONENT_MUTEX, "MUTEX ", {0, 0}, {0x100, 0}, 0}
};

static struct mtk_drm_debugfs_table gdrm_disp3_reg_range[4] = {
	{ DDP_COMPONENT_RDMA2, "RDMA2 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_DSI3, "DSI3 ", {0, 0}, {0x100, 0}, 0},
	{ DDP_COMPONENT_CONFIG, "CONFIG ", {0, 0}, {0x120, 0}, 0},
	{ DDP_COMPONENT_MUTEX, "MUTEX ", {0, 0}, {0x100, 0}, 0}

};

static bool dbgfs_alpha;

#ifdef CONFIG_MTK_DISPLAY_COLOR
extern unsigned int g_color_bypass;
#endif

static void mtk_read_reg(unsigned long addr)
{
	void __iomem *reg_va = NULL;
	unsigned int offset = sizeof(reg_va);

	if (addr > AAL_SW_REG_BASE && addr < AAL_SW_REG_END) {
		int val;

		mtk_aal_read_reg(addr, &val);
		pr_info("r:0x%08lx = 0x%08x\n", addr, val);
		return;
	}
	if (addr >= 0x14000000 && addr < 0x15000000) {
		reg_va = ioremap_nocache(addr, offset);
		pr_info("r:0x%08lx = 0x%08x\n", addr, readl(reg_va));
		iounmap(reg_va);
		return;
	}
}

static void mtk_write_reg(unsigned long addr, unsigned long val)
{
	void __iomem *reg_va = NULL;
	unsigned int offset = sizeof(reg_va);

	if (addr > AAL_SW_REG_BASE && addr < AAL_SW_REG_END) {
		mtk_aal_write_reg(addr, val);
		return;
	}
	if (addr >= 0x14000000 && addr < 0x15000000) {
		reg_va = ioremap_nocache(addr, offset);
		writel(val, reg_va);
		iounmap(reg_va);
		return;
	}
}

/* ------------------------------------------------------------------------- */
/* Debug Options */
/* ------------------------------------------------------------------------- */
static char STR_HELP[] =
	"\n"
	"USAGE\n"
	"        echo [ACTION]... > mtkdrm\n"
	"\n"
	"ACTION\n"
	"\n"
	"        dump:\n"
	"             dump all hw registers\n"
	"\n"
	"        regw:addr=val\n"
	"             write hw register\n"
	"\n"
	"        regr:addr\n"
	"             read hw register\n";

/* ------------------------------------------------------------------------- */
/* Command Processor */
/* ------------------------------------------------------------------------- */
static void process_dbg_opt(char *opt)
{
	if (strncmp(opt, "regw:", 5) == 0) {
		char *p = (char *)opt + 5;
		char *np;
		unsigned long addr, val;
		u64 i;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr) != 0)
			goto error;

		if (p == NULL)
			goto error;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val) != 0)
			goto error;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
			    0x1000UL) {
				writel(val, gdrm_disp1_base[i] + addr -
					gdrm_disp1_reg_range[i].reg_base);
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr >= gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base +
			    0x1000UL) {
				writel(val, gdrm_disp2_base[i] + addr -
					gdrm_disp2_reg_range[i].reg_base);
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp3_reg_range); i++) {
			if (addr >= gdrm_disp3_reg_range[i].reg_base &&
			    addr < gdrm_disp3_reg_range[i].reg_base +
			    0x1000UL) {
				writel(val, gdrm_disp3_base[i] + addr -
					gdrm_disp3_reg_range[i].reg_base);
				break;
			}
		}

	} else if (strncmp(opt, "regr:", 5) == 0) {
		char *p = (char *)opt + 5;
		unsigned long addr;
		u64 i;

		if (kstrtoul(p, 16, &addr) != 0)
			goto error;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
			    0x1000UL) {
				DRM_INFO("%8s Read register 0x%08lX: 0x%08X\n",
					 gdrm_disp1_reg_range[i].name, addr,
					 readl(gdrm_disp1_base[i] + addr -
				gdrm_disp1_reg_range[i].reg_base));
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr >= gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base +
			    0x1000UL) {
				DRM_INFO("%8s Read register 0x%08lX: 0x%08X\n",
					 gdrm_disp2_reg_range[i].name, addr,
					 readl(gdrm_disp2_base[i] + addr -
				gdrm_disp2_reg_range[i].reg_base));
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp3_reg_range); i++) {
			if (addr >= gdrm_disp3_reg_range[i].reg_base &&
			    addr < gdrm_disp3_reg_range[i].reg_base +
			    0x1000UL) {
				DRM_INFO("%8s Read register 0x%08lX: 0x%08X\n",
					 gdrm_disp3_reg_range[i].name, addr,
					 readl(gdrm_disp3_base[i] + addr -
				gdrm_disp3_reg_range[i].reg_base));
				break;
			}
		}

	} else if (strncmp(opt, "autoregr:", 9) == 0) {
		DRM_INFO("Set the register addr for Auto-test\n");
	} else if (strncmp(opt, "dump:", 5) == 0) {
		u64 i;
		u32 j;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (gdrm_disp1_base[i] == NULL)
				continue;
			for (j = gdrm_disp1_reg_range[i].offset[0];
			     j < gdrm_disp1_reg_range[i].offset[0] +
			     gdrm_disp1_reg_range[i].length[0]; j += 16U)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					gdrm_disp1_reg_range[i].name,
					gdrm_disp1_reg_range[i].reg_base + j,
					readl(gdrm_disp1_base[i] + j),
					readl(gdrm_disp1_base[i] + j + 0x4),
					readl(gdrm_disp1_base[i] + j + 0x8),
					readl(gdrm_disp1_base[i] + j + 0xc));

			for (j = gdrm_disp1_reg_range[i].offset[1];
			     j < gdrm_disp1_reg_range[i].offset[1] +
			     gdrm_disp1_reg_range[i].length[1]; j += 16U)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					gdrm_disp1_reg_range[i].name,
					gdrm_disp1_reg_range[i].reg_base + j,
					readl(gdrm_disp1_base[i] + j),
					readl(gdrm_disp1_base[i] + j + 0x4),
					readl(gdrm_disp1_base[i] + j + 0x8),
					readl(gdrm_disp1_base[i] + j + 0xc));
		}
		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (gdrm_disp2_base[i] == NULL)
				continue;
			for (j = gdrm_disp2_reg_range[i].offset[0];
			     j < gdrm_disp2_reg_range[i].offset[0] +
			     gdrm_disp2_reg_range[i].length[0]; j += 16U)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					gdrm_disp2_reg_range[i].name,
					gdrm_disp2_reg_range[i].reg_base + j,
					readl(gdrm_disp2_base[i] + j),
					readl(gdrm_disp2_base[i] + j + 0x4),
					readl(gdrm_disp2_base[i] + j + 0x8),
					readl(gdrm_disp2_base[i] + j + 0xc));

			for (j = gdrm_disp2_reg_range[i].offset[1];
			     j < gdrm_disp2_reg_range[i].offset[1] +
			     gdrm_disp2_reg_range[i].length[1]; j += 16U)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					gdrm_disp2_reg_range[i].name,
					gdrm_disp2_reg_range[i].reg_base + j,
					readl(gdrm_disp2_base[i] + j),
					readl(gdrm_disp2_base[i] + j + 0x4),
					readl(gdrm_disp2_base[i] + j + 0x8),
					readl(gdrm_disp2_base[i] + j + 0xc));
		}
		for (i = 0; i < ARRAY_SIZE(gdrm_disp3_reg_range); i++) {
			if (gdrm_disp3_base[i] == NULL)
				continue;
			for (j = gdrm_disp3_reg_range[i].offset[0];
			     j < gdrm_disp3_reg_range[i].offset[0] +
			     gdrm_disp3_reg_range[i].length[0]; j += 16U)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					gdrm_disp3_reg_range[i].name,
					gdrm_disp3_reg_range[i].reg_base + j,
					readl(gdrm_disp3_base[i] + j),
					readl(gdrm_disp3_base[i] + j + 0x4),
					readl(gdrm_disp3_base[i] + j + 0x8),
					readl(gdrm_disp3_base[i] + j + 0xc));

			for (j = gdrm_disp3_reg_range[i].offset[1];
			     j < gdrm_disp3_reg_range[i].offset[1] +
			     gdrm_disp3_reg_range[i].length[1]; j += 16U)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					gdrm_disp3_reg_range[i].name,
					gdrm_disp3_reg_range[i].reg_base + j,
					readl(gdrm_disp3_base[i] + j),
					readl(gdrm_disp3_base[i] + j + 0x4),
					readl(gdrm_disp3_base[i] + j + 0x8),
					readl(gdrm_disp3_base[i] + j + 0xc));
		}

	} else if (strncmp(opt, "hdmi:", 5) == 0) {
	} else if (strncmp(opt, "alpha", 5) == 0) {
		if (dbgfs_alpha) {
			DRM_INFO("set src alpha to src alpha\n");
			dbgfs_alpha = false;
		} else {
			DRM_INFO("set src alpha to ONE\n");
			dbgfs_alpha = true;
		}
	} else if (strncmp(opt, "r:", 2) == 0) {
		char *p = (char *)opt + 2;
		unsigned long addr;

		if (kstrtoul(p, 16, &addr) != 0)
			goto error;

		mtk_read_reg(addr);
	} else if (strncmp(opt, "w:", 2) == 0) {
		char *p = (char *)opt + 2;
		char *np;
		unsigned long addr, val;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr) != 0)
			goto error;

		if (p == NULL)
			goto error;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val) != 0)
			goto error;

		mtk_write_reg(addr, val);
#ifdef CONFIG_MTK_DISPLAY_COLOR
	} else if (strncmp(opt, "ncs:", 4) == 0) {
		char *p = (char *)opt + 4;
		unsigned int mode;

		if (kstrtouint(p, 10, &mode) != 0)
			goto error;

		g_color_bypass = mode;
		mtk_color_set_ncs_mode(mode);
#endif
	} else {
		goto error;
	}

	return;
 error:
	DRM_ERROR("Parse command error!\n\n%s", STR_HELP);
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* ------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* ------------------------------------------------------------------------- */
static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char dis_cmd_buf[512];
static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	if (strncmp(dis_cmd_buf, "regr:", 5) == 0) {
		char read_buf[512] = "\0";
		char *p = (char *)dis_cmd_buf + 5;
		unsigned long addr;
		int ret;
		u64 i;

		if (kstrtoul(p, 16, &addr) != 0)
			return 0;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
			    0x1000UL) {
				ret = sprintf(read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp1_reg_range[i].name, addr,
					readl(gdrm_disp1_base[i] + addr -
					gdrm_disp1_reg_range[i].reg_base));
				if (ret <= 0L)
					DRM_INFO("autoregr fail\n");
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr >= gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base +
			    0x1000UL) {
				ret = sprintf(read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp2_reg_range[i].name, addr,
					readl(gdrm_disp2_base[i] + addr -
					gdrm_disp2_reg_range[i].reg_base));
				if (ret <= 0L)
					DRM_INFO("autoregr fail\n");
				break;
			}
		}

		return simple_read_from_buffer(ubuf, count, ppos, read_buf,
						strlen(read_buf));
	} else if (strncmp(dis_cmd_buf, "autoregr:", 9) == 0) {
		char read_buf[512] = "\0";
		char read_buf2[512] = "\0";
		char *p = (char *)dis_cmd_buf + 9;
		unsigned long addr;
		unsigned long addr2;
		int ret;
		u64 i;

		if (kstrtoul(p, 16, &addr) != 0)
			return 0;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
			    0x1000UL) {
				ret = sprintf(read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp1_reg_range[i].name, addr,
					readl(gdrm_disp1_base[i] + addr -
					gdrm_disp1_reg_range[i].reg_base));
				if (ret <= 0L)
					DRM_INFO("autoregr fail\n");
				break;
			}
		}
		addr2 = addr + 0x1000ULL;
		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr2 >= gdrm_disp2_reg_range[i].reg_base &&
			    addr2 < gdrm_disp2_reg_range[i].reg_base +
			    0x1000UL) {
				ret = sprintf(read_buf2,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp2_reg_range[i].name, addr2,
					readl(gdrm_disp2_base[i] + addr2 -
					gdrm_disp2_reg_range[i].reg_base));
				if (ret <= 0L)
					DRM_INFO("autoregr fail\n");
				break;
			}
		}
		p = strncat(read_buf, read_buf2,
			(sizeof(read_buf) - strlen(read_buf) - 1));
		if (p == NULL)
			DRM_INFO("autoregr strcat fail\n");
		return simple_read_from_buffer(ubuf, count, ppos, read_buf,
						strlen(read_buf));
	} else {
		return simple_read_from_buffer(ubuf, count, ppos, STR_HELP,
						strlen(STR_HELP));
	}
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	const u64 debug_bufmax = sizeof(dis_cmd_buf) - 1ULL;
	ssize_t ret;

	ret = (ssize_t)count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&dis_cmd_buf, ubuf, count) != 0ULL)
		return -EFAULT;

	dis_cmd_buf[count] = '\0';

	process_dbg_cmd(dis_cmd_buf);

	return ret;
}

static struct dentry *mtkdrm_dbgfs;
static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};

bool force_alpha(void)
{
	return dbgfs_alpha;
}

static void mtk_drm_debugfs_hw_range(struct mtk_drm_private *priv,
				     void __iomem *base[10],
				     struct mtk_drm_debugfs_table *reg_range,
				     unsigned long reg_range_size,
				     enum mtk_ddp_comp_id *path,
				     unsigned int path_len)
{
	void __iomem *mutex_regs;
	unsigned long mutex_phys;
	struct device_node *np;
	struct resource res;
	enum mtk_ddp_comp_id comp_id;
	int ret;
	u32 i;
	u64 j;

	for (i = 0; i < path_len; i++) {
		if (path[i] == DDP_COMPONENT_PWM0 ||
		    path[i] == DDP_COMPONENT_PWM1 ||
		    path[i] == DDP_COMPONENT_PWM2)
			break;
		comp_id = path[i];
		for (j = 0UL;  j < reg_range_size; j++) {
			if (comp_id == reg_range[j].id) {
				np = priv->comp_node[comp_id];
				if (priv->ddp_comp[comp_id] != NULL)
					base[j] =
						priv->ddp_comp[comp_id]->regs;
				ret = of_address_to_resource(np, 0, &res);
				if (ret < 0)
					DRM_INFO("np[%d] map fail\n", i);
				reg_range[j].reg_base = res.start;
			}
		}
	}
	j = reg_range_size - 2UL;
	if (reg_range[j].id == DDP_COMPONENT_CONFIG) {
		base[j] = priv->config_regs;
		reg_range[j++].reg_base = 0x14000000;
	} else {
		DRM_INFO("reg_range[%lld].id != DDP_COMPONENT_CONFIG", j);
	}
	mutex_regs = of_iomap(priv->mutex_node, 0);
	ret = of_address_to_resource(priv->mutex_node, 0, &res);
	if (ret < 0)
		DRM_INFO("mutex_node map address fail\n");
	mutex_phys = res.start;
	base[j] = mutex_regs;
	reg_range[j].reg_base = mutex_phys;
}

void mtk_drm_debugfs_init(struct drm_device *dev,
			  struct mtk_drm_private *priv)
{
	DRM_DEBUG_DRIVER("%s\n", __func__);
	mtkdrm_dbgfs = debugfs_create_file("mtkdrm", 0644, NULL, (void *)0,
					   &debug_fops);

	mtk_drm_debugfs_hw_range(priv, gdrm_disp1_base, gdrm_disp1_reg_range,
				 ARRAY_SIZE(gdrm_disp1_reg_range),
				 priv->data->main_path, priv->data->main_len);

	mtk_drm_debugfs_hw_range(priv, gdrm_disp2_base, gdrm_disp2_reg_range,
				 ARRAY_SIZE(gdrm_disp2_reg_range),
				 priv->data->ext_path, priv->data->ext_len);

	mtk_drm_debugfs_hw_range(priv, gdrm_disp3_base, gdrm_disp3_reg_range,
				 ARRAY_SIZE(gdrm_disp3_reg_range),
				 priv->data->third_path, priv->data->third_len);

	DRM_DEBUG_DRIVER("%s..done\n", __func__);
}

void mtk_drm_debugfs_deinit(void)
{
	debugfs_remove(mtkdrm_dbgfs);
}
