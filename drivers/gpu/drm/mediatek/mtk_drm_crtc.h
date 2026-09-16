/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_DRM_CRTC_H
#define MTK_DRM_CRTC_H

#include <drm/drm_crtc.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include <drm/drm_writeback.h>

#define MTK_LUT_SIZE	512
#define MTK_MAX_BPC	10
#define MTK_MIN_BPC	3

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#define USER_INVALID	0xf
#define IPC_LAYER_NR	4UL
#endif

/* AP-IPC-CM4 */
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
struct mtk_display_layer_config {
	uint8_t				enable;
	uint32_t			addr;
	uint32_t			pitch;
	uint32_t			format;
	uint32_t			x;
	uint32_t			y;
	uint32_t			width;
	uint32_t			height;
	uint32_t			color_matrix;
	uint32_t			alpha;
	uint32_t			surfl_en;
	uint32_t			d_alpha;
	uint32_t			s_alpha;
};

struct mtk_display_ipc_open {
	uint64_t	mtk_crtc_addr;
};

struct mtk_display_ipc_close {
	uint8_t id;
};

struct mtk_display_ipc_open_rsp {
	uint8_t id;
};

struct mtk_display_ipc_layer_data {
	uint8_t	user_id;
	uint8_t layer_mask;
	struct mtk_display_layer_config config[IPC_LAYER_NR];
};
#endif

/**
 * struct mtk_drm_crtc - MediaTek specific crtc structure.
 * @base: crtc object.
 * @enabled: records whether crtc_enable succeeded
 * @planes: array of 4 drm_plane structures, one for each overlay plane
 * @pending_planes: whether any plane has pending changes to be applied
 * @config_regs: memory mapped mmsys configuration register space
 * @mutex: handle to one of the ten disp_mutex streams
 * @ddp_comp_nr: number of components in ddp_comp
 * @ddp_comp: array of pointers the mtk_ddp_comp structures used by this crtc
 */
struct mtk_drm_crtc {
	struct drm_crtc			base;
	bool				enabled;

	bool				pending_needs_vblank;
	struct drm_pending_vblank_event	*event;
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	struct cmdq_client		*cmdq_client;
	u32				cmdq_event;
#endif

	struct drm_plane		*planes;
	unsigned int			layer_nr;
	bool				pending_planes;

	void __iomem			*config_regs;
	struct mtk_disp_mutex		*mutex;
	unsigned int			ddp_comp_nr;
	struct mtk_ddp_comp		**ddp_comp;
	struct drm_writeback_connector	wb_connector;
	bool				wb_enable;
	bool				wb_hw_enable;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	uint8_t			user_id;
	struct mtk_display_ipc_layer_data layer_data;
#endif

};

void mtk_drm_crtc_commit(struct drm_crtc *crtc);
void mtk_crtc_ddp_irq(struct drm_crtc *crtc, struct mtk_ddp_comp *comp);
int mtk_drm_crtc_create(struct drm_device *drm_dev,
			const enum mtk_ddp_comp_id *path,
			unsigned int path_len);
void mtk_drm_crtc_plane_update(struct drm_crtc *crtc,
	struct drm_plane *plane,
	struct mtk_plane_pending_state *pending);
void mtk_drm_crtc_atomic_enable_test(struct drm_crtc *crtc,
				       struct drm_crtc_state *old_state);

#endif /* MTK_DRM_CRTC_H */
