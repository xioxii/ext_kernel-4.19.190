/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_DRM_DEBUGFS_H
#define MTK_DRM_DEBUGFS_H

#include "mtk_drm_drv.h"

#ifdef CONFIG_DEBUG_FS
void mtk_drm_debugfs_init(struct drm_device *dev,
			  struct mtk_drm_private *priv);
void mtk_drm_debugfs_deinit(void);
#else
static inline void mtk_drm_debugfs_init(struct drm_device *dev,
					struct mtk_drm_private *priv) {}
static inline void mtk_drm_debugfs_deinit(void) {}
#endif
bool force_alpha(void);

#ifdef CONFIG_DEBUG_FS
void mtk_mdp_tuning_read(unsigned long addr);
void mtk_mdp_tuning_write(unsigned long addr, unsigned int val);
#endif

#endif /* MTK_DRM_DEBUGFS_H */
