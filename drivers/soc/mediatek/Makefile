ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE_mtk-dvfsrc.o := y
GCOV_PROFILE_mtk-mmdvfs.o := y
GCOV_PROFILE_mtk-pmic-wrap.o := y
endif
obj-$(CONFIG_MTK_CMDQ) += mtk-cmdq-helper.o
obj-$(CONFIG_MTK_INFRACFG) += mtk-infracfg.o mtk-scpsys-ext.o
obj-$(CONFIG_MTK_DVFSRC) += mtk-dvfsrc.o mtk-dvfsrc-start.o
obj-$(CONFIG_MTK_PMIC_WRAP) += mtk-pmic-wrap.o
obj-$(CONFIG_MTK_SCPSYS) += mtk-scpsys.o
obj-$(CONFIG_MTK_SCPSYS_BRINGUP) += mtk-scpsys-bringup.o
obj-$(CONFIG_MTK_DEVAPC) += devapc/
obj-$(CONFIG_MTK_MMDVFS) += mtk-mmdvfs.o
obj-$(CONFIG_MTK_MBOX) += mtk-mbox.o
obj-$(CONFIG_MTK_IPI) += mtk_tinysys_ipi.o
obj-$(CONFIG_MTK_LPM) += mtk-lpm.o
