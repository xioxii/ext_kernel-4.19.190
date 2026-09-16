/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_H__
#define __MTK_H__

#include <linux/delay.h>
#include <mtk_lpm_type.h>
#include <mtk_common.h>

int mtk_do_mcusys_prepare_pdn(unsigned int status,
					   unsigned int *resource_req);

int mtk_do_mcusys_prepare_on_ex(unsigned int clr_status);

int mtk_do_mcusys_prepare_on(void);

#endif
