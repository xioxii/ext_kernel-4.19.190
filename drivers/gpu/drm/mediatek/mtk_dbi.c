// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 */

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_of.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <video/mipi_display.h>

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_dbi.h"

//#define ENABLE_UT   //Hardcode testing without LCM driver
//#define ENABLE_DBI_PATTERN  //DBI Self pattern

// Use to enable debug log, default should be turned off
//#define ENABLE_DEBUG_LOG

// Manual config ARB register to ensure data correction
#define ENABLE_ARB_MANUAL_CONFIG

#ifdef ENABLE_DEBUG_LOG
#define dbi_dev_info(dev, fmt, ...)	        \
	_dev_info(dev, dev_fmt(fmt), ##__VA_ARGS__)
#else
#define dbi_dev_info(dev, fmt, ...)         \
({                                          \
    if(0)                                   \
        _dev_info(dev, dev_fmt(fmt), ##__VA_ARGS__);    \
})
#endif

// NLI ARB address mapping
#define NLI_ARB_CON_BASE 0x11f30000
#define NLI_ARB_CON 0x014  //NLI_ARB_CON
#define NLI_ARB_CON_FAVOR_NLI_DBI_MASK BIT(24) | BIT(25)
//favor_nli: NLI_ARB_CON[25]
//favor_dbi: NLI_ARB_CON[24]
// Need to clear favor_nli and favor_dbi flag to allow DBI-B
#define NLI_ARB_CON_FAVOR_DBI_SHIFT 24
#define NLI_ARB_TEMP 0x020
#define NLI_ARB_CON_DBI_MASK BIT(16) | BIT(17)

// GPIO address mapping
// TODO: change to config in dts file
#define GPIO_BASE 0x10005000
#define GPIO_MODE25 0x490  //GPIO 207
//[30:28] GPIO207: 001 DSI_TE, 010 LPTE
#define GPIO_MODE25_MASK BIT(30) | BIT(29) | BIT(28)
#define GPIO_MODE25_VALUE 0x20000000

#define GPIO_MODE26 0x4A0  //GPIO 209~214
//[6:4]GPIO209:   001 LPWRB,  010 LSCK
//[10:8]GPIO210:  001 LPRDB,  010 LSDA
//[14:12]GPIO211: 001 LPA0,   010 LSA0
//[18:16]GPIO212: 001 LPCE0B, 010 LSCE0B
//[22:20]GPIO213: 001 LPD8,   010 LSDI
//[26:24]GPIO214: 001 LPRSTB
//[30:28]GPIO215: 001 NLD0
#define GPIO_MODE26_MASK_DBI_B 0x77777770
#define GPIO_MODE26_MASK_DBI_C 0x07777770
#define GPIO_MODE26_VALUE_DBI_B_9BIT 0x11111110
#define GPIO_MODE26_VALUE_DBI_B_8BIT 0x11011110
#define GPIO_MODE26_VALUE_DBI_C      0x01222220
#define GPIO_MODE26_214_MASK BIT(26) | BIT(25) | BIT(24)

#define GPIO_MODE27 0x4B0  //GPIO 216~222
//[3:0]GPIO216:   001 NLD1
//[6:4]GPIO217:   001 NLD2
//[10:8]GPIO218:  001 NLD3
//[14:12]GPIO219: 001 NLD4
//[18:16]GPIO220: 001 NLD5
//[22:20]GPIO221: 001 NLD6
//[26:24]GPIO222: 001 NLD7
#define GPIO_MODE27_MASK 0x07777777
#define GPIO_MODE27_VALUE_DBI_B 0x01111111

#define IOCFG_TL_BASE 0x11f00000
#define IOCFG_TL_DRV_CFG0 0x10
#define IOCFG_TL_DRV_CFG0_MASK 0x3FFC0

// DBI registers
#define DBI_BASE        0x1400f000
#define DBI_START       0x0C
#define DBI_START_FLAG  BIT(15)
#define DBI_SRST_FLAG   BIT(0)      //Software reset

#define DBI_STA       0x00
#define DBI_STA_BUSY        BIT(4)

#define DBI_INTEN       0x04
#define DBI_INTSTA      0x08
#define DBI_INT_FLAG_CPL  BIT(0) //Enable frame complete interrupt
#define DBI_INT_FLAG_HTT  BIT(2) //Enable HTT calculation interrupt
#define DBI_INT_FLAG_SYNC BIT(3) //Enable TE sync interrupt
#define DBI_INT_FLAG_TE   BIT(4) //Enable TE detection interrupt
#define DBI_INT_FLAG_APB_TIMEOUT   BIT(5) //Enable LCD timeout interrupt

#define DBI_SIF_TIMING0 0x1c
#define CSS             0x00f00000
#define CSH             0x000f0000
#define RD1ST           0x0000f000
#define RD2ND           0x00000f00
#define WR1ST           0x000000f0
#define WR2ND           0x0000000f

#define DBI_SCNF        0x28
#define SIF_HW_CS       BIT(24)
#define SIF0_DIV2       BIT(7)
#define SIF0_SCK_DEF    BIT(6)
#define SIF0_1ST_POL    BIT(5)
#define SIF0_SDI        BIT(4)
#define SIF0_3WIRE      BIT(3)
#define SIF0_SIZE       BIT(0) | BIT(1) | BIT(2)

#define DBI_PCNF0       0x30
#define C2RH_SHIFT      28
#define C2RH            0xf0000000
#define C2RS_SHIFT      24
#define C2RS            0x0f000000
#define RLT_SHIFT       16
#define RLT             0x003f0000
#define C2WH_SHIFT      12
#define C2WH            0x0000f000
#define C2WS_SHIFT      8
#define C2WS            0x00000f00
#define WST             0x3f

#define DBI_PDW         0x3c
#define P0DW            0x7
#define P1DW            0x7 << 4
#define P2DW            0x7 << 8
#define P0CHW           0xf << 16
#define P0CHW_SHIFT     16

#define DBI_TECON       0x40
#define TE_ENABLE         BIT(0)
#define TE_EDGE            BIT(1)
#define TE_SYNC            BIT(2)
#define TE_REPEART      BIT(3)
#define SW_TE           BIT(15)

#define DBI_ROICON      0x60
#define COLOR_SEQ       BIT(0)
#define COLOR_FORMAT    BIT(3) | BIT(4) | BIT(5)    //Bit[3:5] color format
#define ITF_SIZE        BIT(6) | BIT(7)

#define DBI_ROI_CADD    0x64
#define CADD_ADDR       0xf0

#define DBI_ROI_DADD    0x68
#define DADD_ADDR       0xf0

#define DBI_ROI_SIZE    0x6c
#define ROI_SIZE_ROW    0x7ff << 16
#define ROI_SIZE_COL    0x7ff

#define DBI_PCMD0       0xf00
#define DBI_PDAT0       0xf10

#define DBI_SCMD0       0xf80
#define DBI_SDAT0       0xf90

#define DBI_ULTRA_CON   0x90
#define DBI_PATTERN     0xe8
#define DBI_RSTB        0x10

// ENUM
typedef enum {
    RETURN_OK = 0,
    RETURN_ERROR = -1,
} DBI_RETURN_VALUE;

typedef enum {
    LCM_DBI_DATA_INTERFACE_8BITS = 0,
    LCM_DBI_DATA_INTERFACE_9BITS = 1,
} LCM_DBI_DATA_INTERFACE;

struct mtk_dbi {
    struct mtk_ddp_comp ddp_comp;
    struct device *dev;
    struct drm_encoder encoder;
    struct drm_connector conn;
    struct drm_panel *panel;
    struct drm_bridge *bridge;
    struct regmap *mmsys_sw_rst_b;
    u32 sw_rst_b;

    void __iomem *regs;
    void __iomem *gpio_base;
    void __iomem *iocfg_tl_base;
    void __iomem *nli_arb_base;
    void __iomem *ovl_base;

	struct clk *mm_clk;
	struct clk *mm_dbi_clk;
	struct clk *infra_nfi_clk;
	struct clk *engine_clk;
	struct clk *parent_clk_26;
	struct clk *parent_clk_78;
	struct clk *parent_clk_91;
	struct clk *parent_clk_104;
	struct clk *parent_clk_125;
	struct clk *parent_clk_137;
	struct clk *parent_clk_156;
	struct clk *parent_clk_182;
	struct clk *mm_smi_clk;

    int refcount;
    bool enabled;
    bool panel_enabled;
    bool need_update;
    u32 irq_data;
    wait_queue_head_t irq_wait_queue;

    struct drm_display_mode mode;
    struct mipi_dbi_device host_info;
};

static struct drm_panel *_panel = NULL;
static bool _panel_attached = false;
static struct mipi_dbi_device _host_info;

static inline struct mtk_dbi *encoder_to_dbi(struct drm_encoder *e)
{
    return container_of(e, struct mtk_dbi, encoder);
}

static inline struct mtk_dbi *connector_to_dbi(struct drm_connector *c)
{
    return container_of(c, struct mtk_dbi, conn);
}

static void mtk_dbi_mask(struct mtk_dbi *dbi, u32 offset, u32 mask, u32 data)
{
    u32 temp = readl(dbi->regs + offset);

    writel((temp & ~mask) | (data & mask), dbi->regs + offset);
}

static void mtk_dbi_mask_arb(struct mtk_dbi *dbi, u32 offset, u32 mask, u32 data)
{
    u32 temp = readl(dbi->nli_arb_base + offset);

    writel((temp & ~mask) | (data & mask), dbi->nli_arb_base + offset);
}

static void mtk_dbi_mask_driving(struct mtk_dbi *dbi, u32 offset, u32 mask, u32 data)
{
    u32 temp = readl(dbi->iocfg_tl_base + offset);

    writel((temp & ~mask) | (data & mask), dbi->iocfg_tl_base + offset);
}

static void mtk_dbi_mask_gpio(struct mtk_dbi *dbi, u32 offset, u32 mask, u32 data)
{
    u32 temp = readl(dbi->gpio_base+ offset);

    writel((temp & ~mask) | (data & mask), dbi->gpio_base + offset);
}

static void mtk_dbi_dump_regs(struct mtk_dbi *dbi) {
    dev_info(dbi->dev, "[%s] enter ctrl-type = %d", __func__, dbi->host_info.ctrl_type);
    // Dump arbiter
    dev_info(dbi->dev, "[arbiter]Address: 0x%08X, Value: 0x%08X",
            NLI_ARB_CON_BASE + NLI_ARB_CON, readl(dbi->nli_arb_base + NLI_ARB_CON));
    dev_info(dbi->dev, "[arbiter]Address: 0x%08X, Value: 0x%08X",
            NLI_ARB_CON_BASE + NLI_ARB_TEMP, readl(dbi->nli_arb_base + NLI_ARB_TEMP));

    // Dump GPIO
    dev_info(dbi->dev, "[GPIO]Address: 0x%08X, Value: 0x%08X",
            GPIO_BASE + GPIO_MODE25, readl(dbi->gpio_base + GPIO_MODE25));
    dev_info(dbi->dev, "[GPIO]Address: 0x%08X, Value: 0x%08X",
            GPIO_BASE + GPIO_MODE26, readl(dbi->gpio_base + GPIO_MODE26));
    dev_info(dbi->dev, "[GPIO]Address: 0x%08X, Value: 0x%08X",
            GPIO_BASE + GPIO_MODE27, readl(dbi->gpio_base + GPIO_MODE27));

    // Dump driving current
    dev_info(dbi->dev, "[Driving]Address: 0x%08X, Value: 0x%08X",
            IOCFG_TL_BASE + IOCFG_TL_DRV_CFG0,
            readl(dbi->iocfg_tl_base + IOCFG_TL_DRV_CFG0));

    // Dump DBI registers
    dev_info(dbi->dev, "[DBI_START]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_START, readl(dbi->regs + DBI_START));
    dev_info(dbi->dev, "[DBI_INTEN]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_INTEN, readl(dbi->regs + DBI_INTEN));
    dev_info(dbi->dev, "[DBI_INTSTA]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_INTSTA, readl(dbi->regs + DBI_INTSTA));
    dev_info(dbi->dev, "[DBI_ROI_CADD]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_ROI_CADD, readl(dbi->regs + DBI_ROI_CADD));
    dev_info(dbi->dev, "[DBI_ROI_DADD]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_ROI_DADD, readl(dbi->regs + DBI_ROI_DADD));
    dev_info(dbi->dev, "[DBI_ROICON]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_ROICON, readl(dbi->regs + DBI_ROICON));
    dev_info(dbi->dev, "[DBI_ROI_SIZE]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_ROI_SIZE, readl(dbi->regs + DBI_ROI_SIZE));
    dev_info(dbi->dev, "[DBI_TECON]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_TECON, readl(dbi->regs + DBI_TECON));
    dev_info(dbi->dev, "[DBI_PATTERN]Address: 0x%08X, Value: 0x%08X",
            DBI_BASE + DBI_PATTERN, readl(dbi->regs + DBI_PATTERN));

    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_PARALLEL_DBI) {
        dev_info(dbi->dev, "[DBI_PCNF0]Address: 0x%08X, Value: 0x%08X",
                DBI_BASE + DBI_PCNF0, readl(dbi->regs + DBI_PCNF0));
        dev_info(dbi->dev, "[DBI_PDW]Address: 0x%08X, Value: 0x%08X",
                DBI_BASE + DBI_PDW, readl(dbi->regs + DBI_PDW));
    } else {
        dev_info(dbi->dev, "[DBI_SCNF]Address: 0x%08X, Value: 0x%08X",
                DBI_BASE + DBI_SCNF, readl(dbi->regs + DBI_SCNF));
        dev_info(dbi->dev, "[DBI_SIF_TIMING0]Address: 0x%08X, Value: 0x%08X",
                DBI_BASE + DBI_SIF_TIMING0, readl(dbi->regs + DBI_SIF_TIMING0));
    }

    dev_info(dbi->dev, "[%s] exit", __func__);
}

static const struct of_device_id mtk_dbi_of_match[] = {
    { .compatible = "mediatek,mt6890-dbi0"},
    { },
};

// TODO: change to use dts pinctrl
static int mtk_dbi_set_gpio(struct mtk_dbi *dbi) {
    int ret = RETURN_OK;
    struct device_node *node;

    dbi_dev_info(dbi->dev, "[%s] start ctrl_type = %d\n", __func__, dbi->host_info.ctrl_type);

    /* get GPIO base address */
    node = of_find_compatible_node(NULL, NULL, "mediatek,gpio");
    if (!node) {
        pr_err("error: can't find GPIO node\n");
        WARN_ON(1);
        return 0;
    }

    dbi->gpio_base = of_iomap(node, 0);
    if (!dbi->gpio_base) {
        pr_err("error: iomap fail for GPIO\n");
        WARN_ON(1);
        return 0;
    }

    /* get NLI arbiter base address */
    node = of_find_compatible_node(NULL, NULL, "mediatek,nli_arb");
    if (!node) {
        pr_err("error: can't find nli_arb node\n");
        WARN_ON(1);
        return 0;
    }

    dbi->nli_arb_base = of_iomap(node, 0);
    if (!dbi->nli_arb_base) {
        pr_err("error: iomap fail for nli_arb\n");
        WARN_ON(1);
        return 0;
    }

    switch(dbi->host_info.ctrl_type) {
        // Set DBI-B GPIO
        case MTK_DBI_LCM_CTRL_TYPE_PARALLEL_DBI:
            // Set NLI_ARB_CON to allow DBI-B interface, need to clear faver_nfi and faver_dbi
            mtk_dbi_mask_arb(dbi, NLI_ARB_CON, NLI_ARB_CON_FAVOR_NLI_DBI_MASK,
                    0 << NLI_ARB_CON_FAVOR_DBI_SHIFT);

            // Set DBI-B GPIO
            mtk_dbi_mask_gpio(dbi, GPIO_MODE25, GPIO_MODE25_MASK, GPIO_MODE25_VALUE);

            if (dbi->host_info.dbi_b_params.itf_size == MTK_DBI_LCM_INTERFACE_SIZE_8BITS) {
                mtk_dbi_mask_gpio(dbi, GPIO_MODE26, GPIO_MODE26_MASK_DBI_B, GPIO_MODE26_VALUE_DBI_B_8BIT);
            } else if (dbi->host_info.dbi_b_params.itf_size == MTK_DBI_LCM_INTERFACE_SIZE_9BITS) {
                mtk_dbi_mask_gpio(dbi, GPIO_MODE26, GPIO_MODE26_MASK_DBI_B, GPIO_MODE26_VALUE_DBI_B_9BIT);
            }

            mtk_dbi_mask_gpio(dbi, GPIO_MODE27, GPIO_MODE27_MASK, GPIO_MODE27_VALUE_DBI_B);
            break;
        case MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI:
            // Set DBI-C GPIO
            mtk_dbi_mask_gpio(dbi, GPIO_MODE25, GPIO_MODE25_MASK, GPIO_MODE25_VALUE);
            mtk_dbi_mask_gpio(dbi, GPIO_MODE26, GPIO_MODE26_MASK_DBI_C, GPIO_MODE26_VALUE_DBI_C);
            break;

        default:  // Non-DBI, set DBI PIN to GPIO mode
            // Keep LPRSTB in GPIO mode to avoid unexpected behavior
            mtk_dbi_mask_gpio(dbi, GPIO_MODE26, GPIO_MODE26_214_MASK, 0);
            DRM_ERROR("mtk_dbi_set_gpio: unknown ctrl type = %d\n",
                    dbi->host_info.ctrl_type);
            ret = RETURN_ERROR;
    }

   dbi_dev_info(dbi->dev, "[%s] end\n", __func__);

    return ret;
}

static int mtk_dbi_set_driving_current(struct mtk_dbi *dbi) {
    int ret = RETURN_OK;
    int driving_current;
    struct device_node *node;

    /* get iocfg_tl base address */
    node = of_find_compatible_node(NULL, NULL, "mediatek,iocfg_tl");
    if (!node) {
        pr_err("error: can't find iocfg_tl node\n");
        WARN_ON(1);
        return 0;
    }

    dbi->iocfg_tl_base = of_iomap(node, 0);
    if (!dbi->iocfg_tl_base) {
        pr_err("error: iomap fail for iocfg_tl\n");
        WARN_ON(1);
        return 0;
    }

    // IOCFG_TL_BASE+0x0010[17:15] for PAD_LPA0,PAD_LSDI
    // IOCFG_TL_BASE+0x0010[14:12] for PAD_LPCE0B,PAD_LPRDB,PAD_LPRSTB,PAD_LPWRB
    // IOCFG_TL_BASE+0x0010[11:9]  for PAD_NLD2,PAD_NLD4,PAD_NLD6,PAD_NLD7
    // IOCFG_TL_BASE+0x0010[8:6]   for PAD_NLD1,PAD_NLD3,PAD_NLD5,PAD_NLD0
    driving_current = ((dbi->host_info.driving_current << 6)
            + (dbi->host_info.driving_current << 9)
            + (dbi->host_info.driving_current << 12)
            + (dbi->host_info.driving_current << 15));

    mtk_dbi_mask_driving(dbi, IOCFG_TL_DRV_CFG0, IOCFG_TL_DRV_CFG0_MASK, driving_current);

    return ret;
}

static void mtk_dbi_lock_data_pin(struct mtk_dbi *dbi) {
    dbi_dev_info(dbi->dev, "[%s]ctrl_type=%d", __func__, dbi->host_info.ctrl_type);

#ifdef ENABLE_ARB_MANUAL_CONFIG
    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_PARALLEL_DBI) {
        // Workaround for ARB
        mtk_dbi_mask_arb(dbi, NLI_ARB_CON, NLI_ARB_CON_FAVOR_NLI_DBI_MASK,
                1 << NLI_ARB_CON_FAVOR_DBI_SHIFT);
        writel(0x01, dbi->nli_arb_base + NLI_ARB_TEMP);

        dbi_dev_info(dbi->dev, "[%s], write arb to block nfi", __func__);
    }
#endif
}

static void mtk_dbi_unlock_data_pin(struct mtk_dbi *dbi) {
    dbi_dev_info(dbi->dev, "[%s]ctrl_type=%d", __func__, dbi->host_info.ctrl_type);

#ifdef ENABLE_ARB_MANUAL_CONFIG
    // Workaround for ARB
    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_PARALLEL_DBI) {
        mtk_dbi_mask_arb(dbi, NLI_ARB_CON, NLI_ARB_CON_FAVOR_NLI_DBI_MASK,
                0 << NLI_ARB_CON_FAVOR_DBI_SHIFT);
        writel(0x0, dbi->nli_arb_base + NLI_ARB_TEMP);


        dbi_dev_info(dbi->dev, "[%s], write arb to avoid blocking nfi", __func__);
    }
#endif
}

static void mtk_dbi_start(struct mtk_dbi *dbi) {

    dbi_dev_info(dbi->dev, "[%s], enter", __func__);

    mtk_dbi_lock_data_pin(dbi);

    //Reset
    mtk_dbi_mask(dbi, DBI_START, DBI_SRST_FLAG, 1);
    mtk_dbi_mask(dbi, DBI_START, DBI_SRST_FLAG, 0);

    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI) {
        writel(0x2C, dbi->regs + DBI_SCMD0);
    } else {
        writel(0x2C, dbi->regs + DBI_PCMD0);
    }

    mtk_dbi_mask(dbi, DBI_START, DBI_START_FLAG, 0 << 15);
    mtk_dbi_mask(dbi, DBI_START, DBI_START_FLAG, 1 << 15);
}

static void mtk_dbi_stop(struct mtk_dbi *dbi) {
   mtk_dbi_mask(dbi, DBI_START, DBI_START_FLAG,  0 << 15);
}

static void mtk_dbi_set_roi_cmd_data(struct mtk_dbi *dbi) {
    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI) {
        /* Command and data are sent to DBI-C LCM CS0 */
        mtk_dbi_mask(dbi, DBI_ROI_CADD, CADD_ADDR, 8 << 4);
        mtk_dbi_mask(dbi, DBI_ROI_DADD, DADD_ADDR, 9 << 4);
    } else {
        /* Command and data are sent to DBI-B LCM CS0 */
        mtk_dbi_mask(dbi, DBI_ROI_CADD, CADD_ADDR, 0 << 4);
        mtk_dbi_mask(dbi, DBI_ROI_DADD, DADD_ADDR, 1 << 4);
    }
}

static void mtk_dbi_set_roi_con(struct mtk_dbi *dbi) {

    if (dbi->host_info.color_order == MTK_DBI_LCM_COLOR_ORDER_BGR) {
        mtk_dbi_mask(dbi, DBI_ROICON, COLOR_SEQ, 0);    //0: BGR
    } else {
        mtk_dbi_mask(dbi, DBI_ROICON, COLOR_SEQ, 1);    //1: RGB
    }

    //[Color format] 000: RGB332, 001: RGB444, 010: RGB565, 011: RGB666, 100: RGB888
    switch (dbi->host_info.color_format) {
        case MTK_DBI_LCM_COLOR_FORMAT_RGB332:
            mtk_dbi_mask(dbi, DBI_ROICON, COLOR_FORMAT, 0);
            break;
        case MTK_DBI_LCM_COLOR_FORMAT_RGB444:
            mtk_dbi_mask(dbi, DBI_ROICON, COLOR_FORMAT, 1 << 3);
            break;
        case MTK_DBI_LCM_COLOR_FORMAT_RGB565:
            mtk_dbi_mask(dbi, DBI_ROICON, COLOR_FORMAT, 2 << 3);
            break;
        case MTK_DBI_LCM_COLOR_FORMAT_RGB666:
            mtk_dbi_mask(dbi, DBI_ROICON, COLOR_FORMAT, 3 << 3);
            break;
        case MTK_DBI_LCM_COLOR_FORMAT_RGB888:
            mtk_dbi_mask(dbi, DBI_ROICON, COLOR_FORMAT, 4 << 3);
            break;

        default:
            DRM_ERROR("mtk_dbi_set_roi_con, unknown color format:%d\n",
                    dbi->host_info.color_format);
    }

    //if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI) {
    //    DRM_WARN("mtk_dbi_set_roi_con, ignore data_width setting for DBI-C\n");
    //} else {
        // [Interface size] 00: 8bit, 01: 16bit, 10: 9bit, 11:18bit
        switch (dbi->host_info.dbi_b_params.itf_size) {
            case MTK_DBI_LCM_INTERFACE_SIZE_8BITS:
                mtk_dbi_mask(dbi, DBI_ROICON, ITF_SIZE, 0);
                break;
            case MTK_DBI_LCM_INTERFACE_SIZE_9BITS:
                mtk_dbi_mask(dbi, DBI_ROICON, ITF_SIZE, 2 << 6);
                break;
            case MTK_DBI_LCM_INTERFACE_SIZE_16BITS:
                mtk_dbi_mask(dbi, DBI_ROICON, ITF_SIZE, 1 << 6);
                break;
            case MTK_DBI_LCM_INTERFACE_SIZE_18BITS:
                mtk_dbi_mask(dbi, DBI_ROICON, ITF_SIZE, 3 << 6);
                break;
            default:
                DRM_ERROR("mtk_dbi_set_roi_con, unknown data_width:%d\n",
                        dbi->host_info.dbi_b_params.itf_size);
        }
    //}
}

static void mtk_dbi_set_roi_size(struct mtk_dbi *dbi) {
    mtk_dbi_mask(dbi, DBI_ROI_SIZE, ROI_SIZE_ROW, dbi->mode.vdisplay << 16);
    mtk_dbi_mask(dbi, DBI_ROI_SIZE, ROI_SIZE_COL, dbi->mode.hdisplay);
}

static void mtk_dbi_set_timing(struct mtk_dbi *dbi){
    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI) {
        // Set DBI serial interface configuration
        // 0: Chip selection of serial interface is controlled by software
        //    by manipulating register DBI_SCNF_CS.
        // 1: Chip selection of serial interface is controlled by hardware.
        mtk_dbi_mask(dbi, DBI_SCNF, SIF_HW_CS, 1 << 24);

        // 0: Default of LSCK is low.
        // 1: Default of LSCK is high.
        mtk_dbi_mask(dbi, DBI_SCNF, SIF0_SCK_DEF, 1 << 6);

        // Set to 1 to read data from LSDI pin;
        //otherwise LCD will use the bi-directional LSDA pin.
        mtk_dbi_mask(dbi, DBI_SCNF, SIF0_SDI, 0 << 4);

        if (dbi->host_info.dbi_c_params.wire_num == MTK_DBI_LCM_INTERFACE_3WIRE) {
            mtk_dbi_mask(dbi, DBI_SCNF, SIF0_3WIRE, 1 << 3);
        }

        mtk_dbi_mask(dbi, DBI_SCNF, SIF0_SIZE, dbi->host_info.data_width);

        // Set DBI serial interface read/write timing
        mtk_dbi_mask(dbi, DBI_SIF_TIMING0, CSS,
                dbi->host_info.dbi_c_params.rw_timing.cs_setup_time << 20);
        mtk_dbi_mask(dbi, DBI_SIF_TIMING0, CSH,
                dbi->host_info.dbi_c_params.rw_timing.cs_hold_time << 16);
        mtk_dbi_mask(dbi, DBI_SIF_TIMING0, RD1ST,
                dbi->host_info.dbi_c_params.rw_timing.read_1st << 12);
        mtk_dbi_mask(dbi, DBI_SIF_TIMING0, RD2ND,
                dbi->host_info.dbi_c_params.rw_timing.read_2nd << 8);
        mtk_dbi_mask(dbi, DBI_SIF_TIMING0, WR1ST,
                dbi->host_info.dbi_c_params.rw_timing.write_1st << 4);
        mtk_dbi_mask(dbi, DBI_SIF_TIMING0, WR2ND,
                dbi->host_info.dbi_c_params.rw_timing.write_2nd);

    } else {
        mtk_dbi_mask(dbi, DBI_PCNF0, C2RH,
                dbi->host_info.dbi_b_params.rw_timing.cs_to_read_hold_time << C2RH_SHIFT);
        mtk_dbi_mask(dbi, DBI_PCNF0, C2RS,
                dbi->host_info.dbi_b_params.rw_timing.cs_to_read_setup_time << C2RS_SHIFT);
        mtk_dbi_mask(dbi, DBI_PCNF0, RLT,
                dbi->host_info.dbi_b_params.rw_timing.read_latency_time << RLT_SHIFT);
        mtk_dbi_mask(dbi, DBI_PCNF0, C2WH,
                dbi->host_info.dbi_b_params.rw_timing.cs_to_write_hold_time << C2WH_SHIFT);
        mtk_dbi_mask(dbi, DBI_PCNF0, C2WS,
                dbi->host_info.dbi_b_params.rw_timing.cs_to_write_setup_time << C2WS_SHIFT);
        mtk_dbi_mask(dbi, DBI_PCNF0, WST,
                dbi->host_info.dbi_b_params.rw_timing.write_wait_state_time);

        mtk_dbi_mask(dbi, DBI_PDW, P0CHW,
                dbi->host_info.dbi_b_params.rw_timing.cs_remain_high_time << P0CHW_SHIFT);
    }
}

static void mtk_dbi_set_data_width(struct mtk_dbi *dbi) {
    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_PARALLEL_DBI) {
        // DBI PDW
        mtk_dbi_mask(dbi, DBI_PDW, P0DW, dbi->host_info.data_width);
    } else {
        mtk_dbi_mask(dbi, DBI_SCNF, SIF0_SIZE, dbi->host_info.data_width);
    }
}

static void mtk_dbi_set_te_control(struct mtk_dbi *dbi) {
    dbi_dev_info(dbi->dev, "[%s], TE_ENABLE=%d", __func__,
            dbi->host_info.te_control.te_enable);

    mtk_dbi_mask(dbi, DBI_TECON, TE_ENABLE,
            dbi->host_info.te_control.te_enable);
    mtk_dbi_mask(dbi, DBI_TECON, TE_EDGE,
            dbi->host_info.te_control.te_edge << 1);
    mtk_dbi_mask(dbi, DBI_TECON, TE_SYNC,
            dbi->host_info.te_control.sync_mode << 2);
    mtk_dbi_mask(dbi, DBI_TECON, TE_REPEART,
            dbi->host_info.te_control.te_repeat<< 3);
    mtk_dbi_mask(dbi, DBI_TECON, SW_TE,
            dbi->host_info.te_control.software_te << 15);
}

static void mtk_dbi_reset_unused_regs(struct mtk_dbi *dbi) {
    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_PARALLEL_DBI) {
        // Clear DBI-C register
        writel(0, dbi->regs + DBI_SCNF);
        writel(0, dbi->regs + DBI_SIF_TIMING0);
    } else {
        // Clear DBI-B register
        writel(0, dbi->regs + DBI_PDW);
        writel(0, dbi->regs + DBI_PCNF0);
    }
}

#ifdef ENABLE_DBI_PATTERN
static void mtk_dbi_set_pattern(struct mtk_dbi *dbi) {
    dbi_dev_info(dbi->dev, "[%s], value=0x00ff0041", __func__);

    writel(0x00ff0041, dbi->regs + DBI_PATTERN);
}
#endif

static void mtk_dbi_send_cmd(
        struct drm_panel *panel, unsigned int cmd) {
    struct mtk_dbi *dbi = connector_to_dbi(panel->connector);

    dbi_dev_info(dbi->dev, "[%s]ctrl_type=%d, cmd=%02X", __func__,
            dbi->host_info.ctrl_type, cmd);

    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI) {
        writel(cmd, dbi->regs + DBI_SCMD0);
    } else {    // DBI-B
        writel(cmd, dbi->regs + DBI_PCMD0);
    }
}

static void mtk_dbi_send_data(
        struct drm_panel *panel, unsigned int data) {
    struct mtk_dbi *dbi = connector_to_dbi(panel->connector);

    dbi_dev_info(dbi->dev, "[%s]ctrl_type=%d, data=%02X", __func__,
            dbi->host_info.ctrl_type, data);

    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI) {
        writel(data, dbi->regs + DBI_SDAT0);
    } else {    // DBI-B
        writel(data, dbi->regs + DBI_PDAT0);
    }
}

static unsigned int mtk_dbi_read_data(struct drm_panel *panel) {
    struct mtk_dbi *dbi = connector_to_dbi(panel->connector);
    unsigned int data = 0;

    if (dbi->host_info.ctrl_type == MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI) {
        data = readl(dbi->regs + DBI_SDAT0);
    } else {    // DBI-B
        data = readl(dbi->regs + DBI_PDAT0);
    }

    dbi_dev_info(dbi->dev, "[%s]ctrl_type=%d, data=%02X", __func__,
            dbi->host_info.ctrl_type, data);

    return data;
}

static void mtk_dbi_occupy_data_pin(struct drm_panel *panel) {
    struct mtk_dbi *dbi = connector_to_dbi(panel->connector);

    dbi_dev_info(dbi->dev, "[%s]ctrl_type=%d", __func__, dbi->host_info.ctrl_type);

    mtk_dbi_lock_data_pin(dbi);
}

static void mtk_dbi_release_data_pin(struct drm_panel *panel) {
    struct mtk_dbi *dbi = connector_to_dbi(panel->connector);

    dbi_dev_info(dbi->dev, "[%s]ctrl_type=%d", __func__, dbi->host_info.ctrl_type);

    mtk_dbi_unlock_data_pin(dbi);
}

static void mtk_dbi_set_interrupt_enable(struct mtk_dbi *dbi)
{
    // TODO: not sure which should be enabled.
    u32 inten = DBI_INT_FLAG_CPL;

    if (dbi->host_info.te_control.te_enable == 1) {
        inten = inten | DBI_INT_FLAG_SYNC | DBI_INT_FLAG_TE;
    }

    dbi_dev_info(dbi->dev, "[%s] inten=%d", __func__, inten);

    writel(inten, dbi->regs + DBI_INTEN);
}

#if 0
static void mtk_dbi_irq_data_set(struct mtk_dbi *dbi, u32 irq_bit)
{
    dbi->irq_data |= irq_bit;
}
#endif

static irqreturn_t mtk_dbi_irq(int irq, void *dev_id)
{
    struct mtk_dbi *dbi = dev_id;
    u32 status;
    u32 flag = DBI_INT_FLAG_CPL | DBI_INT_FLAG_SYNC | DBI_INT_FLAG_TE;

    status = readl(dbi->regs + DBI_INTSTA) & flag;

    // dbi_dev_info(dbi->dev, "[%s] status=%d", __func__, status);

    if (status) {


        mtk_dbi_mask(dbi, DBI_INTSTA, status, 0);

        if (status & DBI_INT_FLAG_CPL) {
            mtk_dbi_unlock_data_pin(dbi);
        }


    }

    return IRQ_HANDLED;
}

static void mtk_dbi_copy_host_info_data(
        struct mipi_dbi_device *target, struct mipi_dbi_device *src) {
    target->ctrl_type = src->ctrl_type;
    target->driving_current = src->driving_current;
    target->clk_freq = src->clk_freq;

    // Color control
    target->data_width = src->data_width;
    target->color_order = src->color_order;
    target->color_format = src->color_format;

    // Tearing control
    target->te_control.software_te
            = src->te_control.software_te;
    target->te_control.te_repeat
            = src->te_control.te_repeat;
    target->te_control.sync_mode
            = src->te_control.sync_mode;
    target->te_control.te_edge
            = src->te_control.te_edge;
    target->te_control.te_enable
            = src->te_control.te_enable;

    if (src->ctrl_type == MTK_DBI_LCM_CTRL_TYPE_PARALLEL_DBI) {
        // Interface size
        target->dbi_b_params.itf_size = src->dbi_b_params.itf_size;

        // Timing
        target->dbi_b_params.rw_timing.cs_to_read_hold_time
                = src->dbi_b_params.rw_timing.cs_to_read_hold_time;
        target->dbi_b_params.rw_timing.cs_to_read_setup_time
                = src->dbi_b_params.rw_timing.cs_to_read_setup_time;
        target->dbi_b_params.rw_timing.read_latency_time
                = src->dbi_b_params.rw_timing.read_latency_time;
        target->dbi_b_params.rw_timing.cs_to_write_hold_time
                = src->dbi_b_params.rw_timing.cs_to_write_hold_time;
        target->dbi_b_params.rw_timing.cs_to_write_setup_time
                = src->dbi_b_params.rw_timing.cs_to_write_setup_time;
        target->dbi_b_params.rw_timing.write_wait_state_time
                = src->dbi_b_params.rw_timing.write_wait_state_time;
        target->dbi_b_params.rw_timing.cs_remain_high_time
                = src->dbi_b_params.rw_timing.cs_remain_high_time;
    } else if (MTK_DBI_LCM_CTRL_TYPE_SERIAL_DBI) {
        // Wire number
        target->dbi_c_params.wire_num = src->dbi_c_params.wire_num;

        // Timing
        target->dbi_c_params.rw_timing.cs_setup_time
                = src->dbi_c_params.rw_timing.cs_setup_time;
        target->dbi_c_params.rw_timing.cs_hold_time
                = src->dbi_c_params.rw_timing.cs_hold_time;
        target->dbi_c_params.rw_timing.read_1st
                = src->dbi_c_params.rw_timing.read_1st;
        target->dbi_c_params.rw_timing.read_2nd
                = src->dbi_c_params.rw_timing.read_2nd;
        target->dbi_c_params.rw_timing.write_1st
                = src->dbi_c_params.rw_timing.write_1st;
        target->dbi_c_params.rw_timing.write_2nd
                = src->dbi_c_params.rw_timing.write_2nd;
    }

}

static int mtk_dbi_poweron(struct mtk_dbi *dbi) {
    int ret = 0;
    struct clk* parent_clk;

    dbi_dev_info(dbi->dev, "[%s] Enter\n", __func__);

    // Enable clk
    ret = clk_prepare_enable(dbi->mm_clk);
    if (ret < 0) {
        dev_err(dbi->dev, "Failed to enable mm clock: %d\n", ret);
        goto err_disable_mm_clk;
    }

    ret = clk_prepare_enable(dbi->mm_dbi_clk);
    if (ret < 0) {
        dev_err(dbi->dev, "Failed to enable mm mm_dbi_clk: %d\n", ret);
        goto err_disable_mm_dbi_clk;
    }

    ret = clk_prepare_enable(dbi->mm_smi_clk);
    if (ret < 0) {
        dev_err(dbi->dev, "Failed to enable mm_smi_clk clock: %d\n", ret);
        goto err_disable_mm_smi_clk;
    }

    ret = clk_prepare_enable(dbi->infra_nfi_clk);
    if (ret < 0) {
        dev_err(dbi->dev, "Failed to enable infra_nfi_clk clock: %d\n", ret);
        goto err_disable_infra_nfi_clk;
    }

    // Set DBI clock MUX and parent clk
    ret = clk_prepare_enable(dbi->engine_clk);
    if (ret < 0) {
        dev_err(dbi->dev, "Failed to engine_clk clock: %d\n", ret);
        goto err_disable_engine_clk;
    }

	switch (dbi->host_info.clk_freq) {
	case MTK_DBI_LCM_CLOCK_FREQ_26M:
		parent_clk = dbi->parent_clk_26;
		break;
	case MTK_DBI_LCM_CLOCK_FREQ_78M:
		parent_clk = dbi->parent_clk_78;
		break;
	case MTK_DBI_LCM_CLOCK_FREQ_91M:
		parent_clk = dbi->parent_clk_91;
		break;
	case MTK_DBI_LCM_CLOCK_FREQ_104M:
		parent_clk = dbi->parent_clk_104;
		break;
	case MTK_DBI_LCM_CLOCK_FREQ_125M:
		parent_clk = dbi->parent_clk_125;
		break;
	case MTK_DBI_LCM_CLOCK_FREQ_137M:
		parent_clk = dbi->parent_clk_137;
		break;
	case MTK_DBI_LCM_CLOCK_FREQ_156M:
		parent_clk = dbi->parent_clk_156;
		break;
	case MTK_DBI_LCM_CLOCK_FREQ_182M:
		parent_clk = dbi->parent_clk_182;
		break;
	default:
		dev_err(dbi->dev, "Unknown clock freq: %d\n",
			dbi->host_info.clk_freq);
		goto err_disable_engine_clk;
	}

    ret = clk_set_parent(dbi->engine_clk, parent_clk);
    if (ret < 0) {
        dev_err(dbi->dev, "Failed to parent clock: %d\n", ret);
        goto err_disable_engine_clk;
    }

    dbi_dev_info(dbi->dev, "[%s] set clock done.\n", __func__);

    mtk_dbi_reset_unused_regs(dbi);

    mtk_dbi_set_roi_cmd_data(dbi);
    mtk_dbi_set_timing(dbi);
    mtk_dbi_set_roi_con(dbi);
    mtk_dbi_set_data_width(dbi);
    mtk_dbi_set_roi_size(dbi);
    mtk_dbi_set_te_control(dbi);
    mtk_dbi_set_interrupt_enable(dbi);

    // Add for debug
    mtk_dbi_dump_regs(dbi);
    if (dbi->panel) {
        if (drm_panel_prepare(dbi->panel) < 0) {
            DRM_ERROR("failed to prepare the panel\n");
            ret = -1;
            goto err_disable_engine_clk;
        }
    }

    dbi_dev_info(dbi->dev, "[%s] Exit (ret = %d)\n", __func__, ret);

    return 0;

err_disable_engine_clk:
    clk_disable_unprepare(dbi->engine_clk);
err_disable_infra_nfi_clk:
    clk_disable_unprepare(dbi->infra_nfi_clk);
err_disable_mm_smi_clk:
    clk_disable_unprepare(dbi->mm_smi_clk);
err_disable_mm_dbi_clk:
    clk_disable_unprepare(dbi->mm_dbi_clk);
err_disable_mm_clk:
    clk_disable_unprepare(dbi->mm_clk);

    dbi_dev_info(dbi->dev, "[%s] Exit (ret = %d)\n", __func__, ret);
    return ret;
}

static int mtk_dbi_poweroff(struct mtk_dbi *dbi) {
    int ret = 0;

    mtk_dbi_stop(dbi);

    if (dbi->panel) {
        if (drm_panel_unprepare(dbi->panel)) {
            DRM_ERROR("failed to unprepare the panel\n");
            return RETURN_ERROR;
        }
    }

    // Disable clk
    clk_disable_unprepare(dbi->engine_clk);
    clk_disable_unprepare(dbi->infra_nfi_clk);
    clk_disable_unprepare(dbi->mm_smi_clk);
    clk_disable_unprepare(dbi->mm_dbi_clk);
    clk_disable_unprepare(dbi->mm_clk);

    return ret;
}

static void mtk_dbi_ddp_prepare(struct mtk_ddp_comp *comp) {
    struct mtk_dbi *dbi = container_of(comp, struct mtk_dbi, ddp_comp);

    mtk_dbi_poweron(dbi);
}

static void mtk_dbi_ddp_unprepare(struct mtk_ddp_comp *comp) {
    struct mtk_dbi *dbi = container_of(comp, struct mtk_dbi, ddp_comp);

    mtk_dbi_poweroff(dbi);
}

static int mtk_dbi_connector_get_modes(struct drm_connector *connector)
{
    struct mtk_dbi *dbi = connector_to_dbi(connector);

    return drm_panel_get_modes(dbi->panel);
}

static const struct drm_connector_funcs mtk_dbi_connector_funcs = {
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy = drm_connector_cleanup,
    .reset = drm_atomic_helper_connector_reset,
    .atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs
    mtk_dbi_connector_helper_funcs = {
    .get_modes = mtk_dbi_connector_get_modes,
};


static int mtk_dbi_create_connector(struct drm_device *drm, struct mtk_dbi *dbi)
{
    int ret;

    ret = drm_connector_init(drm, &dbi->conn, &mtk_dbi_connector_funcs,
                 DRM_MODE_CONNECTOR_DBI);
    if (ret) {
        DRM_ERROR("Failed to connector init to drm\n");
        return ret;
    }

    drm_connector_helper_add(&dbi->conn, &mtk_dbi_connector_helper_funcs);

    dbi->conn.dpms = DRM_MODE_DPMS_OFF;
    drm_connector_attach_encoder(&dbi->conn, &dbi->encoder);

    if (dbi->panel) {
        ret = drm_panel_attach(dbi->panel, &dbi->conn);
        if (ret) {
            DRM_ERROR("Failed to attach panel to drm\n");
            goto err_connector_cleanup;
        }
    }

    return 0;

err_connector_cleanup:
    drm_connector_cleanup(&dbi->conn);
    dev_err(dbi->dev, "[%s] ret = %d", __func__, ret);
    return ret;
}

static bool mtk_dbi_encoder_mode_fixup(struct drm_encoder *encoder,
                       const struct drm_display_mode *mode,
                       struct drm_display_mode *adjusted_mode)
{
    return true;
}

static void mtk_dbi_encoder_mode_set(struct drm_encoder *encoder,
                     struct drm_display_mode *mode,
                     struct drm_display_mode *adjusted)
{
    struct mtk_dbi *dbi = encoder_to_dbi(encoder);

    dbi->mode.clock = mode->clock;
    dbi->mode.hdisplay = mode->hdisplay;
    dbi->mode.vdisplay = mode->vdisplay;
    dbi->mode.height_mm = mode->height_mm;
    dbi->mode.width_mm = mode->width_mm;
}

static void mtk_output_dbi_enable(struct mtk_dbi *dbi) {
    int ret;

    dbi_dev_info(dbi->dev, "[%s] Enter, enabled=%d\n", __func__, dbi->enabled);

    if (dbi->enabled)
        return;

    ret = mtk_dbi_poweron(dbi);
    if (ret < 0) {
        DRM_ERROR("failed to power on dbi\n");
        return;
    }

#ifdef ENABLE_DBI_PATTERN
    mtk_dbi_set_pattern(dbi);
#endif

    /*mtk_dbi_start(dbi);

    if (dbi->panel) {
        if (drm_panel_enable(dbi->panel)) {
            DRM_ERROR("failed to enable the panel\n");
            goto err_dbi_power_off;
        }
    }*/

    dbi->enabled = true;

    // Add for debug
    //mtk_dbi_dump_regs(dbi);

    dbi_dev_info(dbi->dev, "[%s] Exit\n", __func__);

    return;
//err_dbi_power_off:
//    mtk_dbi_stop(dbi);
//    mtk_dbi_poweroff(dbi);
}

static void mtk_dbi_encoder_enable(struct drm_encoder *encoder)
{
    struct mtk_dbi *dbi = encoder_to_dbi(encoder);

    mtk_output_dbi_enable(dbi);
}

static void mtk_output_dbi_disable(struct mtk_dbi *dbi)
{
    dbi_dev_info(dbi->dev, "[%s] Enter, enabled=%d\n", __func__, dbi->enabled);

    if (!dbi->enabled)
        return;

    if (dbi->panel) {
        if (drm_panel_disable(dbi->panel)) {
            DRM_ERROR("failed to disable the panel\n");
            return;
        }

        dbi->panel_enabled = false;
    }

    mtk_dbi_poweroff(dbi);

    dbi->enabled = false;

    dbi_dev_info(dbi->dev, "[%s] Exit\n", __func__);
}

static void mtk_dbi_encoder_disable(struct drm_encoder *encoder)
{
    struct mtk_dbi *dbi = encoder_to_dbi(encoder);

    mtk_output_dbi_disable(dbi);
}

static int mtk_dbi_encoder_atomic_check	(struct drm_encoder *encoder,
			    struct drm_crtc_state *crtc_state,
			    struct drm_connector_state *conn_state)
{
    struct mtk_dbi *dbi = encoder_to_dbi(encoder);

    dbi_dev_info(dbi->dev, "[%s] Enter\n", __func__);

    if (dbi->enabled) {
        dbi->need_update = true;
    }

    return 0;
}

static const struct drm_encoder_funcs mtk_dbi_encoder_funcs = {
    .destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs mtk_dbi_encoder_helper_funcs = {
    .mode_fixup = mtk_dbi_encoder_mode_fixup,
    .mode_set = mtk_dbi_encoder_mode_set,
    .disable = mtk_dbi_encoder_disable,
    .enable = mtk_dbi_encoder_enable,
    .atomic_check = mtk_dbi_encoder_atomic_check,
};

static int mtk_dbi_create_conn_enc(struct drm_device *drm, struct mtk_dbi *dbi) {
    int ret;

    ret = drm_encoder_init(drm, &dbi->encoder, &mtk_dbi_encoder_funcs,
                   DRM_MODE_ENCODER_DBI, NULL);
    if (ret) {
        DRM_ERROR("Failed to encoder init to drm\n");
        return ret;
    }
    drm_encoder_helper_add(&dbi->encoder, &mtk_dbi_encoder_helper_funcs);

    /*
     * Currently display data paths are statically assigned to a crtc each.
     * crtc 0 is OVL0 -> RDMA0 -> PQ modules -> DBI0 -> PWM
     */
    dbi->encoder.possible_crtcs =
        mtk_drm_find_possible_crtc_by_comp(drm, dbi->ddp_comp);
    dbi_dev_info(dbi->dev, "[%s] dbi->encoder.possible_crtcs:%d\n", __func__, dbi->encoder.possible_crtcs);

    /* If there's a bridge, attach to it and let it create the connector */
    if (dbi->bridge) {
        ret = drm_bridge_attach(&dbi->encoder, dbi->bridge, NULL);
        if (ret) {
            DRM_ERROR("Failed to attach bridge to drm\n");
            goto err_encoder_cleanup;
        }
    } else {
        /* Otherwise create our own connector and attach to a panel */
        ret = mtk_dbi_create_connector(drm, dbi);
        if (ret)
            goto err_encoder_cleanup;
    }

    return 0;

err_encoder_cleanup:
    drm_encoder_cleanup(&dbi->encoder);
    dev_err(dbi->dev, "[%s] ret=%d\n", __func__, ret);
    return ret;
}

#ifdef ENABLE_UT // Add for testing
static void mtk_dbi_enable_for_testing(struct mtk_dbi *dbi) {
    dbi->host_info.ctrl_type = MTK_DBI_LCM_CTRL_TYPE_PARALLEL_DBI;
    dbi->host_info.clk_freq = MTK_DBI_LCM_CLOCK_FREQ_125M;
    dbi->host_info.driving_current = MTK_DBI_LCM_DRIVING_CURRENT_6MA;
    dbi->host_info.data_width = MTK_DBI_LCM_DATA_WIDTH_18BITS;
    dbi->host_info.color_order = MTK_DBI_LCM_COLOR_ORDER_RGB;
    dbi->host_info.color_format = MTK_DBI_LCM_COLOR_FORMAT_RGB666;

    dbi->host_info.te_control.software_te = 0;
    dbi->host_info.te_control.te_repeat = 0;
    dbi->host_info.te_control.sync_mode = 1;
    dbi->host_info.te_control.te_edge = 0;
    dbi->host_info.te_control.te_enable = 1;

    dbi->host_info.dbi_b_params.itf_size = MTK_DBI_LCM_INTERFACE_SIZE_9BITS;
    dbi->host_info.dbi_b_params.rw_timing.cs_to_read_hold_time = 1;
    dbi->host_info.dbi_b_params.rw_timing.cs_to_read_setup_time = 1;
    dbi->host_info.dbi_b_params.rw_timing.read_latency_time = 4;
    dbi->host_info.dbi_b_params.rw_timing.cs_to_write_hold_time = 1;
    dbi->host_info.dbi_b_params.rw_timing.cs_to_write_setup_time = 0;
    dbi->host_info.dbi_b_params.rw_timing.write_wait_state_time = 5;

    dbi->mode.hdisplay = 320;
    dbi->mode.vdisplay = 480;

    mtk_dbi_dump_regs(dbi);

    mtk_dbi_set_gpio(dbi);
    mtk_dbi_set_driving_current(dbi);
    mtk_output_dbi_enable(dbi);

    mtk_dbi_dump_regs(dbi);
}
#endif

static int mtk_dbi_bind(
        struct device *dev, struct device *master, void *data) {
    int ret;
    struct drm_device *drm = data;
    struct mtk_dbi *dbi = dev_get_drvdata(dev);

    dbi_dev_info(dbi->dev, "[%s] enter\n", __func__);

    ret = mtk_ddp_comp_register(drm, &dbi->ddp_comp);
    if (ret < 0) {
        dev_err(dev, "Failed to register component %pOF: %d\n",
            dev->of_node, ret);
        return ret;
    }

    ret = mtk_dbi_create_conn_enc(drm, dbi);
    if (ret) {
        DRM_ERROR("Encoder create failed with %d\n", ret);
        goto err_unregister;
    }

    dbi_dev_info(dbi->dev, "[%s] exit\n", __func__);

    mtk_dbi_dump_regs(dbi);

    return 0;

err_unregister:
    dev_err(dbi->dev, "[%s] ret=%d\n", __func__, ret);
    mtk_ddp_comp_unregister(drm, &dbi->ddp_comp);
    return ret;
}

static void mtk_dbi_destroy_conn_enc(struct mtk_dbi *dbi)
{
    drm_encoder_cleanup(&dbi->encoder);
    /* Skip connector cleanup if creation was delegated to the bridge */
    if (dbi->conn.dev)
        drm_connector_cleanup(&dbi->conn);
    if (dbi->panel)
        drm_panel_detach(dbi->panel);
}

static void mtk_dbi_unbind(
        struct device *dev, struct device *master, void *data) {
    struct drm_device *drm = data;
    struct mtk_dbi *dbi = dev_get_drvdata(dev);

    mtk_dbi_destroy_conn_enc(dbi);
    mtk_ddp_comp_unregister(drm, &dbi->ddp_comp);
}

static void mtk_dbi_ddp_atomic_flush(struct mtk_ddp_comp *comp)
{
    struct mtk_dbi *dbi = container_of(comp, struct mtk_dbi, ddp_comp);

    dbi_dev_info(dbi->dev, "[%s] enter, need_update=%d", __func__, dbi->need_update);

    if (dbi->need_update && dbi->enabled) {
        mtk_dbi_start(dbi);

        // Add for testing
        if (!dbi->panel_enabled) {
            if (dbi->panel) {
                if (drm_panel_enable(dbi->panel)) {
                    DRM_ERROR("failed to enable the panel\n");
                }
            }

            dbi->panel_enabled = true;
        }

        dbi->need_update = false;
    }
}

static const struct mtk_ddp_comp_funcs mtk_dbi_funcs = {
    .prepare = mtk_dbi_ddp_prepare,
    .unprepare = mtk_dbi_ddp_unprepare,
    .atomic_flush = mtk_dbi_ddp_atomic_flush,
};

static const struct component_ops mtk_dbi_component_ops = {
    .bind = mtk_dbi_bind,
    .unbind = mtk_dbi_unbind,
};

static const struct mipi_dbi_host_ops mtk_dbi_ops = {
    .send_cmd = mtk_dbi_send_cmd,
    .send_data = mtk_dbi_send_data,
    .read_data = mtk_dbi_read_data,
    .occupy_data_pin = mtk_dbi_occupy_data_pin,
    .release_data_pin = mtk_dbi_release_data_pin,
};

void mtk_dbi_host_attach(struct drm_panel *panel,
        struct mipi_dbi_device *host,
        const struct mipi_dbi_host_ops **ops) {

    *ops = &mtk_dbi_ops;

    _panel = panel;
    _panel_attached = true;
    mtk_dbi_copy_host_info_data(&_host_info, host);
}

void mtk_dbi_host_detach(struct mipi_dbi_device *host) {
    //host.ops = &mtk_dbi_ops;
}

static int mtk_dbi_probe(struct platform_device *pdev) {
    struct mtk_dbi *dbi;
    struct device *dev = &pdev->dev;
    const struct of_device_id *of_id;

    struct resource *regs;
    int irq_num;
    int comp_id;
    int ret;
    //struct regmap *regmap;

    dbi = devm_kzalloc(dev, sizeof(*dbi), GFP_KERNEL);
    if (!dbi)
        return -ENOMEM;

    dbi->dev = dev;
    dbi->need_update = true;

#ifndef CONFIG_DRM_DSI_SUPPORT
    dev_info(dbi->dev, "[%s] setting panel, panel_attached=%d\n", __func__, _panel_attached);

    if (!_panel_attached) {
        ret = -EPROBE_DEFER;
        dev_err(dbi->dev, "[%s] panel not attached, retry again\n", __func__);
        goto error;
    }
    dbi->panel = _panel;
#endif

    mtk_dbi_copy_host_info_data(&dbi->host_info, &_host_info);
    mtk_dbi_set_gpio(dbi);
    mtk_dbi_set_driving_current(dbi);

    of_id = of_match_device(mtk_dbi_of_match, &pdev->dev);

    dbi->mm_clk = devm_clk_get(dev, "clk_mm_dbpi");
    if (IS_ERR(dbi->mm_clk)) {
        ret = PTR_ERR(dbi->mm_clk);
        dev_err(dev, "Failed to get mm clock: %d\n", ret);
        goto error;
    }

    dbi->mm_dbi_clk = devm_clk_get(dev, "clk_mm_dbi");
    if (IS_ERR(dbi->mm_dbi_clk)) {
        ret = PTR_ERR(dbi->mm_dbi_clk);
        dev_err(dev, "Failed to get clk_mm_dbi clock: %d\n", ret);
        goto error;
    }

    dbi->mm_smi_clk = devm_clk_get(dev, "clk_smi_common");
    if (IS_ERR(dbi->mm_smi_clk)) {
        ret = PTR_ERR(dbi->mm_smi_clk);
        dev_err(dev, "Failed to get mm_smi_clk clock: %d\n", ret);
        goto error;
    }


    dbi->infra_nfi_clk = devm_clk_get(dev, "clk_infra_nfi");
    if (IS_ERR(dbi->infra_nfi_clk)) {
        ret = PTR_ERR(dbi->infra_nfi_clk);
        dev_err(dev, "Failed to get clk_infra_nfi: %d\n", ret);
        goto error;
    }

    // Get clock and top clock
    dbi->engine_clk = devm_clk_get(dev, "clk_engine");
    if (IS_ERR(dbi->engine_clk)) {
        ret = PTR_ERR(dbi->engine_clk);
        dev_err(dev, "Failed to get engine_clk: %d\n", ret);
        goto error;
    }

	dbi->parent_clk_26 = devm_clk_get(dev, "clk_top_26");
	if (IS_ERR(dbi->parent_clk_26)) {
		ret = PTR_ERR(dbi->parent_clk_26);
		dev_err(dev, "Failed to get clk_top_26: %d\n", ret);
		goto error;
	}

	dbi->parent_clk_78 = devm_clk_get(dev, "clk_top_78");
	if (IS_ERR(dbi->parent_clk_78)) {
		ret = PTR_ERR(dbi->parent_clk_78);
		dev_err(dev, "Failed to get clk_top_78: %d\n", ret);
		goto error;
	}

	dbi->parent_clk_91 = devm_clk_get(dev, "clk_top_91");
	if (IS_ERR(dbi->parent_clk_91)) {
		ret = PTR_ERR(dbi->parent_clk_91);
		dev_err(dev, "Failed to get clk_top_91: %d\n", ret);
		goto error;
	}

	dbi->parent_clk_104 = devm_clk_get(dev, "clk_top_104");
	if (IS_ERR(dbi->parent_clk_104)) {
		ret = PTR_ERR(dbi->parent_clk_104);
		dev_err(dev, "Failed to get clk_top_104: %d\n", ret);
		goto error;
	}

	dbi->parent_clk_125 = devm_clk_get(dev, "clk_top_125");
	if (IS_ERR(dbi->parent_clk_125)) {
		ret = PTR_ERR(dbi->parent_clk_125);
		dev_err(dev, "Failed to get clk_top_125: %d\n", ret);
		goto error;
	}

	dbi->parent_clk_137 = devm_clk_get(dev, "clk_top_137");
	if (IS_ERR(dbi->parent_clk_137)) {
		ret = PTR_ERR(dbi->parent_clk_137);
		dev_err(dev, "Failed to get clk_top_137: %d\n", ret);
		goto error;
	}

	dbi->parent_clk_156 = devm_clk_get(dev, "clk_top_156");
	if (IS_ERR(dbi->parent_clk_156)) {
		ret = PTR_ERR(dbi->parent_clk_156);
		dev_err(dev, "Failed to get clk_top_156: %d\n", ret);
		goto error;
	}

	dbi->parent_clk_182 = devm_clk_get(dev, "clk_top_182");
	if (IS_ERR(dbi->parent_clk_182)) {
		ret = PTR_ERR(dbi->parent_clk_182);
		dev_err(dev, "Failed to get clk_top_182: %d\n", ret);
		goto error;
	}

    regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    dbi->regs = devm_ioremap_resource(dev, regs);
    if (IS_ERR(dbi->regs)) {
        ret = PTR_ERR(dbi->regs);
        dev_err(dev, "Failed to ioremap memory: %d\n", ret);
        goto error;
    }

    comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DBI);
    if (comp_id < 0) {
        dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
        ret = comp_id;
        goto error;
    }

    dbi_dev_info(dbi->dev, "[%s] comp_id: %d\n", __func__, comp_id);

    ret = mtk_ddp_comp_init(dev, dev->of_node, &dbi->ddp_comp, comp_id,
                &mtk_dbi_funcs);
    if (ret) {
        dev_err(dev, "Failed to initialize component: %d\n", ret);
        goto error;
    }

    irq_num = platform_get_irq(pdev, 0);
    if (irq_num < 0) {
        dev_err(&pdev->dev, "failed to get dbi irq_num: %d\n", irq_num);
        ret = irq_num;
        goto error;
    }


    irq_set_status_flags(irq_num, IRQ_TYPE_LEVEL_HIGH);

    init_waitqueue_head(&dbi->irq_wait_queue);
    ret = devm_request_irq(&pdev->dev, irq_num, mtk_dbi_irq,
                   IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), dbi);
    if (ret) {
        dev_err(&pdev->dev, "failed to request mediatek dbi irq\n");
        goto error;
    }

    platform_set_drvdata(pdev, dbi);

    ret = component_add(&pdev->dev, &mtk_dbi_component_ops);
    if (ret) {
        dev_err(&pdev->dev, "failed to add component: %d\n", ret);
        goto error;
    }

    return 0;

error:
    dev_err(dbi->dev, "[%s] exit, ret=%d\n", __func__, ret);
    return ret;
}

static int mtk_dbi_remove(struct platform_device *pdev) {
    struct mtk_dbi *dbi = platform_get_drvdata(pdev);

    mtk_output_dbi_disable(dbi);
    mtk_dbi_stop(dbi);
    mtk_dbi_poweroff(dbi);
    component_del(&pdev->dev, &mtk_dbi_component_ops);

    return 0;
}

struct platform_driver mtk_dbi_driver = {
    .probe = mtk_dbi_probe,
    .remove = mtk_dbi_remove,
    .driver = {
        .name = "mtk-dbi",
        .of_match_table = mtk_dbi_of_match,
    },
};
