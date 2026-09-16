# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_throttling/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/leakage_table_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/clk/mediatek/

obj-$(CONFIG_MTK_GPU_SUPPORT) += mtk_gpufreq_mt6890.o

mtk_gpufreq_mt6890-y += mtk_gpufreq_core.o

