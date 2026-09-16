// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "clk-fhctl.h"
#include "clk-mtk.h"
#include <mt-plat/aee.h>





struct pll_status fh_log;
static struct mtk_fhctl *g_p_fhctl;


/*****************************************************************
 * Global variable operation
 ****************************************************************/
static void __set_fhctl(struct mtk_fhctl *pfhctl)
{
	g_p_fhctl = pfhctl;
}

static struct mtk_fhctl *__get_fhctl(void)
{
	return g_p_fhctl;
}


enum FH_DEBUG_CMD_ID {
	FH_DBG_CMD_ID = 0x1000,
	FH_DBG_CMD_DVFS = 0x1001,
	FH_DBG_CMD_DVFS_API = 0x1002,
	FH_DBG_CMD_DVFS_SSC_ENABLE = 0x1003,
	FH_DBG_CMD_SSC_ENABLE = 0x1004,
	FH_DBG_CMD_SSC_DISABLE = 0x1005,
	FH_DBG_CMD_TR_BEGIN_LOW = 0x2001,
	FH_DBG_CMD_TR_BEGIN_HIGH = 0x2002,
	FH_DBG_CMD_TR_END_LOW = 0x2003,
	FH_DBG_CMD_TR_END_HIGH = 0x2004,
	FH_DBG_CMD_TR_ID = 0x2005,
	FH_DBG_CMD_TR_VAL = 0x2006,
	FH_DBG_CMD_MAX
};

void mt_fhctl_exit_debugfs(struct mtk_fhctl *fhctl)
{
	debugfs_remove_recursive(fhctl->debugfs_root);
}

void mt_fh_dump_register(void)
{
	int pll_id;
	struct mtk_fhctl *fhctl = __get_fhctl();
	struct clk_mt_fhctl_regs *fh_regs;

	if (fhctl == NULL){
		WARN_ON(1);
		pr_info("[Hopping] get NULL fhctl obj \n");
		}


	for (pll_id = 0 ; pll_id < fhctl->pll_num ; pll_id++) {

		if (fhctl->fh_tbl[pll_id] == NULL || (fhctl->idmap[pll_id] == -1)){
		continue;
		}
		fh_regs = fhctl->fh_tbl[pll_id]->fh_regs ;

		pr_info("PLL_ID:%d HP_EN:%08x\n",pll_id, readl(fh_regs->reg_hp_en));
		pr_info("P:%s CFG:%08x DVFS:%08x DDS:%08x MON:%08x CON_PCW:%08x\n",
		fhctl->fh_tbl[pll_id]->pll_data->pll_name,
		readl(fh_regs->reg_cfg),
		readl(fh_regs->reg_dvfs),
		readl(fh_regs->reg_dds),
		readl(fh_regs->reg_mon),
		readl(fh_regs->reg_con_pcw));

	}
}


void mt_fhctl_log_b4_hopping (struct clk_mt_fhctl *fhctl, unsigned int target_dds, unsigned int tx_id, struct pll_status *fh_log){

	unsigned int pll_id = fhctl->pll_data->pll_id;

	//recording
	fh_log->before_dds = readl(fhctl->fh_regs->reg_con_pcw) & (0x3FFFF);

	fh_log->target_dds = target_dds;

	fh_log->pll_id = pll_id;
		
	fh_log->tx_id = tx_id;
}


void mt_fhctl_log_af_hopping (struct clk_mt_fhctl *fhctl, int ret_from_ipi, unsigned int ack_data, struct pll_status *fh_log, void (*ipi_get_data)(unsigned int), u64 time_ns){

	unsigned int pll_id = fhctl->pll_data->pll_id;
	unsigned int aft_dds = readl(fhctl->fh_regs->reg_con_pcw) & (0x3FFFFF);
	struct mtk_fhctl *fh = __get_fhctl();
	
	if ((aft_dds != fh_log->target_dds) || (fh_log->tx_id != ack_data) || (ret_from_ipi != 0)) {
		pr_info("[Hopping] PLL_ID:%d TX_ID:%d ACK_DATA:%d", pll_id, fh_log->tx_id, ack_data);
		pr_info("[Hopping] pll_id %d hopping fail, cfg %x, bef %x, aft %x, tgt %x, ret_from_ipi %d, time_ns %llx\n",
			pll_id,
			readl(fhctl->fh_regs->reg_cfg),
			fh_log->before_dds,
			aft_dds,
			fh_log->target_dds,
			ret_from_ipi,
			time_ns);

		fh_log->after_dds = aft_dds;
		
		mt_fh_dump_register();

		if (fh->reg_tr)
			pr_info("[Hopping] reg_tr<%x>\n", readl(fh->reg_tr));

		ipi_get_data(FH_DBG_CMD_TR_BEGIN_LOW);
		ipi_get_data(FH_DBG_CMD_TR_BEGIN_HIGH);
		ipi_get_data(FH_DBG_CMD_TR_END_LOW);
		ipi_get_data(FH_DBG_CMD_TR_END_HIGH);
		ipi_get_data(FH_DBG_CMD_TR_ID);
		ipi_get_data(FH_DBG_CMD_TR_VAL);

		aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
					"[Hopping] IPI to CPUEB\n",
					"IPI timeout");
		
	//BUG();
	}

}


static int __fh_ctrl_cmd_handler(struct clk_mt_fhctl *fh,
			unsigned int cmd,
			int pll_id,
			unsigned int p1)

{
	int ret;

	pr_info("pll_id:0x%x cmd: %x p1:%x", pll_id, cmd, p1);

	if (fh == NULL) {
		pr_info("Error: fh is null!");
		return 0;
	}

	switch (cmd) {
	case FH_DBG_CMD_SSC_ENABLE:
		ret = fh->hal_ops->pll_ssc_enable(fh, p1);
		break;
	case FH_DBG_CMD_SSC_DISABLE:
		ret = fh->hal_ops->pll_ssc_disable(fh);
		break;
	case FH_DBG_CMD_DVFS:
		ret = fh->hal_ops->pll_hopping(fh, p1, -1);
		break;
	case FH_DBG_CMD_DVFS_API:
		ret = !(mtk_fh_set_rate(pll_id, p1, -1));
		break;
	default:
		pr_info(" Not Support CMD:%x\n", cmd);
		ret = -EINVAL;
		break;
	}

	if (ret)
		pr_info(" Debug CMD fail err:%d\n", ret);

	return ret;
}


/***************************************************************************
 * FHCTL Debug CTRL OPS
 ***************************************************************************/
static ssize_t fh_ctrl_proc_write(struct file *file,
				const char *buffer, size_t count, loff_t *data)
{
	int ret, n;
	char kbuf[256];
	int pll_id;
	size_t len = 0;
	unsigned int cmd, p1;
	struct clk_mt_fhctl *fh;
	struct mtk_fhctl *fhctl = file->f_inode->i_private;

	len = min(count, (sizeof(kbuf) - 1));

	pr_info("count: %ld", count);
	if (count == 0)
		return -1;

	if (count > 255)
		count = 255;

	ret = copy_from_user(kbuf, buffer, count);
	if (ret < 0)
		return -1;

	kbuf[count] = '\0';

	n = sscanf(kbuf, "%x %x %x", &cmd, &pll_id, &p1);
	if ((n != 3) && (n != 2)) {
		pr_info("error input format\n");
		return -EINVAL;
	}

	pr_info("pll:0x%x cmd:%x p1:%x", pll_id, cmd, p1);

	if ((cmd < FH_DBG_CMD_ID) && (cmd > FH_DBG_CMD_MAX)) {
		pr_info("cmd not support:%x", cmd);
		return -EINVAL;
	}

	if (pll_id >= fhctl->pll_num) {
		pr_info("pll_id is illegal:%d", pll_id);
		return -EINVAL;
	}

	fh = mtk_fh_get_fh_obj_tbl(fhctl, pll_id);

	__fh_ctrl_cmd_handler(fh, cmd, pll_id, p1);

	pr_debug("reg_cfg:0x%08x", readl(fh->fh_regs->reg_cfg));
	pr_debug("reg_updnlmt:0x%08x", readl(fh->fh_regs->reg_updnlmt));
	pr_debug("reg_dds:0x%08x", readl(fh->fh_regs->reg_dds));
	pr_debug("reg_dvfs:0x%08x", readl(fh->fh_regs->reg_dvfs));
	pr_debug("reg_mon:0x%08x", readl(fh->fh_regs->reg_mon));
	pr_debug("reg_con0:0x%08x", readl(fh->fh_regs->reg_con0));
	pr_debug("reg_con_pcw:0x%08x", readl(fh->fh_regs->reg_con_pcw));

	return count;
}

static int fh_ctrl_proc_read(struct seq_file *m, void *v)
{
	int i;
	struct mtk_fhctl *fhctl = m->private;

	seq_puts(m, "====== FHCTL CTRL Description ======\n");

	seq_puts(m, "[PLL Name and ID Table]\n");
	for (i = 0 ; i < fhctl->pll_num ; i++)
		seq_printf(m, "PLL_ID:%d PLL_NAME: %s\n",
				i, fhctl->fh_tbl[i]->pll_data->pll_name);

	seq_puts(m, "\n[Command Description]\n");
	seq_puts(m, "	 [SSC Enable]\n");
	seq_puts(m, "	 /> echo '1004 <PLL-ID> <SSC-Rate>' > ctrl\n");
	seq_puts(m, "	 Example: echo '1004 2 2' > ctrl\n");
	seq_puts(m, "	 [SSC Disable]\n");
	seq_puts(m, "	 /> echo '1005 <PLL-ID>' > ctrl\n");
	seq_puts(m, "	 Example: echo '1005 2' > ctrl\n");
	seq_puts(m, "	 [SSC Hopping]\n");
	seq_puts(m, "	 /> echo '1001 <PLL-ID> <DDS>' > ctrl\n");
	seq_puts(m, "	 Example: echo '1001 2 ec200' > ctrl\n");
	seq_puts(m, "	 [CLK API Hopping]\n");
	seq_puts(m, "	 /> echo '1002 <PLL-ID> <DDS>' > ctrl\n");
	seq_puts(m, "	 Example: echo '1002 2 ec200' > ctrl\n");

	return 0;
}


static int fh_ctrl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fh_ctrl_proc_read, inode->i_private);
}

static const struct file_operations ctrl_fops = {
	.owner = THIS_MODULE,
	.open = fh_ctrl_proc_open,
	.read = seq_read,
	.write = fh_ctrl_proc_write,
	.release = single_release,
};

static int __sample_period_dds(struct clk_mt_fhctl *fh)
{
	int i, ssc_rate = 0;
	struct clk_mt_fhctl_regs *fh_regs;
	unsigned int mon_dds;
	unsigned int dds;

	fh_regs = fh->fh_regs;

	mon_dds = readl(fh_regs->reg_mon) & fh->pll_data->dds_mask;
	dds = readl(fh_regs->reg_dds) & fh->pll_data->dds_mask;

	fh->pll_data->dds_max = dds;
	fh->pll_data->dds_min = mon_dds;


	/* Sample 200*10us */
	for (i = 0 ; i < 200 ; i++) {
		mon_dds = readl(fh_regs->reg_mon) & fh->pll_data->dds_mask;

		if (mon_dds > fh->pll_data->dds_max)
			fh->pll_data->dds_max = mon_dds;

		if (mon_dds < fh->pll_data->dds_min)
			fh->pll_data->dds_min = mon_dds;

		udelay(10);
	}

	if ((fh->pll_data->dds_max == 0) ||
		(fh->pll_data->dds_min == 0))
		ssc_rate = 0;
	else {
		int diff = (fh->pll_data->dds_max - fh->pll_data->dds_min);

		ssc_rate = (diff * 1000) / fh->pll_data->dds_max;
	}

	return ssc_rate;
}

static int mt_fh_dumpregs_read(struct seq_file *m, void *data)
{
	struct mtk_fhctl *fhctl = dev_get_drvdata(m->private);
	int i, ssc_rate;
	struct clk_mt_fhctl *fh;
	struct clk_mt_fhctl_regs *fh_regs;

	if (fhctl == NULL) {
		seq_puts(m, "Cannot Get FHCTL driver data\n");
		return 0;
	}

	seq_puts(m, "FHCTL dumpregs Read\n");

	for (i = 0; i < fhctl->pll_num ; i++) {
		fh = mtk_fh_get_fh_obj_tbl(fhctl, i);
		if (fh == NULL) {
			pr_info(" fh:NULL pll_id:%d", i);
			seq_printf(m, "ERROR PLL_ID:%d clk_mt_fhctl is NULL\r\n", i);
			return 0;
		}

		if (fh->pll_data->pll_type == FH_PLL_TYPE_NOT_SUPPORT) {
			pr_debug(" Not support: %s", fh->pll_data->pll_name);
			continue;
		}

		fh_regs = fh->fh_regs;
		if (fh_regs == NULL) {
			pr_info("%s Not support dumpregs!",
				fh->pll_data->pll_name);
			seq_printf(m, "PLL_%d: %s Not support dumpregs!\n",
						i, fh->pll_data->pll_name);
			continue;
		}

		pr_debug("fh:0x%p fh_regs:0x%p", fh, fh_regs);

		if (i == 0) {
			seq_printf(m, "\r\nFHCTL_HP_EN:\r\n0x%08x\r\n",
				readl(fh_regs->reg_hp_en));
			seq_printf(m, "\r\nFHCTL_CLK_CON:\r\n0x%08x\r\n",
				readl(fh_regs->reg_clk_con));
			seq_printf(m, "\r\nFHCTL_SLOPE0:\r\n0x%08x\r\n",
				readl(fh_regs->reg_slope0));
			seq_printf(m, "\r\nFHCTL_SLOPE1:\r\n0x%08x\r\n\n",
				readl(fh_regs->reg_slope1));
		}

		ssc_rate = __sample_period_dds(fh);

		seq_printf(m, "PLL_ID:%d (%s) type:%d \r\n",
			i, fh->pll_data->pll_name, fh->pll_data->pll_type);

		seq_puts(m, "CFG, UPDNLMT, DDS, DVFS, MON\r\n");
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
			readl(fh_regs->reg_cfg), readl(fh_regs->reg_updnlmt),
			readl(fh_regs->reg_dds), readl(fh_regs->reg_dvfs),
			readl(fh_regs->reg_mon));
		seq_puts(m, "CON0, CON_PCW\r\n");
		seq_printf(m, "0x%08x 0x%08x\r\n",
			readl(fh_regs->reg_con0), readl(fh_regs->reg_con_pcw));

		seq_printf(m,
			"DDS max:0x%08x min:0x%08x ssc(1/1000):%d\r\n\r\n",
			fh->pll_data->dds_max,
			fh->pll_data->dds_min,
			ssc_rate);


		pr_debug("pll_id:%d", i);
		pr_debug("pll_type:%d", fh->pll_data->pll_type);
		pr_debug("reg_hp_en:0x%08x", readl(fh_regs->reg_hp_en));
		pr_debug("reg_clk_con:0x%08x", readl(fh_regs->reg_clk_con));
		pr_debug("reg_rst_con:0x%08x", readl(fh_regs->reg_rst_con));
		pr_debug("reg_slope0:0x%08x", readl(fh_regs->reg_slope0));
		pr_debug("reg_slope1:0x%08x", readl(fh_regs->reg_slope1));

		pr_debug("reg_cfg:0x%08x", readl(fh_regs->reg_cfg));
		pr_debug("reg_updnlmt:0x%08x", readl(fh_regs->reg_updnlmt));
		pr_debug("reg_dds:0x%08x", readl(fh_regs->reg_dds));
		pr_debug("reg_dvfs:0x%08x", readl(fh_regs->reg_dvfs));
		pr_debug("reg_mon:0x%08x", readl(fh_regs->reg_mon));
		pr_debug("reg_con0:0x%08x", readl(fh_regs->reg_con0));
		pr_debug("reg_con_pcw:0x%08x", readl(fh_regs->reg_con_pcw));

	}
	return 0;
}

void mt_fhctl_init_debugfs(struct mtk_fhctl *fhctl)
{
	struct dentry *root;
	struct dentry *fh_dumpregs_dir;
	struct dentry *fh_ctrl_dir;
	struct device *dev = fhctl->dev;

	root = debugfs_create_dir("fhctl", NULL);
	if (IS_ERR(root)) {
		dev_info(dev, "create debugfs fail");
		return;
	}
	__set_fhctl(fhctl);
	fhctl->debugfs_root = root;

	/* /sys/kernel/debug/fhctl/dumpregs */
	fh_dumpregs_dir = debugfs_create_devm_seqfile(dev,
				"dumpregs", root, mt_fh_dumpregs_read);
	if (IS_ERR(fh_dumpregs_dir)) {
		dev_info(dev, "failed to create dumpregs debugfs");
		return;
	}

	/* /sys/kernel/debug/fhctl/ctrl */
	fh_ctrl_dir = debugfs_create_file("ctrl", 0664,
						root, fhctl, &ctrl_fops);
	if (IS_ERR(fh_ctrl_dir)) {
		dev_info(dev, "failed to create ctrl debugfs");
		return;
	}

	dev_dbg(dev, "Create debugfs success!");
}


