ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1


ifeq ($(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mcupm/mt6880/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_trace_event/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/mt6880/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/power_gs_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/platform/mt6880/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/platform/mt6880/suspend
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6330/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

obj-y += mtk.o
obj-y += mtk_lp_plat_apmcu.o
obj-y += mtk_lp_plat_apmcu_mbox.o
obj-y += mtk_lp_plat_pll.o

obj-y += suspend/mtk_suspend.o
