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

#include <asm/barrier.h>
#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#ifdef CONFIG_MTK_DISPLAY_CMDQ
#include <linux/of_address.h>
// #include <dt-bindings/gce/mt2712-gce.h>
#include <dt-bindings/gce/mt6890-gce.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#endif


#include "mtk_drm_drv.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"
#include "mtk_writeback.h"
#ifdef CONFIG_MTK_DISPLAY_COLOR
#include "mtk_drm_color.h"
#endif

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include "ipi/scp_ipi.h"
#endif

struct mtk_crtc_state {
	struct drm_crtc_state		base;

	bool				pending_config;
	unsigned int			pending_width;
	unsigned int			pending_height;
	unsigned int			pending_vrefresh;
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	struct cmdq_pkt			*cmdq_handle;
#else
	void				*cmdq_handle;
#endif
};

#ifdef CONFIG_MTK_DISPLAY_CMDQ
struct mtk_cmdq_cb_data {
	struct cmdq_pkt			*cmdq_handle;
	struct mtk_drm_crtc		*mtk_crtc;
};
#endif

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#define OVL0_FASTRVC_LAYER_ID  3
#endif

// For some reasons, the first attempt of OVL_RST in mtk_crtc_ddp_hw_init stage is not successfully.
// It leads to an image shifting issue. So we trigger another OVL_RST in mtk_drm_crtc_plane_update
// If we finish the reset, then pull up this flag
static int g_ovl_reset_done = 0;

static inline struct mtk_drm_crtc *to_mtk_crtc(struct drm_crtc *c)
{
	return container_of(c, struct mtk_drm_crtc, base);
}

static inline struct mtk_crtc_state *to_mtk_crtc_state(struct drm_crtc_state *s)
{
	return container_of(s, struct mtk_crtc_state, base);
}

static void mtk_drm_crtc_finish_page_flip(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	unsigned long flags;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	drm_crtc_send_vblank_event(crtc, mtk_crtc->event);
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	drm_crtc_vblank_put(crtc);
#endif
	mtk_crtc->event = NULL;
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}
#ifdef CONFIG_MTK_DISPLAY_CMDQ
#else
static void mtk_drm_finish_page_flip(struct mtk_drm_crtc *mtk_crtc)
{
	if (mtk_crtc->pending_needs_vblank) {
		mtk_drm_crtc_finish_page_flip(mtk_crtc);
		mtk_crtc->pending_needs_vblank = false;
	}
}
#endif

static void mtk_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	mtk_disp_mutex_put(mtk_crtc->mutex);

	drm_crtc_cleanup(crtc);
}

static void mtk_drm_crtc_reset(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state;

	if (crtc->state) {
		__drm_atomic_helper_crtc_destroy_state(crtc->state);

		state = to_mtk_crtc_state(crtc->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		crtc->state = &state->base;
	}

	state->base.crtc = crtc;
}

static struct drm_crtc_state *mtk_drm_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);

	WARN_ON(state->base.crtc != crtc);
	state->base.crtc = crtc;

	return &state->base;
}

static void mtk_drm_crtc_destroy_state(struct drm_crtc *crtc,
				       struct drm_crtc_state *state)
{
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(to_mtk_crtc_state(state));
}

static bool mtk_drm_crtc_mode_fixup(struct drm_crtc *crtc,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	/* Nothing to do here, but this callback is mandatory. */
	return true;
}

static void mtk_drm_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);

	state->pending_width = crtc->mode.hdisplay;
	state->pending_height = crtc->mode.vdisplay;
	state->pending_vrefresh = crtc->mode.vrefresh;
	wmb();	/* Make sure the above parameters are set before update */
	state->pending_config = true;
}

static int mtk_drm_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];

	mtk_ddp_comp_enable_vblank(comp, &mtk_crtc->base, NULL);

	return 0;
}

static void mtk_drm_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];

	mtk_ddp_comp_disable_vblank(comp, NULL);
}

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
static int mtk_crtc_ddp_clk_enable(struct mtk_drm_crtc *mtk_crtc)
{
	int ret;
	int i;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++) {
		ret = clk_prepare_enable(mtk_crtc->ddp_comp[i]->clk);
		DRM_DEBUG_DRIVER("%s enable clock %d: %d\n", __func__, i, ret);
		if (ret) {
			DRM_ERROR("Failed to enable clock %d: %d\n", i, ret);
			goto err;
		}
	}

	return 0;
err:
	while (--i >= 0)
		clk_disable_unprepare(mtk_crtc->ddp_comp[i]->clk);
	return ret;
}

static void mtk_crtc_ddp_clk_disable(struct mtk_drm_crtc *mtk_crtc)
{
	int i;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
		clk_disable_unprepare(mtk_crtc->ddp_comp[i]->clk);
}
#endif

#ifdef CONFIG_MTK_DISPLAY_CMDQ
static void ddp_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct mtk_drm_crtc *mtk_crtc = cb_data->mtk_crtc;

	if (mtk_crtc->pending_needs_vblank) {
		mtk_drm_crtc_finish_page_flip(mtk_crtc);
		mtk_crtc->pending_needs_vblank = false;
	}

	if (mtk_crtc->wb_enable == true) {
		mtk_crtc->wb_enable = false;
		drm_writeback_signal_completion(&mtk_crtc->wb_connector, 0);
	}

	// cmdq_pkt_destroy(cb_data->cmdq_handle);
	// kfree(cb_data);
}
#endif


static int mtk_crtc_ddp_hw_init(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_connector_list_iter conn_iter;

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	struct cmdq_pkt *cmdq_handle;
#else
	void *cmdq_handle = NULL;
#endif

	unsigned int width, height, vrefresh, bpc = MTK_MAX_BPC;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	int ret;
#endif
	int i;

	printk(KERN_DEBUG "mtk_crtc_ddp_hw_init\n");
	DRM_DEBUG_DRIVER("%s\n", __func__);
	if (WARN_ON(!crtc->state))
		return -EINVAL;

	width = crtc->state->adjusted_mode.hdisplay;
	height = crtc->state->adjusted_mode.vdisplay;
	vrefresh = crtc->state->adjusted_mode.vrefresh;


	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;

		drm_connector_list_iter_begin(crtc->dev, &conn_iter);
		drm_for_each_connector_iter(connector, &conn_iter) {
			if (connector->encoder != encoder)
				continue;
			if (connector->display_info.bpc != 0 &&
			    bpc > connector->display_info.bpc)
				bpc = connector->display_info.bpc;
		}
		drm_connector_list_iter_end(&conn_iter);
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	ret = pm_runtime_get_sync(crtc->dev->dev);
	if (ret < 0) {
		DRM_ERROR("Failed to enable power domain: %d\n", ret);
		return ret;
	}

	ret = mtk_disp_mutex_prepare(mtk_crtc->mutex);
	if (ret < 0) {
		DRM_ERROR("Failed to enable mutex clock: %d\n", ret);
		goto err_pm_runtime_put;
	}

	ret = mtk_crtc_ddp_clk_enable(mtk_crtc);
	if (ret < 0) {
		DRM_ERROR("Failed to enable component clocks: %d\n", ret);
		goto err_mutex_unprepare;
	}
#endif

	DRM_DEBUG_DRIVER("mediatek_ddp_ddp_path_setup\n");

	mtk_clean_mutex(mtk_crtc->mutex);   // test

	for (i = 0; i < mtk_crtc->ddp_comp_nr - 1; i++) {
		mtk_ddp_comp_prepare(mtk_crtc->ddp_comp[i]);
		mtk_ddp_add_comp_to_path(mtk_crtc->config_regs,
					 mtk_crtc->ddp_comp[i]->id,
					 mtk_crtc->ddp_comp[i + 1]->id);
		mtk_disp_mutex_add_comp(mtk_crtc->mutex,
					mtk_crtc->ddp_comp[i]->id);
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	mtk_wb_add_comp_to_path(mtk_crtc);
#endif
	// temp test
	mtk_ddp_comp_prepare(mtk_crtc->ddp_comp[i]);
	mtk_disp_mutex_add_comp(mtk_crtc->mutex,
		mtk_crtc->ddp_comp[i]->id);
	mtk_disp_mutex_enable(mtk_crtc->mutex);

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	printk(KERN_DEBUG "CONFIG_MTK_DISPLAY_CMDQ ON\n");
	DRM_DEBUG_DRIVER("ddp_disp_path_power_on %dx%d\n", width, height);
	cmdq_handle = cmdq_pkt_create(mtk_crtc->cmdq_client, PAGE_SIZE);
	// cmdq_pkt_add_cmd_buffer(cmdq_handle);
#endif

	// temp test
	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++) {
		struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[i];

		mtk_ddp_comp_config(comp, width, height, vrefresh,
			bpc, cmdq_handle);
		mtk_ddp_comp_start(comp, cmdq_handle);
	}

	/* Initially configure all planes */
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (mtk_crtc->ddp_comp[0]->id != DDP_COMPONENT_OVL0)
#endif
	{
		for (i = 0; i < mtk_crtc->layer_nr; i++) {
			struct mtk_plane_pending_state pending = {
						.enable = false };

			mtk_ddp_comp_layer_config(mtk_crtc->ddp_comp[0], i,
						  &pending, cmdq_handle);
		}
	}
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	// cmdq_pkt_set_event(cmdq_handle, CMDQ_EVENT_DISP_STREAM_DONE_ENG_EVENT_0);   // sheldobn test
	cmdq_pkt_flush(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
#endif

	return 0;

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
err_mutex_unprepare:
	mtk_disp_mutex_unprepare(mtk_crtc->mutex);
err_pm_runtime_put:
	pm_runtime_put(crtc->dev->dev);
	return ret;
#endif
}

#ifdef CONFIG_MTK_DISPLAY_CMDQ
void mtk_drm_crtc_plane_update(struct drm_crtc *crtc,
		struct drm_plane *plane,
		struct mtk_plane_pending_state *pending)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int plane_index = plane - mtk_crtc->planes;
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc_state);
	struct mtk_ddp_comp *ovl = mtk_crtc->ddp_comp[0];
	unsigned int v = (u32)crtc->state->adjusted_mode.vdisplay;
	unsigned int h = (u32)crtc->state->adjusted_mode.hdisplay;

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (drm_crtc_index(crtc) == 0) {
		if (mtk_crtc->user_id == USER_INVALID)
			return;

		mtk_crtc->layer_data.layer_mask |= BIT(plane_index);
		mtk_crtc->layer_data.config[plane_index].enable =
			(uint8_t)(pending->enable);
		mtk_crtc->layer_data.config[plane_index].addr =
			pending->enable ? (uint32_t)(pending->addr) : 0;
		mtk_crtc->layer_data.config[plane_index].pitch =
			(uint32_t)(pending->pitch);
		mtk_crtc->layer_data.config[plane_index].format =
			(uint32_t)(pending->format);
		mtk_crtc->layer_data.config[plane_index].x =
			(uint32_t)(pending->x);
		mtk_crtc->layer_data.config[plane_index].y =
			(uint32_t)(pending->y);
		mtk_crtc->layer_data.config[plane_index].width =
			(uint32_t)(pending->width);
		mtk_crtc->layer_data.config[plane_index].height =
			(uint32_t)(pending->height);
		mtk_crtc->layer_data.config[plane_index].color_matrix =
			(uint32_t)(pending->color_matrix);
		mtk_crtc->layer_data.config[plane_index].alpha =
			(uint32_t)(pending->alpha);
		mtk_crtc->layer_data.config[plane_index].surfl_en =
			(uint32_t)(pending->surfl_en);
		mtk_crtc->layer_data.config[plane_index].d_alpha =
			(uint32_t)(pending->d_alpha);
		mtk_crtc->layer_data.config[plane_index].s_alpha =
			(uint32_t)(pending->s_alpha);

		DRM_DEBUG_ATOMIC("%s commit: plane_index:%u, addr:%p\n",
			__func__, plane_index, (void *)(pending->addr));

		return;
	}
#endif

	if (!g_ovl_reset_done) {
	    mtk_ddp_comp_config(ovl, state->pending_width,
                                state->pending_height,
	                            state->pending_vrefresh, 0,
	                            state->cmdq_handle);
	    g_ovl_reset_done = 1;
	}

	mtk_ddp_comp_layer_config(ovl, plane_index, pending,
						state->cmdq_handle);
	mtk_wb_atomic_commit(mtk_crtc, v, h, state->cmdq_handle);
}
#endif

static void mtk_crtc_ddp_hw_fini(struct mtk_drm_crtc *mtk_crtc)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	struct drm_device *drm = mtk_crtc->base.dev;
#endif

	void *cmdq_handle = NULL;

	struct drm_crtc *crtc = &mtk_crtc->base;
	int i;

	DRM_DEBUG_DRIVER("%s\n", __func__);

	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
		mtk_ddp_comp_stop(mtk_crtc->ddp_comp[i], cmdq_handle);

	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
		mtk_disp_mutex_remove_comp(mtk_crtc->mutex,
					   mtk_crtc->ddp_comp[i]->id);
	mtk_disp_mutex_disable(mtk_crtc->mutex);
	for (i = 0; i < mtk_crtc->ddp_comp_nr - 1; i++) {
		mtk_ddp_remove_comp_from_path(mtk_crtc->config_regs,
					      mtk_crtc->ddp_comp[i]->id,
					      mtk_crtc->ddp_comp[i + 1]->id);
		mtk_disp_mutex_remove_comp(mtk_crtc->mutex,
					   mtk_crtc->ddp_comp[i]->id);
		mtk_ddp_comp_unprepare(mtk_crtc->ddp_comp[i]);
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	mtk_wb_remove_comp_from_path(mtk_crtc);
#endif
	mtk_disp_mutex_remove_comp(mtk_crtc->mutex,
		mtk_crtc->ddp_comp[i]->id);
	mtk_ddp_comp_unprepare(mtk_crtc->ddp_comp[i]);
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	mtk_crtc_ddp_clk_disable(mtk_crtc);
	mtk_disp_mutex_unprepare(mtk_crtc->mutex);
	pm_runtime_put(drm->dev);
#endif

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
		spin_unlock_irq(&crtc->dev->event_lock);
	}
}

#ifdef CONFIG_MTK_DISPLAY_CMDQ
#else
static void mtk_crtc_ddp_config(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *state =
			to_mtk_crtc_state(mtk_crtc->base.state);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];
	unsigned int i;

    printk(KERN_DEBUG "[%s] enter \n", __func__);

	/*
	 * TODO: instead of updating the registers here, we should prepare
	 * working registers in atomic_commit and let the hardware command
	 * queue update module registers on vblank.
	 */
	if (state->pending_config) {
		mtk_ddp_comp_config(comp, state->pending_width,
			state->pending_height,
			state->pending_vrefresh, 0,
			state->cmdq_handle);

		state->pending_config = false;
	}

	if ((mtk_crtc->pending_planes) == true) {
		mtk_wb_atomic_commit(mtk_crtc);
		for (i = 0; i < mtk_crtc->layer_nr; i++) {
			struct drm_plane *plane = &mtk_crtc->planes[i];
			struct mtk_plane_state *plane_state;

			plane_state = to_mtk_plane_state(plane->state);

			if (plane_state->pending.config) {
				mtk_ddp_comp_layer_config(comp, i,
				&plane_state->pending, state->cmdq_handle);
				plane_state->pending.config = false;
			}
		}
		mtk_crtc->pending_planes = false;
	}
	if (mtk_crtc->wb_enable == true) {
		mtk_crtc->wb_enable = false;
		drm_writeback_signal_completion(&mtk_crtc->wb_connector, 0);
	}
}
#endif

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static void mtk_drm_ipc_open_rsp_handler(void *private, char *data)
{
	struct mtk_drm_crtc *mtk_crtc = (struct mtk_drm_crtc *)private;
	struct mtk_display_ipc_open_rsp *rsp =
		(struct mtk_display_ipc_open_rsp *)data;

	DRM_DEBUG_DRIVER("%s MTK_DISP_IPC_OPEN_RSP recv user_id: %u\n",
		__func__, rsp->id);

	mtk_crtc->user_id = rsp->id;
}

static void mtk_drm_ipc_pageflip_done_handler(void *addr)
{
	struct mtk_drm_crtc *mtk_crtc =
		(struct mtk_drm_crtc *)(*(uint64_t *)addr);

	if (mtk_crtc->pending_needs_vblank && mtk_crtc->event) {
		mtk_drm_crtc_finish_page_flip(mtk_crtc);

		DRM_DEBUG_ATOMIC("%s mtk_drm_crtc_finish_page_flip done\n",
			__func__);
	}

	mtk_crtc->layer_data.layer_mask = 0;
	mtk_crtc->pending_needs_vblank = false;

}
#endif

static void mtk_drm_crtc_atomic_enable(struct drm_crtc *crtc,
				       struct drm_crtc_state *old_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	// struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];  // temp test
	// int ret;  // temp test
	int ret = 0;  // temp test
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	struct queue_msg_t msg;
	struct mtk_display_ipc_open data;
	unsigned int i = 0;
#endif

	DRM_DEBUG_DRIVER("%s %d\n", __func__, crtc->base.id);

	// temp test
	// ret = pm_runtime_get_sync(comp->larb_dev);
	if (ret < 0) {
		DRM_ERROR("Failed to get larb: %d\n", ret);
		// return;    // temp test
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (drm_crtc_index(crtc) == 0) {
		scp_uid_queue_register(MTK_DISP_IPC_OPEN_RSP,
			mtk_drm_ipc_open_rsp_handler,
			(void *)mtk_crtc);
		scp_ipi_register(IPI_FUNC_PAGE_FLIP_DONE,
			mtk_drm_ipc_pageflip_done_handler);

		msg.msg_id = MTK_DISP_IPC_OPEN;
		msg.magic = IPI_MSG_MAGIC_NUMBER;
		data.mtk_crtc_addr = (uint64_t)(uintptr_t)mtk_crtc;
		memcpy(msg.data, &data, sizeof(data));
		DRM_DEBUG_DRIVER("%s IPC_OPEN send\n", __func__);

		while ((scp_uid_queue_send(&msg) != IPI_UID_RET_OK)
			&& (i < 16U)) {
			DRM_DEBUG_DRIVER("IPC_OPEN resend\n");
			mdelay(1);
			i++;
		}

		drm_crtc_vblank_on(crtc);

		mtk_crtc->enabled = true;
		return;
	}
#endif

	ret = mtk_crtc_ddp_hw_init(mtk_crtc);

	// temp test
	// if (ret) {
	// 	pm_runtime_put_sync(comp->larb_dev);
	// 	return;
	// }

	drm_crtc_vblank_on(crtc);
	mtk_crtc->enabled = true;
}

static void mtk_drm_crtc_atomic_disable(struct drm_crtc *crtc,
					struct drm_crtc_state *old_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_crtc->ddp_comp[0];
	int i;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	struct queue_msg_t msg;
	struct mtk_display_ipc_close data;
#endif

	DRM_DEBUG_DRIVER("%s %d\n", __func__, crtc->base.id);

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (drm_crtc_index(crtc) == 0) {
		if (mtk_crtc->user_id == USER_INVALID)
			return;

		drm_crtc_vblank_off(crtc);

		msg.msg_id = MTK_DISP_IPC_CLOSE;
		msg.magic = IPI_MSG_MAGIC_NUMBER;
		data.id = mtk_crtc->user_id;
		memcpy(msg.data, &data, sizeof(data));
		if (scp_uid_queue_send(&msg) != IPI_UID_RET_OK)
			DRM_WARN("%s MTK_DISP_IPC_CLOSE send error\n",
			__func__);
		else
			DRM_DEBUG_DRIVER("%s MTK_DISP_IPC_CLOSE send ok\n",
			__func__);

		pm_runtime_put_sync(comp->larb_dev);

		mtk_crtc->enabled = false;

		return;
	}
#endif

	if (!mtk_crtc->enabled)
		return;

	/* Set all pending plane state to disabled */
	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i];
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		plane_state->pending.enable = false;
		plane_state->pending.config = true;
		plane_state->pending.alpha = 0xff;
		plane_state->pending.color_matrix = 0;
	}
	mtk_crtc->pending_planes = true;

	/* Wait for planes to be disabled */
	drm_crtc_wait_one_vblank(crtc);

	drm_crtc_vblank_off(crtc);
	mtk_crtc_ddp_hw_fini(mtk_crtc);

	pm_runtime_put_sync(comp->larb_dev);

	mtk_crtc->enabled = false;

	printk(KERN_DEBUG "mtk_drm_crtc_atomic_disable end\n");
}

static void mtk_drm_crtc_atomic_begin(struct drm_crtc *crtc,
				      struct drm_crtc_state *old_crtc_state)
{
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

#ifdef CONFIG_MTK_DISPLAY_RVC
	unsigned int i = 0;

	while (mtk_crtc->event && state->base.event && (i < 32U)) {
		usleep_range(500, 1000);
		i++;
	}
#endif
	if (mtk_crtc->event && state->base.event) {
		DRM_ERROR("new event while there is still a pending event\n");
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
		drm_crtc_vblank_put(crtc);
#endif
	}
	if (state->base.event) {
		state->base.event->pipe = drm_crtc_index(crtc);
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
#endif
		mtk_crtc->event = state->base.event;
		state->base.event = NULL;
#ifdef CONFIG_MTK_DISPLAY_CMDQ
		mtk_crtc->pending_needs_vblank = true;
#endif
	}

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (drm_crtc_index(crtc) == 0) {
		if (mtk_crtc->user_id == USER_INVALID)
			return;

		mtk_crtc->pending_needs_vblank = true;
		return;
	}
#endif

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	state->cmdq_handle = cmdq_pkt_create(mtk_crtc->cmdq_client, PAGE_SIZE);
	// cmdq_pkt_add_cmd_buffer(state->cmdq_handle);
	cmdq_pkt_clear_event(state->cmdq_handle, mtk_crtc->cmdq_event);
	// cmdq_pkt_wfe(state->cmdq_handle, mtk_crtc->cmdq_event);
#endif

}

static void mtk_drm_crtc_atomic_flush(struct drm_crtc *crtc,
				      struct drm_crtc_state *old_crtc_state)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	struct drm_crtc_state *crtc_state = crtc->state;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc_state);
	struct cmdq_pkt *cmdq_handle = state->cmdq_handle;
	struct mtk_cmdq_cb_data *cb_data;
#else
	void *cmdq_handle = NULL;

	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int pending_planes = 0;
#endif
	int i;

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (drm_crtc_index(crtc) == 0) {
		struct queue_msg_t msg;

		msg.msg_id = MTK_DISP_IPC_LAYER_DATA_COMMIT;
		msg.magic = IPI_MSG_MAGIC_NUMBER;
		if (mtk_crtc->layer_data.layer_mask == 0) {
			DRM_WARN("%s no layer data committed\n", __func__);
			return;
		}

		mtk_crtc->layer_data.user_id = mtk_crtc->user_id;
		memcpy(msg.data, &(mtk_crtc->layer_data),
				sizeof(mtk_crtc->layer_data));
		if (scp_uid_queue_send(&msg) != IPI_UID_RET_OK)
			DRM_WARN("IPC_LAYER_DATA_COMMIT send err\n");
		else
			DRM_DEBUG_ATOMIC("IPC_LAYER_DATA_COMMIT send ok\n");
		return;
	}
#endif

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DRM_ERROR("cmdq cb_data alloc err\n");
		return;
	}

	cb_data->cmdq_handle = cmdq_handle;
	cb_data->mtk_crtc = mtk_crtc;
	(void)cmdq_pkt_flush_async(cmdq_handle, ddp_cmdq_cb,
						(void *) cb_data);

	// test +
	cmdq_pkt_wait_complete(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
    kfree(cb_data);
    // -
#else

	if (mtk_crtc->event)
		mtk_crtc->pending_needs_vblank = true;
	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i];
		struct mtk_plane_state *plane_state;

		plane_state = to_mtk_plane_state(plane->state);
		if (plane_state->pending.dirty) {
			plane_state->pending.config = true;
			plane_state->pending.dirty = false;
			pending_planes |= BIT(i);
		}
	}
	if (pending_planes)
		mtk_crtc->pending_planes = true;
	if (crtc->state->color_mgmt_changed)
		for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
			mtk_ddp_gamma_set(mtk_crtc->ddp_comp[i],
						crtc->state, cmdq_handle);

	if (priv->data->shadow_register) {
		mtk_disp_mutex_acquire(mtk_crtc->mutex);
		mtk_crtc_ddp_config(crtc);
		mtk_disp_mutex_release(mtk_crtc->mutex);
	}

    printk(KERN_DEBUG "[%s] before mtk_ddp_atomic_flush", __func__);
#endif

    for (i = 0; i < mtk_crtc->ddp_comp_nr; i++)
        mtk_ddp_atomic_flush(mtk_crtc->ddp_comp[i]);

}

static uint64_t
mtk_drm_crtc_plane_using_state(struct drm_crtc *crtc)
{
	uint64_t val = 0;
	unsigned int zpos;
	struct drm_plane *plane;
	const struct mtk_plane_state *plane_state;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	int ret;

	if (drm_crtc_index(crtc) == 0) {
		ret = mtk_drm_ipi_get_property(crtc_plane_using);
		if (ret < 0) {
			DRM_ERROR("crtc get property send ipi failed\n");
			/* we return this value, so it will re-set again*/
			val = 0xff;
			return val;
		}

		ret = wait_event_timeout(priv->pending_get_prop,
			((1 << crtc_plane_using) == priv->get_prop_data.type),
			msecs_to_jiffies(100));
		if (ret == 0) {
			DRM_ERROR("crtc get plane using state timeout\n");
			val = 0xff;
		} else {
			val = priv->get_prop_data.val;
			priv->get_prop_data.type = 0;
		}
	} else {
#endif
		for (zpos = 0; zpos < mtk_crtc->layer_nr; zpos++) {
			plane = &mtk_crtc->planes[zpos];
			plane_state = container_of(plane->state,
				const struct mtk_plane_state, base);
			if (plane_state->reserved_user != 0)
				val |= (1 << zpos);
		}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	}
#endif
	return val;
}

#ifdef CONFIG_MTK_DISPLAY_COLOR
static int mtk_create_color_property(struct drm_device *drm)
{
	struct mtk_drm_private *priv = drm->dev_private;

	priv->mtk_prop_pqparam = drm_property_create_range(drm,
			0, "PQPARAM", 0, 0xffffffffffffffff);
	if (priv->mtk_prop_pqparam == NULL)
		return -ENOMEM;

	priv->mtk_prop_pqindex = drm_property_create_range(drm,
			0, "PQINDEX", 0, 0xffffffffffffffff);
	if (priv->mtk_prop_pqindex == NULL)
		return -ENOMEM;

	priv->mtk_prop_color_bypass = drm_property_create_range(drm,
			0, "COLOR_BYPASS", 0, 0xffffffffffffffff);
	if (priv->mtk_prop_color_bypass == NULL)
		return -ENOMEM;

	priv->mtk_prop_color_window = drm_property_create_range(drm,
			0, "COLOR_WINDOW", 0, 0xffffffffffffffff);
	if (priv->mtk_prop_color_window == NULL)
		return -ENOMEM;

	priv->mtk_prop_get_pqparam = drm_property_create_range(drm,
			0, "GET_PQPARAM", 0, 0xffffffffffffffff);
	if (priv->mtk_prop_get_pqparam == NULL)
		return -ENOMEM;

	return 0;
}

static void mtk_attach_color_property(struct drm_mode_object *obj,
		struct drm_device *drm)
{
	struct mtk_drm_private *priv = drm->dev_private;

	drm_object_attach_property(obj, priv->mtk_prop_pqparam, 0);
	drm_object_attach_property(obj, priv->mtk_prop_pqindex, 0);
	drm_object_attach_property(obj, priv->mtk_prop_color_bypass, 0);
	drm_object_attach_property(obj, priv->mtk_prop_color_window, 0);
	drm_object_attach_property(obj, priv->mtk_prop_get_pqparam, 0);
}

static int mtk_set_property(struct drm_crtc *crtc,
		struct drm_crtc_state *state,
		struct drm_property *property,
		uint64_t val)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	enum mtk_color_property property_enum;

	if (strcmp(property->name, priv->mtk_prop_pqparam->name) == 0)
		property_enum = COLOR_PROPERTY_PQPARAM;
	else if (strcmp(property->name, priv->mtk_prop_pqindex->name) == 0)
		property_enum = COLOR_PROPERTY_PQINDEX;
	else if (strcmp(property->name, priv->mtk_prop_color_bypass->name) == 0)
		property_enum = COLOR_PROPERTY_COLOR_BYPASS;
	else if (strcmp(property->name, priv->mtk_prop_color_window->name) == 0)
		property_enum = COLOR_PROPERTY_COLOR_WINDOW;
	else if (strcmp(property->name, priv->mtk_prop_get_pqparam->name) == 0)
		property_enum = COLOR_PROPERTY_GET_PQPARAM;
	else {
		DRM_ERROR("%s : Unknown property '%s'\n",
				__func__, property->name);
		return -EINVAL;
	}

	mtk_color_set_property_handler(priv, property_enum, val);

	return 0;
}
#endif

static int mtk_get_property(struct drm_crtc *crtc,
		const struct drm_crtc_state *state,
		struct drm_property *property,
		uint64_t *val)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (property == priv->plane_using_state)
		*val = mtk_drm_crtc_plane_using_state(crtc);
#ifdef CONFIG_MTK_DISPLAY_COLOR
	else if (strcmp(property->name, priv->mtk_prop_pqparam->name) == 0)
		return 0;
	else if (strcmp(property->name, priv->mtk_prop_pqindex->name) == 0)
		return 0;
	else if (strcmp(property->name, priv->mtk_prop_color_bypass->name) == 0)
		return 0;
	else if (strcmp(property->name, priv->mtk_prop_color_window->name) == 0)
		return 0;
	else if (strcmp(property->name, priv->mtk_prop_get_pqparam->name) == 0)
		return 0;
#endif
	else
		return -EINVAL;

	return 0;
}

static const struct drm_crtc_funcs mtk_crtc_funcs = {
	.set_config		= drm_atomic_helper_set_config,
	.page_flip		= drm_atomic_helper_page_flip,
	.destroy		= mtk_drm_crtc_destroy,
	.reset			= mtk_drm_crtc_reset,
	.atomic_duplicate_state	= mtk_drm_crtc_duplicate_state,
	.atomic_destroy_state	= mtk_drm_crtc_destroy_state,
	.gamma_set		= drm_atomic_helper_legacy_gamma_set,
	.enable_vblank		= mtk_drm_crtc_enable_vblank,
	.disable_vblank		= mtk_drm_crtc_disable_vblank,
	#ifdef CONFIG_MTK_DISPLAY_COLOR
	.atomic_set_property	= mtk_set_property,
	#endif
	.atomic_get_property	= mtk_get_property,
};

static const struct drm_crtc_helper_funcs mtk_crtc_helper_funcs = {
	.mode_fixup	= mtk_drm_crtc_mode_fixup,
	.mode_set_nofb	= mtk_drm_crtc_mode_set_nofb,
	.atomic_begin	= mtk_drm_crtc_atomic_begin,
	.atomic_flush	= mtk_drm_crtc_atomic_flush,
	.atomic_enable	= mtk_drm_crtc_atomic_enable,
	.atomic_disable	= mtk_drm_crtc_atomic_disable,
};

static int mtk_drm_crtc_init(struct drm_device *drm,
			     struct mtk_drm_crtc *mtk_crtc,
			     unsigned int pipe)
{
	struct mtk_drm_private *priv = drm->dev_private;
	struct drm_plane *primary = NULL;
	struct drm_plane *cursor = NULL;
	int i, ret;

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		if (mtk_crtc->planes[i].type == DRM_PLANE_TYPE_PRIMARY)
			primary = &mtk_crtc->planes[i];
		else if (mtk_crtc->planes[i].type == DRM_PLANE_TYPE_CURSOR)
			cursor = &mtk_crtc->planes[i];
	}

	ret = drm_crtc_init_with_planes(drm, &mtk_crtc->base, primary, cursor,
					&mtk_crtc_funcs, NULL);
	if (ret) {
		printk(KERN_DEBUG "drm_crtc_init_with_planes failed\n");
		goto err_cleanup_crtc;
	}

	drm_crtc_helper_add(&mtk_crtc->base, &mtk_crtc_helper_funcs);

	#ifdef CONFIG_MTK_DISPLAY_COLOR
	ret = mtk_create_color_property(drm);
	if (ret) {
		DRM_ERROR("failed to create properties\n");

		goto err_cleanup_crtc;
	}
	mtk_attach_color_property(&mtk_crtc->base.base, drm);
	#endif

	drm_object_attach_property(&mtk_crtc->base.base,
		priv->plane_using_state, 0);

	return 0;

err_cleanup_crtc:

	printk(KERN_DEBUG "mtk_drm_crtc_init failed\n");
	drm_crtc_cleanup(&mtk_crtc->base);
	return ret;
}

void mtk_crtc_ddp_irq(struct drm_crtc *crtc, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
#ifdef CONFIG_MTK_DISPLAY_CMDQ
#else

	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (!priv->data->shadow_register)
		mtk_crtc_ddp_config(crtc);
#endif

	// printk(KERN_DEBUG "mtk_crtc_ddp_irq\n");

	(void)drm_crtc_handle_vblank(&mtk_crtc->base);
#ifdef CONFIG_MTK_DISPLAY_CMDQ
#else
	mtk_drm_finish_page_flip(mtk_crtc);
#endif
}

int mtk_drm_crtc_create(struct drm_device *drm_dev,
			const enum mtk_ddp_comp_id *path, unsigned int path_len)
{
	struct mtk_drm_private *priv = drm_dev->dev_private;
	struct device *dev = drm_dev->dev;
	struct mtk_drm_crtc *mtk_crtc;
	enum drm_plane_type type;
	unsigned int zpos;
	int pipe = priv->num_pipes;
	int ret;
	int i;

	if (!path)
		return 0;

	for (i = 0; i < path_len; i++) {
		enum mtk_ddp_comp_id comp_id = path[i];
		struct device_node *node;

		node = priv->comp_node[comp_id];
		if (!node) {
			if (comp_id == DDP_COMPONENT_PWM0 ||
			    comp_id == DDP_COMPONENT_PWM1 ||
			    comp_id == DDP_COMPONENT_PWM2) {
				path_len--;
				break;
			}

			dev_info(dev,
				 "Not creating crtc %d because component %d is disabled or missing\n",
				 pipe, comp_id);
			return 0;
		}
	}

	mtk_crtc = devm_kzalloc(dev, sizeof(*mtk_crtc), GFP_KERNEL);
	if (!mtk_crtc)
		return -ENOMEM;

	mtk_crtc->config_regs = priv->config_regs;
	mtk_crtc->ddp_comp_nr = path_len;
	mtk_crtc->ddp_comp = devm_kmalloc_array(dev, mtk_crtc->ddp_comp_nr,
						sizeof(*mtk_crtc->ddp_comp),
						GFP_KERNEL);
	if (!mtk_crtc->ddp_comp)
		return -ENOMEM;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	mtk_crtc->user_id = USER_INVALID;
#endif

	mtk_crtc->mutex = mtk_disp_mutex_get(priv->mutex_dev, pipe);
	if (IS_ERR(mtk_crtc->mutex)) {
		ret = PTR_ERR(mtk_crtc->mutex);
		dev_err(dev, "Failed to get mutex: %d\n", ret);
		return ret;
	}
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	if (priv->num_pipes == 0U) {
		printk(KERN_DEBUG "cmdq_event: CMDQ_EVENT_DISP_STREAM_DONE_ENG_EVENT_0\n");
		mtk_crtc->cmdq_client = cmdq_mbox_create(dev, 0, 2000);
		// mtk_crtc->cmdq_event = CMDQ_EVENT_MUTEX0_STREAM_EOF;
		mtk_crtc->cmdq_event = CMDQ_EVENT_DISP_STREAM_DONE_ENG_EVENT_0;   // mt6890
	} else if (priv->num_pipes == 1U) {
		printk(KERN_DEBUG "cmdq_event: CMDQ_EVENT_DISP_STREAM_DONE_ENG_EVENT_1\n");
		mtk_crtc->cmdq_client = cmdq_mbox_create(dev, 1, 2000);
		mtk_crtc->cmdq_event = CMDQ_EVENT_DISP_STREAM_DONE_ENG_EVENT_1;    // mt6890
		// mtk_crtc->cmdq_event = CMDQ_EVENT_MUTEX1_STREAM_EOF;

	} else {
		printk(KERN_DEBUG "cmdq_event: CMDQ_EVENT_DISP_STREAM_DONE_ENG_EVENT_2\n");
		mtk_crtc->cmdq_client = cmdq_mbox_create(dev, 2, 2000);
		mtk_crtc->cmdq_event = CMDQ_EVENT_DISP_STREAM_DONE_ENG_EVENT_2;    // mt6890
		// mtk_crtc->cmdq_event = CMDQ_EVENT_MUTEX2_STREAM_EOF;
	}

	DRM_INFO("mtk cmdq enabled\n");
#else
	DRM_INFO("mtk cmdq disabled\n");
#endif

	for (i = 0; i < mtk_crtc->ddp_comp_nr; i++) {
		enum mtk_ddp_comp_id comp_id = path[i];
		struct mtk_ddp_comp *comp;
		struct device_node *node;

		node = priv->comp_node[comp_id];
		comp = priv->ddp_comp[comp_id];
		if (!comp) {
			dev_err(dev, "Component %pOF not initialized\n", node);
			return -ENODEV;
		}
#ifdef CONFIG_MTK_DISPLAY_CMDQ
		if (comp->dev)
			comp->cmdq_base = cmdq_register_device(comp->dev);
#endif
		if (comp_id == DDP_COMPONENT_WDMA0 ||
		    comp_id == DDP_COMPONENT_WDMA1) {
			ret = mtk_wb_connector_init(drm_dev, mtk_crtc);
			if (ret != 0)
				return ret;

			ret = mtk_wb_set_possible_crtcs(drm_dev,
				mtk_crtc, comp);
			if (ret != 0)
				return ret;
		}

		mtk_crtc->ddp_comp[i] = comp;
	}

	mtk_crtc->layer_nr = mtk_ddp_comp_layer_nr(mtk_crtc->ddp_comp[0]);
	mtk_crtc->planes = devm_kcalloc(dev, mtk_crtc->layer_nr,
					sizeof(struct drm_plane),
					GFP_KERNEL);

	for (zpos = 0; zpos < mtk_crtc->layer_nr; zpos++) {
		type = (zpos == 0) ? DRM_PLANE_TYPE_PRIMARY :
						DRM_PLANE_TYPE_OVERLAY;
		ret = mtk_plane_init(drm_dev, &mtk_crtc->planes[zpos],
				     BIT(pipe), type);
		if (ret)
			return ret;
	}

	ret = mtk_drm_crtc_init(drm_dev, mtk_crtc, pipe);
	if (ret < 0)
		return ret;
	drm_mode_crtc_set_gamma_size(&mtk_crtc->base, MTK_LUT_SIZE);
	drm_crtc_enable_color_mgmt(&mtk_crtc->base, 0, false, MTK_LUT_SIZE);
	priv->num_pipes++;

	return 0;
}
