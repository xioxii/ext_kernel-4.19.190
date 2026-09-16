subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6330)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6330
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/pmic/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_PMIC_COMMON) += common/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6330) += mt6330/

ifneq (,$(findstring evb,$(CONFIG_ARCH_MTK_PROJECT)))
	subdir-ccflags-y += -DMTK_EVB_PLATFORM
endif
