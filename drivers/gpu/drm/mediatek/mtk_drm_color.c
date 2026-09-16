// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yuesheng Wang <yuesheng.wang@mediatek.com>
 */

#include <linux/io.h>
#include <linux/uaccess.h>
#include <drm/drmP.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_color.h"
#include "mtk_color_index.h"

#define DISP_COLOR_CFG_MAIN           0x400U
#define DISP_COLOR_PXL_MAIN           0x404U
#define DISP_COLOR_LNE_MAIN           0x408U
#define DISP_COLOR_WIN_X_MAIN         0x40CU
#define DISP_COLOR_WIN_Y_MAIN         0x410U
#define DISP_COLOR_DBG_CFG_MAIN       0x420U
#define DISP_COLOR_C_BOOST_MAIN       0x428U
#define DISP_COLOR_C_BOOST_MAIN_2     0x42CU
#define DISP_COLOR_LUMA_ADJ           0x430U
#define DISP_COLOR_G_PIC_ADJ_MAIN_1   0x434U
#define DISP_COLOR_G_PIC_ADJ_MAIN_2   0x438U
#define DISP_COLOR_Y_SLOPE_1_0_MAIN   0x4A0U
#define DISP_COLOR_LOCAL_HUE_CD_0     0x620U
#ifdef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
#define DISP_COLOR_LOCAL_HUE_CD_1     0x624U
#define DISP_COLOR_LOCAL_HUE_CD_2     0x628U
#define DISP_COLOR_LOCAL_HUE_CD_3     0x62CU
#define DISP_COLOR_LOCAL_HUE_CD_4     0x630U
#endif
#define DISP_COLOR_TWO_D_WINDOW_1     0x740U
#define DISP_COLOR_TWO_D_W1_RESULT    0x74CU
#define DISP_COLOR_PART_SAT_GAIN1_0   0x7FCU
#define DISP_COLOR_PART_SAT_GAIN1_1   0x800U
#define DISP_COLOR_PART_SAT_GAIN1_2   0x804U
#define DISP_COLOR_PART_SAT_GAIN1_3   0x808U
#define DISP_COLOR_PART_SAT_GAIN1_4   0x80CU
#define DISP_COLOR_PART_SAT_GAIN2_0   0x810U
#define DISP_COLOR_PART_SAT_GAIN2_1   0x814U
#define DISP_COLOR_PART_SAT_GAIN2_2   0x818U
#define DISP_COLOR_PART_SAT_GAIN2_3   0x81CU
#define DISP_COLOR_PART_SAT_GAIN2_4   0x820U
#define DISP_COLOR_PART_SAT_GAIN3_0   0x824U
#define DISP_COLOR_PART_SAT_GAIN3_1   0x828U
#define DISP_COLOR_PART_SAT_GAIN3_2   0x82CU
#define DISP_COLOR_PART_SAT_GAIN3_3   0x830U
#define DISP_COLOR_PART_SAT_GAIN3_4   0x834U
#define DISP_COLOR_PART_SAT_POINT1_0  0x838U
#define DISP_COLOR_PART_SAT_POINT1_1  0x83CU
#define DISP_COLOR_PART_SAT_POINT1_2  0x840U
#define DISP_COLOR_PART_SAT_POINT1_3  0x844U
#define DISP_COLOR_PART_SAT_POINT1_4  0x848U
#define DISP_COLOR_PART_SAT_POINT2_0  0x84CU
#define DISP_COLOR_PART_SAT_POINT2_1  0x850U
#define DISP_COLOR_PART_SAT_POINT2_2  0x854U
#define DISP_COLOR_PART_SAT_POINT2_3  0x858U
#define DISP_COLOR_PART_SAT_POINT2_4  0x85CU
#define DISP_COLOR_START              0xC00U
#define DISP_COLOR_INTEN              0xC04U
#define DISP_COLOR_OUT_SEL            0xC08U
#define DISP_COLOR_CK_ON              0xC28U
#define DISP_COLOR_INTERNAL_IP_WIDTH  0xC50U
#define DISP_COLOR_INTERNAL_IP_HEIGHT 0xC54U
#define DISP_COLOR_CM1_EN             0xC60U
#define DISP_COLOR_CM2_EN             0xCA0U

static unsigned int g_color_bypass[COLOR_NUM];
static unsigned int g_ncs_tuning_mode;

void mtk_color_set_ncs_mode(unsigned int mode)
{
	g_ncs_tuning_mode = mode;
}

static unsigned int mtk_color_get_color_id(struct mtk_ddp_comp *comp)
{
	unsigned int color_id;

	if (comp->id == DDP_COMPONENT_COLOR0)
		color_id = 0U;
	else if (comp->id == DDP_COMPONENT_COLOR1)
		color_id = 1U;
	else
		color_id = 0U;

	return color_id;
}

static enum mtk_ddp_comp_id mtk_color_get_comp_id(unsigned int color_id)
{
	enum mtk_ddp_comp_id comp_id;

	if (color_id == 0U)
		comp_id = DDP_COMPONENT_COLOR0;
	else if (color_id == 1U)
		comp_id = DDP_COMPONENT_COLOR1;
	else
		comp_id = DDP_COMPONENT_COLOR0;

	return comp_id;
}

static struct mtk_color_param *get_color_config(unsigned int color_id)
{
	return &g_color_param[color_id];
}

#ifndef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
static inline struct mtk_color_idx *get_color_index(void)
{
	return &g_color_index;
}
#endif

void mtk_color_init(struct mtk_ddp_comp *comp,
		unsigned int w, unsigned int h)
{
	void __iomem *addr = comp->regs;
	unsigned int color_id = 0U;
	struct mtk_color_param *p_param;

	writel(w, addr + DISP_COLOR_INTERNAL_IP_WIDTH);
	writel(h, addr + DISP_COLOR_INTERNAL_IP_HEIGHT);

	writel((1UL << 29U), addr + DISP_COLOR_CFG_MAIN);
	writel(0x1, addr + DISP_COLOR_START);

	/* enable interrupt */
	writel(0x7, addr + DISP_COLOR_INTEN);

	/* Set 10bit->8bit Rounding */
	writel(0x333, addr + DISP_COLOR_OUT_SEL);

	color_id = mtk_color_get_color_id(comp);
	p_param = get_color_config(color_id);
	mtk_color_config_param(addr, color_id, p_param);
}

static void mtk_color_config_brightness_contrast(void __iomem *addr,
		struct mtk_color_param *p_param)
{
	unsigned int val = 0;
	#ifndef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
	struct mtk_color_idx *pid = get_color_index();
	#endif

	#ifdef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
	val = (p_param->u4Brightness << 16U) |
		p_param->u4Contrast;
	writel(val, addr + DISP_COLOR_G_PIC_ADJ_MAIN_1);
	val = (0x200UL << 16U) |
		p_param->u4SatGain;
	writel(val, addr + DISP_COLOR_G_PIC_ADJ_MAIN_2);
	#else
	val = (pid->BRIGHTNESS[p_param->u4Brightness] << 16U) |
		pid->CONTRAST[p_param->u4Contrast];
	writel(val, addr + DISP_COLOR_G_PIC_ADJ_MAIN_1);
	val = (0x200UL << 16U) |
		pid->GLOBAL_SAT[p_param->u4SatGain];
	writel(val, addr + DISP_COLOR_G_PIC_ADJ_MAIN_2);
	#endif
}

#ifdef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
static void mtk_color_config_hue(void __iomem *addr,
		struct mtk_color_param *p_param)
{
	unsigned int val = 0;

	val = p_param->u4HueCd0;
	writel(val, addr + DISP_COLOR_LOCAL_HUE_CD_0);

	val = p_param->u4HueCd1;
	writel(val, addr + DISP_COLOR_LOCAL_HUE_CD_1);

	val = p_param->u4HueCd2;
	writel(val, addr + DISP_COLOR_LOCAL_HUE_CD_2);

	val = p_param->u4HueCd3;
	writel(val, addr + DISP_COLOR_LOCAL_HUE_CD_3);

	val = p_param->u4HueCd4;
	writel(val, addr + DISP_COLOR_LOCAL_HUE_CD_4);
}
#endif

#ifndef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
static void mtk_color_config_partial_Y(void __iomem *addr,
		struct mtk_color_param *p_param)
{
	int index = 0;
	unsigned int val = 0;
	struct mtk_color_idx *pid = get_color_index();

	for (index = 0; index < 8; index++) {
		val = (pid->PARTIAL_Y[p_param->u4PartialY][2 * index] |
		       pid->PARTIAL_Y[p_param->u4PartialY][2 * index + 1] << 16U
			   );
		writel(val, addr + DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index);
	}
}

static void mtk_color_config_partial_saturation(void __iomem *addr,
		struct mtk_color_param *p_param)
{
	unsigned int val = 0;
	struct mtk_color_idx *pid = get_color_index();

	val = (pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG1][0] |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG1][1] << 8U |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG1][2] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG1][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN1_0);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG1][1] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG1][2] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG1][3] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG1][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN1_1);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG1][5] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG1][6] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG1][7] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG1][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN1_2);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG1][1] |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG1][2] << 8U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG1][3] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG1][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN1_3);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG1][5] |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG1][0] << 8U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG1][1] << 16U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG1][2] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN1_4);

	val = (pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG2][0] |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG2][1] << 8U |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG2][2] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG2][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN2_0);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG2][1] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG2][2] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG2][3] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG2][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN2_1);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG2][5] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG2][6] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG2][7] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG2][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN2_2);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG2][1] |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG2][2] << 8U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG2][3] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG2][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN2_3);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG2][5] |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG2][0] << 8U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG2][1] << 16U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG2][2] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN2_4);

	val = (pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG3][0] |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG3][1] << 8U |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SG3][2] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG3][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN3_0);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG3][1] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG3][2] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG3][3] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG3][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN3_1);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG3][5] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG3][6] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SG3][7] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG3][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN3_2);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG3][1] |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG3][2] << 8U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG3][3] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG3][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN3_3);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SG3][5] |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG3][0] << 8U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG3][1] << 16U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SG3][2] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_GAIN3_4);

	val = (pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SP1][0] |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SP1][1] << 8U |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SP1][2] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP1][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT1_0);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP1][1] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP1][2] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP1][3] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP1][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT1_1);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP1][5] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP1][6] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP1][7] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP1][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT1_2);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP1][1] |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP1][2] << 8U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP1][3] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP1][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT1_3);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP1][5] |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SP1][0] << 8U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SP1][1] << 16U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SP1][2] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT1_4);

	val = (pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SP2][0] |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SP2][1] << 8U |
	       pid->PURP_TONE_S[p_param->u4SatAdj[PURP_TONE]][SP2][2] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP2][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT2_0);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP2][1] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP2][2] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP2][3] << 16U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP2][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT2_1);

	val = (pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP2][5] |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP2][6] << 8U |
	       pid->SKIN_TONE_S[p_param->u4SatAdj[SKIN_TONE]][SP2][7] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP2][0] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT2_2);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP2][1] |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP2][2] << 8U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP2][3] << 16U |
	       pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP2][4] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT2_3);

	val = (pid->GRASS_TONE_S[p_param->u4SatAdj[GRASS_TONE]][SP2][5] |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SP2][0] << 8U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SP2][1] << 16U |
	       pid->SKY_TONE_S[p_param->u4SatAdj[SKY_TONE]][SP2][2] << 24U);
	writel(val, addr + DISP_COLOR_PART_SAT_POINT2_4);
}

static void mtk_color_config_partial_hue(void __iomem *addr,
		struct mtk_color_param *p_param)
{
	int index = 0;
	unsigned int h_series[20] = {0U};
	unsigned int u4Temp = 0U;
	struct mtk_color_idx *pid = get_color_index();

	for (index = 0; index < 3; index++)
		h_series[index + PURP_TONE_START] =
			pid->PURP_TONE_H[p_param->u4HueAdj[PURP_TONE]][index];

	for (index = 0; index < 8; index++)
		h_series[index + SKIN_TONE_START] =
			pid->SKIN_TONE_H[p_param->u4HueAdj[SKIN_TONE]][index];

	for (index = 0; index < 6; index++)
		h_series[index + GRASS_TONE_START] =
			pid->GRASS_TONE_H[p_param->u4HueAdj[GRASS_TONE]][index];

	for (index = 0; index < 3; index++)
		h_series[index + SKY_TONE_START] =
			pid->SKY_TONE_H[p_param->u4HueAdj[SKY_TONE]][index];

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) +
			(h_series[4 * index + 1] << 8U) +
			(h_series[4 * index + 2] << 16U) +
			(h_series[4 * index + 3] << 24U);
		writel(u4Temp, addr + DISP_COLOR_LOCAL_HUE_CD_0 + 4 * index);
	}
}
#endif

void mtk_color_config_param(void __iomem *addr,
		unsigned int color_id,
		struct mtk_color_param *p_param)
{
	#ifndef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
	if (p_param->u4SatGain >= COLOR_TUNING_INDEX ||
			p_param->u4HueAdj[PURP_TONE] >= COLOR_TUNING_INDEX ||
			p_param->u4HueAdj[SKIN_TONE] >= COLOR_TUNING_INDEX ||
			p_param->u4HueAdj[GRASS_TONE] >= COLOR_TUNING_INDEX ||
			p_param->u4HueAdj[SKY_TONE] >= COLOR_TUNING_INDEX ||
			p_param->u4SatAdj[PURP_TONE] >= COLOR_TUNING_INDEX ||
			p_param->u4SatAdj[SKIN_TONE] >= COLOR_TUNING_INDEX ||
			p_param->u4SatAdj[GRASS_TONE] >= COLOR_TUNING_INDEX ||
			p_param->u4SatAdj[SKY_TONE] >= COLOR_TUNING_INDEX ||
			p_param->u4Contrast >= CONTRAST_SIZE ||
			p_param->u4Brightness >= BRIGHTNESS_SIZE) {
		DRM_ERROR("%s : Tuning index range error!\n", __func__);
		return;
	}
	#endif

	if (color_id >= COLOR_NUM) {
		DRM_ERROR("%s : Color id range error!\n", __func__);
		return;
	}

	if (g_color_bypass[color_id] == 0U) {
		/* enable R2Y/Y2R in Color Wrapper */
		writel(1, addr + DISP_COLOR_CM1_EN);
		writel(1, addr + DISP_COLOR_CM2_EN);
	}

	/* set brightness and contrast in Y */
	mtk_color_config_brightness_contrast(addr, p_param);

	#ifdef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
	/* set hue */
	mtk_color_config_hue(addr, p_param);
	#else
	/* Partial Y Function */
	mtk_color_config_partial_Y(addr, p_param);

	/* Partial Saturation Function */
	mtk_color_config_partial_saturation(addr, p_param);

	/* Partial Hue Function */
	mtk_color_config_partial_hue(addr, p_param);
	#endif

	/* Saturation and Hue upper/lower bound, mm setting */
	writel(0x40106051, addr + DISP_COLOR_TWO_D_WINDOW_1);
}

static void ddp_color_bypass_color(struct mtk_drm_private *private,
		struct mtk_color_bypass *bypass_param)
{
	unsigned int color_id = bypass_param->color_id;
	enum mtk_ddp_comp_id comp_id;
	void __iomem *addr = NULL;

	if (color_id >= COLOR_NUM) {
		DRM_ERROR("%s : Color id range error!\n", __func__);
		return;
	}

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (color_id == COLOR_ID_0)
		return;
#endif

	comp_id = mtk_color_get_comp_id(color_id);
	addr = private->ddp_comp[comp_id]->regs;

	if (bypass_param->bypass != 0U) {
		g_color_bypass[color_id] = 1U;
		DISP_REG_MASK(addr + DISP_COLOR_CFG_MAIN, (1U << 7U), 0x80U);
	} else {
		g_color_bypass[color_id] = 0U;
		DISP_REG_MASK(addr + DISP_COLOR_CFG_MAIN, (0U << 7U), 0x80U);
	}
}

static void ddp_color_set_window(struct mtk_drm_private *private,
		struct mtk_color_window *win_param)
{
	unsigned int color_id = win_param->color_id;
	enum mtk_ddp_comp_id comp_id;
	void __iomem *addr = NULL;
	unsigned int split_en = 0U;
	unsigned int split_window_x = 0xFFFF0000U;
	unsigned int split_window_y = 0xFFFF0000U;

	if (color_id >= COLOR_NUM) {
		DRM_ERROR("%s : Color id range error!\n", __func__);
		return;
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (color_id == COLOR_ID_0)
		return;
#endif

	comp_id = mtk_color_get_comp_id(color_id);
	addr = private->ddp_comp[comp_id]->regs;

	/* don't save to global */
	if (win_param->split_en != 0U) {
		split_en = 1U;
		split_window_x =
				(win_param->end_x << 16U) | win_param->start_x;
		split_window_y =
				(win_param->end_y << 16U) | win_param->start_y;
	}

	DISP_REG_MASK(addr + DISP_COLOR_DBG_CFG_MAIN,
			(split_en << 3U), 0x8U);
	writel(split_window_x, addr + DISP_COLOR_WIN_X_MAIN);
	writel(split_window_y, addr + DISP_COLOR_WIN_Y_MAIN);
}

static void mtk_color_set_pqparam(struct mtk_drm_private *private,
		uint64_t val)
{
	struct mtk_color_param param;
	struct mtk_color_param *p_param = NULL;
	unsigned int color_id = 0U;
	enum mtk_ddp_comp_id comp_id;
	void __iomem *addr;

	if (g_ncs_tuning_mode != 0U) {
		DRM_INFO("%s : is ncs_tuning mode!\n", __func__);
		return;
	}

	if (copy_from_user(&param,
			(void *)(long)val,
			sizeof(struct mtk_color_param)) != 0U) {
		DRM_ERROR("%s : Copy from user failed!\n", __func__);
		return;
	}

#ifdef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
	if (param.u4FullAdj == 0) {
		DRM_ERROR("%s : user data not full tuning!\n", __func__);
		return;
	}
#endif

	color_id = param.color_id;
	if (color_id >= COLOR_NUM) {
		DRM_ERROR("%s : Color id range error!\n", __func__);
		return;
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	if (color_id == COLOR_ID_0)
		return;
#endif

	p_param = get_color_config(color_id);
	memcpy(p_param, &param, sizeof(struct mtk_color_param));

	comp_id = mtk_color_get_comp_id(color_id);
	addr = private->ddp_comp[comp_id]->regs;

	mtk_color_config_param(addr, color_id, p_param);
}

static void mtk_color_get_pqparam(uint64_t val)
{
	struct mtk_color_param param;
	struct mtk_color_param *p_param;
	unsigned int color_id = 0U;

	if (copy_from_user(&param,
			(void *)(long)val,
			sizeof(struct mtk_color_param)) != 0U) {
		DRM_ERROR("%s : Copy from user failed!\n", __func__);
		return;
	}

	color_id = param.color_id;
	if (color_id >= COLOR_NUM) {
		DRM_ERROR("%s : Color id range error!\n", __func__);
		return;
	}

	p_param = get_color_config(color_id);

	if (copy_to_user((void *)(long)val,
			p_param, sizeof(struct mtk_color_param)) != 0U) {
		DRM_ERROR("%s : Copy to user failed!\n", __func__);
		return;
	}
}

#ifndef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
static void mtk_color_set_pqindex(uint64_t val)
{
	struct mtk_color_idx *pid = get_color_index();

	if (copy_from_user(pid,
			(void *)(long)val,
			sizeof(struct mtk_color_idx)) != 0U) {
		DRM_ERROR("%s : Copy from user failed!\n", __func__);
		return;
	}
}
#endif

static void mtk_color_set_bypass(struct mtk_drm_private *private,
		uint64_t val)
{
	struct mtk_color_bypass bypass_param;

	if (copy_from_user(&bypass_param,
			(void *)(long)val,
			sizeof(struct mtk_color_bypass)) != 0U) {
		DRM_ERROR("%s : Copy from user failed!\n", __func__);
		return;
	}

	ddp_color_bypass_color(private, &bypass_param);
}

static void mtk_color_set_window(struct mtk_drm_private *private,
		uint64_t val)
{
	struct mtk_color_window win_param;

	if (copy_from_user(&win_param,
			(void *)(long)val,
			sizeof(struct mtk_color_window)) != 0U) {
		DRM_ERROR("%s : Copy from user failed!\n", __func__);
		return;
	}

	ddp_color_set_window(private, &win_param);
}

void mtk_color_set_property_handler(struct mtk_drm_private *private,
		enum mtk_color_property property,
		uint64_t val)
{
	switch (property) {
	case COLOR_PROPERTY_PQPARAM:
		mtk_color_set_pqparam(private, val);
		break;
	case COLOR_PROPERTY_PQINDEX:
		#ifndef CONFIG_MTK_DISPLAY_FULL_TUNING_COLOR
		mtk_color_set_pqindex(val);
		#endif
		break;
	case COLOR_PROPERTY_COLOR_BYPASS:
		mtk_color_set_bypass(private, val);
		break;
	case COLOR_PROPERTY_COLOR_WINDOW:
		mtk_color_set_window(private, val);
		break;
	case COLOR_PROPERTY_GET_PQPARAM:
		mtk_color_get_pqparam(val);
		break;
	default:
		DRM_ERROR("%s : not support property!\n", __func__);
		break;
	}
}
