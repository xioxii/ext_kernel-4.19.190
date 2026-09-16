ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/spmi

subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include

#ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
#obj-y += pmic_ipi.o
#endif

obj-$(CONFIG_MTK_PMIC_CHIP_MT6330)	+= pmic.o pmic_common.o \
		pmic_debugfs.o \
		pmic_initial_setting.o \
		pmic_api.o \
		pmic_lp_api.o \
		pmic_oc_debug.o \
		mt6330_buck_manager.o \
#		pmic_efuse.o \
#		regulator_codegen.o \
#		pmic_auxadc.o \
#		pmic_irq.o \
