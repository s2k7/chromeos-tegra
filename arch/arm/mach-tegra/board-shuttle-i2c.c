/*
 * arch/arm/mach-tegra/board-shuttle-i2c.c
 *
 * Copyright (C) 2011 Eduardo José Tagle <ejtagle@tutopia.com>
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>

#include <asm/mach-types.h>
#include <mach/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "board.h"
#include "devices.h"
#include "gpio-names.h"
#include "board-shuttle.h"


static struct tegra_i2c_platform_data shuttle_i2c1_platform_data = {
	.adapter_nr   = 0,
	.bus_count    = 1,
	.bus_clk_rate = { 100000 },
};

static struct tegra_i2c_platform_data shuttle_i2c2_platform_data = {
	.adapter_nr   = 1,
	.bus_count    = 1,
	.bus_clk_rate = { 100000 },
};

static struct tegra_i2c_platform_data shuttle_i2c3_platform_data = {
	.adapter_nr   = 2,
	.bus_count    = 1,
	.bus_clk_rate = { 100000 },
};

static struct tegra_i2c_platform_data shuttle_i2c4_platform_data = {
	.adapter_nr   = 3,
	.bus_count    = 1,
	.bus_clk_rate = { 100000 },
	.is_dvc       = true,
};

void __init shuttle_i2c_initialize_vars(void)
{
	tegra_i2c_device1.dev.platform_data = &shuttle_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &shuttle_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &shuttle_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &shuttle_i2c4_platform_data;
}

static struct platform_device *shuttle_i2c_devices[] __initdata = {
	&tegra_i2c_device1,
	&tegra_i2c_device2,
	&tegra_i2c_device3,
	&tegra_i2c_device4,
};

int __init shuttle_i2c_register_devices(void)
{
	return platform_add_devices(shuttle_i2c_devices, ARRAY_SIZE(shuttle_i2c_devices));
}

