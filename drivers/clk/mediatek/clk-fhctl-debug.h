/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */


#ifndef __DRV_CLK_FHCTL_DEBUG_H
#define __DRV_CLK_FHCTL_DEBUG_H

#if defined(CONFIG_DEBUG_FS)
void mt_fhctl_init_debugfs(struct mtk_fhctl *fhctl);
void mt_fhctl_exit_debugfs(struct mtk_fhctl *fhctl);
void mt_fhctl_log_b4_hopping(struct clk_mt_fhctl *fhctl, unsigned int target_dds, unsigned int tx_id, struct pll_status *fh_log);
void mt_fhctl_log_af_hopping(struct clk_mt_fhctl *fhctl, int ret_from_ipi, unsigned int tx_id, struct pll_status *fh_log, void (*ipi_get_data)(unsigned int), u64 time_ns);
void mt_fh_dump_register(void);

#else
static inline void mt_fhctl_init_debugfs(struct mtk_fhctl *fhctl)
{
}
static inline void mt_fhctl_exit_debugfs(struct mtk_fhctl *fhctl)
{
}
static inline void mt_fhctl_log_b4_hopping(struct clk_mt_fhctl *fhctl, unsigned int target_dds, unsigned int tx_id, struct pll_status *fh_log)
{
}
static inline void mt_fhctl_log_af_hopping(struct clk_mt_fhctl *fhctl, int ret_from_ipi, unsigned int ack_data, struct pll_status *fh_log, void (*ipi_get_data)(unsigned int), u64 time_ns)
{
}
static inline void mt_fh_dump_register(void)
{
}

#endif

#endif /* __DRV_CLK_FHCTL_DEBUG_H */

