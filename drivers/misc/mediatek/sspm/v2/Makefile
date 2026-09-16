
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/include/soc/mediatek/


obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm.o

sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_helper.o
sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_logger_impl.o
sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_plt.o
sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_reservedmem.o
sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_sysfs.o
sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_ipi_timeout_cb.o
sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_timesync.o
sspm-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm_emi_mpu.o
