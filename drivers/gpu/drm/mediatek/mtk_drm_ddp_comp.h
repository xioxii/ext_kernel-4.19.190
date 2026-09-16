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

#ifndef MTK_DRM_DDP_COMP_H
#define MTK_DRM_DDP_COMP_H

#include <linux/io.h>
#ifdef CONFIG_MTK_DISPLAY_CMDQ
#include <linux/soc/mediatek/mtk-cmdq.h>
#endif
#include "mtk_drm_plane.h"
#include "mtk_drm_gem.h"

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct mtk_plane_pending_state;
struct drm_crtc_state;

enum mtk_ddp_comp_type {
	MTK_DISP_OVL,
	MTK_DISP_RDMA,
	MTK_DISP_WDMA,
	MTK_DISP_COLOR,
	MTK_DISP_AAL,
	MTK_DISP_GAMMA,
	MTK_DISP_UFOE,
	MTK_DSI,
	MTK_DBI,
	MTK_DPI,
	MTK_LVDS,
	MTK_DISP_PWM,
	MTK_DISP_MUTEX,
	MTK_DISP_OD,
	MTK_DISP_BLS,
	MTK_DDP_COMP_TYPE_MAX,
};

enum mtk_ddp_comp_id {
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_AAL1,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_DSI0,  //7
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_DSI2,
	DDP_COMPONENT_DSI3,
	DDP_COMPONENT_DBI0, // 11
	DDP_COMPONENT_LVDS0,
	DDP_COMPONENT_LVDS1,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_OVL0, //17
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_PWM0, //19
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_PWM2,
	DDP_COMPONENT_RDMA0,  //22
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_WDMA0,
	DDP_COMPONENT_WDMA1,
	DDP_COMPONENT_CONFIG,
	DDP_COMPONENT_MUTEX,
	DDP_COMPONENT_ID_MAX,
};

struct mtk_ddp_comp;

struct mtk_ddp_comp_funcs {
	void (*config)(struct mtk_ddp_comp *comp, unsigned int w,
		       unsigned int h, unsigned int vrefresh,
		       unsigned int bpc, void *handle);
	void (*prepare)(struct mtk_ddp_comp *comp);
	void (*unprepare)(struct mtk_ddp_comp *comp);
	void (*start)(struct mtk_ddp_comp *comp, void *handle);
	void (*stop)(struct mtk_ddp_comp *comp, void *handle);
	void (*enable_vblank)(struct mtk_ddp_comp *comp,
		struct drm_crtc *crtc, void *handle);
	void (*disable_vblank)(struct mtk_ddp_comp *comp, void *handle);
	unsigned int (*layer_nr)(struct mtk_ddp_comp *comp);
	void (*layer_on)(struct mtk_ddp_comp *comp,
		unsigned int idx, void *handle);
	void (*layer_off)(struct mtk_ddp_comp *comp,
		unsigned int idx, void *handle);
	void (*layer_config)(struct mtk_ddp_comp *comp,
		unsigned int idx, struct mtk_plane_pending_state *pending,
		void *handle);
	void (*gamma_set)(struct mtk_ddp_comp *comp,
			  struct drm_crtc_state *state, void *handle);
 	void (*atomic_flush)(struct mtk_ddp_comp *comp);
};

struct mtk_ddp_comp {
	struct clk *clk;
	void __iomem *regs;
	int irq;
	struct device *larb_dev;
	struct device *dev;
	enum mtk_ddp_comp_id id;
	struct mtk_drm_gem_obj *mtk_gem;
	const struct mtk_ddp_comp_funcs *funcs;
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	struct cmdq_base *cmdq_base;
#endif

};

static inline void mtk_ddp_comp_config(struct mtk_ddp_comp *comp,
		unsigned int w, unsigned int h,
		unsigned int vrefresh, unsigned int bpc, void *handle)
{
	if (comp->funcs && comp->funcs->config)
		comp->funcs->config(comp, w, h, vrefresh, bpc, handle);
}

static inline void mtk_ddp_comp_prepare(struct mtk_ddp_comp *comp)
{
	if (comp->funcs && comp->funcs->prepare)
		comp->funcs->prepare(comp);
}

static inline void mtk_ddp_comp_unprepare(struct mtk_ddp_comp *comp)
{
	if (comp->funcs && comp->funcs->unprepare)
		comp->funcs->unprepare(comp);
}

static inline void mtk_ddp_comp_start(struct mtk_ddp_comp *comp,
		void *handle)
{
	if (comp->funcs && comp->funcs->start)
		comp->funcs->start(comp, handle);
}

static inline void mtk_ddp_comp_stop(struct mtk_ddp_comp *comp,
		void *handle)
{
	if (comp->funcs && comp->funcs->stop)
		comp->funcs->stop(comp, handle);
}

static inline void mtk_ddp_comp_enable_vblank(struct mtk_ddp_comp *comp,
		struct drm_crtc *crtc, void *handle)
{
	if (comp->funcs && comp->funcs->enable_vblank)
		comp->funcs->enable_vblank(comp, crtc, handle);
}

static inline void mtk_ddp_comp_disable_vblank(struct mtk_ddp_comp *comp,
		void *handle)
{
	if (comp->funcs && comp->funcs->disable_vblank)
		comp->funcs->disable_vblank(comp, handle);
}

static inline unsigned int mtk_ddp_comp_layer_nr(struct mtk_ddp_comp *comp)
{
	if (comp->funcs && comp->funcs->layer_nr)
		return comp->funcs->layer_nr(comp);

	return 0;
}

static inline void mtk_ddp_comp_layer_on(struct mtk_ddp_comp *comp,
		unsigned int idx, void *handle)
{
	if (comp->funcs && comp->funcs->layer_on)
		comp->funcs->layer_on(comp, idx, handle);
}

static inline void mtk_ddp_comp_layer_off(struct mtk_ddp_comp *comp,
		unsigned int idx, void *handle)
{
	if (comp->funcs && comp->funcs->layer_off)
		comp->funcs->layer_off(comp, idx, handle);
}

static inline void mtk_ddp_comp_layer_config(struct mtk_ddp_comp *comp,
		unsigned int idx, struct mtk_plane_pending_state *pending,
		void *handle)
{
	if (comp->funcs && comp->funcs->layer_config)
		comp->funcs->layer_config(comp, idx,  pending, handle);
}

static inline void mtk_ddp_gamma_set(struct mtk_ddp_comp *comp,
				     struct drm_crtc_state *state, void *handle)
{
	if (comp->funcs && comp->funcs->gamma_set)
		comp->funcs->gamma_set(comp, state, handle);
}

static inline void mtk_ddp_atomic_flush(struct mtk_ddp_comp *comp)
{
	if (comp->funcs && comp->funcs->atomic_flush)
		comp->funcs->atomic_flush(comp);
}

int mtk_ddp_comp_get_id(struct device_node *node,
			enum mtk_ddp_comp_type comp_type);
struct mtk_ddp_comp *mtk_ddp_comp_find_by_id(struct drm_crtc *crtc,
					     enum mtk_ddp_comp_id comp_id);
unsigned int mtk_drm_find_possible_crtc_by_comp(struct drm_device *drm,
						struct mtk_ddp_comp ddp_comp);
int mtk_ddp_comp_init(struct device *dev, struct device_node *comp_node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs);
int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp);
void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp);
void mtk_dither_set(struct mtk_ddp_comp *comp, unsigned int bpc,
		    unsigned int CFG, void *handle);
void mtk_ddp_write(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, void *handle);
void mtk_ddp_write_relaxed(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, void *handle);
void mtk_ddp_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, unsigned int mask, void *handle);

#endif /* MTK_DRM_DDP_COMP_H */
