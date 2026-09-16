// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2018-2019 MediaTek Inc.

/* A library for configuring path from GMAC/GDM to target PHY
 *
 * Author: Sean Wang <sean.wang@mediatek.com>
 *
 */

#include <linux/phy.h>
#include <linux/regmap.h>

#include "ra_soc.h"


static const char * const mtk_eth_mux_name[] = {
	"mux_gdm1_to_gmac1_esw", "mux_gmac2_gmac0_to_gephy",
	"mux_u3_gmac2_to_qphy", "mux_gmac1_gmac2_to_sgmii_rgmii",
	"mux_gmac12_to_gephy_sgmii",
};

static const char * const mtk_eth_path_name[] = {
	"gmac1_rgmii", "gmac1_trgmii", "gmac1_sgmii", "gmac2_rgmii",
	"gmac2_sgmii", "gmac2_gephy", "gdm1_esw",
};


int mtk_eth_mux_setup(struct END_DEVICE *ei_local, int path)
{


	if (!MTK_HAS_CAPS(ei_local->soc->caps, MTK_PATH_BIT(path))) {
		pr_info("path %s isn't support on the SoC\n",
			 mtk_eth_path_name[path]);
		return -EINVAL;
	}

	if (!MTK_HAS_CAPS(ei_local->soc->caps, MTK_MUX)) {
 		pr_info("mux no CAP on the SoC\n");
		return 0;
	}
	return 0;
}

int mtk_gmac_sgmii_path_setup(struct END_DEVICE *ei_local, int mac_id)
{
	unsigned int val = 0;
	int sid, err, path;

	path = (mac_id == 0) ?  MTK_ETH_PATH_GMAC1_SGMII :
				MTK_ETH_PATH_GMAC2_SGMII;

	pr_info("mtk_gmac_sgmii_path_setup %d", mac_id);

	/* Setup proper MUXes along the path */
	err = mtk_eth_mux_setup(ei_local, path);
	if (err)
		return err;

	/* The path GMAC to SGMII will be enabled once the SGMIISYS is being
	 * setup done.
	 */
	regmap_read(ei_local->ethsys, ETHSYS_SYSCFG0, &val);

	regmap_update_bits(ei_local->ethsys, ETHSYS_SYSCFG0,
			   SYSCFG0_SGMII_MASK, ~(u32)SYSCFG0_SGMII_MASK);

	/* Decide how GMAC and SGMIISYS be mapped */
	sid = (MTK_HAS_CAPS(ei_local->soc->caps, MTK_SHARED_SGMII)) ? 0 : mac_id;

	/* Setup SGMIISYS with the determined property */
	if (MTK_HAS_FLAGS(ei_local->sgmii->flags[sid], MTK_SGMII_PHYSPEED_AN))
		err = mtk_sgmii_setup_mode_an(ei_local->sgmii, sid);
	else
		err = mtk_sgmii_setup_mode_force(ei_local->sgmii, sid, ei_local);

	if (err)
		return err;

	regmap_update_bits(ei_local->ethsys, ETHSYS_SYSCFG0,
			   SYSCFG0_SGMII_MASK, 0x3 << 8);


	return 0;
}

int mtk_gmac_gephy_path_setup(struct END_DEVICE *ei_local, int mac_id)
{
	int err, path = 0;

	if (mac_id == 1)
		path = MTK_ETH_PATH_GMAC2_GEPHY;

	if (!path)
		return -EINVAL;

	/* Setup proper MUXes along the path */
	err = mtk_eth_mux_setup(ei_local, path);
	if (err)
		return err;

	return 0;
}

int mtk_gmac_rgmii_path_setup(struct END_DEVICE *ei_local, int mac_id)
{
	int err, path;

	path = (mac_id == 0) ?  MTK_ETH_PATH_GMAC1_RGMII :
				MTK_ETH_PATH_GMAC2_RGMII;

	/* Setup proper MUXes along the path */
	err = mtk_eth_mux_setup(ei_local, path);
	if (err)
		return err;

	return 0;
}

int mtk_setup_hw_path(struct END_DEVICE *ei_local, int mac_id, int phymode)
{
	int err;

	switch (phymode) {
	case PHY_INTERFACE_MODE_TRGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_REVMII:
	case PHY_INTERFACE_MODE_RMII:
		if (MTK_HAS_CAPS(ei_local->soc->caps, MTK_RGMII)) {
			err = mtk_gmac_rgmii_path_setup(ei_local, mac_id);
			if (err)
				return err;
		}
		break;
	case PHY_INTERFACE_MODE_SGMII:
		if (MTK_HAS_CAPS(ei_local->soc->caps, MTK_SGMII)) {
			err = mtk_gmac_sgmii_path_setup(ei_local, mac_id);
			if (err)
				return err;
		}
		break;
	case PHY_INTERFACE_MODE_GMII:
		if (MTK_HAS_CAPS(ei_local->soc->caps, MTK_GEPHY)) {
			err = mtk_gmac_gephy_path_setup(ei_local, mac_id);
			if (err)
				return err;
		}
		break;
	default:
		break;
	}

	return 0;
}
