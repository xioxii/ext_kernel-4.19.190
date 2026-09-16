// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <asm-generic/io.h>

#define IOCTL_DEV_IOCTLID	'P'
#define PCIE_SMT_TEST_SLOT      _IOW(IOCTL_DEV_IOCTLID, 0, int)

#define PCIE_PHY0_BASE		0x11e40000
#define PCIE_PHY1_BASE		0x11e60000
#define PCIE_PHY2_BASE		0x11e80000
#define PCIE_PHY3_BASE		0x11ea0000
#define PCIE_SMT_DEV_NAME	"pcie_smt"
#define PCIE_SLOT_MAX		4

/*begin: modify pcie_loopback_test,by laizhenhao,2021.06.25*/
extern void power_on_pcie(int slot);
extern void power_off_pcie(int slot);
/*end: modify pcie_loopback_test,by laizhenhao,2021.06.25*/


struct mtk_pcie_smt {
	char *name;
	void __iomem *regs[PCIE_SLOT_MAX];
	int err_count;
	/* char device */
	struct cdev smt_cdev;
};

static int mtk_pcie_test_open(struct inode *inode, struct file *file)
{
	struct mtk_pcie_smt *pcie_smt;

	pcie_smt = container_of(inode->i_cdev, struct mtk_pcie_smt, smt_cdev);
	file->private_data = pcie_smt;
	pr_info("mtk_pcie_smt open: successful\n");

	return 0;
}

static int mtk_pcie_test_release(struct inode *inode, struct file *file)
{

	pr_info("mtk_pcie_smt release: successful\n");
	return 0;
}

static ssize_t mtk_pcie_test_read(struct file *file, char *buf, size_t count,
		loff_t *ptr)
{

	pr_info("mtk_pcie_smt read: returning zero bytes\n");
	return 0;
}

static ssize_t mtk_pcie_test_write(struct file *file, const char *buf,
		size_t count, loff_t *ppos)
{

	pr_info("mtk_pcie_smt write: accepting zero bytes\n");
	return 0;
}

/* Loopback test for PCIe port
 * Port 0 -> 2 lane
 * Port 1 -> 2 lane
 * Port 2 -> 1 lane
 * Port 3 -> 1 lane
 */
/*begin: modify pcie_loopback_test,by laizhenhao,2021.06.25*/
static int pcie_loopback_test(void __iomem *phy_base, int slot)
{
	int val = 0, ret = 0,i = 1,lane = 0;
	int err_count = 0;

	off_t pcie0_lane1_offset = 0x100; //offset of pcie0-lane1
	off_t pcie_ctr0_addr = 0x4010; //TX output Pattern
	off_t pcie_ctr1_addr = 0x501c;	//TX output Pattern
	off_t pcie_ctr2_addr = 0x50c8;	//TX output Pattern

    power_on_pcie(slot);
    
	if(slot == 0)
	{
		lane = 2;
	}else
		lane = 1;

    if(slot < 0 && slot >3)
    {
		pr_info("error slot!\n");
		return -1;
    }
        
	
	pr_info("pcie loopback test start\n");

	if (!phy_base) {
		pr_info("phy_base is not initialed!\n");
		return -1;
	}

	for(i = 0; i < lane; i++)
	{
		writel(1, phy_base + 0x28);
		/* L1ss = enable */
		val = readl(phy_base + 0x28);
		val |= (0x01 << 5);
		writel(val, phy_base + 0x28);
		
		val = readl(phy_base + 0x28);
		val |= (0x01 << 4);
		writel(val, phy_base + 0x28);
		
		val = readl(phy_base + 0x28);
		val &= (~0x200);
		writel(val, phy_base + 0x28);
		
		val = readl(phy_base + 0x28);
		val |= (0x01 << 8);
		writel(val, phy_base + 0x28);
		
		val = readl(phy_base + 0x28);
		val &= (~0x800);
		writel(val, phy_base + 0x28);
		
		val = readl(phy_base + 0x28);
		val |= (0x01 << 10);
		writel(val, phy_base + 0x28);
		
		/* Set Rate=Gen1 */
		usleep_range(1, 2);
		val = readl(phy_base + 0x70);
		val |= (0x01);
		writel(val, phy_base + 0x70);
		
		val = readl(phy_base + 0x70);
		val &= (~0x30000);
		writel(val, phy_base + 0x70);
		
		val = readl(phy_base + 0x70);
		val |= (0x01 << 4);
		writel(val, phy_base + 0x70);
		
		usleep_range(1, 2);
		val = readl(phy_base + 0x70);
		val &= (~0x10);
		writel(val, phy_base + 0x70);
		
		/* Force PIPE (P0) */
		val = readl(phy_base + 0x70);
		val |= (0x01);
		writel(val, phy_base + 0x70);
		
		val = readl(phy_base + 0x70);
		val &= (~0xc00);
		writel(val, phy_base + 0x70);
		
		val = readl(phy_base + 0x70);
		val &= (~0x3000);
		writel(val, phy_base + 0x70);
		
		val = readl(phy_base + 0x70);
		val |= (0x01 << 8);
		writel(val, phy_base + 0x70);
		
		val = readl(phy_base + 0x70);
		val |= (0x01 << 4);
		writel(val, phy_base + 0x70);
		
		usleep_range(1, 2);
		val = readl(phy_base + 0x70);
		val &= (~0x10);
		writel(val, phy_base + 0x70);
		/* Set TX output Pattern */
		usleep_range(1, 2);
		val = readl(phy_base + pcie_ctr0_addr);
		val |= (0x0d);
		writel(val, phy_base + pcie_ctr0_addr);
		
		
		/* Set TX PTG Enable */
		val = readl(phy_base + 0x28);
		val |= (0x01 << 30);
		writel(val, phy_base + 0x28);
		
		/* Set RX Pattern Checker (Type & Enable) */
		val = readl(phy_base + pcie_ctr1_addr);
		val |= (0x01 << 1);
		writel(val, phy_base + pcie_ctr1_addr);
		
		val = readl(phy_base + pcie_ctr1_addr);
		val |= (0x0d << 4);
		writel(val, phy_base + pcie_ctr1_addr);
		
		/* toggle ptc_en for status counter clear */
		val = readl(phy_base + pcie_ctr1_addr);
		val &= (~0x2);
		writel(val, phy_base + pcie_ctr1_addr);
		
		val = readl(phy_base + pcie_ctr1_addr);
		val |= (0x01 << 1);
		writel(val, phy_base + pcie_ctr1_addr);
		
		/* Check status */
		msleep(50);
		val = readl(phy_base + pcie_ctr2_addr);
		if ((val & 0x3) != 0x3) {
			err_count = val >> 12;
			pr_info("PCIe test failed: %#x!\n", val);
			pr_info("lane%d error count: %d\n", i, err_count);
			if(slot !=0)
			{
				ret = -1;
				continue;
			}

			ret += i+1;
		}
		pcie_ctr0_addr += pcie0_lane1_offset;
		pcie_ctr1_addr += pcie0_lane1_offset;
		pcie_ctr2_addr += pcie0_lane1_offset;
 	}

	return ret;
}
/*end: modify pcie_loopback_test,by laizhenhao,2021.06.25*/

static int pcie_smt_test_slot(struct mtk_pcie_smt *pcie_smt,unsigned int slot)
{
	if (!pcie_smt) {
		pr_info("pcie_smt not found\n");
		return -ENODEV;
	}

	if (slot >= PCIE_SLOT_MAX) {
		pr_info("Unsupported slot number: [%d]\n", slot);
		return -EINVAL;
	}
	
	return pcie_loopback_test(pcie_smt->regs[slot], slot);
}

static long mtk_pcie_test_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct mtk_pcie_smt *pcie_smt = file->private_data;

	switch (cmd) {
		case PCIE_SMT_TEST_SLOT:
			pr_info("pcie_smt slot: %d\r\n", (unsigned int)arg);
			return pcie_smt_test_slot(pcie_smt, (unsigned int)arg);
		default:
			return -ENOTTY;
	}

	return 0;
}

static const struct file_operations pcie_smt_fops = {
	.owner = THIS_MODULE,
	.read = mtk_pcie_test_read,
	.write = mtk_pcie_test_write,
	.unlocked_ioctl = mtk_pcie_test_ioctl,
	.compat_ioctl = mtk_pcie_test_ioctl,
	.open = mtk_pcie_test_open,
	.release = mtk_pcie_test_release,
};

static struct class *f_class;
static struct mtk_pcie_smt *pcie_smt;

static int __init mtk_pcie_smt_init(void)
{
	dev_t dev;
	int ret = 0;
	struct cdev *dev_ctx;

	pcie_smt = kzalloc(sizeof(*pcie_smt), GFP_KERNEL);
	if (!pcie_smt) {
		pr_info("pcie_smt alloc failed\n");
		return -ENOMEM;
	}

	pcie_smt->name = PCIE_SMT_DEV_NAME;
	pcie_smt->regs[0] = ioremap(PCIE_PHY0_BASE, 0x6000);
	pcie_smt->regs[1] = ioremap(PCIE_PHY1_BASE, 0x6000);
	pcie_smt->regs[2] = ioremap(PCIE_PHY2_BASE, 0x6000);
	pcie_smt->regs[3] = ioremap(PCIE_PHY3_BASE, 0x6000);
	dev_ctx = &pcie_smt->smt_cdev;

	ret = alloc_chrdev_region(&dev, 0, 1, pcie_smt->name);
	if (ret) {
		pr_info("pcie_smt_init alloc_chrdev_region failed -\n");
		return ret;
	}

	cdev_init(dev_ctx, &pcie_smt_fops);
	dev_ctx->owner = THIS_MODULE;

	ret = cdev_add(dev_ctx, dev, 1);
	if (ret)
		return ret;

	f_class = class_create(THIS_MODULE, pcie_smt->name);
	device_create(f_class, NULL, dev_ctx->dev, NULL, pcie_smt->name);

	return ret;
}

static void __exit mtk_pcie_smt_exit(void)
{
	struct cdev *dev_ctx = &pcie_smt->smt_cdev;
	int slot;

	/* remove char device */
	device_destroy(f_class, dev_ctx->dev);
	class_destroy(f_class);
	unregister_chrdev_region(dev_ctx->dev, MINORMASK);

	for (slot = 0; slot < PCIE_SLOT_MAX; slot ++)
		iounmap(pcie_smt->regs[slot]);

	kfree(pcie_smt);
}

module_init(mtk_pcie_smt_init);
module_exit(mtk_pcie_smt_exit);
