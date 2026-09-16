/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SSPM_IPI_TABLE_H__
#define __SSPM_IPI_TABLE_H__

#include "sspm_ipi_id.h"
#include "sspm_mbox_pin.h"

/*
 * mbox information
 *
 * mbdev  :mbox device
 * irq_num:identity of mbox irq
 * id     :mbox id
 * slot   :how many slots that mbox used, up to 1GB
 * opt    :option for mbox or share memory, 0:mbox, 1:share memory
 * enable :mbox status, 0:disable, 1: enable
 * is64d  :mbox is64d status, 0:32d, 1: 64d
 * base   :mbox base address
 * set_irq_reg  : mbox set irq register
 * clr_irq_reg  : mbox clear irq register
 * init_base_reg: mbox initialize register
 * send_status_reg: mbox send status register
 * recv_status_reg: mbox recv status register
 * mbox lock    : lock of mbox
 */
struct mtk_mbox_info sspm_mbox_table[SSPM_MBOX_TOTAL] = {
	{.id=0, .slot=32, .opt=0, .enable=1, .is64d=0,},
	{.id=1, .slot=32, .opt=0, .enable=1, .is64d=0,},
	{.id=2, .slot=32, .opt=0, .enable=1, .is64d=0,},
	{.id=3, .slot=32, .opt=0, .enable=1, .is64d=0,},
	{.id=4, .slot=32, .opt=0, .enable=1, .is64d=0,},
};

/*
 * mbox pin structure, this is for send defination,
 * ipi=endpoint=pin
 * mbox     : (mbox number)mbox number of the pin, up to 16(plt)
 * offset   : (slot)msg offset in share memory, up to 1024*4 KB(plt)
 * send_opt : (opt)send opt, 0:send ,1: send for response(plt)
 * lock     : (lock)polling lock 0:unuse,1:used
 * msg_size : (slot)message size in words, 4 bytes alignment(plt)
 * pin_index  : (bit offset)pin index in the mbox(plt)
 * chan_id    : (u32) ipc channel id(plt)
 * mutex      : (mutex)mutex for remote response
 * completion : (completion)completion for remote response
 * pin_lock   : (spinlock_t)lock of the pin
 */
struct mtk_mbox_pin_send sspm_mbox_pin_send[] = {
/* the following will use mbox 0 */
	{.mbox=0, .offset=IPIS_C_PPM_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_PPM_OUT_SIZE,
	 .pin_index=IPIS_C_PPM_OUT_OFFSET, .chan_id=IPIS_C_PPM},
	{.mbox=0, .offset=IPIS_C_QOS_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_QOS_OUT_SIZE,
	 .pin_index=IPIS_C_QOS_OUT_OFFSET, .chan_id=IPIS_C_QOS},
	{.mbox=0, .offset=IPIS_C_PMIC_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_PMIC_OUT_SIZE,
	 IPIS_C_PMIC_OUT_OFFSET, .chan_id=IPIS_C_PMIC},
	{.mbox=0, .offset=IPIS_C_MET_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_MET_OUT_SIZE,
	 .pin_index=IPIS_C_MET_OUT_OFFSET, .chan_id=IPIS_C_MET},
	{.mbox=0, .offset=IPIS_C_THERMAL_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_THERMAL_OUT_SIZE,
	 .pin_index=IPIS_C_THERMAL_OUT_OFFSET, .chan_id=IPIS_C_THERMAL},
	{.mbox=0, .offset=IPIS_C_GPU_DVFS_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_GPU_DVFS_OUT_SIZE,
	 .pin_index=IPIS_C_GPU_DVFS_OUT_OFFSET, .chan_id=IPIS_C_GPU_DVFS},
	{.mbox=0, .offset=IPIS_C_GPU_PM_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_GPU_PM_OUT_SIZE,
	 .pin_index=IPIS_C_GPU_PM_OUT_OFFSET, .chan_id=IPIS_C_GPU_PM},
/* the following will use mbox 1 */
	{.mbox=1, .offset=IPIS_C_PLATFORM_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_PLATFORM_OUT_SIZE,
	 .pin_index=IPIS_C_PLATFORM_OUT_OFFSET, .chan_id=IPIS_C_PLATFORM},
	{.mbox=1, .offset=IPIS_C_SMI_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_SMI_OUT_SIZE,
	 IPIS_C_SMI_OUT_OFFSET, .chan_id=IPIS_C_SMI},
	{.mbox=1, .offset=IPIS_C_CM_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_CM_OUT_SIZE,
	 .pin_index=IPIS_C_CM_OUT_OFFSET, .chan_id=IPIS_C_CM},
	{.mbox=1, .offset=IPIS_C_SLBC_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_SLBC_OUT_SIZE,
	 .pin_index=IPIS_C_SLBC_OUT_OFFSET, .chan_id=IPIS_C_SLBC},
	{.mbox=1, .offset=IPIS_C_SPM_SUSPEND_OUT_OFFSET, .send_opt=1, .msg_size=IPIS_C_SPM_SUSPEND_OUT_SIZE,
	 .pin_index=IPIS_C_SPM_SUSPEND_OUT_OFFSET, .chan_id=IPIS_C_SPM_SUSPEND},
	{.mbox=1, .offset=IPIR_C_MET_OUT_OFFSET, .send_opt=1, 0, .msg_size=IPIR_C_MET_OUT_SIZE,
	 .pin_index=IPIR_C_MET_OUT_OFFSET, .chan_id=IPIR_C_MET},
	{.mbox=1, .offset=IPIR_C_GPU_DVFS_OUT_OFFSET, .send_opt=1, .msg_size=IPIR_C_GPU_DVFS_OUT_SIZE,
	 .pin_index=IPIR_C_GPU_DVFS_OUT_OFFSET, .chan_id=IPIR_C_GPU_DVFS},
	{.mbox=1, .offset=IPIR_C_PLATFORM_OUT_OFFSET, .send_opt=1, .msg_size=IPIR_C_PLATFORM_OUT_SIZE,
	 .pin_index=IPIR_C_PLATFORM_OUT_OFFSET, .chan_id=IPIR_C_PLATFORM},
	{.mbox=1, .offset=IPIR_C_THERMAL_OUT_OFFSET, .send_opt=1, .msg_size=IPIR_C_THERMAL_OUT_SIZE,
	 .pin_index=IPIR_C_THERMAL_OUT_OFFSET, .chan_id=IPIR_C_THERMAL},
};

/*
 * mbox pin structure, this is for receive defination,
 * ipi=endpoint=pin
 * mbox     : (mbox number)mbox number of the pin, up to 16(plt)
 * offset   : (slot)msg offset in share memory, up to 1024*4 KB(plt)
 * recv_opt : (opt)recv option,  0:receive ,1: response(plt)
 * lock     : (lock)polling lock 0:unuse,1:used
 * buf_full_opt : (opt)buffer option 0:drop, 1:assert, 2:overwrite(plt)
 * cb_ctx_opt : (opt)callback option 0:isr context, 1:process context(plt)
 * msg_size   : (slot)msg used slots in the mbox, 4 bytes alignment(plt)
 * pin_index  : (bit offset)pin index in the mbox(plt)
 * chan_id : (u32) ipc channel id(plt)
 * notify     : (completion)notify process
 * mbox_pin_cb: (cb)cb function
 * pin_buf : (void*)buffer point
 * prdata  : (void*)private data
 * pin_lock: (spinlock_t)lock of the pin
 */
struct mtk_mbox_pin_recv sspm_mbox_pin_recv[] = {
/* the following will use mbox 2 */
	{.mbox=2, .offset=IPIR_I_QOS_IN_OFFSET, .recv_opt=0, .buf_full_opt=1, .cb_ctx_opt=1,
	 .msg_size=IPIR_I_QOS_IN_SIZE, .pin_index=IPIR_I_QOS_IN_OFFSET,
	 .chan_id=IPIR_I_QOS },
	{.mbox=2, .offset=IPIR_C_MET_IN_OFFSET, .recv_opt=0, .buf_full_opt=1, .cb_ctx_opt=1,
	 .msg_size=IPIR_C_MET_IN_SIZE, .pin_index=IPIR_C_MET_IN_OFFSET,
	 .chan_id=IPIR_C_MET },
	{.mbox=2, .offset=IPIR_C_GPU_DVFS_IN_OFFSET, .recv_opt=0, .buf_full_opt=1, .cb_ctx_opt=1,
	 .msg_size=IPIR_C_GPU_DVFS_IN_SIZE, .pin_index=IPIR_C_GPU_DVFS_IN_OFFSET,
	 .chan_id=IPIR_C_GPU_DVFS},
	{.mbox=2, .offset=IPIR_C_PLATFORM_IN_OFFSET, .recv_opt=0, .buf_full_opt=1, .cb_ctx_opt=1,
	 .msg_size=IPIR_C_PLATFORM_IN_SIZE, .pin_index=IPIR_C_PLATFORM_IN_OFFSET,
	 .chan_id=IPIR_C_PLATFORM},
	{.mbox=2, .offset=IPIR_C_THERMAL_IN_OFFSET, .recv_opt=0, .buf_full_opt=1, .cb_ctx_opt=1,
	 .msg_size=IPIR_C_THERMAL_IN_SIZE, .pin_index=IPIR_C_THERMAL_IN_OFFSET,
	 .chan_id=IPIR_C_THERMAL},
	{.mbox=2, .offset=IPIS_C_PPM_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_PPM_IN_SIZE, .pin_index=IPIS_C_PPM_IN_OFFSET,
	 .chan_id=IPIS_C_PPM},
	{.mbox=2, .offset=IPIS_C_QOS_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_QOS_IN_SIZE, .pin_index=IPIS_C_QOS_IN_OFFSET,
	 .chan_id=IPIS_C_QOS},
	{.mbox=2, .offset=IPIS_C_PMIC_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_PMIC_IN_SIZE, .pin_index=IPIS_C_PMIC_IN_OFFSET,
	 .chan_id=IPIS_C_PMIC},
	{.mbox=2, .offset=IPIS_C_MET_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_MET_IN_SIZE, .pin_index=IPIS_C_MET_IN_OFFSET,
	 .chan_id=IPIS_C_MET},
	{.mbox=2, .offset=IPIS_C_THERMAL_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_THERMAL_IN_SIZE, .pin_index=IPIS_C_THERMAL_IN_OFFSET,
	 .chan_id=IPIS_C_THERMAL},
	{.mbox=2, .offset=IPIS_C_GPU_DVFS_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_GPU_DVFS_IN_SIZE, .pin_index=IPIS_C_GPU_DVFS_IN_OFFSET,
	 .chan_id=IPIS_C_GPU_DVFS},
	{.mbox=2, .offset=IPIS_C_GPU_PM_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_GPU_PM_IN_SIZE, .pin_index=IPIS_C_GPU_PM_IN_OFFSET,
	 .chan_id=IPIS_C_GPU_PM},
	{.mbox=2, .offset=IPIS_C_PLATFORM_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_PLATFORM_IN_SIZE, .pin_index=IPIS_C_PLATFORM_IN_OFFSET,
	 .chan_id=IPIS_C_PLATFORM},
	{.mbox=2, .offset=IPIS_C_SMI_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_SMI_IN_SIZE, .pin_index=IPIS_C_SMI_IN_OFFSET,
	 .chan_id=IPIS_C_SMI},
	{.mbox=2, .offset=IPIS_C_CM_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_CM_IN_SIZE, .pin_index=IPIS_C_CM_IN_OFFSET,
	 .chan_id=IPIS_C_CM},
	{.mbox=2, .offset=IPIS_C_SLBC_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_SLBC_IN_SIZE, .pin_index=IPIS_C_SLBC_IN_OFFSET,
	 .chan_id=IPIS_C_SLBC},
	{.mbox=2, .offset=IPIS_C_SPM_SUSPEND_IN_OFFSET, .recv_opt=1, .buf_full_opt=1, .cb_ctx_opt=0,
	 .msg_size=IPIS_C_SPM_SUSPEND_IN_SIZE, .pin_index=IPIS_C_SPM_SUSPEND_IN_OFFSET,
	 .chan_id=IPIS_C_SPM_SUSPEND},
};

#define SSPM_TOTAL_SEND_PIN	(sizeof(sspm_mbox_pin_send) \
				 / sizeof(struct mtk_mbox_pin_send))
#define SSPM_TOTAL_RECV_PIN	(sizeof(sspm_mbox_pin_recv) \
				 / sizeof(struct mtk_mbox_pin_recv))

struct mtk_mbox_device sspm_mboxdev = {
	.name = "sspm_mboxdev",
	.pin_recv_table = &sspm_mbox_pin_recv[0],
	.pin_send_table = &sspm_mbox_pin_send[0],
	.info_table = &sspm_mbox_table[0],
	.count = SSPM_MBOX_TOTAL,
	.recv_count = SSPM_TOTAL_RECV_PIN,
	.send_count = SSPM_TOTAL_SEND_PIN,
};

extern void sspm_ipi_timeout_cb(int ipi_id);
struct mtk_ipi_device sspm_ipidev = {
	.name = "sspm_ipidev",
	.id = IPI_DEV_SSPM,
	.mbdev = &sspm_mboxdev,
	.timeout_handler = sspm_ipi_timeout_cb,
	.post_cb = (mbox_rx_cb_t)sspm_clr_spm_reg,
};

#endif /* __SSPM_IPI_TABLE_H__ */
