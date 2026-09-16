/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: YT SHEN <yt.shen@mediatek.com>
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
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_of.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_fbdev.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_debugfs.h"
#include "mtk_drm_plane.h"

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include "ipi/scp_ipi.h"
#include <linux/regmap.h>
#endif

#define DRIVER_NAME "mediatek"
#define DRIVER_DESC "Mediatek SoC DRM"
#define DRIVER_DATE "20150513"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0

#define SMI_LARB0_MMU_EN       0x380
#define MMU_EN                 BIT(0)

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
struct mtk_drm_private *g_private;
bool mtk_drm_suspend;
#endif

struct mtk_commit {
	struct drm_atomic_state *state;
	struct work_struct work;
};

static void mtk_smi_larb(struct device *dev, u32 offset, u32 mask, u32 data);
static void mtk_atomic_work(struct work_struct *work);
static void mtk_atomic_schedule(struct mtk_commit *commit)
{
	schedule_work(&commit->work);
}

static struct mtk_commit *mtk_commit_init(struct drm_atomic_state *state)
{
	struct mtk_commit *c = kzalloc(sizeof(*c), GFP_KERNEL);

	if (!c)
		return NULL;

	c->state = state;

	INIT_WORK(&c->work, mtk_atomic_work);

	return c;
}

static void mtk_commit_destroy(struct mtk_commit *c)
{
	kfree(c);
}

static void mtk_atomic_wait_for_fences(struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	int i;

	for_each_new_plane_in_state(state, plane, new_plane_state, i)
		mtk_fb_wait(new_plane_state->fb);
}

static int mtk_atomic_get_state_crtcs(struct drm_atomic_state *state)
{
	struct drm_crtc_state *old_crtc_state;
	struct drm_crtc *crtc;
	unsigned int i = 0;
	unsigned int crtc_mask = 0;
	/*
	 * Figure out what crtcs we have:
	 */
	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i)
		crtc_mask |= drm_crtc_mask(crtc);

	return crtc_mask;
}

static int mtk_atomic_get_crtcs(struct mtk_drm_private *priv,
				struct drm_atomic_state *state)
{
	int ret;
	unsigned int crtc_mask = mtk_atomic_get_state_crtcs(state);

	/* wait the previous pending crtc work done */
	spin_lock(&priv->pending_crtcs_event.lock);
	ret = wait_event_interruptible_locked(priv->pending_crtcs_event,
			!(priv->pending_crtcs & crtc_mask));
	if (ret == 0) {
		DRM_DEBUG_ATOMIC("start: %08x\n", crtc_mask);
		priv->pending_crtcs |= crtc_mask;
	}
	spin_unlock(&priv->pending_crtcs_event.lock);

	return ret;
}

static void mtk_atomic_put_crtcs(struct mtk_drm_private *priv,
				struct drm_atomic_state *state)
{
	unsigned int crtc_mask = mtk_atomic_get_state_crtcs(state);

	spin_lock(&priv->pending_crtcs_event.lock);
	DRM_DEBUG_ATOMIC("end: %08x\n", crtc_mask);
	priv->pending_crtcs &= ~crtc_mask;
	wake_up_all_locked(&priv->pending_crtcs_event);
	spin_unlock(&priv->pending_crtcs_event.lock);
}

static void mtk_atomic_complete(struct drm_device *drm,
				struct drm_atomic_state *state)
{
	struct mtk_drm_private *private = drm->dev_private;

	mtk_atomic_wait_for_fences(state);

	/*
	 * Mediatek drm supports runtime PM, so plane registers cannot be
	 * written when their crtc is disabled.
	 *
	 * The comment for drm_atomic_helper_commit states:
	 *     For drivers supporting runtime PM the recommended sequence is
	 *
	 *     drm_atomic_helper_commit_modeset_disables(dev, state);
	 *     drm_atomic_helper_commit_modeset_enables(dev, state);
	 *     drm_atomic_helper_commit_planes(dev, state,
	 *                                     DRM_PLANE_COMMIT_ACTIVE_ONLY);
	 *
	 * See the kerneldoc entries for these three functions for more details.
	 */
	drm_atomic_helper_commit_modeset_disables(drm, state);
	drm_atomic_helper_commit_modeset_enables(drm, state);
	drm_atomic_helper_commit_planes(drm, state,
					DRM_PLANE_COMMIT_ACTIVE_ONLY);

	drm_atomic_helper_wait_for_vblanks(drm, state);

	mtk_atomic_put_crtcs(private, state);
	drm_atomic_helper_cleanup_planes(drm, state);
	drm_atomic_state_put(state);
}

static void mtk_atomic_work(struct work_struct *work)
{
	struct mtk_commit *commit = container_of(work, struct mtk_commit, work);
	struct drm_atomic_state *state = commit->state;
	struct drm_device *dev = state->dev;

	mtk_atomic_complete(dev, state);
	mtk_commit_destroy(commit);
}

static int mtk_atomic_commit(struct drm_device *drm,
			     struct drm_atomic_state *state,
			     bool async)
{
	struct mtk_drm_private *private = drm->dev_private;
	struct mtk_commit *commit;
	int ret;

	ret = drm_atomic_helper_prepare_planes(drm, state);
	if (ret)
		return ret;

	commit = mtk_commit_init(state);
	if (!commit) {
		DRM_ERROR("commit init failed\n");
		ret = -ENOMEM;
		goto clean;
	}

	ret = mtk_atomic_get_crtcs(private, state);
	if (ret) {
		DRM_ERROR("%s error\n", __func__);
		mtk_commit_destroy(commit);
		goto clean;
	}

	mutex_lock(&private->commit.lock);
	ret = drm_atomic_helper_swap_state(state, true);
	if (ret) {
		mutex_unlock(&private->commit.lock);
		drm_atomic_helper_cleanup_planes(drm, state);
		return ret;
	}

	drm_atomic_state_get(state);
	if (async)
		mtk_atomic_schedule(commit);
	else
		mtk_atomic_complete(drm, state);
	mutex_unlock(&private->commit.lock);
	return 0;
clean:
	drm_atomic_helper_cleanup_planes(drm, state);
	return ret;
}

static void mtk_drm_mode_output_poll_changed(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *fb_helper = &priv->fb_helper;

	if (fb_helper)
		drm_fb_helper_hotplug_event(fb_helper);
}

static const struct drm_mode_config_funcs mtk_drm_mode_config_funcs = {
	.fb_create = mtk_drm_mode_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = mtk_atomic_commit,
	.output_poll_changed = mtk_drm_mode_output_poll_changed,
};

static enum mtk_ddp_comp_id mt6890_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0,
#ifdef CONFIG_DRM_DSI_SUPPORT
	DDP_COMPONENT_DSI0,
#else
	DDP_COMPONENT_DBI0,
#endif
    DDP_COMPONENT_PWM0,
};

static enum mtk_ddp_comp_id mt6890_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0,
	// DDP_COMPONENT_PWM0,
	DDP_COMPONENT_DSI0,
	// DDP_COMPONENT_OVL0,
	// DDP_COMPONENT_RDMA0,
	// DDP_COMPONENT_COLOR0,
	// DDP_COMPONENT_CCORR0,
	// DDP_COMPONENT_AAL0,
	// DDP_COMPONENT_GAMMA0,
	// DDP_COMPONENT_DITHER0,
	// DDP_COMPONENT_PWM0,
	// DDP_COMPONENT_DSI0,
};

static enum mtk_ddp_comp_id mt2701_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_DSI0,
};

static enum mtk_ddp_comp_id mt2701_mtk_ddp_ext[] = {
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI0,
};

static enum mtk_ddp_comp_id mt2712_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_LVDS0,
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	DDP_COMPONENT_WDMA0,
#endif
	DDP_COMPONENT_PWM0,
};

static enum mtk_ddp_comp_id mt2712_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_AAL1,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_LVDS1,
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#else
	DDP_COMPONENT_WDMA1,
#endif
	DDP_COMPONENT_PWM1,
};

static enum mtk_ddp_comp_id mt2712_mtk_ddp_third[] = {
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_DSI3,
	DDP_COMPONENT_PWM2,
};

static enum mtk_ddp_comp_id mt8173_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_OD0,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static enum mtk_ddp_comp_id mt8173_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI0,
};

static const struct mtk_mmsys_driver_data mt6890_mmsys_driver_data = {
	.main_path = mt6890_mtk_ddp_main,
	.main_len = ARRAY_SIZE(mt6890_mtk_ddp_main),
	.ext_path = mt6890_mtk_ddp_ext,
	.ext_len = ARRAY_SIZE(mt6890_mtk_ddp_ext),
	.shadow_register = true,
	//.shadow_register = false,   // for mt6890
};

static const struct mtk_mmsys_driver_data mt2701_mmsys_driver_data = {
	.main_path = mt2701_mtk_ddp_main,
	.main_len = ARRAY_SIZE(mt2701_mtk_ddp_main),
	.ext_path = mt2701_mtk_ddp_ext,
	.ext_len = ARRAY_SIZE(mt2701_mtk_ddp_ext),
	.shadow_register = true,
};

static const struct mtk_mmsys_driver_data mt2712_mmsys_driver_data = {
	.main_path = mt2712_mtk_ddp_main,
	.main_len = ARRAY_SIZE(mt2712_mtk_ddp_main),
	.ext_path = mt2712_mtk_ddp_ext,
	.ext_len = ARRAY_SIZE(mt2712_mtk_ddp_ext),
	.third_path = mt2712_mtk_ddp_third,
	.third_len = ARRAY_SIZE(mt2712_mtk_ddp_third),
};

static const struct mtk_mmsys_driver_data mt8173_mmsys_driver_data = {
	.main_path = mt8173_mtk_ddp_main,
	.main_len = ARRAY_SIZE(mt8173_mtk_ddp_main),
	.ext_path = mt8173_mtk_ddp_ext,
	.ext_len = ARRAY_SIZE(mt8173_mtk_ddp_ext),
};

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
int mtk_drm_ipi_set_property(unsigned int type, uint64_t val)
{
	struct queue_msg_t msg;
	struct mtk_drm_ipc_prop_data prop_data;

	prop_data.type = (uint8_t)type;
	prop_data.val = (uint32_t)val;

	msg.msg_id = MTK_DISP_IPC_SET_PROP;
	msg.magic = IPI_MSG_MAGIC_NUMBER;

	memcpy(msg.data, &(prop_data), sizeof(prop_data));

	if (scp_uid_queue_send(&msg) != IPI_UID_RET_OK) {
		DRM_WARN("MTK_DISP_IPC_SET_PROP send err\n");
		return -1;
	}

	DRM_DEBUG_ATOMIC("MTK_DISP_IPC_SET_PROP send ok\n");

	return 0;
}

int mtk_drm_ipi_get_property(unsigned int type)
{
	struct queue_msg_t msg;
	struct mtk_drm_ipc_prop_data prop_data;

	prop_data.type = (uint8_t)type;
	prop_data.val = 0;

	msg.msg_id = MTK_DISP_IPC_GET_PROP;
	msg.magic = IPI_MSG_MAGIC_NUMBER;

	memcpy(msg.data, &(prop_data), sizeof(prop_data));

	if (scp_uid_queue_send(&msg) != IPI_UID_RET_OK) {
		DRM_WARN("MTK_DISP_IPC_GET_PROP send err\n");
		return -1;
	}

	DRM_DEBUG_ATOMIC("MTK_DISP_IPC_GET_PROP send ok, type=%d\n", type);

	return 0;

}
void mtk_drm_ipi_get_property_rsp(void *private, char *data)
{
	struct mtk_drm_private *priv = (struct mtk_drm_private *)private;
	struct mtk_drm_ipc_prop_data *rsp =
		(struct mtk_drm_ipc_prop_data *)data;

	DRM_DEBUG_ATOMIC("MTK_DISP_IPC_GET_PROP_RSP type=%d, value=%d\n",
		rsp->type, rsp->val);

	priv->get_prop_data.type = 1 << rsp->type;
	priv->get_prop_data.val = rsp->val;
	wake_up_all(&priv->pending_get_prop);
}
#endif

static int mtk_drm_create_properties(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	static const struct drm_prop_enum_list props[] = {
		{ __builtin_ffs(MTK_PLANE_0) - 1, "mtk-plane-0" },
		{ __builtin_ffs(MTK_PLANE_1) - 1, "mtk-plane-1" },
		{ __builtin_ffs(MTK_PLANE_2) - 1, "mtk-plane-2" },
		{ __builtin_ffs(MTK_PLANE_3) - 1, "mtk-plane-3" },
	};

	priv->alpha = drm_property_create_range(dev, 0, "alpha", 0, 255);
	if (priv->alpha == NULL)
		return -ENOMEM;

	priv->color_matrix = drm_property_create_range(dev,
				DRM_MODE_PROP_ATOMIC, "color_matrix", 0, 3);
	if (priv->color_matrix == NULL)
		return -ENOMEM;

	priv->surfl_en = drm_property_create_range(dev,
				DRM_MODE_PROP_ATOMIC,
				"sf_alpha_blending_en", 0, 1);
	if (priv->surfl_en == NULL)
		return -ENOMEM;

	priv->d_alpha = drm_property_create_range(dev,
				DRM_MODE_PROP_ATOMIC, "dest_alpha", 0, 3);
	if (priv->d_alpha == NULL)
		return -ENOMEM;

	priv->s_alpha = drm_property_create_range(dev,
				DRM_MODE_PROP_ATOMIC, "src_alpha", 0, 3);
	if (priv->s_alpha == NULL)
		return -ENOMEM;

	priv->plane_reserved_user = drm_property_create_range(dev,
				DRM_MODE_PROP_ATOMIC, "plane_user",
				0, UINT_MAX);
	if (priv->plane_reserved_user == NULL)
		return -ENOMEM;

	priv->plane_using_state = drm_property_create_bitmask(dev,
				DRM_MODE_PROP_ATOMIC, "plane_using", props,
				ARRAY_SIZE(props), MTK_PLANE_MASK);
	if (priv->plane_using_state == NULL)
		return -ENOMEM;

	return 0;
}

static int mtk_drm_kms_init(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;
	struct platform_device *pdev;
	struct device_node *np;
	struct device *dma_dev;
	int ret;
	// struct drm_crtc *crtc;  // test

	dev_err(drm->dev, "mtk_drm_kms_init");

	if (!iommu_present(&platform_bus_type)) {
		dev_err(drm->dev, "iommu_present failed");
		return -EPROBE_DEFER;
	}

	pdev = of_find_device_by_node(private->mutex_node);
	if (!pdev) {
		dev_err(drm->dev, "Waiting for disp-mutex device %pOF\n",
			private->mutex_node);
		of_node_put(private->mutex_node);
		return -EPROBE_DEFER;
	}
	private->mutex_dev = &pdev->dev;

	drm_mode_config_init(drm);

	drm->mode_config.min_width = 64;
	drm->mode_config.min_height = 64;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	drm->mode_config.max_width = 5760;
	drm->mode_config.max_height = 5760;
	drm->mode_config.funcs = &mtk_drm_mode_config_funcs;

	ret = mtk_drm_create_properties(drm);
	if (ret) {

		DRM_ERROR("failed to create properties\n");
		goto err_config_cleanup;
	}

	ret = component_bind_all(drm->dev, drm);
	if (ret) {
		dev_err(drm->dev, "failed component_bind_all\n");
		goto err_config_cleanup;
	}

	/*
	 * We currently support two fixed data streams, each optional,
	 * and each statically assigned to a crtc:
	 * OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> DSI0 ...
	 */
	ret = mtk_drm_crtc_create(drm, private->data->main_path,
				  private->data->main_len);
	if (ret < 0) {
		dev_err(drm->dev, "failed mtk_drm_crtc_create main\n");
		goto err_component_unbind;
	}

	// mt6890 temp remove.
	/* ... and OVL1 -> COLOR1 -> GAMMA -> RDMA1 -> DPI0. */
	// ret = mtk_drm_crtc_create(drm, private->data->ext_path,
	// 			  private->data->ext_len);
	// if (ret < 0) {
	// 	dev_err(drm->dev, "failed mtk_drm_crtc_create ext_path\n");
	// 	goto err_component_unbind;
	// }

	// mt6890 temp remove.
	// ret = mtk_drm_crtc_create(drm, private->data->third_path,
	// 			  private->data->third_len);
	// if (ret < 0)
	// 	goto err_component_unbind;

	/* Use OVL device for all DMA memory allocations */
	np = private->comp_node[private->data->main_path[0]] ?:
	     private->comp_node[private->data->ext_path[0]];
	pdev = of_find_device_by_node(np);
	if (!pdev) {
		ret = -ENODEV;
		dev_err(drm->dev, "Need at least one OVL device\n");
		goto err_component_unbind;
	}

	dma_dev = &pdev->dev;
	private->dma_dev = dma_dev;

	/*
	 * Configure the DMA segment size to make sure we get contiguous IOVA
	 * when importing PRIME buffers.
	 */
	if (!dma_dev->dma_parms) {
		private->dma_parms_allocated = true;
		dma_dev->dma_parms =
			devm_kzalloc(drm->dev, sizeof(*dma_dev->dma_parms),
				     GFP_KERNEL);
	}
	if (!dma_dev->dma_parms) {
		ret = -ENOMEM;
		dev_err(drm->dev, "failed devm_kzalloc\n");
		goto err_component_unbind;
	}

	ret = dma_set_max_seg_size(dma_dev, (unsigned int)DMA_BIT_MASK(32));
	if (ret) {
		dev_err(drm->dev, "Failed to set DMA segment size\n");
		goto err_unset_dma_parms;
	}

	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	drm->irq_enabled = true;
	ret = drm_vblank_init(drm, MAX_CRTC);
	if (ret < 0) {
		dev_err(drm->dev, "failed drm_vblank_init\n");
		goto err_unset_dma_parms;
	}

	drm_kms_helper_poll_init(drm);
	drm_mode_config_reset(drm);

	mtk_drm_debugfs_init(drm, private);

	ret = mtk_fbdev_init(drm);
	if (ret) {
		goto err_kms_helper_poll_fini;
    }

	// test enable module.
  	// drm_for_each_crtc(crtc, drm) {
  	// 	mtk_drm_crtc_atomic_enable_test(crtc, NULL);
  	// }

	dev_info(drm->dev, "mtk_drm_kms_init end\n");
	return 0;

err_kms_helper_poll_fini:
	drm_kms_helper_poll_fini(drm);
err_unset_dma_parms:
	if (private->dma_parms_allocated)
		dma_dev->dma_parms = NULL;
err_component_unbind:
	component_unbind_all(drm->dev, drm);
err_config_cleanup:
	drm_mode_config_cleanup(drm);

	return ret;
}

static void mtk_drm_kms_deinit(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;

	mtk_fbdev_fini(drm);
	drm_kms_helper_poll_fini(drm);
	drm_atomic_helper_shutdown(drm);

	if (private->dma_parms_allocated)
		private->dma_dev->dma_parms = NULL;

	component_unbind_all(drm->dev, drm);
	drm_mode_config_cleanup(drm);
}

static const struct file_operations mtk_drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = mtk_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.compat_ioctl = drm_compat_ioctl,
};

/*
 * We need to override this because the device used to import the memory is
 * not dev->dev, as drm_gem_prime_import() expects.
 */
struct drm_gem_object *mtk_drm_gem_prime_import(struct drm_device *dev,
						struct dma_buf *dma_buf)
{
	struct mtk_drm_private *private = dev->dev_private;

	return drm_gem_prime_import_dev(dev, dma_buf, private->dma_dev);
}

static struct drm_driver mtk_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME |
			   DRIVER_ATOMIC,

	.gem_free_object_unlocked = mtk_drm_gem_free_object,
	.gem_vm_ops = &drm_gem_cma_vm_ops,
	.dumb_create = mtk_drm_gem_dumb_create,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = drm_gem_prime_export,
	.gem_prime_import = mtk_drm_gem_prime_import,
	.gem_prime_get_sg_table = mtk_gem_prime_get_sg_table,
	.gem_prime_import_sg_table = mtk_gem_prime_import_sg_table,
	.gem_prime_mmap = mtk_drm_gem_mmap_buf,
	.fops = &mtk_drm_fops,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int mtk_drm_bind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm;
	int ret;

	dev_info(dev, "mtk_drm_bind\n");

	drm = drm_dev_alloc(&mtk_drm_driver, dev);
	if (IS_ERR(drm)) {
		dev_info(dev, "drm_dev_alloc failed\n");
		return PTR_ERR(drm);
	}

	drm->dev_private = private;
	private->drm = drm;

	ret = mtk_drm_kms_init(drm);
	if (ret < 0) {
		dev_info(dev, "mtk_drm_kms_init failed\n");
		goto err_free;
	}

	ret = drm_dev_register(drm, 0);
	if (ret < 0) {
		dev_info(dev, "drm_dev_register failed\n");
		goto err_deinit;
	}

	mtk_smi_larb(dev, SMI_LARB0_MMU_EN, MMU_EN, 1);

	return 0;

err_deinit:
	dev_info(dev, "err_deinit\n");
	mtk_drm_kms_deinit(drm);
err_free:
	dev_info(dev, "err_free, ret: %d\n", ret);
	drm_dev_put(drm);
	return ret;
}

static void mtk_drm_unbind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	drm_dev_unregister(private->drm);
	mtk_drm_kms_deinit(private->drm);
	drm_dev_put(private->drm);
	private->num_pipes = 0;
	private->drm = NULL;
}

static const struct component_master_ops mtk_drm_ops = {
	.bind		= mtk_drm_bind,
	.unbind		= mtk_drm_unbind,
};

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static const struct regmap_config mtk_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int mtk_drm_power_mode_init(struct mtk_drm_private *private,
	struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *toprgu_np;
	void __iomem *base;
	struct mtk_drm_power_mode *power_mode;

	power_mode = devm_kzalloc(dev, sizeof(*power_mode), GFP_KERNEL);
	if (!power_mode)
		return -ENOMEM;

	power_mode->mask = BIT(21) | BIT(22);

	toprgu_np = of_parse_phandle(np, "rcu-regmap", 0);

	if (!of_device_is_compatible(toprgu_np, "mediatek,toprgu"))
		return -EINVAL;

	base = of_iomap(toprgu_np, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	power_mode->map = devm_regmap_init_mmio(dev, base, &mtk_regmap_config);
	if (IS_ERR(power_mode->map))
		return PTR_ERR(power_mode->map);

	if (of_property_read_u32(np, "rcu-offset", &power_mode->offset))
		return -EINVAL;

	of_property_read_u32(np, "rcu-mask", &power_mode->mask);
	DRM_DEBUG_DRIVER("rcu-offset=0x%x, rcu-mask=0x%x\n",
		power_mode->offset, power_mode->mask);

	private->power_mode = power_mode;

	return 0;
}
#endif

static const struct of_device_id mtk_ddp_comp_dt_ids[] = {
	{ .compatible = "mediatek,mt2701-disp-ovl",
	  .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,mt8173-disp-ovl",
	  .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,mt2701-disp-rdma",
	  .data = (void *)MTK_DISP_RDMA },
	{ .compatible = "mediatek,mt8173-disp-rdma",
	  .data = (void *)MTK_DISP_RDMA },
	{ .compatible = "mediatek,mt8173-disp-wdma",
	  .data = (void *)MTK_DISP_WDMA },
	{ .compatible = "mediatek,mt2701-disp-color",
	  .data = (void *)MTK_DISP_COLOR },
	{ .compatible = "mediatek,mt8173-disp-color",
	  .data = (void *)MTK_DISP_COLOR },
	{ .compatible = "mediatek,mt8173-disp-aal",
	  .data = (void *)MTK_DISP_AAL},
	{ .compatible = "mediatek,mt8173-disp-gamma",
	  .data = (void *)MTK_DISP_GAMMA, },
	{ .compatible = "mediatek,mt8173-disp-ufoe",
	  .data = (void *)MTK_DISP_UFOE },
	{ .compatible = "mediatek,mt2701-dsi",
	  .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt8173-dsi",
	  .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt2701-dpi",
	  .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt2712-dpi",
	  .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt8173-dpi",
	  .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt8173-lvds",
	  .data = (void *)MTK_LVDS },
	{ .compatible = "mediatek,mt2701-disp-mutex",
	  .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt2712-disp-mutex",
	  .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt8173-disp-mutex",
	  .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt2701-disp-pwm",
	  .data = (void *)MTK_DISP_BLS },
	{ .compatible = "mediatek,mt8173-disp-pwm",
	  .data = (void *)MTK_DISP_PWM },
	{ .compatible = "mediatek,mt8173-disp-od",
	  .data = (void *)MTK_DISP_OD },

	// MT6890, TBD
	{ .compatible = "mediatek,disp_ovl0",
	  .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,disp_rdma0",
	  .data = (void *)MTK_DISP_RDMA },
	// { .compatible = "mediatek,disp_wdma0",
	//   .data = (void *)MTK_DISP_WDMA },
	// { .compatible = "mediatek,disp_color0",
	//   .data = (void *)MTK_DISP_COLOR },
	// { .compatible = "mediatek,disp_aal0",
	//   .data = (void *)MTK_DISP_AAL},
	// { .compatible = "mediatek,disp_gamma0",
	//   .data = (void *)MTK_DISP_GAMMA, },
	{ .compatible = "mediatek,dsi0",
	  .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt6890-dbi0",
	  .data = (void *)MTK_DBI },
	// { .compatible = "mediatek,dbpi0",
	//   .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,disp_mutex0",
	  .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,disp_pwm0",
	  .data = (void *)MTK_DISP_PWM },
	{ }
};


struct component_match {
	size_t alloc;
	size_t num;
	struct component_match_array *compare;
};

static int mtk_drm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_drm_private *private;
	struct resource *mem;
	struct device_node *node;
	struct component_match *match = NULL;
	int ret;
	int i;

	drm_debug = 0xFF; /* DRIVER messages */

	// pm_runtime_enable(&pdev->dev);  // test
	// pm_runtime_get_sync(&pdev->dev);  // test

	private = devm_kzalloc(dev, sizeof(*private), GFP_KERNEL);
	if (!private)
		return -ENOMEM;

	mutex_init(&private->commit.lock);
	INIT_WORK(&private->commit.work, mtk_atomic_work);

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	init_waitqueue_head(&private->pending_get_prop);
	scp_uid_queue_register(MTK_DISP_IPC_GET_PROP_RSP,
		mtk_drm_ipi_get_property_rsp,
		(void *)private);

#endif
	init_waitqueue_head(&private->pending_crtcs_event);
	private->data = of_device_get_match_data(dev);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	private->config_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->config_regs)) {
		ret = PTR_ERR(private->config_regs);
		dev_err(dev, "Failed to ioremap mmsys-config resource: %d\n",
			ret);
		return ret;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6880-smi-larb");

	if (!node) {
		pr_err("error: can't find smi_larb0 node.\n");
		WARN_ON(1);
		return 0;
	}

	private->smi_larb_base = of_iomap(node, 0);
	if (!private->smi_larb_base) {
		pr_err("error: iomap fail for smi_larb0.\n");
		WARN_ON(1);
		return 0;
	}

	/* Iterate over sibling DISP function blocks */
	for_each_child_of_node(dev->of_node->parent, node) {
		struct device_node *port, *ep, *remote;
		const struct of_device_id *of_id;
		enum mtk_ddp_comp_type comp_type, remote_comp_type;
		int comp_id;
		enum mtk_ddp_comp_id rp_id;

		of_id = of_match_node(mtk_ddp_comp_dt_ids, node);
		if (!of_id)
			continue;

		if (!of_device_is_available(node)) {
			dev_dbg(dev, "Skipping disabled component %pOF\n",
				node);
			continue;
		}

		comp_type = (enum mtk_ddp_comp_type)of_id->data;

		if (comp_type == MTK_DISP_MUTEX) {
			private->mutex_node = of_node_get(node);
			continue;
		}

		comp_id = mtk_ddp_comp_get_id(node, comp_type);
		if (comp_id < 0) {
			dev_warn(dev, "Skipping unknown component %pOF\n",
				 node);
			continue;
		}

		private->comp_node[comp_id] = of_node_get(node);

		/*
		 * Currently only the COLOR, OVL, RDMA, DSI, and DPI blocks have
		 * separate component platform drivers and initialize their own
		 * DDP component structure. The others are initialized here.
		 */

		dev_info(dev, "comp_type: %d, comp_id: %d\n", comp_type, comp_id);

		if (comp_type == MTK_DISP_COLOR ||
		    comp_type == MTK_DISP_OVL ||
		    comp_type == MTK_DISP_RDMA ||
		    comp_type == MTK_DSI ||
  		    comp_type == MTK_DBI ||
		    comp_type == MTK_DPI ||
		    comp_type == MTK_LVDS) {
			dev_info(dev, "Adding component match for %pOF\n",
				 node);
			drm_of_component_match_add(dev, &match, compare_of,
						   node);
		} else {
			struct mtk_ddp_comp *comp;

			comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
			if (!comp) {
				ret = -ENOMEM;
				of_node_put(node);
				goto err_node;
			}

			ret = mtk_ddp_comp_init(dev, node, comp, comp_id, NULL);
			if (ret) {
				of_node_put(node);
				goto err_node;
			}

			private->ddp_comp[comp_id] = comp;
		}

		if (comp_type != MTK_DSI && comp_type != MTK_DPI
					&& comp_type != MTK_DBI
					&& comp_type != MTK_LVDS) {
			port = of_graph_get_port_by_id(node, 0);
			if (port == NULL)
				continue;

			ep = of_get_child_by_name(port, "endpoint");
			of_node_put(port);
			if (ep == NULL)
				continue;

			remote = of_graph_get_remote_port_parent(ep);
			of_node_put(ep);
			if (remote == NULL)
				continue;

			of_id = of_match_node(mtk_ddp_comp_dt_ids, remote);
			if (of_id == NULL)
				continue;

			remote_comp_type = (enum mtk_ddp_comp_type)of_id->data;
			rp_id = mtk_ddp_comp_get_id(remote, remote_comp_type);
			for (i = 0; i < private->data->main_len - 1UL; i++)
				if (private->data->main_path[i] == comp_id) {
					private->data->main_path[i + 1] = rp_id;

					if (remote_comp_type == MTK_DSI)
						private->data->main_path[i + 2]
							= rp_id;
				}

			for (i = 0; i < private->data->ext_len - 1UL; i++)
				if (private->data->ext_path[i] == comp_id) {
					private->data->ext_path[i + 1] = rp_id;
					if (remote_comp_type == MTK_DSI)
						private->data->ext_path[i + 2]
							= rp_id;
				}

			for (i = 0; i < private->data->third_len - 1UL; i++)
				if (private->data->third_path[i] == comp_id) {
					private->data->third_path[i + 1]
						= rp_id;
					if (remote_comp_type == MTK_DSI)
						private->data->third_path[i + 2]
							= rp_id;
				}
		}
	}

	if (!private->mutex_node) {
		dev_err(dev, "Failed to find disp-mutex node\n");
		ret = -ENODEV;
		goto err_node;
	}

	// pm_runtime_enable(dev);    // tmp test
	// pm_runtime_enable(&pdev->dev);    // tmp test
	// pm_runtime_get_sync(dev);   // tmp test

	pm_runtime_enable(&pdev->dev);  // test
	pm_runtime_get_sync(&pdev->dev);  // test

	platform_set_drvdata(pdev, private);

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
		g_private = private;

		ret = mtk_drm_power_mode_init(private, dev);
		if (ret) {
			dev_dbg(dev, "Failed to init drm power mode\n");
			goto err_node;
		}
#endif

	if (!match) {
		dev_dbg(dev, "Failed to find match node\n");
		ret = -ENODEV;
		goto err_pm;
	}

	ret = component_master_add_with_match(dev, &mtk_drm_ops, match);

	dev_info(dev, "component_master_add_with_match: %d, match num: %lu\n", ret, match->num);

	if (ret)
		goto err_pm;

	return 0;

err_pm:
	dev_info(dev, "err_pm component_master_add_with_match.\n");

	pm_runtime_disable(dev);
err_node:
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++) {
		of_node_put(private->comp_node[i]);
		if (private->ddp_comp[i]) {
			put_device(private->ddp_comp[i]->larb_dev);
			private->ddp_comp[i] = NULL;
		}
	}
	return ret;
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	struct mtk_drm_private *private = platform_get_drvdata(pdev);
	int i;

	component_master_del(&pdev->dev, &mtk_drm_ops);
	pm_runtime_disable(&pdev->dev);
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#define MTK_DRM_SUSPEND    BIT(21)
#define MTK_DRM_RESUME    (BIT(21) | BIT(22))

void mtk_drm_sys_resume_callback(void)
{
	struct mtk_drm_power_mode *power_mode = g_private->power_mode;

	if (mtk_drm_suspend == true) {
		DRM_DEBUG_DRIVER("drm resume set wdg bit\n");
		regmap_update_bits(power_mode->map, power_mode->offset,
			power_mode->mask, MTK_DRM_RESUME);
		mtk_drm_suspend = false;
	}
}
#endif

static int mtk_drm_sys_suspend(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	struct mtk_drm_power_mode *power_mode = private->power_mode;
#endif
	int ret;

	ret = drm_mode_config_helper_suspend(drm);
	DRM_DEBUG_DRIVER("mtk_drm_sys_suspend\n");
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	ret |= regmap_update_bits(power_mode->map, power_mode->offset,
		power_mode->mask, MTK_DRM_SUSPEND);
	mtk_drm_suspend = true;
#endif
	return ret;
}

static void mtk_smi_larb(struct device *dev, u32 offset, u32 mask, u32 data) {
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	int i;

	u32 temp;

	for (i = 0; i < 6; i++) {
		temp = readl(private->smi_larb_base + offset + i * 4);
		printk(KERN_DEBUG "read mtk_smi_larb: %u, write: %u\n", temp, (temp & ~mask) | (data & mask));
		writel((temp & ~mask) | (data & mask), private->smi_larb_base + offset + i * 4);
	}
}

static int mtk_drm_sys_resume(struct device *dev)
{
	int ret = 0;


	// pm_runtime_enable(dev);     // test
	// pm_runtime_get_sync(dev);  // test

	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;

	if (!private->smi_larb_base) {
		pr_err("error: smi_larb_base is invalid, resume failed.\n");
		WARN_ON(1);
		return 0;
	}
	// mtk_smi_larb(dev, SMI_LARB0_MMU_EN, MMU_EN, 0);

	ret = drm_mode_config_helper_resume(drm);

	DRM_DEBUG_DRIVER("mtk_drm_sys_resume\n");

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_drm_pm_ops, mtk_drm_sys_suspend,
			 mtk_drm_sys_resume);

static const struct of_device_id mtk_drm_of_ids[] = {
	{ .compatible = "mediatek,mt2701-mmsys",
	  .data = &mt2701_mmsys_driver_data},
	{ .compatible = "mediatek,mt2712-display",
	  .data = &mt2712_mmsys_driver_data},
	{ .compatible = "mediatek,mt8173-mmsys",
	  .data = &mt8173_mmsys_driver_data},
	  { .compatible = "mediatek,mmsys_config",
	  .data = &mt6890_mmsys_driver_data},
	{ }
};

static struct platform_driver mtk_drm_platform_driver = {
	.probe	= mtk_drm_probe,
	.remove	= mtk_drm_remove,
	.driver	= {
		.name	= "mediatek-drm",
		.of_match_table = mtk_drm_of_ids,
		.pm     = &mtk_drm_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

// for mt6890
static struct platform_driver * const mtk_drm_drivers[] = {
	&mtk_ddp_driver,
	// &mtk_disp_color_driver,
	&mtk_disp_ovl_driver,
	&mtk_disp_rdma_driver,
	// &mtk_dpi_driver,
	&mtk_drm_platform_driver,
	&mtk_dsi_driver,
	&mtk_dbi_driver,
	&mtk_mipi_tx_driver,
	&mtk_lvds_tx_driver,
	&mtk_lvds_driver,
};

static int __init mtk_drm_init(void)
{
	return platform_register_drivers(mtk_drm_drivers,
					 ARRAY_SIZE(mtk_drm_drivers));
}

static void __exit mtk_drm_exit(void)
{
	platform_unregister_drivers(mtk_drm_drivers,
				    ARRAY_SIZE(mtk_drm_drivers));
}

module_init(mtk_drm_init);
module_exit(mtk_drm_exit);

MODULE_AUTHOR("YT SHEN <yt.shen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC DRM driver");
MODULE_LICENSE("GPL v2");
