/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Carlos Huang <carlos.huang@mediatek.com>
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>

#include <linux/kernel.h>
#include <linux/sched.h>

#include "raether.h"

extern struct net_device *dev_raether;

#define PHY_CONTROL_0		0x0004
#define MDIO_PHY_CONTROL_0	(RALINK_ETH_MAC_BASE + PHY_CONTROL_0)
#define enable_mdio(x)

