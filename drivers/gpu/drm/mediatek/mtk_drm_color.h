/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yuesheng Wang <yuesheng.wang@mediatek.com>
 */

#ifndef MTK_DRM_COLOR_H
#define MTK_DRM_COLOR_H

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"

#define DISP_REG_MASK(reg, val, mask) \
	writel((unsigned int)(readl(reg)&~(mask))|(val), reg)

#define REG_FLD(width, shift) \
	((unsigned int)((((width) & 0xff) << 16U) | ((shift) & 0xff)))

#define CFG_MAIN_FLD_COLOR_DBUF_EN      REG_FLD(1, 29)
#define START_FLD_DISP_COLOR_START      REG_FLD(1, 0)

#define COLOR_NUM    3U

#ifndef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
#define PURP_TONE    0
#define SKIN_TONE    1
#define GRASS_TONE   2
#define SKY_TONE     3

#define PURP_TONE_START    0
#define PURP_TONE_END      2
#define SKIN_TONE_START    3
#define SKIN_TONE_END     10
#define GRASS_TONE_START  11
#define GRASS_TONE_END    16
#define SKY_TONE_START    17
#define SKY_TONE_END      19

#define SG1 0
#define SG2 1
#define SG3 2
#define SP1 3
#define SP2 4
#endif

#define COLOR_TUNING_INDEX   19U
#define PARTIAL_Y_INDEX      10U
#define GLOBAL_SAT_SIZE      10U
#define CONTRAST_SIZE        10U
#define BRIGHTNESS_SIZE      10U
#define PARTIAL_Y_SIZE       16U
#define PQ_HUE_ADJ_PHASE_CNT 4U
#define PQ_SAT_ADJ_PHASE_CNT 4U
#define PQ_PARTIALS          5U
#define PURP_TONE_SIZE       3U
#define SKIN_TONE_SIZE       8U /* (-6) */
#define GRASS_TONE_SIZE      6U /* (-2) */
#define SKY_TONE_SIZE        3U

struct mtk_color_param {
	u32 u4SatGain;
	u32 u4PartialY;
	u32 u4HueAdj[PQ_HUE_ADJ_PHASE_CNT]; /* 0~18 */
	u32 u4SatAdj[PQ_SAT_ADJ_PHASE_CNT]; /* 0~18 */
	u32 u4Contrast;
	u32 u4FullAdj;
#ifdef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
	u32 u4HueCd0;
	u32 u4HueCd1;
	u32 u4HueCd2;
	u32 u4HueCd3;
	u32 u4HueCd4;
#endif
	u32 u4Brightness;
	u32 color_id; /* 0~2 */
};

struct mtk_color_idx {
	u32 GLOBAL_SAT[GLOBAL_SAT_SIZE];
	u32 CONTRAST[CONTRAST_SIZE];
	u32 BRIGHTNESS[BRIGHTNESS_SIZE];
	u32 PARTIAL_Y[PARTIAL_Y_INDEX][PARTIAL_Y_SIZE];
	u32 PURP_TONE_S[COLOR_TUNING_INDEX][PQ_PARTIALS][PURP_TONE_SIZE];
	u32 SKIN_TONE_S[COLOR_TUNING_INDEX][PQ_PARTIALS][SKIN_TONE_SIZE];
	u32 GRASS_TONE_S[COLOR_TUNING_INDEX][PQ_PARTIALS][GRASS_TONE_SIZE];
	u32 SKY_TONE_S[COLOR_TUNING_INDEX][PQ_PARTIALS][SKY_TONE_SIZE];
	u32 PURP_TONE_H[COLOR_TUNING_INDEX][PURP_TONE_SIZE];
	u32 SKIN_TONE_H[COLOR_TUNING_INDEX][SKIN_TONE_SIZE];
	u32 GRASS_TONE_H[COLOR_TUNING_INDEX][GRASS_TONE_SIZE];
	u32 SKY_TONE_H[COLOR_TUNING_INDEX][SKY_TONE_SIZE];
};

struct mtk_color_bypass {
	unsigned int bypass;
	unsigned int color_id;
};

struct mtk_color_window {
	unsigned int split_en;
	unsigned int start_x;
	unsigned int start_y;
	unsigned int end_x;
	unsigned int end_y;
	unsigned int color_id;
};

enum mtk_color_id {
	COLOR_ID_0 = 0,
	COLOR_ID_1 = 1,
	COLOR_ID_2 = 2,
};

enum mtk_color_property {
	COLOR_PROPERTY_PQPARAM,
	COLOR_PROPERTY_PQINDEX,
	COLOR_PROPERTY_COLOR_BYPASS,
	COLOR_PROPERTY_COLOR_WINDOW,
	COLOR_PROPERTY_GET_PQPARAM,
};

extern unsigned int mtk_color_if_bypass(void);

void mtk_color_set_ncs_mode(unsigned int mode);
void mtk_color_init(struct mtk_ddp_comp *comp,
		unsigned int w, unsigned int h);
void mtk_color_config_param(void __iomem *addr,
		unsigned int color_id,
		struct mtk_color_param *p_param);
void mtk_color_set_property_handler(struct mtk_drm_private *private,
		enum mtk_color_property property,
		uint64_t val);
#endif
