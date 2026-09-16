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

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include <drm/drmP.h>

#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"

// #define DISP_REG_CONFIG_DISP_OVL0_MOUT_EN	0x040
#define DISP_REG_CONFIG_DISP_OVL0_MOUT_EN	0xf18
#define DISP_REG_CONFIG_DISP_OVL1_MOUT_EN	0x044
#define DISP_REG_CONFIG_DISP_OD_MOUT_EN		0x048
#define DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN	0x04c
#define DISP_REG_CONFIG_DISP_UFOE_MOUT_EN	0x050
#define DISP_REG_CONFIG_DISP_COLOR0_SEL_IN	0x084
#define DISP_REG_CONFIG_DISP_WDMA0_SEL_IN	0x098
#define DISP_REG_CONFIG_DISP_WDMA1_SEL_IN	0x09c
#define DISP_REG_CONFIG_DISP_COLOR1_SEL_IN	0x088
#define DISP_REG_CONFIG_DSIE_SEL_IN		0x0a4
#define DISP_REG_CONFIG_DSIO_SEL_IN		0x0a8
#define DISP_REG_CONFIG_DPI_SEL_IN		0x0ac
#define DISP_REG_CONFIG_DISP_RDMA2_SOUT		0x0b8
#define DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN	0x0c4
#define DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN	0x0c8
#define DISP_REG_CONFIG_MMSYS_CG_CON0		0x100

// + MT6890
#define DISP_REG_CONFIG_MMSYS_CG_SET0		0x104
#define DISP_REG_CONFIG_MMSYS_CG_CLR0		0x108
#define DISP_REG_CONFIG_MMSYS_CG_CON1		0x110
#define DISP_REG_CONFIG_MMSYS_CG_SET1		0x114
#define DISP_REG_CONFIG_MMSYS_CG_CLR1		0x118
#define DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN	0xf1c
#define DISP_REG_CONFIG_DSI0_SEL_IN		        0xf24
#define DISP_REG_CONFIG_DBI0_SEL_IN		        0xf28
#define DISP_REG_CONFIG_MMSYS_CG_CON2		0x120
#define DISP_REG_CONFIG_MMSYS_CG_SET2		0x124
#define DISP_REG_CONFIG_MMSYS_CG_CLR2		0x128
#define DISP_REG_CONFIG_MMSYS_CG_CON3		0x130
#define DISP_REG_CONFIG_MMSYS_CG_SET3		0x134
#define DISP_REG_CONFIG_MMSYS_CG_CLR3		0x138
#define DISP_REG_CONFIG_MMSYS_CG_CON4		0x140
#define DISP_REG_CONFIG_MMSYS_CG_SET4		0x144
#define DISP_REG_CONFIG_MMSYS_CG_CLR4		0x148
// -

#define DISP_REG_CONFIG_DISP_OVL_MOUT_EN	0x030
#define DISP_REG_CONFIG_OUT_SEL			0x04c
#define DISP_REG_CONFIG_DSI_SEL			0x050

#define DISP_REG_MUTEX_INTEN 0x00    //mt6890
#define DISP_REG_MUTEX_INTSTA 0x04   // mt6890
#define DISP_REG_MUTEX_EN(n)	(0x20 + 0x20 * (n))
#define DISP_REG_MUTEX(n)	(0x24 + 0x20 * (n))
#define DISP_REG_MUTEX_RST(n)	(0x28 + 0x20 * (n))
#define DISP_REG_MUTEX_MOD(n)	(0x30 + 0x20 * (n))    // mt6890
#define DISP_REG_MUTEX_SOF(n)	(0x2c + 0x20 * (n))    // mt6890
// #define DISP_REG_MUTEX_MOD(n)	(0x2c + 0x20 * (n))
// #define DISP_REG_MUTEX_SOF(n)	(0x30 + 0x20 * (n))
#define DISP_REG_MUTEX_MOD2(n)	(0x34 + 0x20 * (n))

#define INT_MUTEX				BIT(1)

#define MT6890_MUTEX_MOD_DISP_PWM0      14
#define MT6890_MUTEX_MOD_DISP_DBI0      13
#define MT6890_MUTEX_MOD_DISP_DSI0      12
#define MT6890_MUTEX_MOD_DISP_WDMA0     11
#define MT6890_MUTEX_MOD_DISP_DITHER0   10
#define MT6890_MUTEX_MOD_DISP_GAMMA0    9
#define MT6890_MUTEX_MOD_DISP_AAL0      8
#define MT6890_MUTEX_MOD_DISP_CCORR0    7
#define MT6890_MUTEX_MOD_DISP_COLOR0    6
#define MT6890_MUTEX_MOD_DISP_RDMA0     5
#define MT6890_MUTEX_MOD_DISP_OVL0      4

#define MT8173_MUTEX_MOD_DISP_OVL0		11
#define MT8173_MUTEX_MOD_DISP_OVL1		12
#define MT8173_MUTEX_MOD_DISP_RDMA0		13
#define MT8173_MUTEX_MOD_DISP_RDMA1		14
#define MT8173_MUTEX_MOD_DISP_RDMA2		15
#define MT8173_MUTEX_MOD_DISP_WDMA0		16
#define MT8173_MUTEX_MOD_DISP_WDMA1		17
#define MT8173_MUTEX_MOD_DISP_COLOR0		18
#define MT8173_MUTEX_MOD_DISP_COLOR1		19
#define MT8173_MUTEX_MOD_DISP_AAL		20
#define MT8173_MUTEX_MOD_DISP_GAMMA		21
#define MT8173_MUTEX_MOD_DISP_UFOE		22
#define MT8173_MUTEX_MOD_DISP_PWM0		23
#define MT8173_MUTEX_MOD_DISP_PWM1		24
#define MT8173_MUTEX_MOD_DISP_OD		25

#define MT2712_MUTEX_MOD_DISP_PWM2		10
#define MT2712_MUTEX_MOD_DISP_OVL0		11
#define MT2712_MUTEX_MOD_DISP_OVL1		12
#define MT2712_MUTEX_MOD_DISP_RDMA0		13
#define MT2712_MUTEX_MOD_DISP_RDMA1		14
#define MT2712_MUTEX_MOD_DISP_RDMA2		15
#define MT2712_MUTEX_MOD_DISP_WDMA0		16
#define MT2712_MUTEX_MOD_DISP_WDMA1		17
#define MT2712_MUTEX_MOD_DISP_COLOR0		18
#define MT2712_MUTEX_MOD_DISP_COLOR1		19
#define MT2712_MUTEX_MOD_DISP_AAL0		20
#define MT2712_MUTEX_MOD_DISP_UFOE		22
#define MT2712_MUTEX_MOD_DISP_PWM0		23
#define MT2712_MUTEX_MOD_DISP_PWM1		24
#define MT2712_MUTEX_MOD_DISP_OD0		25
#define MT2712_MUTEX_MOD2_DISP_AAL1		33
#define MT2712_MUTEX_MOD2_DISP_OD1		34

#define MT2701_MUTEX_MOD_DISP_OVL		3
#define MT2701_MUTEX_MOD_DISP_WDMA		6
#define MT2701_MUTEX_MOD_DISP_COLOR		7
#define MT2701_MUTEX_MOD_DISP_BLS		9
#define MT2701_MUTEX_MOD_DISP_RDMA0		10
#define MT2701_MUTEX_MOD_DISP_RDMA1		12

#define MUTEX_SOF_SINGLE_MODE		0
#define MUTEX_SOF_DSI0   12
#define MUTEX_SOF_DBPI0   13

#define MT6890_MUTEX_SOF_DSI0 1
#define MT6890_MUTEX_EOF_DSI0 (MT6890_MUTEX_SOF_DSI0 << 6)

#define MT6890_MUTEX_SOF_DBI0 2
#define MT6890_MUTEX_EOF_DBI0 (MT6890_MUTEX_SOF_DBI0 << 6)

// #define MUTEX_SOF_DSI0			1
// #define MUTEX_SOF_DSI1			2
// #define MUTEX_SOF_DPI0			3
// #define MUTEX_SOF_DPI1			4
// #define MUTEX_SOF_DSI2			5
// #define MUTEX_SOF_DSI3			6

// #define MT6890_MUTEX_SOF_SINGLE_MODE 0
// #define MT6890_MUTEX_SOF_DSI0   12
// #define MT6890_MUTEX_SOF_DPI0   13

#define OVL0_MOUT_EN_COLOR0		0x1
#define OD_MOUT_EN_RDMA0		0x1
#define OD_MOUT_EN_WDMA0		0x4
#define OD1_MOUT_EN_RDMA1		BIT(16)
#define OD1_MOUT_EN_WDMA1		BIT(18)
#define UFOE_MOUT_EN_DSI0		0x1
#define COLOR0_SEL_IN_OVL0		0x1
#define OVL1_MOUT_EN_COLOR1		0x1
#define GAMMA_MOUT_EN_RDMA1		0x1
#define RDMA0_SOUT_DPI0			0x2
#define RDMA0_SOUT_DPI1			0x3
#define RDMA0_SOUT_DSI0         0x1   // mt6890
#define RDMA0_SOUT_DBI0         0x2   // mt6890
#define RDMA0_SOUT_DSI1			0x1
#define RDMA0_SOUT_DSI2			0x4
#define RDMA0_SOUT_DSI3			0x5
#define RDMA1_SOUT_DPI0			0x2
#define RDMA1_SOUT_DPI1			0x3
#define RDMA1_SOUT_DSI1			0x1
#define RDMA1_SOUT_DSI2			0x4
#define RDMA1_SOUT_DSI3			0x5
#define RDMA2_SOUT_DPI0			0x2
#define RDMA2_SOUT_DPI1			0x3
#define RDMA2_SOUT_DSI1			0x1
#define RDMA2_SOUT_DSI2			0x4
#define RDMA2_SOUT_DSI3			0x5
#define DPI0_SEL_IN_RDMA1		0x1
#define DPI0_SEL_IN_RDMA2		0x3
#define DPI1_SEL_IN_RDMA1		(0x1 << 8)
#define DPI1_SEL_IN_RDMA2		(0x3 << 8)
#define DSI0_SEL_IN_RDMA0		0x1   // mt6890
#define DSI0_SEL_IN_RDMA1		0x1
#define DSI0_SEL_IN_RDMA2		0x4
#define DSI1_SEL_IN_RDMA1		0x1
#define DSI1_SEL_IN_RDMA2		0x4
#define DSI2_SEL_IN_RDMA1		(0x1 << 16)
#define DSI2_SEL_IN_RDMA2		(0x4 << 16)
#define DSI3_SEL_IN_RDMA1		(0x1 << 16)
#define DSI3_SEL_IN_RDMA2		(0x4 << 16)
#define DBI0_SEL_IN_RDMA0		0x1   // mt6890
#define COLOR1_SEL_IN_OVL1		0x1
#define WDMA0_SEL_IN_OD0		0x0
#define WDMA1_SEL_IN_OD1		0x0

#define OVL_MOUT_EN_RDMA		0x1   // mt6890
// #define OVL_MOUT_EN_RDMA		0x0   // mt6890

#define BLS_TO_DSI_RDMA1_TO_DPI1	0x8
#define DSI_SEL_IN_BLS			0x0

struct mtk_disp_mutex {
	int id;
	bool claimed;
};

struct mtk_ddp {
	struct device			*dev;
	struct clk			*clk;
	void __iomem			*regs;
	struct mtk_disp_mutex		mutex[10];
	const unsigned int		*mutex_mod;
};

static const unsigned int mt2701_mutex_mod[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_BLS] = MT2701_MUTEX_MOD_DISP_BLS,
	[DDP_COMPONENT_COLOR0] = MT2701_MUTEX_MOD_DISP_COLOR,
	[DDP_COMPONENT_OVL0] = MT2701_MUTEX_MOD_DISP_OVL,
	[DDP_COMPONENT_RDMA0] = MT2701_MUTEX_MOD_DISP_RDMA0,
	[DDP_COMPONENT_RDMA1] = MT2701_MUTEX_MOD_DISP_RDMA1,
	[DDP_COMPONENT_WDMA0] = MT2701_MUTEX_MOD_DISP_WDMA,
};

static const unsigned int mt2712_mutex_mod[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_AAL0] = MT2712_MUTEX_MOD_DISP_AAL0,
	[DDP_COMPONENT_AAL1] = MT2712_MUTEX_MOD2_DISP_AAL1,
	[DDP_COMPONENT_COLOR0] = MT2712_MUTEX_MOD_DISP_COLOR0,
	[DDP_COMPONENT_COLOR1] = MT2712_MUTEX_MOD_DISP_COLOR1,
	[DDP_COMPONENT_OD0] = MT2712_MUTEX_MOD_DISP_OD0,
	[DDP_COMPONENT_OD1] = MT2712_MUTEX_MOD2_DISP_OD1,
	[DDP_COMPONENT_OVL0] = MT2712_MUTEX_MOD_DISP_OVL0,
	[DDP_COMPONENT_OVL1] = MT2712_MUTEX_MOD_DISP_OVL1,
	[DDP_COMPONENT_PWM0] = MT2712_MUTEX_MOD_DISP_PWM0,
	[DDP_COMPONENT_PWM1] = MT2712_MUTEX_MOD_DISP_PWM1,
	[DDP_COMPONENT_PWM2] = MT2712_MUTEX_MOD_DISP_PWM2,
	[DDP_COMPONENT_RDMA0] = MT2712_MUTEX_MOD_DISP_RDMA0,
	[DDP_COMPONENT_RDMA1] = MT2712_MUTEX_MOD_DISP_RDMA1,
	[DDP_COMPONENT_RDMA2] = MT2712_MUTEX_MOD_DISP_RDMA2,
	[DDP_COMPONENT_UFOE] = MT2712_MUTEX_MOD_DISP_UFOE,
	[DDP_COMPONENT_WDMA0] = MT2712_MUTEX_MOD_DISP_WDMA0,
	[DDP_COMPONENT_WDMA1] = MT2712_MUTEX_MOD_DISP_WDMA1,
};

static const unsigned int mt8173_mutex_mod[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_AAL0] = MT8173_MUTEX_MOD_DISP_AAL,
	[DDP_COMPONENT_COLOR0] = MT8173_MUTEX_MOD_DISP_COLOR0,
	[DDP_COMPONENT_COLOR1] = MT8173_MUTEX_MOD_DISP_COLOR1,
	[DDP_COMPONENT_GAMMA] = MT8173_MUTEX_MOD_DISP_GAMMA,
	[DDP_COMPONENT_OD0] = MT8173_MUTEX_MOD_DISP_OD,
	[DDP_COMPONENT_OVL0] = MT8173_MUTEX_MOD_DISP_OVL0,
	[DDP_COMPONENT_OVL1] = MT8173_MUTEX_MOD_DISP_OVL1,
	[DDP_COMPONENT_PWM0] = MT8173_MUTEX_MOD_DISP_PWM0,
	[DDP_COMPONENT_PWM1] = MT8173_MUTEX_MOD_DISP_PWM1,
	[DDP_COMPONENT_RDMA0] = MT8173_MUTEX_MOD_DISP_RDMA0,
	[DDP_COMPONENT_RDMA1] = MT8173_MUTEX_MOD_DISP_RDMA1,
	[DDP_COMPONENT_RDMA2] = MT8173_MUTEX_MOD_DISP_RDMA2,
	[DDP_COMPONENT_UFOE] = MT8173_MUTEX_MOD_DISP_UFOE,
	[DDP_COMPONENT_WDMA0] = MT8173_MUTEX_MOD_DISP_WDMA0,
	[DDP_COMPONENT_WDMA1] = MT8173_MUTEX_MOD_DISP_WDMA1,
};

static const unsigned int mt6890_mutex_mod[DDP_COMPONENT_ID_MAX] = {
	[DDP_COMPONENT_AAL0] = MT6890_MUTEX_MOD_DISP_AAL0,
	[DDP_COMPONENT_COLOR0] = MT6890_MUTEX_MOD_DISP_COLOR0,
	[DDP_COMPONENT_GAMMA] = MT6890_MUTEX_MOD_DISP_GAMMA0,
	[DDP_COMPONENT_OVL0] = MT6890_MUTEX_MOD_DISP_OVL0,
	[DDP_COMPONENT_PWM0] = MT6890_MUTEX_MOD_DISP_PWM0,
	[DDP_COMPONENT_RDMA0] = MT6890_MUTEX_MOD_DISP_RDMA0,
	[DDP_COMPONENT_WDMA0] = MT6890_MUTEX_MOD_DISP_WDMA0,
	[DDP_COMPONENT_DSI0] = MT6890_MUTEX_MOD_DISP_DSI0,
	[DDP_COMPONENT_DBI0] = MT6890_MUTEX_MOD_DISP_DBI0,
};

static unsigned int mtk_ddp_mout_en(enum mtk_ddp_comp_id cur,
				    enum mtk_ddp_comp_id next,
				    unsigned int *addr)
{
	unsigned int value;

	if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_COLOR0) {
		*addr = DISP_REG_CONFIG_DISP_OVL0_MOUT_EN;
		value = OVL0_MOUT_EN_COLOR0;
	} else if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_RDMA0) {
		*addr = DISP_REG_CONFIG_DISP_OVL0_MOUT_EN;    // mt6890
		value = OVL_MOUT_EN_RDMA;
	} else if (cur == DDP_COMPONENT_OD0 && next == DDP_COMPONENT_RDMA0) {
		*addr = DISP_REG_CONFIG_DISP_OD_MOUT_EN;
		value = OD_MOUT_EN_RDMA0;
	} else if (cur == DDP_COMPONENT_UFOE && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DISP_UFOE_MOUT_EN;
		value = UFOE_MOUT_EN_DSI0;
	} else if (cur == DDP_COMPONENT_OVL1 && next == DDP_COMPONENT_COLOR1) {
		*addr = DISP_REG_CONFIG_DISP_OVL1_MOUT_EN;
		value = OVL1_MOUT_EN_COLOR1;
	} else if (cur == DDP_COMPONENT_GAMMA && next == DDP_COMPONENT_RDMA1) {
		*addr = DISP_REG_CONFIG_DISP_GAMMA_MOUT_EN;
		value = GAMMA_MOUT_EN_RDMA1;
	} else if (cur == DDP_COMPONENT_OD1 && next == DDP_COMPONENT_RDMA1) {
		*addr = DISP_REG_CONFIG_DISP_OD_MOUT_EN;
		value = OD1_MOUT_EN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DPI0;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DPI1) {
		*addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DPI1;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI1) {
		*addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DSI1;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI2) {
		*addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DSI2;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI3) {
		*addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		value = RDMA0_SOUT_DSI3;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI1) {
		*addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DSI1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI2) {
		*addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DSI2;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI3) {
		*addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DSI3;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DPI0;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI1) {
		*addr = DISP_REG_CONFIG_DISP_RDMA1_SOUT_EN;
		value = RDMA1_SOUT_DPI1;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DPI0;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DPI1) {
		*addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DPI1;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI1) {
		*addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DSI1;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI2) {
		*addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DSI2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI3) {
		*addr = DISP_REG_CONFIG_DISP_RDMA2_SOUT;
		value = RDMA2_SOUT_DSI3;
	} else if (cur == DDP_COMPONENT_OD0 && next == DDP_COMPONENT_WDMA0) {
		*addr = DISP_REG_CONFIG_DISP_OD_MOUT_EN;
		value = OD_MOUT_EN_WDMA0;
	} else if (cur == DDP_COMPONENT_OD1 && next == DDP_COMPONENT_WDMA1) {
		*addr = DISP_REG_CONFIG_DISP_OD_MOUT_EN;
		value = OD1_MOUT_EN_WDMA1;
	} else {
		value = 0;
	}

	return value;
}

static unsigned int mtk_ddp_sel_in(enum mtk_ddp_comp_id cur,
				   enum mtk_ddp_comp_id next,
				   unsigned int *addr)
{
	unsigned int value;

	if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_COLOR0) {
		*addr = DISP_REG_CONFIG_DISP_COLOR0_SEL_IN;
		value = COLOR0_SEL_IN_OVL0;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI0) {  // MT6890
		*addr = DISP_REG_CONFIG_DSI0_SEL_IN;
		value = DSI0_SEL_IN_RDMA0;
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DBI0) {  // MT6890
		*addr = DISP_REG_CONFIG_DBI0_SEL_IN;
		value = DBI0_SEL_IN_RDMA0;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DPI_SEL_IN;
		value = DPI0_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DPI1) {
		*addr = DISP_REG_CONFIG_DPI_SEL_IN;
		value = DPI1_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI0_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI1) {
		*addr = DISP_REG_CONFIG_DSIO_SEL_IN;
		value = DSI1_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI2) {
		*addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI2_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA1 && next == DDP_COMPONENT_DSI3) {
		*addr = DISP_REG_CONFIG_DSIO_SEL_IN;
		value = DSI3_SEL_IN_RDMA1;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DPI0) {
		*addr = DISP_REG_CONFIG_DPI_SEL_IN;
		value = DPI0_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DPI1) {
		*addr = DISP_REG_CONFIG_DPI_SEL_IN;
		value = DPI1_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI0_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI1) {
		*addr = DISP_REG_CONFIG_DSIO_SEL_IN;
		value = DSI1_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI2) {
		*addr = DISP_REG_CONFIG_DSIE_SEL_IN;
		value = DSI2_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_RDMA2 && next == DDP_COMPONENT_DSI3) {
		*addr = DISP_REG_CONFIG_DSIO_SEL_IN;
		value = DSI3_SEL_IN_RDMA2;
	} else if (cur == DDP_COMPONENT_OVL1 && next == DDP_COMPONENT_COLOR1) {
		*addr = DISP_REG_CONFIG_DISP_COLOR1_SEL_IN;
		value = COLOR1_SEL_IN_OVL1;
	} else if (cur == DDP_COMPONENT_BLS && next == DDP_COMPONENT_DSI0) {
		*addr = DISP_REG_CONFIG_DSI_SEL;
		value = DSI_SEL_IN_BLS;
	} else if (cur == DDP_COMPONENT_OD0 && next == DDP_COMPONENT_WDMA0) {
		*addr = DISP_REG_CONFIG_DISP_WDMA0_SEL_IN;
		value = WDMA0_SEL_IN_OD0;
	} else if (cur == DDP_COMPONENT_OD1 && next == DDP_COMPONENT_WDMA1) {
		*addr = DISP_REG_CONFIG_DISP_WDMA1_SEL_IN;
		value = WDMA1_SEL_IN_OD1;
	} else {
		value = 0;
	}

	return value;
}

static void mtk_ddp_sout_sel(void __iomem *config_regs,
			     enum mtk_ddp_comp_id cur,
			     enum mtk_ddp_comp_id next)
{
	if (cur == DDP_COMPONENT_BLS && next == DDP_COMPONENT_DSI0) {
		writel_relaxed(BLS_TO_DSI_RDMA1_TO_DPI1,
			       config_regs + DISP_REG_CONFIG_OUT_SEL);
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DSI0) {   // mt6890
		// *addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		printk(KERN_DEBUG "mtk_ddp_sout_sel, DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN\n");
		writel_relaxed(RDMA0_SOUT_DSI0,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN);
	} else if (cur == DDP_COMPONENT_RDMA0 && next == DDP_COMPONENT_DBI0) {   // mt6890
		// *addr = DISP_REG_CONFIG_DISP_RDMA0_SOUT_EN;
		printk(KERN_DEBUG "mtk_ddp_sout_sel, DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN\n");
		writel_relaxed(RDMA0_SOUT_DBI0,
			       config_regs + DISP_REG_CONFIG_DISP_RDMA0_SOUT_SEL_IN);
	}
}

void mtk_ddp_add_comp_to_path(void __iomem *config_regs,
			      enum mtk_ddp_comp_id cur,
			      enum mtk_ddp_comp_id next)
{
	unsigned int addr, value, reg;

	value = mtk_ddp_mout_en(cur, next, &addr);

	// if (value) {
	if (cur == DDP_COMPONENT_OVL0 && next == DDP_COMPONENT_RDMA0) {
		reg = readl_relaxed(config_regs + addr) | value;
		writel_relaxed(reg, config_regs + addr);
	}

	mtk_ddp_sout_sel(config_regs, cur, next);

	value = mtk_ddp_sel_in(cur, next, &addr);
	if (value) {
		reg = readl_relaxed(config_regs + addr) | value;
		writel_relaxed(reg, config_regs + addr);
	}
}

void mtk_ddp_remove_comp_from_path(void __iomem *config_regs,
				   enum mtk_ddp_comp_id cur,
				   enum mtk_ddp_comp_id next)
{
	unsigned int addr, value, reg;

	value = mtk_ddp_mout_en(cur, next, &addr);
	if (value) {
		reg = readl_relaxed(config_regs + addr) & ~value;
		writel_relaxed(reg, config_regs + addr);
	}

	value = mtk_ddp_sel_in(cur, next, &addr);
	if (value) {
		reg = readl_relaxed(config_regs + addr) & ~value;
		writel_relaxed(reg, config_regs + addr);
	}
}

struct mtk_disp_mutex *mtk_disp_mutex_get(struct device *dev, unsigned int id)
{
	struct mtk_ddp *ddp = dev_get_drvdata(dev);

	if (id >= 10)
		return ERR_PTR(-EINVAL);
	if (ddp->mutex[id].claimed)
		return ERR_PTR(-EBUSY);

	ddp->mutex[id].claimed = true;

	return &ddp->mutex[id];
}

void mtk_disp_mutex_put(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	mutex->claimed = false;
}

int mtk_disp_mutex_prepare(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	return clk_prepare_enable(ddp->clk);
}

void mtk_disp_mutex_unprepare(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	clk_disable_unprepare(ddp->clk);
}

void mtk_clean_mutex(struct mtk_disp_mutex *mutex) {
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[0]);

	unsigned int offset = DISP_REG_MUTEX_MOD(0);
	unsigned int reg = 0;

	printk(KERN_DEBUG "mtk_clean_mutex\n");

	writel_relaxed(reg, ddp->regs + offset);
}

void mtk_disp_mutex_add_comp(struct mtk_disp_mutex *mutex,
			     enum mtk_ddp_comp_id id)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	unsigned int reg;
	unsigned int offset;

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	switch (id) {
	case DDP_COMPONENT_DSI0:
		// reg = MUTEX_SOF_DSI0;
		reg = MT6890_MUTEX_SOF_DSI0 | MT6890_MUTEX_EOF_DSI0;
		writel_relaxed(reg, ddp->regs + DISP_REG_MUTEX_SOF(mutex->id));
		break;
	// case DDP_COMPONENT_DSI1:
	// 	reg = MUTEX_SOF_DSI0;
	// 	break;
	// case DDP_COMPONENT_DSI2:
	// 	reg = MUTEX_SOF_DSI2;
	// 	break;
	// case DDP_COMPONENT_DSI3:
	// 	reg = MUTEX_SOF_DSI3;
	// 	break;
	case DDP_COMPONENT_DBI0:
		// reg = MUTEX_SOF_DBPI0;
		reg = MT6890_MUTEX_SOF_DBI0 | MT6890_MUTEX_EOF_DBI0;
        writel_relaxed(reg, ddp->regs + DISP_REG_MUTEX_SOF(mutex->id));
		break;
	case DDP_COMPONENT_DPI0:
		reg = MUTEX_SOF_DBPI0;
		break;
	// case DDP_COMPONENT_DPI1:
	// 	reg = MUTEX_SOF_DPI1;
	// 	break;
	default:
		if (ddp->mutex_mod[id] < 32) {
			offset = DISP_REG_MUTEX_MOD(mutex->id);
			reg = readl_relaxed(ddp->regs + offset);
			reg |= 1 << ddp->mutex_mod[id];
			writel_relaxed(reg, ddp->regs + offset);
		} else {
			offset = DISP_REG_MUTEX_MOD2(mutex->id);
			reg = readl_relaxed(ddp->regs + offset);
			reg |= 1 << (ddp->mutex_mod[id] - 32);
			writel_relaxed(reg, ddp->regs + offset);
		}
		return;
	}

    if (id == DDP_COMPONENT_DSI0 || id == DDP_COMPONENT_DBI0) {
		if (ddp->mutex_mod[id] < 32) {
			offset = DISP_REG_MUTEX_MOD(mutex->id);
			reg = readl_relaxed(ddp->regs + offset);
			reg |= 1 << ddp->mutex_mod[id];
			writel_relaxed(reg, ddp->regs + offset);
		} else {
			offset = DISP_REG_MUTEX_MOD2(mutex->id);
			reg = readl_relaxed(ddp->regs + offset);
			reg |= 1 << (ddp->mutex_mod[id] - 32);
			writel_relaxed(reg, ddp->regs + offset);
		}
	}

	// writel_relaxed(reg, ddp->regs + DISP_REG_MUTEX_SOF(mutex->id));   // test
}

void mtk_disp_mutex_remove_comp(struct mtk_disp_mutex *mutex,
				enum mtk_ddp_comp_id id)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	unsigned int reg;
	unsigned int offset;

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	switch (id) {
	case DDP_COMPONENT_DSI0:
	case DDP_COMPONENT_DSI1:
	case DDP_COMPONENT_DSI2:
	case DDP_COMPONENT_DSI3:
	case DDP_COMPONENT_DPI0:
	case DDP_COMPONENT_DPI1:
    case DDP_COMPONENT_DBI0:
		writel_relaxed(MUTEX_SOF_SINGLE_MODE,
			       ddp->regs + DISP_REG_MUTEX_SOF(mutex->id));
		break;
	default:
		if (ddp->mutex_mod[id] < 32) {
			offset = DISP_REG_MUTEX_MOD(mutex->id);
			reg = readl_relaxed(ddp->regs + offset);
			reg &= ~(1 << ddp->mutex_mod[id]);
			writel_relaxed(reg, ddp->regs + offset);
		} else {
			offset = DISP_REG_MUTEX_MOD2(mutex->id);
			reg = readl_relaxed(ddp->regs + offset);
			reg &= ~(1 << (ddp->mutex_mod[id] - 32));
			writel_relaxed(reg, ddp->regs + offset);
		}
		break;
	}
}

// mt6890
void mtk_disp_mutex_inten_enable(struct mtk_disp_mutex *mutex)
{
  	struct mtk_ddp *ddp =
  		container_of(mutex, struct mtk_ddp, mutex[mutex->id]);
  	unsigned int val;

  	val = readl_relaxed(ddp->regs + DISP_REG_MUTEX_INTEN);
  	val |= (0x1 << (unsigned int)mutex->id);
  	val |= (0x1 << (unsigned int)(mutex->id + DISP_MUTEX_TOTAL));
  	writel_relaxed(val, ddp->regs + DISP_REG_MUTEX_INTEN);
}

void mtk_disp_mutex_enable(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	writel(1, ddp->regs + DISP_REG_MUTEX_EN(mutex->id));

	// mt6890
	mtk_disp_mutex_inten_enable(mutex);
}


void mtk_disp_mutex_disable(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);

	WARN_ON(&ddp->mutex[mutex->id] != mutex);

	writel(0, ddp->regs + DISP_REG_MUTEX_EN(mutex->id));
}

void mtk_disp_mutex_acquire(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);
	u32 tmp;

	writel(1, ddp->regs + DISP_REG_MUTEX_EN(mutex->id));

	writel(1, ddp->regs + DISP_REG_MUTEX(mutex->id));
	if (readl_poll_timeout_atomic(ddp->regs + DISP_REG_MUTEX(mutex->id),
				      tmp, tmp & INT_MUTEX, 1, 10000))
		pr_err("could not acquire mutex %d\n", mutex->id);
}

void mtk_disp_mutex_release(struct mtk_disp_mutex *mutex)
{
	struct mtk_ddp *ddp = container_of(mutex, struct mtk_ddp,
					   mutex[mutex->id]);

	writel(0, ddp->regs + DISP_REG_MUTEX(mutex->id));
}

// static irqreturn_t mtk_disp_mutex_irq_handler(int irq, void *dev_id)
// {
//   	struct mtk_ddp *ddp = dev_id;
//   	unsigned int val = 0;
//   	unsigned int m_id = 0;
//   	int ret = 0;

//   	if (mtk_drm_top_clk_isr_get("mutex_irq") == false) {
//   		DDPIRQ("%s, top clk off\n", __func__);
//   		return IRQ_NONE;
//   	}

//   	val = readl(ddp->regs + DISP_REG_MUTEX_INTSTA) & DISP_MUTEX_INT_MSK;
//   	if (!val) {
//   		ret = IRQ_NONE;
//   		goto out;
//   	}

//   	DRM_MMP_MARK(IRQ, irq, val);

//   	DDPIRQ("MM_MUTEX irq, val:0x%x\n", val);

//  	writel(~val, ddp->regs + DISP_REG_MUTEX_INTSTA);

//  	for (m_id = 0; m_id < DISP_MUTEX_DDP_COUNT; m_id++) {
//   		if (val & (0x1 << m_id)) {
//   			DDPIRQ("[IRQ] mutex%d sof!\n", m_id);
//   			DRM_MMP_MARK(mutex[m_id], val, 0);
//   			disp_aal_on_start_of_frame();
//   		}
//   		if (val & (0x1 << (m_id + DISP_MUTEX_TOTAL))) {
//   			DDPIRQ("[IRQ] mutex%d eof!\n", m_id);
//   			DRM_MMP_MARK(mutex[m_id], val, 1);
//   		}
//   	}
//   	ret = IRQ_HANDLED;

//   out:
//   	mtk_drm_top_clk_isr_put("mutex_irq");

//   	return ret;
// }

static int mtk_ddp_probe(struct platform_device *pdev)
{

	struct device *dev = &pdev->dev;
	struct mtk_ddp *ddp;
	struct resource *regs;
	int i;

	dev_info(dev, "mtk_ddp_probe start\n");

	ddp = devm_kzalloc(dev, sizeof(*ddp), GFP_KERNEL);
	if (!ddp)
		return -ENOMEM;

	for (i = 0; i < 10; i++)
		ddp->mutex[i].id = i;

	ddp->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ddp->clk)) {
		dev_err(dev, "Failed to get clock\n");
		return PTR_ERR(ddp->clk);
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ddp->regs = devm_ioremap_resource(dev, regs);
	if (IS_ERR(ddp->regs)) {
		dev_err(dev, "Failed to map mutex registers\n");
		return PTR_ERR(ddp->regs);
	}

	ddp->mutex_mod = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, ddp);
	dev_info(dev, "mtk_ddp_probe end\n");
	return 0;
}

static int mtk_ddp_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id ddp_driver_dt_match[] = {
	{ .compatible = "mediatek,mt2701-disp-mutex", .data = mt2701_mutex_mod},
	{ .compatible = "mediatek,mt2712-disp-mutex", .data = mt2712_mutex_mod},
	{ .compatible = "mediatek,mt8173-disp-mutex", .data = mt8173_mutex_mod},
	{ .compatible = "mediatek,disp_mutex0", .data = mt6890_mutex_mod},
	{},
};
MODULE_DEVICE_TABLE(of, ddp_driver_dt_match);

struct platform_driver mtk_ddp_driver = {
	.probe		= mtk_ddp_probe,
	.remove		= mtk_ddp_remove,
	.driver		= {
		.name	= "mediatek-ddp",
		.owner	= THIS_MODULE,
		.of_match_table = ddp_driver_dt_match,
	},
};
