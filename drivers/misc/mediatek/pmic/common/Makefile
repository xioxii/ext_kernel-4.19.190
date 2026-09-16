ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
ccflags-y += -I$(srctree)/drivers/spmi
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/pmic/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

#ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
#endif

obj-$(CONFIG_MTK_PMIC_COMMON) += upmu.o upmu_debugfs.o upmu_initial_setting.o
# upmu_regulator.o
