MTK_LPM_PLATFORM := mt6880

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/power_gs_v1/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/mtk_lpm_fs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/inc/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/include/$(MTK_LPM_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lpm/modules/debug/$(MTK_LPM_PLATFORM)/

obj-y += mtk_dbg_fs.o
obj-y += mtk_dbg_lpm_fs.o
obj-y += mtk_dbg_lpm_rc_fs.o
obj-y += mtk_dbg_spm_fs.o
obj-y += mtk_logger.o
#obj-y += mtk_lpm_trace_event.o

obj-y += $(MTK_LPM_PLATFORM)_cpupm_dbg.o
$(MTK_LPM_PLATFORM)_cpupm_dbg-y += mtk_cpupm_dbg.o
$(MTK_LPM_PLATFORM)_cpupm_dbg-y += mtk_cpuidle_status.o
$(MTK_LPM_PLATFORM)_cpupm_dbg-y += mtk_cpuidle_cpc.o
$(MTK_LPM_PLATFORM)_cpupm_dbg-y += mtk_idle_procfs.o
$(MTK_LPM_PLATFORM)_cpupm_dbg-y += mtk_idle_procfs_state.o
$(MTK_LPM_PLATFORM)_cpupm_dbg-y += mtk_idle_procfs_cpc.o
$(MTK_LPM_PLATFORM)_cpupm_dbg-y += mtk_idle_procfs_profile.o
$(MTK_LPM_PLATFORM)_cpupm_dbg-y += mtk_idle_procfs_control.o
