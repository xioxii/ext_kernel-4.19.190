/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SSPM_TIMESYNC_H_
#define _SSPM_TIMESYNC_H_

#include <sspm_mbox_pin.h>

#define SSPM_TS_MBOX				SHAREMBOX_NO_MCDI
#define SSPM_TS_MBOX_OFFSET_BASE	SHAREMBOX_OFFSET_TIMESTAMP

enum mbox_idx {
	MBOX_TICK_H = 0,
	MBOX_TICK_L,
	MBOX_TS_H,
	MBOX_TS_L,
	MBOX_DEBUG_TS_H,
	MBOX_DEBUG_TS_L,
};

void sspm_timesync_suspend(void);
void sspm_timesync_resume(void);
unsigned int __init sspm_timesync_init(void);
#endif // _SSPM_TIMESYNC_H_
