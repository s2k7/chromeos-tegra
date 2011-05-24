/*
 * arch/arm/mach-tegra/board-shuttle-touch.c
 *
 * Copyright (C) 2011 Eduardo José Tagle <ejtagle@tutopia.com>
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/pinmux.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/it7260.h>
#include <linux/input/egalax.h>

#include "board-shuttle.h"
#include "gpio-names.h"

static void shuttle_touch_enable(void)
{
	gpio_direction_output(SHUTTLE_TS_ENABLE, 0); // Power up
	gpio_direction_output(SHUTTLE_TS_IRQ,  0); // Reset
	msleep(20);
	gpio_set_value(TEGRA_GPIO_PB6,  1); // End of reset
	gpio_direction_input(SHUTTLE_TS_IRQ); // End of reset
	msleep(10);
}

static void shuttle_touch_disable(void)
{
	gpio_direction_output(SHUTTLE_TS_IRQ,  0); // Reset
	gpio_direction_output(SHUTTLE_TS_ENABLE, 1); // Power down
	msleep(10);
}

struct it7260_platform_data it7260_pdata = {
	.disable_tp = shuttle_touch_disable,	/* function to disable the touchpad */
	.enable_tp = shuttle_touch_enable,		/* function to enable the touchpad */
};

static struct i2c_board_info __initdata shuttle_i2c_bus1_touch_info_it[] = {
	{
		I2C_BOARD_INFO("it7260", 0x46),
		.irq = TEGRA_GPIO_TO_IRQ(SHUTTLE_TS_IRQ),
		.platform_data = &it7260_pdata,
	},
};

struct egalax_platform_data egalax_pdata = {
	.disable_tp = shuttle_touch_disable,	/* function to disable the touchpad */
	.enable_tp = shuttle_touch_enable,		/* function to enable the touchpad */
};


static struct i2c_board_info __initdata shuttle_i2c_bus1_touch_info_egalax[] = {
	{
		I2C_BOARD_INFO("egalax", 0x04),
		.irq = TEGRA_GPIO_TO_IRQ(SHUTTLE_TS_IRQ),
		.platform_data = &egalax_pdata,
	},
};

int __init shuttle_touch_register_devices(void)
{
	gpio_request(SHUTTLE_TS_ENABLE, "touch_power_down");
	gpio_direction_output(SHUTTLE_TS_ENABLE,1);

	gpio_request(SHUTTLE_TS_IRQ, "touch_irq");
	gpio_direction_output(SHUTTLE_TS_IRQ,0);
	
	i2c_register_board_info(0, shuttle_i2c_bus1_touch_info_it, 1);
	i2c_register_board_info(0, shuttle_i2c_bus1_touch_info_egalax, 1);

	return 0;
}
