// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/bits.h>
#include <linux/string.h>
#include "soc_temp_lvts.h"
/*==================================================
 * Macro or data structure
 *==================================================
 */
#define LVTS_ATTR_RW(_name) \
static struct kobj_attribute _name##_attr = __ATTR_RW(_name)
#define LVTS_ATTR_RO(_name) \
static struct kobj_attribute _name##_attr = __ATTR_RO(_name)
#define LVTS_ATTR_WO(_name) \
static struct kobj_attribute _name##_attr = __ATTR_WO(_name)
/*==================================================
 * Global variable
 *==================================================
 */
struct lvts_data *g_lvts_data;
static DEFINE_MUTEX(platform_data_lock);
static LIST_HEAD(platform_data_list);
static void __iomem *infra_base;
/*==================================================
 * LVTS local common code
 *==================================================
 */
static int lvts_thermal_clock_on(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	int ret;

	ret = clk_prepare_enable(lvts_data->clk);
	if (ret)
		dev_err(dev,
			"Error: Failed to enable lvts controller clock: %d\n",
			ret);

	return ret;
}

static void lvts_thermal_clock_off(struct lvts_data *lvts_data)
{
	clk_disable_unprepare(lvts_data->clk);
}

static void lvts_reset(struct lvts_data *lvts_data)
{
#if 0
	int i;
#endif
	unsigned int temp;

#if 0
	for (i = 0; i < lvts_data->num_domain; i++) {
		if (lvts_data->domain[i].reset)
			reset_control_assert(lvts_data->domain[i].reset);

		if (lvts_data->domain[i].reset)
			reset_control_deassert(lvts_data->domain[i].reset);
	}
#endif
	/* Reset AP domain */
	temp = readl(infra_base + 0x120);
	temp |= BIT(0);
	writel(temp, infra_base + 0x120);

	temp = readl(infra_base + 0x124);
	temp |= BIT(0);
	writel(temp, infra_base + 0x124);

	/* Reset MCU domain */
	temp = readl(infra_base + 0x730);
	temp |= BIT(12);
	writel(temp, infra_base + 0x730);

	temp = readl(infra_base + 0x734);
	temp |= BIT(12);
	writel(temp, infra_base + 0x734);
}

static void device_wait_read_write_finished(struct lvts_data *lvts_data, int tc_id)
{
	/* Check this when LVTS device is doing a register
	 * read or write operation
	 */
	struct device *dev = lvts_data->dev;
	unsigned int cnt;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	cnt = 0;
	while ((readl(LVTS_CONFIG_0 + base) & DEVICE_ACCESS_STARTUS)) {
		cnt++;

		if (cnt == 100) {
			dev_err(dev,
				"Error: LVTS %d DEVICE_ACCESS_START didn't ready\n",
				tc_id);
			break;
		}
		udelay(2);
	}
}

static void device_identification(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i, data;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		lvts_writel_print(ENABLE_LVTS_CTRL_CLK, LVTSCLKEN_0 + base);

		lvts_write_device(lvts_data, RESET_ALL_DEVICES,  i);

		lvts_write_device(lvts_data, READ_BACK_DEVICE_ID,  i);

		/*  Check LVTS device ID */
		data = (readl(LVTS_ID_0 + base) & GENMASK(7,0));
		if (data != (0x81 + i))
			dev_err(dev, "LVTS_TC_%d, Device ID should be 0x%x, but 0x%x\n",
				i, (0x81 + i), data);
	}
}

static int wait_sensing_point_idle(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
	unsigned int cnt = 0, temp, mask, error_code;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);
	mask = BIT(10) | BIT(7) | BIT(0);

	while (1) {
		temp = readl(LVTSMSRCTL1_0 + base);
		if ((temp & mask) == 0)
			break;

		cnt++;

		if (cnt == 100) {
			/*
			 * Error code
			 * 000: IDLE
			 * 001: Write transaction
			 * 010: Waiting for read after Write
			 * 011: Disable Continue fetching on Device
			 * 100: Read transaction
			 * 101: Set Device special Register for Voltage threshold
			 * 111: Set TSMCU number for Fetch
			 */
			error_code = ((temp & BIT(10)) >> 10) * 4 +
				((temp & BIT(7)) >> 7) * 2 +
				(temp & BIT(0)) * 1;
			dev_err(dev,
				"Error LVTS %d sensing points aren't idle, error_code %d\n",
				tc_id, error_code);
			return error_code;
		}
		udelay(2);
	}

	return 0;
}

static void wait_all_tc_sensing_point_idle(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int error_code, offset, size, is_error;
	int i, cnt;
	char buffer[100];

	size = sizeof(buffer);
	for (cnt = 0; cnt < 2; cnt++) {
		is_error = 0;
		offset = snprintf(buffer, size, "Error: Sensing point of LVTS ");

		for (i = 0; i < lvts_data->num_tc; i++) {
			error_code = wait_sensing_point_idle(lvts_data, i);
			if (error_code != 0) {
				is_error = 1;
				offset += snprintf(buffer + offset, size - offset,
					"%d, ", i);
			}
		}

		if (is_error == 0)
			break;

		offset += snprintf(buffer + offset, size - offset,
			"are not idle\n");
		dev_info(dev, "%s", buffer);
	}
}

static void disable_all_sensing_points(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		lvts_writel_print(DISABLE_SENSING_POINT, LVTSMONCTL0_0 + base);
	}
}

static void enable_all_sensing_points(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, num;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		num = tc[i].num_sensor;

		if (num > 4) {
			dev_err(dev,
				"%s, LVTS%d, illegal number of sensors: %d\n",
				__func__, i, tc[i].num_sensor);
			continue;
		}

		lvts_writel_print(ENABLE_SENSING_POINT(num), LVTSMONCTL0_0 + base);
	}
}

static int lvts_raw_to_temp(struct formula_coeff *co, unsigned int msr_raw)
{
	/* This function returns degree mC */

	int temp;

	temp = (co->a * ((unsigned long long int)msr_raw)) >> 14;
	temp = temp + co->golden_temp * 500 + co->b;

	return temp;
}

static unsigned int lvts_temp_to_raw(struct formula_coeff *co, int temp)
{
	unsigned int msr_raw = 0;

	msr_raw = (((long long int)(co->golden_temp) * 500 + co->b - temp) << 14)
		/ (-1 * co->a);

	return msr_raw;
}

static int get_dominator_index(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	int d_index;

	if (tc[tc_id].dominator_sensing_point == ALL_SENSING_POINTS){
		d_index = ALL_SENSING_POINTS;
	} else if (tc[tc_id].dominator_sensing_point <
		tc[tc_id].num_sensor){
		d_index = tc[tc_id].dominator_sensing_point;
	} else {
		dev_err(dev,
			"Error: LVTS%d, dominator_sensing_point= %d should smaller than num_sensor= %d\n",
			tc_id, tc[tc_id].dominator_sensing_point,
			tc[tc_id].num_sensor);

		dev_err(dev, "Use the sensing point 0 as the dominated sensor\n");
		d_index = SENSING_POINT0;
	}

	return d_index;
}

static void disable_hw_reboot_interrupt(struct lvts_data *lvts_data, int tc_id)
{
	unsigned int temp;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	/* LVTS thermal controller has two interrupts for thermal HW reboot
	 * One is for AP SW and the other is for RGU
	 * The interrupt of AP SW can turn off by a bit of a register, but
	 * the other for RGU cannot.
	 * To prevent rebooting device accidentally, we are going to add
	 * a huge offset to LVTS and make LVTS always report extremely low
	 * temperature.
	 */

	/* After adding the huge offset 0x3FFF, LVTS alawys adds the
	 * offset to MSR_RAW.
         * When MSR_RAW is larger, SW will convert lower temperature/
	 */
	temp = readl(LVTSPROTCTL_0 + base);
	lvts_writel_print(temp | 0x3FFF, LVTSPROTCTL_0 + base);

	/* Disable the interrupt of AP SW */
	temp = readl(LVTSMONINT_0 + base);
	lvts_writel_print(temp & ~(STAGE3_INT_EN), LVTSMONINT_0 + base);
}

static void enable_hw_reboot_interrupt(struct lvts_data *lvts_data, int tc_id)
{
	unsigned int temp;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	/* Enable the interrupt of AP SW */
	temp = readl(LVTSMONINT_0 + base);
	lvts_writel_print(temp | STAGE3_INT_EN, LVTSMONINT_0 + base);
	/* Clear the offset */
	temp = readl(LVTSPROTCTL_0 + base);
	lvts_writel_print(temp & ~PROTOFFSET, LVTSPROTCTL_0 + base);
}

static void set_tc_hw_reboot_threshold(struct lvts_data *lvts_data,
	int trip_point, int tc_id)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int msr_raw, temp, config, ts_name, d_index;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);
	d_index = get_dominator_index(lvts_data, tc_id);

	dev_info(dev, "%s: LVTS%d, the dominator sensing point= %d\n",
		__func__, tc_id, d_index);

	disable_hw_reboot_interrupt(lvts_data, tc_id);

	temp = readl(LVTSPROTCTL_0 + base);
	if (d_index == ALL_SENSING_POINTS) {
		ts_name = 0;
		/* Maximum of 4 sensing points */
		config = (0x1 << 16);
		lvts_writel_print(config | temp, LVTSPROTCTL_0 + base);
	} else {
		ts_name = tc[tc_id].sensor_map[d_index];
		/* Select protection sensor */
		config = ((d_index << 2) + 0x2) << 16;
		lvts_writel_print(config | temp, LVTSPROTCTL_0 + base);
	}

	msr_raw = lvts_temp_to_raw(&lvts_data->coeff, trip_point);
	lvts_writel_print(msr_raw, LVTSPROTTC_0 + base);

	enable_hw_reboot_interrupt(lvts_data, tc_id);
}

static void set_all_tc_hw_reboot(struct lvts_data *lvts_data)
{
	struct tc_settings *tc = lvts_data->tc;
	int i, trip_point;

	for (i = 0; i < lvts_data->num_tc; i++) {
		trip_point = tc[i].hw_reboot_trip_point;

		if (tc[i].num_sensor == 0)
			continue;

		if (trip_point == DISABLE_THERMAL_HW_REBOOT) {
			disable_hw_reboot_interrupt(lvts_data, i);
			continue;
		}

		set_tc_hw_reboot_threshold(lvts_data, trip_point, i);
	}
}

static int lvts_read_tc_msr_raw(unsigned int *msr_reg)
{
	if (msr_reg == 0)
		return 0;

	return readl(msr_reg) & MRS_RAW_MASK;
}

static int lvts_read_all_tc_temperature(struct lvts_data *lvts_data)
{
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, j, s_index, msr_raw;
	int max_temp, current_temp;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);
		for (j = 0; j < tc[i].num_sensor; j++) {
			s_index = tc[i].sensor_map[j];

			msr_raw = lvts_read_tc_msr_raw(LVTSMSR0_0 + base + 0x4 * j);
			current_temp = lvts_raw_to_temp(&lvts_data->coeff, msr_raw);

			if (msr_raw == 0)
				current_temp = THERMAL_TEMP_INVALID;

			if (i == 0 && j == 0)
				max_temp = current_temp;
			else if (current_temp > max_temp)
				max_temp = current_temp;

			lvts_data->sen_data[s_index].msr_raw = msr_raw;
			lvts_data->sen_data[s_index].temp = current_temp;
		}
	}

	return max_temp;
}

static int lvts_get_max_temp(struct lvts_data *lvts_data)
{
	int max_temp;

	max_temp = lvts_read_all_tc_temperature(lvts_data);

	return max_temp;
}

static int soc_temp_lvts_read_temp(void *data, int *temperature)
{
	struct soc_temp_tz *lvts_tz = (struct soc_temp_tz *) data;
	struct lvts_data *lvts_data = lvts_tz->lvts_data;

	if (lvts_tz->id == 0)
		*temperature = lvts_get_max_temp(lvts_data);
	else if (lvts_tz->id - 1 < lvts_data->num_sensor)
		*temperature = lvts_data->sen_data[lvts_tz->id - 1].temp;
	else
		return -EINVAL;

	return 0;
}

static const struct thermal_zone_of_device_ops soc_temp_lvts_ops = {
	.get_temp = soc_temp_lvts_read_temp,
};

static void lvts_device_close(struct lvts_data *lvts_data)
{
	unsigned int i;
	void __iomem *base;

	for (i = 0; i < lvts_data->num_tc; i++) {
		base = GET_BASE_ADDR(i);

		lvts_write_device(lvts_data, RESET_ALL_DEVICES,  i);
		lvts_writel_print(DISABLE_LVTS_CTRL_CLK, LVTSCLKEN_0 + base);
	}
}

static void tc_irq_handler(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
	unsigned int ret = 0;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	ret = readl(LVTSMONINTSTS_0 + base);
	/* Write back to clear interrupt status */
	lvts_writel_print(ret, LVTSMONINTSTS_0 + base);

	dev_info(dev, "[Thermal IRQ] LVTS thermal controller %d, LVTSMONINTSTS=0x%08x\n",
		tc_id, ret);

	if (ret & THERMAL_COLD_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: Cold interrupt triggered, sensor point 0\n");

	if (ret & THERMAL_HOT_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: Hot interrupt triggered, sensor point 0\n");

	if (ret & THERMAL_LOW_OFFSET_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: Low offset interrupt triggered, sensor point 0\n");

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: High offset interrupt triggered, sensor point 0\n");

	if (ret & THERMAL_HOT2NORMAL_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: Hot to normal interrupt triggered, sensor point 0\n");

	if (ret & THERMAL_COLD_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: Cold interrupt triggered, sensor point 1\n");

	if (ret & THERMAL_HOT_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: Hot interrupt triggered, sensor point 1\n");

	if (ret & THERMAL_LOW_OFFSET_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: Low offset interrupt triggered, sensor point 1\n");

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: High offset interrupt triggered, sensor point 1\n");

	if (ret & THERMAL_HOT2NORMAL_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: Hot to normal interrupt triggered, sensor point 1\n");

	if (ret & THERMAL_COLD_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: Cold interrupt triggered, sensor point 2\n");

	if (ret & THERMAL_HOT_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: Hot interrupt triggered, sensor point 2\n");

	if (ret & THERMAL_LOW_OFFSET_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: Low offset interrupt triggered, sensor point 2\n");

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: High offset interrupt triggered, sensor point 2\n");

	if (ret & THERMAL_HOT2NORMAL_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: Hot to normal interrupt triggered, sensor point 2\n");

	if (ret & THERMAL_DEVICE_TIMEOUT_INTERRUPT)
		dev_info(dev, "[Thermal IRQ]: Device access timeout triggered\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: Immediate sense interrupt triggered, sensor point 0\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: Immediate sense interrupt triggered, sensor point 1\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: Immediate sense interrupt triggered, sensor point 2\n");

	if (ret & THERMAL_FILTER_INTERRUPT_0)
		dev_info(dev, "[Thermal IRQ]: Filter sense interrupt triggered, sensor point 0\n");

	if (ret & THERMAL_FILTER_INTERRUPT_1)
		dev_info(dev, "[Thermal IRQ]: Filter sense interrupt triggered, sensor point 1\n");

	if (ret & THERMAL_FILTER_INTERRUPT_2)
		dev_info(dev, "[Thermal IRQ]: Filter sense interrupt triggered, sensor point 2\n");

	if (ret & THERMAL_COLD_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: Cold interrupt triggered, sensor point 3\n");

	if (ret & THERMAL_HOT_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: Hot interrupt triggered, sensor point 3\n");

	if (ret & THERMAL_LOW_OFFSET_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: Low offset interrupt triggered, sensor point 3\n");

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: High offset triggered, sensor point 3\n");

	if (ret & THERMAL_HOT2NORMAL_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: Hot to normal interrupt triggered, sensor point 3\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: Immediate sense interrupt triggered, sensor point 3\n");

	if (ret & THERMAL_FILTER_INTERRUPT_3)
		dev_info(dev, "[Thermal IRQ]: Filter sense interrupt triggered, sensor point 3\n");

	if (ret & THERMAL_PROTECTION_STAGE_1)
		dev_info(dev, "[Thermal IRQ]: Thermal protection stage 1 interrupt triggered\n");

	if (ret & THERMAL_PROTECTION_STAGE_2)
		dev_info(dev, "[Thermal IRQ]: Thermal protection stage 2 interrupt triggered\n");

	if (ret & THERMAL_PROTECTION_STAGE_3)
		dev_err(dev, "[Thermal IRQ]: Thermal protection stage 3 interrupt triggered, Thermal HW reboot\n");
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct lvts_data *lvts_data = (struct lvts_data *) dev_id;
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int i, irq_bitmap[lvts_data->num_domain];
	void __iomem *base;

	for (i = 0; i < lvts_data->num_domain; i++) {
		base = lvts_data->domain[i].base;
		irq_bitmap[i] = readl(THERMINTST + base);
		dev_info(dev, "%s : THERMINTST = 0x%x\n", __func__, irq_bitmap[i]);
	}

	for (i = 0; i < lvts_data->num_tc; i++) {
		if ((irq_bitmap[tc[i].domain_index] & tc[i].irq_bit) == 0)
			tc_irq_handler(lvts_data, i);
	}

	return IRQ_HANDLED;
}

static int prepare_calibration_data(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct sensor_cal_data *cal_data = &lvts_data->cal_data;
	struct platform_ops *ops = &lvts_data->ops;
	int i, offset, size;
	char buffer[512];

	cal_data->count_r = devm_kcalloc(dev, lvts_data->num_sensor,
				      sizeof(*cal_data->count_r), GFP_KERNEL);
	if (!cal_data->count_r)
		return -ENOMEM;

	cal_data->count_rc = devm_kcalloc(dev, lvts_data->num_sensor,
				      sizeof(*cal_data->count_rc), GFP_KERNEL);
	if (!cal_data->count_rc)
		return -ENOMEM;

	if (ops->efuse_to_cal_data)
		ops->efuse_to_cal_data(lvts_data);

	cal_data->use_fake_efuse = 1;
	if (cal_data->golden_temp != 0) {
		cal_data->use_fake_efuse = 0;
	} else {
		for (i = 0; i < lvts_data->num_sensor; i++) {
			if (cal_data->count_r[i] != 0 ||
				cal_data->count_rc[i] != 0) {
				cal_data->use_fake_efuse = 0;
				break;
			}
		}
	}

	if (cal_data->use_fake_efuse) {
		/* It means all efuse data are equal to 0 */
		dev_err(dev,
			"[lvts_cal] This sample is not calibrated, fake !!\n");

		cal_data->golden_temp = cal_data->default_golden_temp;
		for (i = 0; i < lvts_data->num_sensor; i++) {
			cal_data->count_r[i] = cal_data->default_count_r;
			cal_data->count_rc[i] = cal_data->default_count_rc;
		}
	}

	lvts_data->coeff.golden_temp = cal_data->golden_temp;

	dev_info(dev, "[lvts_cal] golden_temp = %d\n", cal_data->golden_temp);

	size = sizeof(buffer);
	offset = snprintf(buffer, size, "[lvts_cal] num:g_count:g_count_rc ");
	for (i = 0; i < lvts_data->num_sensor; i++)
		offset += snprintf(buffer + offset, size - offset, "%d:%d:%d ",
				i, cal_data->count_r[i], cal_data->count_rc[i]);

	buffer[offset] = '\0';
	dev_info(dev, "%s\n", buffer);

	return 0;
}

static int get_calibration_data(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	char cell_name[8] = "e_data0";
	struct nvmem_cell *cell;
	u32 *buf;
	size_t len = 0;
	int i, j, index = 0, ret;

	lvts_data->efuse = devm_kcalloc(dev, lvts_data->num_efuse_addr,
				      sizeof(*lvts_data->efuse), GFP_KERNEL);
	if (!lvts_data->efuse)
		return -ENOMEM;

	for (i = 0; i < lvts_data->num_efuse_block; i++) {
		cell_name[6] = '1' + i;
		cell = nvmem_cell_get(dev, cell_name);
		if (IS_ERR(cell)) {
			dev_err(dev, "Error: Failed to get nvmem cell %s\n",
				cell_name);
			return PTR_ERR(cell);
		}

		buf = (u32 *)nvmem_cell_read(cell, &len);
		nvmem_cell_put(cell);

		if (IS_ERR(buf))
			return PTR_ERR(buf);

		for (j = 0; j < (len / sizeof(u32)); j++) {
			if (index >= lvts_data->num_efuse_addr) {
				dev_err(dev, "Array efuse is going to overflow");
				kfree(buf);
				return -EINVAL;
			}

			lvts_data->efuse[index] = buf[j];
			index++;
		}

		kfree(buf);
	}

	ret = prepare_calibration_data(lvts_data);

	return ret;
}

static int lvts_init(struct lvts_data *lvts_data)
{
	struct platform_ops *ops = &lvts_data->ops;
	int ret;

	ret = lvts_thermal_clock_on(lvts_data);
	if (ret)
		return ret;

	lvts_reset(lvts_data);

	device_identification(lvts_data);
	if (ops->device_enable_and_init)
		ops->device_enable_and_init(lvts_data);

	if (IS_ENABLE(FEATURE_DEVICE_AUTO_RCK)) {
		if (ops->device_enable_auto_rck)
			ops->device_enable_auto_rck(lvts_data);
	} else {
		if (ops->device_read_count_rc_n)
			ops->device_read_count_rc_n(lvts_data);
	}

	if (ops->set_cal_data)
		ops->set_cal_data(lvts_data);

	disable_all_sensing_points(lvts_data);
	wait_all_tc_sensing_point_idle(lvts_data);
	if(ops->init_controller)
		ops->init_controller(lvts_data);
	enable_all_sensing_points(lvts_data);

	set_all_tc_hw_reboot(lvts_data);

	return 0;
}

static int of_update_lvts_data(struct lvts_data *lvts_data,
	struct platform_device *pdev)
{
	struct device *dev = lvts_data->dev;
	struct power_domain *domain;
	struct resource *res;
	unsigned int i;
	int ret;

	lvts_data->clk = devm_clk_get(dev, "lvts_clk");
	if (IS_ERR(lvts_data->clk))
		return PTR_ERR(lvts_data->clk);

	domain = devm_kcalloc(dev, lvts_data->num_domain, sizeof(*domain),
			GFP_KERNEL);
	if (!domain)
		return -ENOMEM;

	for (i = 0; i < lvts_data->num_domain; i++) {
		/* Get base address */
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(dev, "No IO resource, index %d\n", i);
			return -ENXIO;
		}

		domain[i].base = devm_ioremap_resource(dev, res);
		if (IS_ERR(domain[i].base)) {
			dev_err(dev, "Failed to remap io, index %d\n", i);
			return PTR_ERR(domain[i].base);
		}

		/* Get interrupt number */
		res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res) {
			dev_err(dev, "No irq resource, index %d\n", i);
			return -EINVAL;
		}
		domain[i].irq_num = res->start;

		/* Get reset control */
		domain[i].reset = devm_reset_control_get_by_index(dev, i);
		if (IS_ERR(domain[i].reset)) {
			dev_err(dev, "Failed to get, index %d\n", i);
			return PTR_ERR(domain[i].reset);
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(dev, "No IO resource, index %d\n", 2);
		return -ENXIO;
	}

	infra_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(infra_base)) {
		dev_err(dev, "Failed to remap io, index %d\n", i);
		return PTR_ERR(infra_base);
	}
	lvts_data->domain = domain;

	lvts_data->sen_data = devm_kcalloc(dev, lvts_data->num_sensor,
				sizeof(*lvts_data->sen_data), GFP_KERNEL);
	if (!lvts_data->sen_data)
		return -ENOMEM;

	ret = get_calibration_data(lvts_data);
	if (ret)
		return ret;

	return 0;
}

static void lvts_close(struct lvts_data *lvts_data)
{
	disable_all_sensing_points(lvts_data);
	wait_all_tc_sensing_point_idle(lvts_data);
	lvts_device_close(lvts_data);
	lvts_thermal_clock_off(lvts_data);
}

static void add_match_data_to_list(struct lvts_match_data *match_data)
{
	if (!match_data)
		return;

	mutex_lock(&platform_data_lock);
	list_add(&match_data->node, &platform_data_list);
	mutex_unlock(&platform_data_lock);
}

static void init_lvts_match_data(void)
{
	struct lvts_match_data *match_data;

	match_data = register_v5_lvts_match_data();
	add_match_data_to_list(match_data);
}

static struct match_entry *get_match_entry(struct match_entry *table, char *chip)
{
	unsigned int index = 0;

	while (!IS_EMPTY_STR(table[index].chip)) {
		if (!strncmp(table[index].chip, chip, strlen(chip) - 1))
			return &table[index];

		index++;
	}

	return NULL;
}

static struct lvts_data *get_lvts_data(struct lvts_id *lvts_id)
{
	struct lvts_match_data *match_data;
	struct lvts_data *lvts_data;
	struct match_entry *table, *entry;

        mutex_lock(&platform_data_lock);
        list_for_each_entry(match_data, &platform_data_list, node) {
                if (match_data->hw_version == lvts_id->hw_version) {
			table = match_data->table;

			entry = get_match_entry(table, lvts_id->chip);

			if (entry) {
				lvts_data = entry->lvts_data;
				match_data->set_up_common_callbacks(lvts_data);

				return lvts_data;
			}
                }
        }
        mutex_unlock(&platform_data_lock);

	return NULL;
}

static int lvts_register_irq_handler(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	unsigned int i;
	int ret;

	for (i = 0; i < lvts_data->num_domain; i++) {
		ret = devm_request_irq(dev, lvts_data->domain[i].irq_num,
			irq_handler, IRQF_TRIGGER_HIGH, "mtk_lvts", lvts_data);

		if (ret) {
			dev_err(dev, "LVTS IRQ register fail\n");
			lvts_close(lvts_data);
			return ret;
		}
	}

	return 0;
}

static int lvts_register_thermal_zones(struct lvts_data *lvts_data)
{
	struct device *dev = lvts_data->dev;
	struct thermal_zone_device *tzdev;
	struct soc_temp_tz *lvts_tz;
	int i, ret;

	for (i = 0; i < lvts_data->num_sensor + 1; i++) {
		lvts_tz = devm_kzalloc(dev, sizeof(*lvts_tz), GFP_KERNEL);
		if (!lvts_tz) {
			dev_err(dev,
				"Error: Failed to allocate memory lvts tz %d\n",
				i);
			lvts_close(lvts_data);
			return -ENOMEM;
		}

		lvts_tz->id = i;
		lvts_tz->lvts_data = lvts_data;

		tzdev = devm_thermal_zone_of_sensor_register(dev, lvts_tz->id,
				lvts_tz, &soc_temp_lvts_ops);

		if (IS_ERR(tzdev)) {
			ret = PTR_ERR(tzdev);
			dev_err(dev,
				"Error: Failed to register lvts tz %d, ret = %d\n",
				lvts_tz->id, ret);
			lvts_close(lvts_data);
			return ret;
		}
	}

	return 0;
}

static void update_all_tc_hw_reboot_point(struct lvts_data *lvts_data,
	int trip_point)
{
	struct tc_settings *tc = lvts_data->tc;
	int i;

	for (i = 0; i < lvts_data->num_tc; i++)
		tc[i].hw_reboot_trip_point = trip_point;
}

static ssize_t lvts_reboot_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char cmd[20];
	int hw_reboot_point;
	struct device *dev = g_lvts_data->dev;


	if ((sscanf(buf, "%20s %d", cmd, &hw_reboot_point) != 2))
		return -EINVAL;

	if (strncmp(cmd, "update", 6))
		return -EINVAL;

	if (hw_reboot_point != DISABLE_THERMAL_HW_REBOOT)
		dev_info(dev,"LVTS: Update HW reboot point to %d\n", hw_reboot_point);
	else
		dev_info(dev,"LVTS: Disable thermal HW reboot\n");

	update_all_tc_hw_reboot_point(g_lvts_data, hw_reboot_point);
	set_all_tc_hw_reboot(g_lvts_data);

	return count;
}

LVTS_ATTR_WO(lvts_reboot);

static struct attribute *lvts_attrs[] = {
	&lvts_reboot_attr.attr,
	NULL
};

static struct attribute_group lvts_attr_group = {
	.name	= "lvts",
	.attrs	= lvts_attrs,
};

static int lvts_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lvts_data *lvts_data;
	struct lvts_id *lvts_id;
	int ret;

	init_lvts_match_data();
	lvts_id = (struct lvts_id *) of_device_get_match_data(dev);
	lvts_data = get_lvts_data(lvts_id);

	if (!lvts_data)	{
		dev_err(dev, "Error: Failed to get lvts platform data\n");
		return -ENODATA;
	}

	lvts_data->dev = &pdev->dev;

	ret = of_update_lvts_data(lvts_data, pdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, lvts_data);
	g_lvts_data = lvts_data;

	ret = lvts_init(lvts_data);
	if (ret)
		return ret;

	ret = lvts_register_irq_handler(lvts_data);
	if (ret)
		return ret;

	ret = lvts_register_thermal_zones(lvts_data);
	if (ret)
		return ret;

	ret = sysfs_create_group(kernel_kobj, &lvts_attr_group);
	if (ret)
		dev_err(dev, "failed to create lvts sysfs, ret=%d!\n", ret);

	return ret;
}

static int lvts_remove(struct platform_device *pdev)
{
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) platform_get_drvdata(pdev);

	lvts_close(lvts_data);
	mutex_destroy(&platform_data_lock);

	sysfs_remove_group(kernel_kobj, &lvts_attr_group);

	return 0;
}

static int lvts_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* TODO: Guarantee suspend sequence with PTP
	 * PTP -> LVTS
	 */
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) platform_get_drvdata(pdev);

	lvts_close(lvts_data);

	return 0;
}

static int lvts_resume(struct platform_device *pdev)
{
	/* TODO: Guarantee resume sequence with PTP
	 * LVTS -> PTP
	 */
	int ret;
	struct lvts_data *lvts_data;

	lvts_data = (struct lvts_data *) platform_get_drvdata(pdev);

	ret = lvts_init(lvts_data);
	if (ret)
		return ret;

	return 0;
}
/*==================================================
 * Extern function
 *==================================================
 */
void lvts_writel_print(unsigned int val, void __iomem *addr)
{
/* TODO: Add Ftrace here
	if (p_data->lvts_debug_log)
		dev_info(dev, "### LVTS_REG: addr 0x%p, val 0x%x\n",
								addr, val);
	pr_err("### LVTS_REG: addr 0x%p, val 0x%x\n", addr, val);
*/
	writel(val, addr);
}
EXPORT_SYMBOL_GPL(lvts_writel_print);

void device_wait_counting_finished(struct lvts_data *lvts_data, int tc_id)
{
	/* Check this when LVTS device is counting for
	 * a temperature or a RC now
	 */
	struct device *dev = lvts_data->dev;
	unsigned int cnt;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	cnt = 0;
	while ((readl(LVTS_CONFIG_0 + base) & DEVICE_SENSING_STATUS)) {
		cnt++;

		if (cnt == 100) {
			dev_err(dev,
				"Error: LVTS %d DEVICE_SENSING_STATUS didn't ready\n",
				tc_id);
			break;
		}
		udelay(2);
	}
}
EXPORT_SYMBOL_GPL(device_wait_counting_finished);

void lvts_write_device(struct lvts_data *lvts_data, unsigned int data,
	int tc_id)
{
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	lvts_writel_print(data, LVTS_CONFIG_0 + base);

	udelay(5);
}
EXPORT_SYMBOL_GPL(lvts_write_device);

unsigned int lvts_read_device(struct lvts_data *lvts_data,
	unsigned int reg_idx, int tc_id)
{
	void __iomem *base;
	unsigned int data;

	base = GET_BASE_ADDR(tc_id);
	lvts_writel_print(READ_DEVICE_REG(reg_idx), LVTS_CONFIG_0 + base);

	device_wait_read_write_finished(lvts_data, tc_id);

	data = (readl(LVTSRDATA0_0 + base));

	return data;
}
EXPORT_SYMBOL_GPL(lvts_read_device);

void set_polling_speed(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int lvtsMonCtl1, lvtsMonCtl2;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);

	lvtsMonCtl1 = (((tc[tc_id].tc_speed.group_interval_delay
			<< 20) & GENMASK(29,20)) |
			(tc[tc_id].tc_speed.period_unit &
			GENMASK(9,0)));
	lvtsMonCtl2 = (((tc[tc_id].tc_speed.filter_interval_delay
			<< 16) & GENMASK(25,16)) |
			(tc[tc_id].tc_speed.sensor_interval_delay
			& GENMASK(9,0)));
	/*
	 * Clock source of LVTS thermal controller is 26MHz.
	 * Period unit is a base for all interval delays
	 * All interval delays must multiply it to convert a setting to time.
	 * Filter interval delay is a delay between two samples of the same sensor
	 * Sensor interval delay is a delay between two samples of differnet sensors
	 * Group interval delay is a delay between different rounds.
	 * For example:
	 *     If Period unit = C, filter delay = 1, sensor delay = 2, group delay = 1,
         *     and two sensors, TS1 and TS2, are in a LVTS thermal controller
	 *     and then
	 *     Period unit = C * 1/26M * 256 = 12 * 38.46ns * 256 = 118.149us
	 *     Filter interval delay = 1 * Period unit = 118.149us
	 *     Sensor interval delay = 2 * Period unit = 236.298us
	 *     Group interval delay = 1 * Period unit = 118.149us
	 *
	 *     TS1    TS1 ... TS1    TS2    TS2 ... TS2    TS1...
         *        <--> Filter interval delay
         *                       <--> Sensor interval delay
         *                                             <--> Group interval delay
	 */
	lvts_writel_print(lvtsMonCtl1, LVTSMONCTL1_0 + base);
	lvts_writel_print(lvtsMonCtl2, LVTSMONCTL2_0 + base);

	udelay(1);
	dev_info(dev, "%s %d, LVTSMONCTL1_0= 0x%x,LVTSMONCTL2_0= 0x%x\n",
		__func__, tc_id,
		readl(LVTSMONCTL1_0 + base),
		readl(LVTSMONCTL2_0 + base));
}
EXPORT_SYMBOL_GPL(set_polling_speed);

void set_hw_filter(struct lvts_data *lvts_data, int tc_id)
{
	struct device *dev = lvts_data->dev;
	struct tc_settings *tc = lvts_data->tc;
	unsigned int option;
	void __iomem *base;

	base = GET_BASE_ADDR(tc_id);
	option = tc[tc_id].hw_filter & 0x7;
        /* hw filter
	 * 000: Get one sample
         * 001: Get 2 samples and average them
         * 010: Get 4 samples, drop max and min, then average the rest of 2 samples
         * 011: Get 6 samples, drop max and min, then average the rest of 4 samples
         * 100: Get 10 samples, drop max and min, then average the rest of 8 samples
         * 101: Get 18 samples, drop max and min, then average the rest of 16 samples
         */
        option = (option << 9) | (option << 6) | (option << 3) | option;

        lvts_writel_print(option, LVTSMSRCTL0_0 + base);
	dev_info(dev, "%s %d, LVTSMSRCTL0_0= 0x%x\n",
		__func__, tc_id, readl(LVTSMSRCTL0_0 + base));
}
EXPORT_SYMBOL_GPL(set_hw_filter);
/*==================================================
 * Support chips
 *==================================================
 */
static struct lvts_id mt6880_lvts_id = {
	.hw_version = 5,
	.chip = "mt6880",
};
static const struct of_device_id lvts_of_match[] = {
	{
		.compatible = "mediatek,mt6880-lvts",
		.data = (void *)&mt6880_lvts_id,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, lvts_of_match);
/*==================================================*/
static struct platform_driver soc_temp_lvts = {
	.probe = lvts_probe,
	.remove = lvts_remove,
	.suspend = lvts_suspend,
	.resume = lvts_resume,
	.driver = {
		.name = "mtk-soc-temp-lvts",
		.of_match_table = lvts_of_match,
	},
};

module_platform_driver(soc_temp_lvts);
MODULE_AUTHOR("Yu-Chia Chang <ethan.chang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek soc temperature driver");
MODULE_LICENSE("GPL v2");
