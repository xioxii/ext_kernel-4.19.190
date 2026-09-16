/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MTK_STATIC_POWER_MTK6779_H__
#define __MTK_STATIC_POWER_MTK6779_H__

/* #define SPOWER_NOT_READY 1 */

/* #define WITHOUT_LKG_EFUSE */

/* record leakage information that read from efuse */
struct spower_leakage_info {
	const char *name;
	unsigned int devinfo_idx;
	unsigned int devinfo_offset;
	unsigned int value;
	unsigned int v_of_fuse;
	int t_of_fuse;
	unsigned int instance;
};

extern struct spower_leakage_info *spower_lkg_info;
extern int devinfo_table[];
#endif
