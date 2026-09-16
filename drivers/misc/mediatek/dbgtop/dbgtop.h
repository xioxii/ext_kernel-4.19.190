// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#ifndef __DBGTOP_H__
#define __DBGTOP_H__

int mtk_dbgtop_dram_reserved(int enable);
int mtk_dbgtop_cfg_dvfsrc(int enable);
int mtk_dbgtop_pause_dvfsrc(int enable);
int mtk_dbgtop_dfd_count_en(int value);
int mtk_dbgtop_dfd_therm1_dis(int value);
int mtk_dbgtop_dfd_therm2_dis(int value);
int mtk_dbgtop_dfd_timeout(int value_abnormal, int value_normal);
int mtk_dbgtop_dfd_timeout_reset(void);
int mtk_dbgtop_mfg_pwr_on(int value);
int mtk_dbgtop_mfg_pwr_en(int value);

#endif
