# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

obj-$(CONFIG_MTK_DEVAPC) := devapc.o

# Core
devapc-$(CONFIG_DEVAPC_ARCH_V1) += devapc-mtk-common.o
devapc-$(CONFIG_DEVAPC_ARCH_V2) += devapc-mtk-multi-ao.o

# SoC
devapc-$(CONFIG_DEVAPC_MT6779) += devapc-mt6779.o
devapc-$(CONFIG_DEVAPC_MT6880) += devapc-mt6880.o
