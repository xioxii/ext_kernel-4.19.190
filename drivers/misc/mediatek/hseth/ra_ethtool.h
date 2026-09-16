/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Harry Huang <harry.huang@mediatek.com>
 */

#ifndef RA_ETHTOOL_H
#define RA_ETHTOOL_H

extern struct net_device *dev_raether;

/* ethtool related */
void ethtool_init(struct net_device *dev);
int et_get_settings(struct net_device *dev, struct ethtool_cmd *cmd);
u32 et_get_link(struct net_device *dev);
unsigned char get_current_phy_address(void);
int mdio_read(struct net_device *dev, int phy_id, int location);
void mdio_write(struct net_device *dev, int phy_id, int location, int value);

/* for pseudo interface */
void ethtool_virt_init(struct net_device *dev);
int et_virt_get_settings(struct net_device *dev, struct ethtool_cmd *cmd);
u32 et_virt_get_link(struct net_device *dev);
int mdio_virt_read(struct net_device *dev, int phy_id, int location);
void mdio_virt_write(struct net_device *dev, int phy_id, int location,
		     int value);

#endif
