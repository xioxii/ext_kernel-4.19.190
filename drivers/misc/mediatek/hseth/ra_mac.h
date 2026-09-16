/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Harry Huang <harry.huang@mediatek.com>
 */

#ifndef RA_MAC_H
#define RA_MAC_H

void ra2880stop(struct END_DEVICE *ei_local);
void set_mac_address(unsigned char p[6]);
void set_mac2_address(unsigned char p[6]);
int str_to_ip(unsigned int *ip, const char *str);
void enable_auto_negotiate(struct END_DEVICE *ei_local);
void set_ge1_force_1000(void);
void set_ge2_force_1000(void);
void set_ge1_an(void);
void set_ge2_an(void);
void set_ge2_gmii(void);
void set_ge0_gmii(void);
void set_ge2_force_link_down(void);
#endif
