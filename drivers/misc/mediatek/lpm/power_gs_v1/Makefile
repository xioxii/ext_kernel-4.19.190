#
# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/power_gs_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-$(CONFIG_MTK_LPM_DBG_COMMON) += mtk_power_gs.o
obj-$(CONFIG_MTK_LPM_DBG_COMMON) += mtk_power_gs_api.o
obj-$(CONFIG_MTK_LPM_MT6880) += mt6880/

