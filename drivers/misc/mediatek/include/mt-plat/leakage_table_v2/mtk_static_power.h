/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#ifndef __MTK_STATIC_POWER_H__
#define __MTK_STATIC_POWER_H__

#include <linux/types.h>

#if defined(CONFIG_MTK_STATIC_POWER)
#include "mtk_static_power_platform.h"
#endif

/*
 * @argument
 * dev: the enum of MT_SPOWER_xxx
 * voltage: the operating voltage, mV.
 * degree: the Tj. (degree C)
 * @return
 *  -1, means sptab is not yet ready.
 *  other value: the mW of leakage value.
 */

extern int mt_spower_init(void);


#endif
