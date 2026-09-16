// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>

#include "proslic_sys.h"

/*****************************************************************************************************/
static int proslic_sys_delay(void *hTimer, int timeInMsec)
{
	if(likely(timeInMsec < SILABS_MIN_MSLEEP_TIME))
	{
		mdelay(timeInMsec);
	}
	else
	{
		msleep((timeInMsec-SILABS_MSLEEP_SLOP));
	}
	return PROSLIC_SPI_OK;
}

/*****************************************************************************************************/
/* Code assumes time value has been allocated */
static int proslic_sys_getTime(void *hTimer, void *time)
{
	if(time != NULL)
	{
		((proslic_timeStamp *)time)->timerObj = current_kernel_time();
		return PROSLIC_SPI_OK;
	}
	else
	{
		return PROSLIC_TIMER_ERROR;
	}
}
/*****************************************************************************************************/
 
static int proslic_sys_timeElapsed(void *hTimer, void *startTime, int *timeInMsec)
{
	if( (startTime != NULL) && (timeInMsec != NULL) )
	{
		struct timespec now = current_kernel_time();
		struct timespec ts_delta = timespec_sub(now, ((proslic_timeStamp *) startTime)->timerObj);
		*timeInMsec = ( (ts_delta.tv_sec *1000) + (ts_delta.tv_nsec / NSEC_PER_MSEC) );
		return PROSLIC_SPI_OK;
	}
	else
	{
		return PROSLIC_TIMER_ERROR;
	}
}


proslic_timer_fptrs_t proslic_timer_if =
{
	proslic_sys_delay,
	proslic_sys_timeElapsed,
	proslic_sys_getTime
};
