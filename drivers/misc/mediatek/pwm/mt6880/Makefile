PWM_PLAT_NAME = $(subst ",,$(CONFIG_MTK_ECCCI_PLATFORM))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/$(PWM_PLAT_NAME)/include

obj-$(CONFIG_MTK_PWM)	:= mtk_pwm_hal.o
