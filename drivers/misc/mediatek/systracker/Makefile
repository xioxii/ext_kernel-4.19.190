#
# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2020 MediaTek Inc.
#

obj-$(CONFIG_MTK_SYSTRACKER_V2) += systracker_interface_v2.o

ifeq ($(CONFIG_ARM64), y)
  obj-$(CONFIG_MTK_SYSTRACKER_V2) += backtrace64.o
else
  obj-$(CONFIG_MTK_SYSTRACKER_V2) += backtrace32.o
  obj-$(CONFIG_MTK_SYSTRACKER_V2) += backtrace32_asm.o
endif

obj-$(CONFIG_MTK_SYSTRACKER_MT6880) += mt6880/
obj-$(CONFIG_MTK_SYSTRACKER_MT6890) += mt6890/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
