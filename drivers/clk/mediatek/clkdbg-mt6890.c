// SPDX-License-Identifier: GPL-2.0

/*

 * Copyright (c) 2019 MediaTek Inc.

 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>

#ifdef CONFIG_MTK_DEVAPC
#include <linux/soc/mediatek/devapc_public.h>
#endif
#include "clk-mux.h"
#include "clkdbg.h"
#include "clkdbg-mt6890.h"
#include "clk-mt6890-fmeter.h"

#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg) { .phys = _phys,	\
		.name = #_id_name, .pg = _pg}

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)
/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[top] = REGBASE_V(0x10000000, top, NULL),
	[dbgsys_dem] = REGBASE_V(0x0d0a0000, dbgsys_dem, NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, NULL),
	[infracfg_ao_bus] = REGBASE_V(0x10001000, infracfg_ao_bus, NULL),
	[peri] = REGBASE_V(0x10003000, peri, NULL),
	[spm] = REGBASE_V(0x10006000, spm, NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, NULL),
	[gce] = REGBASE_V(0x10228000, gce, NULL),
	[audsys] = REGBASE_V(0x11210000, audsys, "MT6890_POWER_DOMAIN_AUDIO"),
	[impe] = REGBASE_V(0x11c46000, impe, NULL),
	[mfgcfg] = REGBASE_V(0x13fbf000, mfgcfg, "MT6890_POWER_DOMAIN_MFG0"),
	[mm] = REGBASE_V(0x14000000, mm, "MT6890_POWER_DOMAIN_DIS"),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top,  0x0010, CLK_CFG_0),
	REGNAME(top,  0x0020, CLK_CFG_1),
	REGNAME(top,  0x0030, CLK_CFG_2),
	REGNAME(top,  0x0040, CLK_CFG_3),
	REGNAME(top,  0x0050, CLK_CFG_4),
	REGNAME(top,  0x0060, CLK_CFG_5),
	REGNAME(top,  0x0070, CLK_CFG_6),
	REGNAME(top,  0x0080, CLK_CFG_7),
	REGNAME(top,  0x0090, CLK_CFG_8),
	REGNAME(top,  0x00A0, CLK_CFG_9),
	REGNAME(top,  0x00B0, CLK_CFG_10),
	REGNAME(top,  0x00C0, CLK_CFG_11),
	REGNAME(top,  0x00D0, CLK_CFG_12),
	REGNAME(top,  0x0320, CLK_AUDDIV_0),
	REGNAME(top,  0x0328, CLK_AUDDIV_2),
	REGNAME(top,  0x0334, CLK_AUDDIV_3),
	/* DBGSYS_DEM register */
	REGNAME(dbgsys_dem,  0x70, ATB),
	REGNAME(dbgsys_dem,  0x2c, DBGBUSCLK_EN),
	REGNAME(dbgsys_dem,  0x30, DBGSYSCLK_EN),
	/* INFRACFG_AO register */
	REGNAME(ifrao,  0x70, INFRA_BUS_DCM_CTRL),
	REGNAME(ifrao,  0x90, MODULE_SW_CG_0),
	REGNAME(ifrao,  0x94, MODULE_SW_CG_1),
	REGNAME(ifrao,  0xac, MODULE_SW_CG_2),
	REGNAME(ifrao,  0xc8, MODULE_SW_CG_3),
	REGNAME(ifrao,  0xe8, MODULE_SW_CG_4),
	REGNAME(ifrao,  0x74, PERI_BUS_DCM_CTRL),
	/* INFRACFG_AO_BUS register */
	REGNAME(infracfg_ao_bus,  0x0710, INFRA_TOPAXI_PROTECTEN_2),
	REGNAME(infracfg_ao_bus,  0x0720, INFRA_TOPAXI_PROTECTEN_STA0_2),
	REGNAME(infracfg_ao_bus,  0x0724, INFRA_TOPAXI_PROTECTEN_STA1_2),
	REGNAME(infracfg_ao_bus,  0x0220, INFRA_TOPAXI_PROTECTEN),
	REGNAME(infracfg_ao_bus,  0x0224, INFRA_TOPAXI_PROTECTEN_STA0),
	REGNAME(infracfg_ao_bus,  0x0228, INFRA_TOPAXI_PROTECTEN_STA1),
	REGNAME(infracfg_ao_bus,  0x0250, INFRA_TOPAXI_PROTECTEN_1),
	REGNAME(infracfg_ao_bus,  0x0254, INFRA_TOPAXI_PROTECTEN_STA0_1),
	REGNAME(infracfg_ao_bus,  0x0258, INFRA_TOPAXI_PROTECTEN_STA1_1),
	REGNAME(infracfg_ao_bus,  0x0B80, INFRA_TOPAXI_PROTECTEN_INFRA_VDNR),
	REGNAME(infracfg_ao_bus,  0x0B8c, INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_STA0),
	REGNAME(infracfg_ao_bus,  0x0B90, INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_STA1),
	REGNAME(infracfg_ao_bus,  0x0BA0, INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_1),
	REGNAME(infracfg_ao_bus,  0x0BAc, INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_1_STA0),
	REGNAME(infracfg_ao_bus,  0x0BB0, INFRA_TOPAXI_PROTECTEN_INFRA_VDNR_1_STA1),
	REGNAME(infracfg_ao_bus,  0x0BB4, INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR),
	REGNAME(infracfg_ao_bus,  0x0BC0, INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_STA0),
	REGNAME(infracfg_ao_bus,  0x0BC4, INFRA_TOPAXI_PROTECTEN_SUB_INFRA_VDNR_STA1),
	REGNAME(infracfg_ao_bus,  0x02D0, INFRA_TOPAXI_PROTECTEN_MM),
	REGNAME(infracfg_ao_bus,  0x02E8, INFRA_TOPAXI_PROTECTEN_MM_STA0),
	REGNAME(infracfg_ao_bus,  0x02EC, INFRA_TOPAXI_PROTECTEN_MM_STA1),
	/* PERICFG register */
	REGNAME(peri,  0x20c, PERIAXI_SI0_CTL),
	/* SPM register */
	REGNAME(spm,  0x308, MFG0_PWR_CON),
	REGNAME(spm,  0x324, IFR_PWR_CON),
	REGNAME(spm,  0x32C, DPY_PWR_CON),
	REGNAME(spm,  0x330, PEXTP_D_2LX1_PWR_CON),
	REGNAME(spm,  0x334, PEXTP_R_2LX1_PWR_CON),
	REGNAME(spm,  0x338, PEXTP_R_1LX2_PWR_CON),
	REGNAME(spm,  0x33C, ETH_PWR_CON),
	REGNAME(spm,  0x34C, NETSYS_PWR_CON),
	REGNAME(spm,  0x350, DIS_PWR_CON),
	REGNAME(spm,  0x354, AUDIO_PWR_CON),
	REGNAME(spm,  0x300, MD1_PWR_CON),
	REGNAME(spm,  0x328, EIP97_PWR_CON),
	REGNAME(spm,  0x304, CONN_PWR_CON),
	REGNAME(spm,  0x3C4, DPY2_PWR_CON),
	REGNAME(spm,  0x3C0, MCUPM_PWR_CON),
	REGNAME(spm,  0x3A4, MSDC_PWR_CON),
	REGNAME(spm,  0x3D0, PERI_PWR_CON),
	REGNAME(spm,  0x344, HSMTOP_PWR_CON),
	REGNAME(spm,  0x340, SSUSB_PWR_CON),
	REGNAME(spm,  0x31C, SSUSB_PHY_PWR_CON),
	REGNAME(spm,  0x358, SGMII_0_PHY_PWR_CON),
	REGNAME(spm,  0x360, SGMII_0_TOP_PWR_CON),
	REGNAME(spm,  0x35C, SGMII_1_PHY_PWR_CON),
	REGNAME(spm,  0x364, SGMII_1_TOP_PWR_CON),
	REGNAME(spm,  0x30C, PEXTP_D_2LX1_PHY_PWR_CON),
	REGNAME(spm,  0x310, PEXTP_R_2LX1_PHY_PWR_CON),
	REGNAME(spm,  0x314, PEXTP_R_1LX2_0P_PHY_PWR_CON),
	REGNAME(spm,  0x318, PEXTP_R_1LX2_1P_PHY_PWR_CON),
	REGNAME(spm,  0x368, DRAMC_MD32_PWR_CON),
	/* APMIXEDSYS register */
	REGNAME(apmixed,  0x204, ARMPLL_LL_CON0),
	REGNAME(apmixed,  0x208, ARMPLL_LL_CON1),
	REGNAME(apmixed,  0x20c, ARMPLL_LL_CON2),
	REGNAME(apmixed,  0x210, ARMPLL_LL_CON3),
	REGNAME(apmixed,  0x214, ARMPLL_LL_CON4),
	REGNAME(apmixed,  0x218, CCIPLL_CON0),
	REGNAME(apmixed,  0x21c, CCIPLL_CON1),
	REGNAME(apmixed,  0x220, CCIPLL_CON2),
	REGNAME(apmixed,  0x224, CCIPLL_CON3),
	REGNAME(apmixed,  0x228, CCIPLL_CON4),
	REGNAME(apmixed,  0x604, MPLL_CON0),
	REGNAME(apmixed,  0x608, MPLL_CON1),
	REGNAME(apmixed,  0x60c, MPLL_CON2),
	REGNAME(apmixed,  0x610, MPLL_CON3),
	REGNAME(apmixed,  0x614, MPLL_CON4),
	REGNAME(apmixed,  0x404, MAINPLL_CON0),
	REGNAME(apmixed,  0x408, MAINPLL_CON1),
	REGNAME(apmixed,  0x40c, MAINPLL_CON2),
	REGNAME(apmixed,  0x410, MAINPLL_CON3),
	REGNAME(apmixed,  0x414, MAINPLL_CON4),
	REGNAME(apmixed,  0x418, UNIVPLL_CON0),
	REGNAME(apmixed,  0x41c, UNIVPLL_CON1),
	REGNAME(apmixed,  0x420, UNIVPLL_CON2),
	REGNAME(apmixed,  0x424, UNIVPLL_CON3),
	REGNAME(apmixed,  0x428, UNIVPLL_CON4),
	REGNAME(apmixed,  0x22c, MSDCPLL_CON0),
	REGNAME(apmixed,  0x230, MSDCPLL_CON1),
	REGNAME(apmixed,  0x234, MSDCPLL_CON2),
	REGNAME(apmixed,  0x238, MSDCPLL_CON3),
	REGNAME(apmixed,  0x23c, MSDCPLL_CON4),
	REGNAME(apmixed,  0x42c, MMPLL_CON0),
	REGNAME(apmixed,  0x430, MMPLL_CON1),
	REGNAME(apmixed,  0x434, MMPLL_CON2),
	REGNAME(apmixed,  0x438, MMPLL_CON3),
	REGNAME(apmixed,  0x43c, MMPLL_CON4),
	REGNAME(apmixed,  0x618, MFGPLL_CON0),
	REGNAME(apmixed,  0x61c, MFGPLL_CON1),
	REGNAME(apmixed,  0x620, MFGPLL_CON2),
	REGNAME(apmixed,  0x624, MFGPLL_CON3),
	REGNAME(apmixed,  0x628, MFGPLL_CON4),
	REGNAME(apmixed,  0x454, APLL1_CON0),
	REGNAME(apmixed,  0x458, APLL1_CON1),
	REGNAME(apmixed,  0x45c, APLL1_CON2),
	REGNAME(apmixed,  0x460, APLL1_CON3),
	REGNAME(apmixed,  0x464, APLL1_CON4),
	REGNAME(apmixed,  0x468, APLL1_CON5),
	REGNAME(apmixed,  0x46c, APLL2_CON0),
	REGNAME(apmixed,  0x470, APLL2_CON1),
	REGNAME(apmixed,  0x474, APLL2_CON2),
	REGNAME(apmixed,  0x478, APLL2_CON3),
	REGNAME(apmixed,  0x47c, APLL2_CON4),
	REGNAME(apmixed,  0x480, APLL2_CON5),
	REGNAME(apmixed,  0x804, NET1PLL_CON0),
	REGNAME(apmixed,  0x808, NET1PLL_CON1),
	REGNAME(apmixed,  0x80c, NET1PLL_CON2),
	REGNAME(apmixed,  0x810, NET1PLL_CON3),
	REGNAME(apmixed,  0x814, NET1PLL_CON4),
	REGNAME(apmixed,  0x818, NET2PLL_CON0),
	REGNAME(apmixed,  0x81c, NET2PLL_CON1),
	REGNAME(apmixed,  0x820, NET2PLL_CON2),
	REGNAME(apmixed,  0x824, NET2PLL_CON3),
	REGNAME(apmixed,  0x828, NET2PLL_CON4),
	REGNAME(apmixed,  0x82c, WEDMCUPLL_CON0),
	REGNAME(apmixed,  0x830, WEDMCUPLL_CON1),
	REGNAME(apmixed,  0x834, WEDMCUPLL_CON2),
	REGNAME(apmixed,  0x838, WEDMCUPLL_CON3),
	REGNAME(apmixed,  0x83c, WEDMCUPLL_CON4),
	REGNAME(apmixed,  0x840, MEDMCUPLL_CON0),
	REGNAME(apmixed,  0x844, MEDMCUPLL_CON1),
	REGNAME(apmixed,  0x848, MEDMCUPLL_CON2),
	REGNAME(apmixed,  0x84c, MEDMCUPLL_CON3),
	REGNAME(apmixed,  0x850, MEDMCUPLL_CON4),
	REGNAME(apmixed,  0x240, SGMIIPLL_CON0),
	REGNAME(apmixed,  0x244, SGMIIPLL_CON1),
	REGNAME(apmixed,  0x248, SGMIIPLL_CON2),
	REGNAME(apmixed,  0x24c, SGMIIPLL_CON3),
	REGNAME(apmixed,  0x250, SGMIIPLL_CON4),
	/* GCE register */
	REGNAME(gce,  0xf0, GCE_CTL_INT0),
	/* AUDIO register */
	REGNAME(audsys,  0x0, AUDIO_TOP_0),
	REGNAME(audsys,  0x4, AUDIO_TOP_1),
	REGNAME(audsys,  0x8, AUDIO_TOP_2),
	/* IMP_IIC_WRAP_E register */
	REGNAME(impe,  0xe00, AP_CLOCK_CG_RO_EST),
	/* MFGCFG register */
	REGNAME(mfgcfg,  0x0, MFG_CG),
	/* MMSYS CONFIG register */
	REGNAME(mm,  0x100, MMSYS_CG_CON0),
	REGNAME(mm,  0x110, MMSYS_CG_CON1),
	REGNAME(mm,  0x120, MMSYS_CG_CON2),
	{},
};

/*
 * clkdbg vf table
 */

struct mtk_vf {
	const char *name;
	int freq_table[4];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3},	\
	}

/*
 * Opp0 : 0p75v
 * Opp1 : 0p65v
 * Opp2 : 0p60v
 * Opp3 : 0p55v
 *//*
static struct mtk_vf vf_table[] = {
	// Opp0, Opp1, Opp2, Opp3
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 136500),
	MTK_VF_TABLE("spm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("bus_aximem_sel", 218400, 156000,156000, 156000),
	MTK_VF_TABLE("mm_sel", 208000, 178285, 178285, 178285),
	MTK_VF_TABLE("mfg_ref_sel", 416000, 416000, 218400, 218400),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("msdc50_0_hclk_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("msdc30_1_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("aud_engen1_sel", 22579, 22579, 22579, 22579),
	MTK_VF_TABLE("aud_engen2_sel", 24576, 24576,24576, 24576),
	MTK_VF_TABLE("aud_1_sel", 180633, 180633, 180633, 180633),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("pwrap_ulposc_sel", 65000, 65000, 65000, 65000),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("pwrmcu_sel", 364000, 312000, 312000, 273000),
	MTK_VF_TABLE("dbi_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_top_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("tl_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("dpmaif_main_sel", 364000, 364000, 364000, 273000),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("spmi_m_mst_sel", 32500, 32500, 32500, 32500),
	MTK_VF_TABLE("spmi_p_mst_sel", 32500, 32500, 32500, 32500),
	MTK_VF_TABLE("dvfsrc_sel", 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("mcupm_sel", 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("sflash_sel", 62400, 62400, 62400, 62400),
	MTK_VF_TABLE("gcpu_sel", 416000, 364000, 364000, 273000),
	MTK_VF_TABLE("spi_sel", 208000, 208000, 208000, 178285),
	MTK_VF_TABLE("spis_sel", 416000, 416000, 312000, 104000),
	MTK_VF_TABLE("ecc_sel", 312000, 242666, 242666, 136500),
	MTK_VF_TABLE("nfi1x_sel", 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("spinfi_bclk_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("netsys_sel", 78000, 356571, 242666, 156000),
	MTK_VF_TABLE("medsys_sel", 78000, 356571, 242666, 156000),
	MTK_VF_TABLE("hsm_crypto_sel", 312000, 312000, 26000, 26000),
	MTK_VF_TABLE("hsm_arc_sel", 26000, 26000, 182000, 182000),
	MTK_VF_TABLE("eip97_sel", 800000, 546000, 364000, 218400),
	MTK_VF_TABLE("snps_eth_312p5m_sel", 312500, 312500, 312500, 312500),
	MTK_VF_TABLE("snps_eth_250m_sel", 250000, 250000, 250000, 250000),
	MTK_VF_TABLE("snps_eth_62p4m_ptp_sel", 62400, 62400, 62400, 62400),
	MTK_VF_TABLE("snps_eth_50m_rmii_sel", 50000, 50000, 50000, 50000),
	MTK_VF_TABLE("netsys_500m_sel", 500000, 500000, 500000, 500000),
	MTK_VF_TABLE("netsys_med_mcu_sel", 580000, 356571, 273000, 104000),
	MTK_VF_TABLE("netsys_wed_mcu_sel", 760000, 436800, 364000, 182000),
	MTK_VF_TABLE("netsys_2x_sel", 800000, 546000, 273000, 124800),
	MTK_VF_TABLE("sgmii_sel", 325000, 325000, 325000, 325000),
	MTK_VF_TABLE("sgmii_sbus_sel", 78000, 78000,78000, 78000),
};*/

/*
 * clkdbg fmeter
 */

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define FMCLK2(_t, _i, _n, _o, _p) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p}
#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x0010, 7),
	FMCLK2(CKGEN, FM_SPM_CK, "fm_spm_ck", 0x0010, 15),
	FMCLK2(CKGEN, FM_BUS_CK, "fm_bus_ck", 0x0010, 23),
	FMCLK2(CKGEN, FM_MM_CK, "fm_mm_ck", 0x0010, 31),
	FMCLK2(CKGEN, FM_MFG_REF_CK, "fm_mfg_ref_ck", 0x0020, 7),
	FMCLK2(CKGEN, FM_FUART_CK, "fm_fuart_ck", 0x0020, 15),
	FMCLK2(CKGEN, FM_MSDC50_0_H_CK, "fm_msdc50_0_h_ck", 0x0020, 23),
	FMCLK2(CKGEN, FM_MSDC50_0_CK, "fm_msdc50_0_ck", 0x0020, 31),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 0x0030, 7),
	FMCLK2(CKGEN, FM_AUDIO_CK, "fm_audio_ck", 0x0030, 15),
	FMCLK2(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 0x0030, 23),
	FMCLK2(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 0x0030, 31),
	FMCLK2(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 0x0040, 7),
	FMCLK2(CKGEN, FM_AUD1_CK, "fm_aud1_ck", 0x0040, 15),
	FMCLK2(CKGEN, FM_AUD2_CK, "fm_aud2_ck", 0x0040, 23),
	FMCLK2(CKGEN, FM_FPWRAP_ULPOSC_CK, "fm_fpwrap_ulposc_ck", 0x0040, 31),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_ck", 0x0050, 7),
	FMCLK2(CKGEN, FM_PWRMCU_CK, "fm_pwrmcu_ck", 0x0050, 15),
	FMCLK2(CKGEN, FM_DBI_CK, "fm_dbi_ck", 0x0050, 23),
	FMCLK2(CKGEN, FM_FDISP_PWM_CK, "fm_fdisp_pwm_ck", 0x0050, 31),
	FMCLK2(CKGEN, FM_FUSB_CK, "fm_fusb_ck", 0x0060, 7),
	FMCLK2(CKGEN, FM_FSSUSB_XHCI_CK, "fm_fssusb_xhci_ck", 0x0060, 15),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0x0060, 23),
	FMCLK2(CKGEN, FM_TL_CK, "fm_tl_ck", 0x0060, 31),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0x0070, 7),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0x0070, 15),
	FMCLK2(CKGEN, FM_SPMI_M_MST_CK, "fm_spmi_m_mst_ck", 0x0070, 23),
	FMCLK2(CKGEN, FM_SPMI_P_MST_CK, "fm_spmi_p_mst_ck", 0x0070, 31),
	FMCLK2(CKGEN, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x0080, 7),
	FMCLK2(CKGEN, FM_MCUPM_CK, "fm_mcupm_ck", 0x0080, 15),
	FMCLK2(CKGEN, FM_SFLASH_CK, "fm_sflash_ck", 0x0080, 23),
	FMCLK2(CKGEN, FM_GCPU_CK, "fm_gcpu_ck", 0x0080, 31),
	FMCLK2(CKGEN, FM_SPI_CK, "fm_spi_ck", 0x0090, 7),
	FMCLK2(CKGEN, FM_SPIS_CK, "fm_spis_ck", 0x0090, 15),
	FMCLK2(CKGEN, FM_ECC_CK, "fm_ecc_ck", 0x0090, 23),
	FMCLK2(CKGEN, FM_NFI1X_CK, "fm_nfi1x_ck", 0x0090, 31),
	FMCLK2(CKGEN, FM_SPINFI_BCLK_CK, "fm_spinfi_bclk_ck", 0x00A0, 7),
	FMCLK2(CKGEN, FM_NETSYS_CK, "fm_netsys_ck", 0x00A0, 15),
	FMCLK2(CKGEN, FM_MEDSYS_CK, "fm_medsys_ck", 0x00A0, 23),
	FMCLK2(CKGEN, FM_HSM_CRYPTO_CK, "fm_hsm_crypto_ck", 0x00A0, 31),
	FMCLK2(CKGEN, FM_HSM_ARC_CK, "fm_hsm_arc_ck", 0x00B0, 7),
	FMCLK2(CKGEN, FM_EIP97_CK, "fm_eip97_ck", 0x00B0, 15),
	FMCLK2(CKGEN, FM_SNPS_ETH_312P5M_CK, "fm_snps_eth_312p5m_ck", 0x00B0, 23),
	FMCLK2(CKGEN, FM_SNPS_ETH_250M_CK, "fm_snps_eth_250m_ck", 0x00B0, 31),
	FMCLK2(CKGEN, FM_SNPS_PTP_CK, "fm_snps_ptp_ck", 0x00C0, 7),
	FMCLK2(CKGEN, FM_SNPS_ETH_50M_RMII_CK, "fm_snps_eth_50m_rmii_ck", 0x00C0, 15),
	FMCLK2(CKGEN, FM_NETSYS_500M_CK, "fm_netsys_500m_ck", 0x00C0, 23),
	FMCLK2(CKGEN, FM_NETSYS_MED_MCU_CK, "fm_netsys_med_mcu_ck", 0x00C0, 31),
	FMCLK2(CKGEN, FM_NETSYS_WED_MCU_CK, "fm_netsys_wed_mcu_ck", 0x00D0, 7),
	FMCLK2(CKGEN, FM_NETSYS_2X_CK, "fm_netsys_2x_ck", 0x00D0, 15),
	FMCLK2(CKGEN, FM_SGMII_CK, "fm_sgmii_ck", 0x00D0, 23),
	FMCLK2(CKGEN, FM_SGMII_SBUS_CK, "fm_sgmii_sbus_ck", 0x00D0, 31),
	/* ABIST Part */
	FMCLK(ABIST, FM_APLL1_CK, "fm_apll1_ck"),
	FMCLK(ABIST, FM_APLL2_CK, "fm_apll2_ck"),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK, "fm_appllgp_mon_fm_ck"),
	FMCLK(ABIST, FM_ARMPLL_LL_CK, "fm_armpll_ll_ck"),
	FMCLK(ABIST, FM_CCIPLL_CK, "fm_ccipll_ck"),
	FMCLK(ABIST, FM_NET1PLL_CK, "fm_net1pll_ck"),
	FMCLK(ABIST, FM_NET2PLL_CK, "fm_net2pll_ck"),
	FMCLK(ABIST, FM_WEDMCUPLL_CK, "fm_wedmcupll_ck"),
	FMCLK(ABIST, FM_MEDMCUPLL_CK, "fm_medmcupll_ck"),
	FMCLK(ABIST, FM_SGMIIPLL_CK, "fm_sgmiipll_ck"),
	FMCLK(ABIST, FM_SNPSETHPLL_CK, "fm_snpsethpll_ck"),
	FMCLK(ABIST, FM_DSI0_LNTC_DSICLK, "fm_dsi0_lntc_dsiclk"),
	FMCLK(ABIST, FM_DSI0_MPPLL_TST_CK, "fm_dsi0_mppll_tst_ck"),
	FMCLK(ABIST, FM_MDPLL1_FS26M_DRF_GUIDE, "fm_mdpll1_fs26m_drf_guide"),
	FMCLK(ABIST, FM_MFG_CK, "fm_mfg_ck"),
	FMCLK(ABIST, FM_MAINPLL_CK, "fm_mainpll_ck"),
	FMCLK(ABIST, FM_MDPLL1_FS26M_GUIDE, "fm_mdpll1_fs26m_guide"),
	FMCLK(ABIST, FM_MFGPLL_CK, "fm_mfgpll_ck"),
	FMCLK(ABIST, FM_MMPLL_CK, "fm_mmpll_ck"),
	FMCLK(ABIST, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck"),
	FMCLK(ABIST, FM_MPLL_CK, "fm_mpll_ck"),
	FMCLK(ABIST, FM_MSDCPLL_CK, "fm_msdcpll_ck"),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CK, "fm_rclrpll_div4_ck"),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CK, "fm_rphypll_div4_ck"),
	FMCLK(ABIST, FM_ULPOSC_CK, "fm_ulposc_ck"),
	FMCLK(ABIST, FM_UNIVPLL_CK, "fm_univpll_ck"),
	FMCLK(ABIST, FMEM_AFT_CH0, "fmem_aft_ch0"),
	FMCLK(ABIST, FMEM_AFT_CH1, "fmem_aft_ch1"),
	FMCLK(ABIST, FM_TRNG_FREQ_DEBUG_OUT0, "fm_trng_freq_debug_out0"),
	FMCLK(ABIST, FM_TRNG_FREQ_DEBUG_OUT1, "fm_trng_freq_debug_out1"),
	FMCLK(ABIST, FMEM_BFE_CH0, "fmem_bfe_ch0"),
	FMCLK(ABIST, FMEM_BFE_CH1, "fmem_bfe_ch1"),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS, "fm_466m_fmem_infrasys"),
	FMCLK(ABIST, FM_MCUSYS_ARM_OUT_ALL, "fm_mcusys_arm_out_all"),
	FMCLK(ABIST, FM_RTC32K_I_VAO, "fm_rtc32k_i_vao"),
	/* ABIST_2 Part */
	FMCLK(ABIST_2, FM_MCUPM_CK, "fm_mcupm_ck"),
	FMCLK(ABIST_2, FM_SFLASH_CK, "fm_sflash_ck"),
	FMCLK(ABIST_2, FM_UNIPLL_SES_CK, "fm_unipll_ses_ck"),
	FMCLK(ABIST_2, FM_ULPOSC_CK, "fm_ulposc_ck"),
	FMCLK(ABIST_2, FM_ULPOSC_CORE_CK, "fm_ulposc_core_ck"),
	FMCLK(ABIST_2, FM_SRCK_CK, "fm_srck_ck"),
	FMCLK(ABIST_2, FM_MAINPLL_H728M_CK, "fm_mainpll_h728m_ck"),
	FMCLK(ABIST_2, FM_MAINPLL_H546M_CK, "fm_mainpll_h546m_ck"),
	FMCLK(ABIST_2, FM_MAINPLL_H436P8M_CK, "fm_mainpll_h436p8m_ck"),
	FMCLK(ABIST_2, FM_MAINPLL_H364M_CK, "fm_mainpll_h364m_ck"),
	FMCLK(ABIST_2, FM_MAINPLL_H312M_CK, "fm_mainpll_h312m_ck"),
	FMCLK(ABIST_2, FM_UNIVPLL_1248M_CK, "fm_univpll_1248m_ck"),
	FMCLK(ABIST_2, FM_UNIVPLL_832M_CK, "fm_univpll_832m_ck"),
	FMCLK(ABIST_2, FM_UNIVPLL_624M_CK, "fm_univpll_624m_ck"),
	FMCLK(ABIST_2, FM_UNIVPLL_499M_CK, "fm_univpll_499m_ck"),
	FMCLK(ABIST_2, FM_UNIVPLL_416M_CK, "fm_univpll_416m_ck"),
	FMCLK(ABIST_2, FM_UNIVPLL_356P6M_CK, "fm_univpll_356p6m_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D4_CK, "fm_mmpll_d4_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D5_CK, "fm_mmpll_d5_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D6_CK, "fm_mmpll_d6_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D7_CK, "fm_mmpll_d7_ck"),
	FMCLK(ABIST_2, FM_MMPLL_D9_CK, "fm_mmpll_d9_ck"),
	FMCLK(ABIST_2, FM_NET1PLL_CK, "fm_net1pll_ck"),
	FMCLK(ABIST_2, FM_NET2PLL_CK, "fm_net2pll_ck"),
	FMCLK(ABIST_2, FM_WEDMCUPLL_CK, "fm_wedmcupll_ck"),
	FMCLK(ABIST_2, FM_MEDMCUPLL_CK, "fm_medmcupll_ck"),
	FMCLK(ABIST_2, FM_SGMIIPLL_CK, "fm_sgmiipll_ck"),
	{},
};

#define _CKGEN(x)		(rb[top].virt + (x))
#define CLK_CFG_0		_CKGEN(0x10)
#define CLK_CFG_1		_CKGEN(0x20)
#define CLK_CFG_2		_CKGEN(0x30)
#define CLK_CFG_3		_CKGEN(0x40)
#define CLK_CFG_4		_CKGEN(0x50)
#define CLK_CFG_5		_CKGEN(0x60)
#define CLK_CFG_6		_CKGEN(0x70)
#define CLK_CFG_7		_CKGEN(0x80)
#define CLK_CFG_8		_CKGEN(0x90)
#define CLK_CFG_9		_CKGEN(0xA0)
#define CLK_CFG_10		_CKGEN(0xB0)
#define CLK_CFG_11		_CKGEN(0xC0)
#define CLK_CFG_12		_CKGEN(0xD0)
//#define CLK_CFG_13		_CKGEN(0xE0)
#define CLK_MISC_CFG_0		_CKGEN(0x140)
#define CLK_DBG_CFG		_CKGEN(0x17C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

/*
 * clkdbg dump_clks
 */
 
 static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0] = "md1",
		[1] = "conn",
		[2] = "mfg0",
		[3] = "pextp_d_2lx1_phy",
		[4] = "pextp_r_2lx1_phy",
		[5] = "pextp_r_1lx2_0p_phy",
		[6] = "pextp_r_1lx2_1p_phy",
		[7] = "ssusb_phy",
		[8] = "sgmii_0_phy",
		[9] = "infra",
		[10] = "sgmii_1_phy",
		[11] = "dpy",
		[12] = "pextp_d_2lx1",
		[13] = "pextp_r_2lx1",
		[14] = "pextp_r_1lx2",
		[15] = "eth",
		[16] = "ssusb",
		[17] = "sgmii_0_top",
		[18] = "sgmii_1_top",
		[19] = "netsys",
		[20] = "dis",
		[21] = "audio",
		[22] = "eip97",
		[23] = "hsmtop",
		[24] = "dramc_md32",
		[25] = "(Reserved)",
		[26] = "(Reserved)",
		[27] = "(Reserved)",
		[28] = "dpy2",
		[29] = "mcupm",
		[30] = "msdc",
		[31] = "peri",
	};
	return pwr_names;
}
 
 static const char * const *get_all_clk_names(void)
{
	return get_mt6890_all_clk_names();
}

 static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void __init init_regbase(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rb); i++) {
		if (!rb[i].phys)
			continue;
		rb[i].virt = ioremap_nocache(rb[i].phys, 0x1000);
	}
}

unsigned int mt_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned long flags;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
		;
	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));
	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);
	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}
	temp = clk_readl(CLK26CALI_1) & 0xFFFF;
	output = (temp * 26000) / 1024;
	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/
	clk_writel(CLK26CALI_0, 0x0000);
	fmeter_unlock(flags);
	if (i > 30)
		return 0;
	else {
		if ((output * 4) < 25000) {
			pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
				__func__,
				clk_readl(CLK_DBG_CFG),
				clk_readl(CLK_MISC_CFG_0),
				clk_readl(CLK26CALI_0),
				clk_readl(CLK26CALI_1));
		}
		return (output * 4);
	}
}

static unsigned int mt_get_abist2_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned long flags;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
		;
	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xC0FFFFFC)
			| (ID << 24) | (0x2));
	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (1 << 24));
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);
	// wait frequency meter finish 
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	// illegal pass 
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}
	temp = clk_readl(CLK26CALI_1) & 0xFFFF;
	output = (temp * 26000) / 1024;
	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	//clk_writel(CLK26CALI_0, clk26cali_0);
	//clk_writel(CLK26CALI_1, clk26cali_1);
	clk_writel(CLK26CALI_0, 0x0000);
	//pr_debug("%s = %d Khz\n", abist_array[ID-1], output);
	fmeter_unlock(flags);
	if (i > 30)
		return 0;
	else
		return (output * 2);
}

static unsigned int check_mux_pdn(unsigned int ID)
{
	int i;
	if ((ID > 0) && (ID < 64)) {
		for (i = 0; i < ARRAY_SIZE(fclks); i++)
			if (fclks[i].id == ID)
				break;
		if (i >= ARRAY_SIZE(fclks))
			return 1;
		if ((clk_readl(rb[top].virt + fclks[i].ofs)
				& BIT(fclks[i].pdn)))
			return 1;
		else
			return 0;
	} else
		return 1;
}

unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned long flags;
	if (check_mux_pdn(ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		return 0;
	}
	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x1000) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));
	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);
	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30)
				break;
		}
	}
	temp = clk_readl(CLK26CALI_1) & 0xFFFF;
	output = (temp * 26000) / 1024;
	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/
	clk_writel(CLK26CALI_0, 0x0000);
	fmeter_unlock(flags);
	/*print("ckgen meter[%d] = %d Khz\n", ID, output);*/
	if (i > 30)
		return 0;
	else {
		if ((output * 4) < 25000) {
			pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
				__func__,
				clk_readl(CLK_DBG_CFG),
				clk_readl(CLK_MISC_CFG_0),
				clk_readl(CLK26CALI_0),
				clk_readl(CLK26CALI_1));
		}
		return (output * 4);
	}
}

 static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type == ABIST)
		return mt_get_abist_freq(fclk->id);
	else if (fclk->type == ABIST_2)
		return mt_get_abist2_freq(fclk->id);
	else if (fclk->type == CKGEN)
		return mt_get_ckgen_freq(fclk->id);
	return 0;
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

void setup_provider_clk(struct provider_clk *pvdck)
{
	static const struct {
		const char *pvdname;
		u32 pwr_mask;
	} pvd_pwr_mask[] = {
	};

	int i;
	const char *pvdname = pvdck->provider_name;

	if (!pvdname)
		return;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_mask); i++) {
		if (strcmp(pvdname, pvd_pwr_mask[i].pvdname) == 0) {
			pvdck->pwr_mask = pvd_pwr_mask[i].pwr_mask;
			return;
		}
	}
}

/*
 * chip_ver functions
 */
/*
static int clkdbg_chip_ver(struct seq_file *s, void *v)
{
	static const char * const sw_ver_name[] = {
		"CHIP_SW_VER_01",
		"CHIP_SW_VER_02",
		"CHIP_SW_VER_03",
		"CHIP_SW_VER_04",
	};

	seq_printf(s, "mt_get_chip_sw_ver(): %d (%s)\n", 0, sw_ver_name[0]);

	return 0;
}*/

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6890_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
};

static void __init init_custom_cmds(void)
{
	/*static const struct cmd_fn cmds[] = {
		CMDFN("chip_ver", clkdbg_chip_ver),
		{}
	};

	set_custom_cmds(cmds);*/
}

static int __init clkdbg_mt6890_init(void)
{
/*	if (!of_machine_is_compatible("mediatek,MT6890"))
		return -ENODEV;
*/
	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6890_ops);

#ifdef CONFIG_MTK_DEVAPC
//	register_devapc_vio_callback(&devapc_vio_handle);
#endif

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
subsys_initcall(clkdbg_mt6890_init);

/*
 * MT6890: for mtcmos debug
 */
static bool is_valid_reg(void __iomem *addr)
{
#ifdef CONFIG_64BIT
	return ((u64)addr & 0xf0000000) != 0UL ||
			(((u64)addr >> 32U) & 0xf0000000) != 0UL;
#else
	return ((u32)addr & 0xf0000000) != 0U;
#endif
}

void print_subsys_reg(enum dbg_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int i;
	/*if (rns == NULL)
		return;*/
	if (id >= dbg_sys_num || id < 0) {
		pr_info("wrong id:%d\n", id);
		return;
	}
	rb_dump = &rb[id];
	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;
		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;
		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
