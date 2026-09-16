/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yuesheng Wang <yuesheng.wang@mediatek.com>
 */

#ifndef MTK_DISP_AAL_H
#define MTK_DISP_AAL_H

#include <linux/io.h>
#include <linux/uaccess.h>

/* aal macros */
#define DISP_AAL_EN			(0x000)
#define DISP_AAL_INTEN      (0x008)
#define DISP_AAL_INTSTA     (0x00c)
#define DISP_AAL_STATUS     (0x010)
#define DISP_AAL_CFG        (0x020)

#define DISP_AAL_SIZE       (0x030)
#define DISP_AAL_CFG_MAIN   (0x200)
#define DISP_AAL_CABC_00    (0x20c)
#define DISP_AAL_CABC_02    (0x214)
#define DISP_AAL_STATUS_00  (0x224)
/* 00 ~ 32: max histogram */
#define DISP_AAL_STATUS_32  (0x2a4)

/* bit 8: dre_gain_force_en */
#define DISP_AAL_DRE_GAIN_FILTER_00		(0x354)
#define DISP_AAL_DRE_FLT_FORCE(idx)     (0x358 + (idx) * 4)
#define DISP_AAL_DRE_CRV_CAL_00         (0x344)
#define DISP_AAL_DRE_MAPPING_00         (0x3b0)
#define DISP_AAL_CABC_GAINLMT_TBL(idx)  (0x40c + (idx) * 4)

#define AAL_OFFSET			0x1000

#define AAL_HIST_BIN        33
#define AAL_DRE_POINT_NUM   29

#define FUNC() pr_info("%s\n", __func__)

#define	AAL_FUNC_ALL_BYPASS 0x0U
#define AAL_FUNC_CABC 0x2U
#define	AAL_FUNC_DRE 0x4U
#define	AAL_FUNC_CABC_DRE 0x6U

struct AalInfo {
	unsigned int need_update;
	unsigned int has_hist;
	unsigned int max_hist[33];
	void __iomem *base_addr;
	unsigned int irq;
	unsigned int function;
	unsigned int cabc_gainlmt[33];
	unsigned int cabc_fltgain_force;
	unsigned int dre_gain_flt_status[29];
	unsigned int colorHist;
};

void __iomem *get_aal_base_addr_by_index(int aal_idx);

irqreturn_t mtk_disp_aal_irq_handler(int irq, void *dev_id);
void mtk_aal_update_max_hist(int aal_idx);
void update_color_hist(int aal_idx);
void mtk_aal_mask(void __iomem *base_addr, u32 offset, u32 mask, u32 data);

void mtk_aal_set_event(int aal_idx, int event);
void mtk_aal_function(int aal_idx);
void mtk_aal_set_cabc_gain_lmt(int aal_idx);
void mtk_aal_set_dre_gain_flt_status(int aal_idx);
void mtk_aal_set_cabc_fltgain_force(int aal_idx);
unsigned int mtk_get_color_hist(int aal_idx);
int mtk_aal_get_max_hist(int aal_idx, __user void *arg);
void mtk_aal_request_partial(int aal_idx);
unsigned int mtk_aal_get_width(int aal_idx);
unsigned int mtk_aal_get_height(int aal_idx);
int mtk_aal_get_resolution(int aal_idx, __user void *arg);
int mtk_aal_eventctl(int aal_idx, __user void *arg);
int mtk_aal_set_param(int aal_idx, __user void *arg);
int mtk_aal_init_reg(int aal_idx, __user void *arg);
int mtk_aal_request_irq(int aal_idx);
int mtk_aal_config2(int aal_idx);
void mtk_aal_config(int aal_idx);

/*
 * AAL ioctl and ioctl Parameters define
 */
#define AAL_HIST_BIN        33	/* [0..32] */
#define AAL_DRE_POINT_NUM   29

#define AAL_SERVICE_FORCE_UPDATE 0x1

struct _disp_aal_initreg {
	/* DRE */
	unsigned int dre_map_bypass;
	/* CABC */
	unsigned int cabc_gainlmt[33];
};

#define DISP_AAL_INITREG struct _disp_aal_initreg

#define TUNING_FLAG_WRITE 0x1U
#define TUNING_FLAG_READ 0x2U

struct _disp_aal_hist {
	unsigned int serviceFlags;
	unsigned int backlight;
	unsigned int colorHist;
	unsigned int maxHist[AAL_HIST_BIN];
	unsigned int requestPartial;
	unsigned int tuningFlag;
};

#define DISP_AAL_HIST struct _disp_aal_hist

enum DISP_AAL_REFRESH_LATENCY {
	AAL_REFRESH_17MS = 17,
	AAL_REFRESH_33MS = 33
};

struct _disp_aal_param {
	unsigned int DREGainFltStatus[AAL_DRE_POINT_NUM];
	unsigned int cabc_fltgain_force;	/* 10-bit ; [0,1023] */
	unsigned int cabc_gainlmt[33];
	unsigned int FinalBacklight;	/* 10-bit ; [0,1023] */
	unsigned int allowPartial;
	unsigned int refreshLatency;	/* DISP_AAL_REFRESH_LATENCY */
};
#define DISP_AAL_PARAM struct _disp_aal_param

#include <linux/ioctl.h>

#define DISP_IOCTL_MAGIC 120U /* 'x' */

/* Add for AAL control - S */
#define DISP_IOCTL_AAL_GET_RESOLUTION _IOW(DISP_IOCTL_MAGIC, 14U, unsigned int)
/* 0 : disable AAL event, 1 : enable AAL event */
#define DISP_IOCTL_AAL_EVENTCTL    _IOW(DISP_IOCTL_MAGIC, 15U, int)
/* Get AAL statistics data. */
#define DISP_IOCTL_AAL_GET_HIST    _IOR(DISP_IOCTL_MAGIC, 16U, DISP_AAL_HIST)
/* Update AAL setting */
#define DISP_IOCTL_AAL_SET_PARAM   _IOW(DISP_IOCTL_MAGIC, 17U, DISP_AAL_PARAM)
#define DISP_IOCTL_AAL_INIT_REG    _IOW(DISP_IOCTL_MAGIC, 18U, DISP_AAL_INITREG)
/* AAL tuning */
#define DISP_IOCTL_AAL_GET_TUNING_PARAM \
		_IOW(DISP_IOCTL_MAGIC, 19U, struct AALReg)
#define DISP_IOCTL_AAL_SET_TUNING_PARAM \
		_IOW(DISP_IOCTL_MAGIC, 20U, struct AALReg)

#define ALI_LEVEL 18
// AAL FW registers
struct AALReg {
	// MaxHistogram
	int maxinfo_cnt_th;
	int maxinfo_type;

	// LABC
	float PQIdx;

	int labc_en;
	int ALI2BLI[ALI_LEVEL];
	int NewALI2BLI[ALI_LEVEL]; //Changing according to userBrightnessLevel

	int labc_stp_cntrl_bypass;
	float labc_stp_up;
	float labc_stp_dwn;

	float labc_on_stp_up;
	float labc_on_stp_dwn;

	float labc_off_stp_up;
	float labc_off_stp_dwn;

	int OpLvlUpperMaxRatio;
	int OpLvlLowerMaxRatio;
	int OpLvlMinBLI;

	int InitBLI; // Initial backlight for phone booting

	// Ambient light status
	int ali_force_en;
	int ali_force_value;
	int ali_status;

	// DRE
	int dre_curve_en;
	int ImgArea;
	int DarkRatio;
	int dre_dark_bin;
	int ALI2GainIdx[ALI_LEVEL];

	int dre_ali_gain_idx_en;

	int dre_bli_gain_idx_en;
	int dre_bli_cnstnt_brght_val;
	int dre_bli_max_gain_idx;

	int dre_cg_prtct_en;
	int dre_nonzero_th;
	int dre_nonzerobin_count_th;

	int dre_gain_flt_en;
	int dre_ali_flt_up_coef;
	int dre_ali_flt_dn_coef;
	int dre_bli_flt_up_coef;
	int dre_bli_flt_dn_coef;
	int dre_dark_strngth1_flt_up_coef;
	int dre_dark_strngth1_flt_dn_coef;
	int dre_dark_strngth2_flt_up_coef;
	int dre_dark_strngth2_flt_dn_coef;
	int dre_ali_prtct_val;
	int dre_map_bypass; // For MaxInfo mapping

	// CABC
	int cabc_en;
	int cabc_max_bl;
	int cabc_min_bl;
	int cabc_maxinfo_th;
	int cabc_bl_up_step;
	int cabc_bl_dn_step;

	int cabc_final_fltup_coef;
	int cabc_flt_up_coef;
	int cabc_flt_up_deadzone;
	int cabc_final_fltdn_coef;
	int cabc_flt_dn_coef_H;
	int cabc_flt_dn_coef_L;

	int cabc_delay_type;
	int cabc_delay_num;
	int PxlGainTbl[33];
	int BLTbl[33];
	int DRatio_th;

	int cabc_ps_th;
	int min_outbl;
	int cabc_ps_speed;
	int cabc_sb_strenth;
	int cabc_sb_range;
	int supdim_cabc_minbl;
	float supdim_gamma;

	// DRE2.2
	int dre_alg_mode;

	// Histogram FIR Weight
	uint32_t bADLWeight1;
	uint32_t bADLWeight2;
	uint32_t bADLWeight3;

	// BS Basic Param
	uint32_t bBSDCGain;
	uint32_t bBSACGain;
	uint32_t bBSLevel;

	// Mid Param
	uint32_t bMIDDCGain;
	uint32_t bMIDACGain;

	// WS Basic Param
	uint32_t bWSDCGain;
	uint32_t bWSACGain;
	uint32_t bWSLevel;

	// Spike Fallback Protection
	uint32_t dre_dync_spike_wgt_min;
	uint32_t dre_dync_spike_wgt_max;
	uint32_t dre_dync_spike_th;
	uint32_t dre_dync_spike_slope;
	uint32_t bSpikeBlendmethod;
	uint32_t bIIRStrengthSpike;

	// Skin processing
	uint32_t bSkinWgtSlope;
	uint32_t bSkinBlendmethod;
	uint32_t bIIRStrengthSkin;

	int dre_dync_flt_coef_min;
	int dre_dync_flt_coef_max;
	int dre_dync_flt_ovr_pxl_th;
	int dre_dync_flt_ovr_pxl_slope;

	// CABC2.2
	int cabc_alg_mode;          // 1: CABC 2.1, 2: CABC 2.2
	int cabcTh_lumaHigh;        // 0 to 127 (128 to 255)
	int cabcTh_lumaLow;         // 0 to 127
	int cabcGain_lumaHigh_Diff; // 0 to 15,  max_top_gain = 1 - 4/100 = 0.96
	int cabcGain_lumaLow_Diff;  // 0 to 15, min_top_gain=1-(4+6)/100=0.90
	int cabcGain_Range;         // 0 to 31, gain_range = 20/100 = 0.20
	int cabcProb_Skin;          // 0 to 127, ex: 32; 32 / 127 = 0.25
	int cabcProb_Flat;          // 0 to 511, ex: 281; 0.489 + 0.281 = 0.770
	int cabcCnt_ZeroBin;        // 0 to 15, ex: 2; 16 + 2 = 18

	// Partial Update
	int prtl_updt_en;
	int prtl_updt_force_en;
	int full_frm_ratio;
	int lrg_prtl_frm_ratio;
	int prtl_updt_ink_en;
	int prtl_updt_ink_value;

	int dre_iir_force_range;
	// Ultra Dimming
	int ud_BL10bThH;
	int ud_BL10bThM;
	int ud_BL10bWhL;
	int ud_PL08bThL;
};


#define AAL_SW_REG_BASE		0x0UL
#define AAL0_SW_REG_BASE	0x0UL
#define AAL0_SW_REG_END		0x10000UL
#define AAL1_SW_REG_BASE	0x10000UL
#define AAL1_SW_REG_END		0x20000UL
#define AAL_SW_REG_END		0x20000UL

int mtk_aal_write_reg(unsigned long addr, int value);
int mtk_aal_read_reg(unsigned long addr, int *value);
int mtk_aal_set_tuning_parameters(int aal_idx, __user void *arg);
int mtk_aal_get_tuning_parameters(int aal_idx, __user void *arg);


#endif
