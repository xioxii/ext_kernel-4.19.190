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

#ifndef MTK_DRM_DRV_H
#define MTK_DRM_DRV_H

#include <drm/drm_fb_helper.h>
#include <linux/io.h>
#include "mtk_drm_ddp_comp.h"

#define MAX_CRTC	3
#define MAX_CONNECTOR	3

// charles
// #undef CONFIG_MTK_DISPLAY_CMDQ

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct drm_property;
struct regmap;

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
enum mtk_ipi_prop_type {
	crtc_plane_using = 0,
	plane0_user,
	plane1_user,
	plane2_user,
	plane3_user,
	invalid_prop_type,
};
struct mtk_drm_ipc_prop_data {
	uint8_t type;
	uint32_t val;
};
extern int mtk_drm_ipi_set_property(unsigned int type, uint64_t val);
extern int mtk_drm_ipi_get_property(unsigned int type);
extern void mtk_drm_ipi_get_property_rsp(void *private, char *data);
#endif

struct mtk_mmsys_driver_data {
	enum mtk_ddp_comp_id *main_path;
	unsigned int main_len;
	enum mtk_ddp_comp_id *ext_path;
	unsigned int ext_len;
	enum mtk_ddp_comp_id *third_path;
	unsigned int third_len;

	bool shadow_register;
};
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
struct mtk_drm_power_mode {
	struct regmap *map;
	unsigned int offset;
	unsigned int mask;
};
#endif
struct mtk_drm_private {
	struct drm_device *drm;
	struct device *dma_dev;

	unsigned int num_pipes;

	struct device_node *mutex_node;
	struct device *mutex_dev;
	void __iomem *config_regs;
	void __iomem *smi_larb_base;

	struct device_node *comp_node[DDP_COMPONENT_ID_MAX];
	struct mtk_ddp_comp *ddp_comp[DDP_COMPONENT_ID_MAX];
	const struct mtk_mmsys_driver_data *data;

	struct drm_property *alpha;
	struct drm_property *color_matrix;
	/*Layer Surface Flinger alpha_blending enable*/
	struct drm_property *surfl_en;
	/*Layer Destination alpha select*/
	struct drm_property *d_alpha;
	/*Layer Source alpha select*/
	struct drm_property *s_alpha;
	/* Layer reserved user */
	struct drm_property *plane_reserved_user;
	/* Layer using state */
	struct drm_property *plane_using_state;

	#ifdef CONFIG_MTK_DISPLAY_COLOR
	struct drm_property *mtk_prop_pqparam;
	struct drm_property *mtk_prop_pqindex;
	struct drm_property *mtk_prop_color_bypass;
	struct drm_property *mtk_prop_color_window;
	struct drm_property *mtk_prop_get_pqparam;
	#endif

	struct {
		struct drm_atomic_state *state;
		struct work_struct work;
		struct mutex lock;
	} commit;

	/* crtcs pending async atomic updates: */
	unsigned int pending_crtcs;
	wait_queue_head_t pending_crtcs_event;

	struct drm_atomic_state *suspend_state;

	bool dma_parms_allocated;

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	wait_queue_head_t pending_get_prop;
	struct mtk_drm_ipc_prop_data get_prop_data;
	struct mtk_drm_power_mode *power_mode;
#endif
	struct drm_fb_helper fb_helper;
	struct drm_gem_object *fbdev_bo;
};

#define MTK_PLANE_0     (1 << 0)
#define MTK_PLANE_1     (1 << 1)
#define MTK_PLANE_2     (1 << 2)
#define MTK_PLANE_3     (1 << 3)

#define MTK_PLANE_MASK (MTK_PLANE_0 | MTK_PLANE_1 | MTK_PLANE_2 | MTK_PLANE_3)

extern struct platform_driver mtk_ddp_driver;
extern struct platform_driver mtk_disp_color_driver;
extern struct platform_driver mtk_disp_ovl_driver;
extern struct platform_driver mtk_disp_rdma_driver;
extern struct platform_driver mtk_dpi_driver;
extern struct platform_driver mtk_dsi_driver;
extern struct platform_driver mtk_dbi_driver;
extern struct platform_driver mtk_mipi_tx_driver;
extern struct platform_driver mtk_lvds_driver;
extern struct platform_driver mtk_lvds_tx_driver;

#endif /* MTK_DRM_DRV_H */
