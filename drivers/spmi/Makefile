#
# Makefile for kernel SPMI framework.
#
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
obj-$(CONFIG_SPMI)	+= spmi.o

obj-$(CONFIG_SPMI_MSM_PMIC_ARB)	+= spmi-pmic-arb.o
obj-$(CONFIG_MTK_PMIF)  += mtk-pmif.o mtk-dbg.o
