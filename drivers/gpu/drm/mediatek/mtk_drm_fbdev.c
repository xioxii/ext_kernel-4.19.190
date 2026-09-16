// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 */

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>

#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"

#define to_drm_private(x) \
		container_of(x, struct mtk_drm_private, fb_helper)

static int mtk_drm_fbdev_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct mtk_drm_private *private = to_drm_private(helper);

	return mtk_drm_gem_mmap_buf(private->fbdev_bo, vma);
}

static int mtk_drm_fbdev_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_mode_set *mode_set;
	struct drm_crtc *crtc;
	struct fb_vblank vblank = {0};
	void __user *argp = (void __user *)arg;
	int ret = 0;

	switch (cmd) {
		case FBIOGET_VBLANK:
			mutex_lock(&fb_helper->lock);
			if (copy_from_user(&vblank, argp, sizeof(vblank)))
				return -EFAULT;

			mode_set = &fb_helper->crtc_info[0].mode_set;
			crtc = mode_set->crtc;
			vblank.flags = FB_VBLANK_HAVE_COUNT;
			vblank.count = drm_crtc_vblank_count(crtc);

			ret = copy_to_user(argp, &vblank, sizeof(vblank)) ? -EFAULT : 0;
			mutex_unlock(&fb_helper->lock);
			break;

		default:
			ret = drm_fb_helper_ioctl(info, cmd, arg);
			break;
	}

	return ret;
}

static struct fb_ops mtk_fbdev_ops = {
	.owner = THIS_MODULE,
	.fb_fillrect = drm_fb_helper_cfb_fillrect,
	.fb_copyarea = drm_fb_helper_cfb_copyarea,
	.fb_imageblit = drm_fb_helper_cfb_imageblit,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_blank = drm_fb_helper_blank,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_ioctl = mtk_drm_fbdev_ioctl,
	.fb_mmap = mtk_drm_fbdev_mmap,
};

static int mtk_fbdev_probe(struct drm_fb_helper *helper,
			   struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = helper->dev;
	struct mtk_drm_private *private = to_drm_private(helper);
	struct drm_mode_fb_cmd2 mode = { 0 };
	struct mtk_drm_gem_obj *mtk_gem;
	struct fb_info *info;
	struct drm_framebuffer *fb;
	unsigned int bytes_per_pixel;
	unsigned long offset;
	size_t size;
	int err;

	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);

	mode.width = sizes->surface_width;
	mode.height = sizes->surface_height;
	mode.pitches[0] = sizes->surface_width * bytes_per_pixel;
	mode.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
						      sizes->surface_depth);

	size = mode.pitches[0] * mode.height;

	mtk_gem = mtk_drm_gem_create(dev, size, true);
	if (IS_ERR(mtk_gem))
		return PTR_ERR(mtk_gem);

	private->fbdev_bo = &mtk_gem->base;

	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		err = PTR_ERR(info);
		dev_err(dev->dev, "failed to allocate framebuffer info, %d\n",
			err);
		goto err_gem_free_object;
	}

	fb = mtk_drm_framebuffer_create(dev, &mode, private->fbdev_bo);
	if (IS_ERR(fb)) {
		err = PTR_ERR(fb);
		dev_err(dev->dev, "failed to allocate DRM framebuffer, %d\n",
			err);
		goto err_release_fbi;
	}
	helper->fb = fb;

	info->par = helper;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->fbops = &mtk_fbdev_ops;

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->format->depth);
	drm_fb_helper_fill_var(info, helper, sizes->fb_width, sizes->fb_height);

	offset = info->var.xoffset * bytes_per_pixel;
	offset += info->var.yoffset * fb->pitches[0];

	dev->mode_config.fb_base = 0;
	info->screen_base = mtk_gem->kvaddr + offset;
	info->screen_size = size;
	info->fix.smem_len = size;

	DRM_DEBUG_KMS("FB [%ux%u]-%u offset=%lu size=%zd\n",
		      fb->width, fb->height, fb->format->depth, offset, size);

	info->skip_vt_switch = true;

	return 0;

err_release_fbi:
err_gem_free_object:
	mtk_drm_gem_free_object(&mtk_gem->base);
	return err;
}

static const struct drm_fb_helper_funcs mtk_drm_fb_helper_funcs = {
	.fb_probe = mtk_fbdev_probe,
};

int mtk_fbdev_init(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = &priv->fb_helper;
	int ret;

	printk(KERN_DEBUG "[%s:%d][%s()] start\n",  __FILE__, __LINE__, __func__);

	if (!dev->mode_config.num_crtc || !dev->mode_config.num_connector)
		return -EINVAL;

	drm_fb_helper_prepare(dev, helper, &mtk_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper,
				 dev->mode_config.num_connector);
	if (ret) {
		dev_err(dev->dev, "failed to initialize DRM FB helper, %d\n",
			ret);
		goto fini;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if (ret) {
		dev_err(dev->dev, "failed to add connectors, %d\n", ret);
		goto fini;
	}

	/* disable all the possible outputs/crtcs before entering KMS mode */
	if (!drm_drv_uses_atomic_modeset(dev)) {
		drm_helper_disable_unused_functions(dev);
	}

	ret = drm_fb_helper_initial_config(helper, 32);
	if (ret) {
		dev_err(dev->dev, "failed to set initial configuration, %d\n",
			ret);
		goto fini;
	}

	printk(KERN_DEBUG "[%s:%d][%s()] end\n",  __FILE__, __LINE__, __func__);

	return 0;

fini:
	drm_fb_helper_fini(helper);

	return ret;
}

void mtk_fbdev_fini(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct drm_fb_helper *helper = &priv->fb_helper;

	drm_fb_helper_unregister_fbi(helper);

	if (helper->fb) {
		drm_framebuffer_unregister_private(helper->fb);
		drm_framebuffer_remove(helper->fb);
	}

	drm_fb_helper_fini(helper);
}
