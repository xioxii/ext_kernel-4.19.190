// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: jitao.shi <jitao.shi@mediatek.com>
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_panel.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"

/* LVDS TOP */
#define LVDSTOP_REG00	0x000
#define LVDSTOP_REG01	0x004
#define LVDSTOP_REG02	0x008
#define LVDSTOP_REG03	0x00c
#define LVDSTOP_REG04	0x010
#define LVDSTOP_REG05	0x014
#define RG_LVDS_CLKDIV_CTRL	(0xf << 23)
#define RG_FIFO_CTRL	(0x3 << 20)
#define RG_FIFO_EN	(3 << 16)

/* PATTERN GEN */
#define PATGEN_REG00	0x604
#define PATGEN_REG01	0x608
#define PATGEN_REG02	0x60c
#define PATGEN_REG03	0x610
#define PATGEN_REG04	0x614
#define PATGEN_REG05	0x618
#define PATGEN_REG06	0x620

#define CFG_REG00	0x700
#define DETECT_REG0	0x704
#define DETECT_REG1	0x708
#define DETECT_REG2	0x70c
#define TD_CTRL_MON	0x714
#define CRC_CHECK_REG0	0x738
#define CRC_CHECK_REG1	0x73c
#define CRC_CHECK_REG2	0x740
#define DETECT_REG3	0x750
#define DETECT_REG4	0x754

#define MODE0	0x800
#define LVDS_CTRL	0x814
#define ANA_TEST	0x824

#define LVDS_CTRL00	0xa00
#define RG_NS_VESA_EN	BIT(1)
#define RG_DUAL			BIT(12)

#define LVDS_CTRL01	0xa04
#define LVDS_CTRL02	0xa08
#define RG_DPMODE	BIT(12)
#define RG_LVDS_74FIFO_EN	BIT(13)

#define CRC0	0xa10
#define CRC1	0xa14
#define CRC2	0xa18
#define CRC3	0xa1c
#define LVDS_TEST01	0xa30
#define LVDS_TEST02	0xa34
#define CRC4	0xa38
#define CRC5	0xa3c
#define CRC6	0xa40

#define LVDSTX_REG00	0xa80

#define LLV_DO_SEL	0x904
#define CKO_SEL	0x90c
#define PN_SWAP	0x930
#define LVDS_CRC0	0x934
#define LVDS_CRC1	0x938
#define LVDS_CRC2	0x93c
#define LVDS_CRC3	0x940
#define LVDS_CRC4	0x944
#define LVDS_CRC5	0x948
#define LVDS_CRC6	0x94c

#define LVDS_MAX_LANE_CNT	8

struct mtk_lvds {
	struct mtk_ddp_comp ddp_comp;
	struct drm_bridge bridge;
	struct drm_connector connector;
	struct drm_panel *panel;
	struct device *dev;
	struct phy *phy;
	struct clk *pix_clk_gate;
	struct clk *cts_clk_gate;
	struct clk *lvdsdpi_sel;
	struct clk *lvds_d1;
	struct clk *lvds_d2;
	struct drm_display_mode mode;
	u32 lane_count;
	u32 lvds_fmt;
	void __iomem *regs;
	bool powered;
	bool enabled;
	bool is_dual;
	int refcount;
	bool scp_ctrl;
};

static inline struct mtk_lvds *
		bridge_to_lvds(struct drm_bridge *bridge)
{
	return container_of(bridge, struct mtk_lvds, bridge);
}

static inline struct mtk_lvds *
		connector_to_lvds(struct drm_connector *connector)
{
	return container_of(connector, struct mtk_lvds, connector);
}

static enum drm_connector_status mtk_lvds_connector_detect(
	struct drm_connector *connector, bool force)
{
	struct mtk_lvds *lvds = connector_to_lvds(connector);

	if (lvds->panel != NULL)
		return connector_status_connected;

	return connector_status_disconnected;
}

static const struct drm_connector_funcs mtk_lvds_connector_funcs = {
	.detect = mtk_lvds_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int mtk_lvds_connector_get_modes(struct drm_connector *connector)
{
	struct mtk_lvds *lvds = connector_to_lvds(connector);

	return drm_panel_get_modes(lvds->panel);
}

static struct drm_encoder *mtk_lvds_connector_best_encoder(
		struct drm_connector *connector)
{
	struct mtk_lvds *lvds = connector_to_lvds(connector);

	return lvds->bridge.encoder;
}

static const struct drm_connector_helper_funcs
		mtk_lvds_connector_helper_funcs = {
	.get_modes = mtk_lvds_connector_get_modes,
	.best_encoder = mtk_lvds_connector_best_encoder,
};

static bool mtk_lvds_bridge_mode_fixup(struct drm_bridge *bridge,
				       const struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void mtk_lvds_poweron(struct mtk_lvds *lvds)
{
	int ret;
	u32 reg;

	if (++lvds->refcount != 1)
		return;

	ret = pm_runtime_get_sync(lvds->dev);
	if (ret < 0)
		DRM_ERROR("Failed to enable power domain: %d\n", ret);

	ret = clk_prepare_enable(lvds->lvdsdpi_sel);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to enable lvdsdpi_sel clock: %d\n",
			ret);
		goto err_phy_power_off;
	}

	usleep_range(20, 100);

	ret = clk_prepare_enable(lvds->pix_clk_gate);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to enable pixel clock gate: %d\n",
			ret);
		goto err_disable_dpisel_clk;
	}

	ret = clk_prepare_enable(lvds->cts_clk_gate);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to enable cts clock gate: %d\n",
			ret);
		goto err_disable_pix_gate_clk;
	}

	ret = phy_power_on(lvds->phy);
	if (ret != 0) {
		dev_err(lvds->dev, "Failed to power phy: %d\n",
			ret);
		goto err_disable_cts_gate_clk;
	}

	writel(RG_FIFO_EN | RG_FIFO_CTRL |
	       (lvds->is_dual ? 1UL : 0UL) << 23, lvds->regs + LVDSTOP_REG05);

	writel((lvds->is_dual ? RG_DUAL : 0UL) |
	       (lvds->lvds_fmt == MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA ? 0U :
		RG_NS_VESA_EN), lvds->regs + LVDS_CTRL00);

	reg = (readl(lvds->regs + LVDS_CTRL02) | RG_LVDS_74FIFO_EN) &
	      (~RG_DPMODE);
	writel(reg, lvds->regs + LVDS_CTRL02);

	return;
err_disable_cts_gate_clk:
	clk_disable_unprepare(lvds->cts_clk_gate);
err_disable_pix_gate_clk:
	clk_disable_unprepare(lvds->pix_clk_gate);
err_disable_dpisel_clk:
	clk_disable_unprepare(lvds->lvdsdpi_sel);
err_phy_power_off:
	lvds->refcount--;
}

static void mtk_lvds_poweroff(struct mtk_lvds *lvds)
{
	int ret;
	u32 reg;

	if (lvds->refcount == 0) {
		WARN_ON(true);
		return;
	}

	if (--lvds->refcount != 0)
		return;

	reg = (readl(lvds->regs + LVDS_CTRL02) | RG_DPMODE) &
	      (~RG_LVDS_74FIFO_EN);
	writel(reg, lvds->regs + LVDS_CTRL02);

	ret = pm_runtime_put_sync(lvds->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);

	clk_disable_unprepare(lvds->cts_clk_gate);
	clk_disable_unprepare(lvds->pix_clk_gate);
	clk_disable_unprepare(lvds->lvdsdpi_sel);
	ret = phy_power_off(lvds->phy);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);
}

static int mtk_lvds_bridge_attach(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);
	int ret;

	if (bridge->encoder == NULL) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	lvds->connector.polled = DRM_CONNECTOR_POLL_HPD;
	ret = drm_connector_init(bridge->dev, &lvds->connector,
				 &mtk_lvds_connector_funcs,
				 DRM_MODE_CONNECTOR_LVDS);
	if (ret != 0) {
		DRM_ERROR("Failed to initialize connector with drm\n");
		return ret;
	}
	drm_connector_helper_add(&lvds->connector,
				 &mtk_lvds_connector_helper_funcs);
	drm_connector_register(&lvds->connector);
	ret = drm_connector_attach_encoder(&lvds->connector,
						bridge->encoder);
	if (ret != 0) {
		DRM_ERROR("Failed to attach connector with encoder\n");
		return ret;
	}

	if (lvds->panel != NULL) {
		ret = drm_panel_attach(lvds->panel, &lvds->connector);
		if (ret != 0) {
			DRM_ERROR("Failed to attach panel with connector\n");
			return ret;
		}
	}

	return ret;
}

static void mtk_lvds_bridge_disable(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);
	int ret;

#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	if (lvds->scp_ctrl)
		return;
#endif
	if (lvds->enabled == false)
		return;

	if (lvds->panel != NULL) {
		if (drm_panel_disable(lvds->panel) != 0) {
			DRM_ERROR("failed to disable panel\n");
			return;
		}
	}
	ret = pm_runtime_put_sync(lvds->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);

	mtk_lvds_poweroff(lvds);

	lvds->enabled = false;
}

static void mtk_lvds_bridge_post_disable(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	if (lvds->scp_ctrl)
		return;
#endif

	if (lvds->powered == false)
		return;

	if (lvds->panel != NULL) {
		if (drm_panel_unprepare(lvds->panel) < 0) {
			DRM_ERROR("failed to unprepare panel\n");
			return;
		}
	}
	lvds->powered = false;
}

static void mtk_lvds_bridge_mode_set(struct drm_bridge *bridge,
				     struct drm_display_mode *mode,
				     struct drm_display_mode *adjusted_mode)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

	if (lvds->connector.display_info.num_bus_formats != 0U)
		lvds->lvds_fmt = lvds->connector.display_info.bus_formats[0];

	drm_mode_copy(&lvds->mode, adjusted_mode);
}

static void mtk_lvds_pre_enable(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	if (lvds->scp_ctrl)
		return;
#endif

	if (lvds->powered)
		return;

	if (lvds->panel != NULL) {
		if (drm_panel_prepare(lvds->panel) < 0) {
			DRM_ERROR("failed to prepare panel\n");
			return;
		}
	}

	lvds->powered = true;
}

static void mtk_lvds_bridge_enable(struct drm_bridge *bridge)
{
	struct mtk_lvds *lvds = bridge_to_lvds(bridge);

#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	if (lvds->scp_ctrl)
		return;
#endif
	if (lvds->enabled)
		return;

	mtk_lvds_poweron(lvds);

	if (lvds->panel != NULL) {
		if (drm_panel_enable(lvds->panel) < 0) {
			DRM_ERROR("failed to enable the panel\n");
			return;
		}
	}

	lvds->enabled = true;
}

static const struct drm_bridge_funcs mtk_lvds_bridge_funcs = {
	.attach = mtk_lvds_bridge_attach,
	.mode_fixup = mtk_lvds_bridge_mode_fixup,
	.disable = mtk_lvds_bridge_disable,
	.post_disable = mtk_lvds_bridge_post_disable,
	.mode_set = mtk_lvds_bridge_mode_set,
	.pre_enable = mtk_lvds_pre_enable,
	.enable = mtk_lvds_bridge_enable,
};

static void mtk_lvds_ddp_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_lvds *lvds = container_of(comp, struct mtk_lvds, ddp_comp);

#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	if (lvds->scp_ctrl)
		return;
#endif

	mtk_lvds_poweron(lvds);
}

static void mtk_lvds_ddp_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_lvds *lvds = container_of(comp, struct mtk_lvds, ddp_comp);

#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	if (lvds->scp_ctrl) {
#if defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
		lvds->scp_ctrl = false;
#endif
		return;
	}
#endif

	mtk_lvds_poweroff(lvds);
}

static const struct mtk_ddp_comp_funcs mtk_lvds_funcs = {
	.prepare = mtk_lvds_ddp_prepare,
	.unprepare = mtk_lvds_ddp_unprepare,
};

static int mtk_lvds_bind(struct device *dev, struct device *master, void *data)
{
	int ret;
	struct drm_device *drm = data;
	struct mtk_lvds *lvds = dev_get_drvdata(dev);

	ret = mtk_ddp_comp_register(drm, &lvds->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
	}

	return ret;
}

static void mtk_lvds_unbind(struct device *dev, struct device *master,
			    void *data)
{
	struct drm_device *drm = data;
	struct mtk_lvds *lvds = dev_get_drvdata(dev);

	mtk_ddp_comp_unregister(drm, &lvds->ddp_comp);
}

static const struct component_ops mtk_lvds_component_ops = {
	.bind = mtk_lvds_bind,
	.unbind = mtk_lvds_unbind,
};

static int mtk_drm_lvds_probe(struct platform_device *pdev)
{
	struct mtk_lvds *lvds;
	struct device *dev = &pdev->dev;
	struct device_node *port, *out_ep;
	struct device_node *panel_node = NULL;
	int ret;
	struct resource *mem;
	int comp_id;

	lvds = devm_kzalloc(dev, sizeof(*lvds), GFP_KERNEL);
	if (lvds == NULL)
		return -ENOMEM;

	lvds->dev = dev;

#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	lvds->scp_ctrl = of_property_read_bool(pdev->dev.of_node,
					       "scp-control");
#endif

	/* port@1 is lvds output port */
	port = of_graph_get_port_by_id(dev->of_node, 1);
	if (port != NULL) {
		out_ep = of_get_child_by_name(port, "endpoint");
		of_node_put(port);
		if (out_ep != NULL) {
			panel_node = of_graph_get_remote_port_parent(out_ep);
			of_node_put(out_ep);
		}
	}

	if (panel_node != NULL) {
		lvds->panel = of_drm_find_panel(panel_node);
		of_node_put(panel_node);
		if (!IS_ERR(lvds->panel))
			ret = 0;
		else
			lvds->panel = NULL;

		if (lvds->panel == NULL)
			return -EPROBE_DEFER;
	}

	lvds->is_dual = of_property_read_bool(dev->of_node,
					      "mediatek,dual-channel");

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	lvds->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(lvds->regs)) {
		ret = PTR_ERR(lvds->regs);
		dev_err(dev, "Failed to ioremap mem resource: %d\n", ret);
		return ret;
	}

	lvds->pix_clk_gate = devm_clk_get(dev, "pixel");
	if (IS_ERR(lvds->pix_clk_gate)) {
		ret = PTR_ERR(lvds->pix_clk_gate);
		dev_err(dev, "Failed to get pixel clock gate: %d\n", ret);
		return ret;
	}

	lvds->cts_clk_gate = devm_clk_get(dev, "cts");
	if (IS_ERR(lvds->cts_clk_gate)) {
		ret = PTR_ERR(lvds->cts_clk_gate);
		dev_err(dev, "Failed to get cts clock gate: %d\n", ret);
		return ret;
	}

	lvds->lvdsdpi_sel = devm_clk_get(dev, "lvdsdpi_sel");
	if (IS_ERR(lvds->lvdsdpi_sel)) {
		ret = PTR_ERR(lvds->lvdsdpi_sel);
		dev_err(dev, "Failed to get lvdsdpi_sel clock: %d\n", ret);
		return ret;
	}

	lvds->lvds_d1 = devm_clk_get(dev, "lvds_d1");
	if (IS_ERR(lvds->lvds_d1)) {
		ret = PTR_ERR(lvds->lvds_d1);
		dev_err(dev, "Failed to get lvds_d1 clock: %d\n", ret);
		return ret;
	}

	lvds->lvds_d2 = devm_clk_get(dev, "lvds_d2");
	if (IS_ERR(lvds->lvds_d2)) {
		ret = PTR_ERR(lvds->lvds_d2);
		dev_err(dev, "Failed to get lvds_d2 clock: %d\n", ret);
		return ret;
	}

#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	if (!lvds->scp_ctrl) {
#endif
		if (lvds->is_dual)
			ret = clk_set_parent(lvds->lvdsdpi_sel, lvds->lvds_d2);
		else
			ret = clk_set_parent(lvds->lvdsdpi_sel, lvds->lvds_d1);

		if (ret < 0) {
			dev_err(&pdev->dev, "failed to clk_set_parent (%d)\n",
				ret);
			return ret;
		}
#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT) ||\
	defined(CONFIG_MTK_SCP_FASTLOGO_ONLY)
	}
#endif

	ret = of_property_read_u32(dev->of_node, "lane-count",
				   &lvds->lane_count);

	lvds->phy = devm_phy_get(dev, "lvds");
	if (IS_ERR(lvds->phy)) {
		ret = PTR_ERR(lvds->phy);
		dev_err(dev, "Failed to get LVDS PHY: %d\n", ret);
		return ret;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_LVDS);
	if (comp_id < 0) {
		ret = comp_id;
		return ret;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &lvds->ddp_comp, comp_id,
				&mtk_lvds_funcs);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, lvds);

	lvds->bridge.funcs = &mtk_lvds_bridge_funcs;
	lvds->bridge.of_node = pdev->dev.of_node;

	drm_bridge_add(&lvds->bridge);

	pm_runtime_enable(dev);

	ret = component_add(&pdev->dev, &mtk_lvds_component_ops);
	if (ret < 0) {
		dev_err(dev, "failed to add compoent, ret = %d\n", ret);
		return ret;
	}

	return 0;
}

static int mtk_drm_lvds_remove(struct platform_device *pdev)
{
	struct mtk_lvds *lvds = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	drm_bridge_remove(&lvds->bridge);
	return 0;
}

static const struct of_device_id mtk_drm_lvds_of_ids[] = {
	{ .compatible = "mediatek,mt8173-lvds", },
	{}
};

struct platform_driver mtk_lvds_driver = {
	.probe = mtk_drm_lvds_probe,
	.remove = mtk_drm_lvds_remove,
	.driver = {
		.name = "mediatek-drm-lvds",
		.of_match_table = mtk_drm_lvds_of_ids,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

