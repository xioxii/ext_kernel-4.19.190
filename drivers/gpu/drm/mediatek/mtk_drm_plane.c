/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: CK Hu <ck.hu@mediatek.com>
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

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"

static const u32 formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_YUYV,
};

static void mtk_plane_reset(struct drm_plane *plane)
{
	struct mtk_plane_state *state;

	if (plane->state) {
		__drm_atomic_helper_plane_destroy_state(plane->state);

		state = to_mtk_plane_state(plane->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		plane->state = &state->base;
	}

	state->base.plane = plane;
	state->pending.alpha = 0xff;
	state->pending.format = DRM_FORMAT_RGB565;
	state->pending.color_matrix = 0;
}

static struct drm_plane_state *mtk_plane_duplicate_state(struct drm_plane *plane)
{
	struct mtk_plane_state *old_state = to_mtk_plane_state(plane->state);
	struct mtk_plane_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	WARN_ON(state->base.plane != plane);

	state->pending = old_state->pending;
	state->reserved_user = old_state->reserved_user;

	return &state->base;
}

static void mtk_drm_plane_destroy_state(struct drm_plane *plane,
					struct drm_plane_state *state)
{
	__drm_atomic_helper_plane_destroy_state(state);
	kfree(to_mtk_plane_state(state));
}

static int
mtk_plane_set_reserved_user(struct drm_plane *plane,
	struct mtk_plane_state *mstate, uint64_t val)
{
	int ret = 0;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	unsigned int index = drm_plane_index(plane);
	uint8_t ipc_type;

	if (index < 4) {
		if (index == 0)
			ipc_type = plane0_user;
		else if (index == 1)
			ipc_type = plane1_user;
		else if (index == 2)
			ipc_type = plane2_user;
		else if (index == 3)
			ipc_type = plane3_user;

		ret = mtk_drm_ipi_set_property(ipc_type, val);
	} else {
#endif
		if (val != 0) {
			if (mstate->reserved_user == 0) {
				mstate->reserved_user = val;
				DRM_DEBUG_ATOMIC("[PLANE:%d]set user id%llu\n",
					mstate->base.plane->base.id, val);
			} else {
				DRM_WARN("[PLANE:%d] used by pid%d, not set\n",
					mstate->base.plane->base.id,
					mstate->reserved_user);
				return -EINVAL;
			}

		} else {
			mstate->reserved_user = val;
			DRM_DEBUG_ATOMIC("[PLANE:%d] reserved user clear\n",
				mstate->base.plane->base.id);
		}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	}
#endif
	return ret;

}

static uint64_t
mtk_plane_get_reserved_user(struct drm_plane *plane,
	const struct mtk_plane_state *mstate)
{
	uint64_t val = 0;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	struct mtk_drm_private *priv = plane->dev->dev_private;
	unsigned int index = drm_plane_index(plane);
	uint8_t ipc_type;
	int ret;

	if (index < 4) {
		if (index == 0)
			ipc_type = plane0_user;
		else if (index == 1)
			ipc_type = plane1_user;
		else if (index == 2)
			ipc_type = plane2_user;
		else if (index == 3)
			ipc_type = plane3_user;

		ret = mtk_drm_ipi_get_property(ipc_type);
		if (ret < 0) {
			DRM_ERROR("plane get property send ipi failed\n");
			/* we return this value, user will re-get it */
			return val;
		}

		ret = wait_event_timeout(priv->pending_get_prop,
			((1 << ipc_type) == priv->get_prop_data.type),
			msecs_to_jiffies(100));
		if (ret == 0) {
			DRM_ERROR("crtc get plane reserved user timeout\n");

		} else {
			val = priv->get_prop_data.val;
			priv->get_prop_data.type = 0;
		}
	} else {
#endif
		val = mstate->reserved_user;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	}
#endif
	return val;

}

static int mtk_plane_atomic_set_property(struct drm_plane *plane,
				struct drm_plane_state *state,
				struct drm_property *property,
				uint64_t val)
{
	struct mtk_plane_state *mstate = to_mtk_plane_state(state);
	struct mtk_drm_private *priv = plane->dev->dev_private;
	int ret;

	if (property == priv->alpha)
		mstate->pending.alpha = val;
	else if (property == priv->color_matrix)
		mstate->pending.color_matrix = val;
	else if (property == priv->surfl_en)
		mstate->pending.surfl_en = val;
	else if (property == priv->d_alpha)
		mstate->pending.d_alpha = val;
	else if (property == priv->s_alpha)
		mstate->pending.s_alpha = val;
	else if (property == priv->plane_reserved_user) {
		ret = mtk_plane_set_reserved_user(plane, mstate, val);
		return ret;
	} else
		return -EINVAL;
	return 0;
}

static int mtk_plane_atomic_get_property(struct drm_plane *plane,
				const struct drm_plane_state *state,
				struct drm_property *property,
				uint64_t *val)
{
	const struct mtk_plane_state *mstate =
		container_of(state, const struct mtk_plane_state, base);
	struct mtk_drm_private *priv = plane->dev->dev_private;

	if (property == priv->alpha)
		*val = mstate->pending.alpha;
	else if (property == priv->color_matrix)
		*val = mstate->pending.color_matrix;
	else if (property == priv->surfl_en)
		*val = mstate->pending.surfl_en;
	else if (property == priv->d_alpha)
		*val = mstate->pending.d_alpha;
	else if (property == priv->s_alpha)
		*val = mstate->pending.s_alpha;
	else if (property == priv->plane_reserved_user)
		*val = mtk_plane_get_reserved_user(plane, mstate);
	else
		return -EINVAL;
	return 0;
}

static const struct drm_plane_funcs mtk_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = mtk_plane_reset,
	.atomic_duplicate_state = mtk_plane_duplicate_state,
	.atomic_destroy_state = mtk_drm_plane_destroy_state,
	.atomic_set_property = mtk_plane_atomic_set_property,
	.atomic_get_property = mtk_plane_atomic_get_property,
};

static int mtk_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	struct drm_framebuffer *fb = state->fb;
	struct drm_crtc_state *crtc_state;

	if (!fb)
		return 0;

	if (!state->crtc)
		return 0;

	crtc_state = drm_atomic_get_crtc_state(state->state, state->crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	return drm_atomic_helper_check_plane_state(state, crtc_state,
						   DRM_PLANE_HELPER_NO_SCALING,
						   DRM_PLANE_HELPER_NO_SCALING,
						   true, true);
}

static void mtk_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);

	state->pending.enable = false;
	wmb(); /* Make sure the above parameter is set before update */
	state->pending.dirty = true;

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	/* Fetch CRTC from old plane state when disabling. */
	mtk_drm_crtc_plane_update(old_state->crtc, plane, &state->pending);
#endif

}

static void mtk_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);
	struct drm_crtc *crtc = plane->state->crtc;
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *gem;
	struct mtk_drm_gem_obj *mtk_gem;
	unsigned int pitch, format;
	dma_addr_t addr;

	if (!crtc || WARN_ON(!fb))
		return;

	if (!plane->state->visible) {
		mtk_plane_atomic_disable(plane, old_state);
		return;
	}

	gem = fb->obj[0];
	mtk_gem = to_mtk_gem_obj(gem);
	addr = mtk_gem->dma_addr;
	pitch = fb->pitches[0];
	format = fb->format->format;

	addr += (unsigned int)(plane->state->src.x1 >> 16) * fb->format->cpp[0];
	addr += (plane->state->src.y1 >> 16) * pitch;

	state->pending.enable = true;
	state->pending.pitch = pitch;
	state->pending.format = format;
	state->pending.addr = addr;
	state->pending.x = plane->state->dst.x1;
	state->pending.y = plane->state->dst.y1;
	state->pending.width = drm_rect_width(&plane->state->dst);
	state->pending.height = drm_rect_height(&plane->state->dst);
	wmb(); /* Make sure the above parameters are set before update */
	state->pending.dirty = true;
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	mtk_drm_crtc_plane_update(crtc, plane, &state->pending);
#endif

}

static const struct drm_plane_helper_funcs mtk_plane_helper_funcs = {
	.atomic_check = mtk_plane_atomic_check,
	.atomic_update = mtk_plane_atomic_update,
	.atomic_disable = mtk_plane_atomic_disable,
};

int mtk_plane_init(struct drm_device *dev, struct drm_plane *plane,
		   unsigned long possible_crtcs, enum drm_plane_type type)
{
	struct mtk_drm_private *priv = dev->dev_private;
	int err;

	err = drm_universal_plane_init(dev, plane, possible_crtcs,
				       &mtk_plane_funcs, formats,
				       ARRAY_SIZE(formats), NULL, type, NULL);
	if (err) {
		DRM_ERROR("failed to initialize plane\n");
		return err;
	}

	drm_plane_helper_add(plane, &mtk_plane_helper_funcs);
	drm_object_attach_property(&plane->base, priv->alpha, 255);
	drm_object_attach_property(&plane->base, priv->color_matrix, 0);
	drm_object_attach_property(&plane->base, priv->surfl_en, 0);
	drm_object_attach_property(&plane->base, priv->d_alpha, 0);
	drm_object_attach_property(&plane->base, priv->s_alpha, 0);
	drm_object_attach_property(&plane->base, priv->plane_reserved_user, 0);

	return 0;
}
