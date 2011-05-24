/*
 * arch/arm/mach-tegra/board-shuttle-usb.c
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

/* All configurations related to USB */
 
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/i2c-tegra.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/clk.h>
#include <mach/usb_phy.h>
#include <mach/system.h>

#include <linux/usb/android_composite.h>
#include <linux/usb/f_accessory.h>

#include "board.h"
#include "board-shuttle.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

#define USB_MANUFACTURER_NAME           "Shuttle"
#define USB_PRODUCT_NAME                "P10AN01"
#define USB_PRODUCT_ID_MTP             	0x70A8
#define USB_PRODUCT_ID_ADB          	0x70A9
#define USB_VENDOR_ID                   0x0955

static char *usb_functions[] = { "usb_mass_storage" };
static char *usb_functions_adb[] = { "usb_mass_storage", "adb" };
 
static struct android_usb_product usb_products[] = {
	{
		.product_id	= USB_PRODUCT_ID_MTP,
		.num_functions	= ARRAY_SIZE(usb_functions),
		.functions	= usb_functions,
	},
	{
		.product_id	= USB_PRODUCT_ID_ADB,
		.num_functions	= ARRAY_SIZE(usb_functions_adb),
		.functions	= usb_functions_adb,
	},
};

/* standard android USB platform data */
static struct android_usb_platform_data andusb_plat = {
	.vendor_id		= USB_VENDOR_ID,
	.product_id		= USB_PRODUCT_ID_ADB,
	.manufacturer_name	= USB_MANUFACTURER_NAME,
	.product_name		= USB_PRODUCT_NAME,
	.serial_number		= "0000",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_adb),
	.functions = usb_functions_adb,
};

static struct platform_device androidusb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data	= &andusb_plat,
	},
};

static struct tegra_utmip_config utmi_phy_config[] = {
	[0] = {
		.hssync_start_delay = 0,
		.idle_wait_delay = 17,
		.elastic_limit = 16,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
	[1] = {
		.hssync_start_delay = 0,
		.idle_wait_delay = 17,
		.elastic_limit = 16,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
};

static struct tegra_ulpi_config ulpi_phy_config = {
	.reset_gpio = SHUTTLE_USB1_RESET,
	.clk = "clk_dev2",
};

static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
	[0] = {
		.phy_config = &utmi_phy_config[0],
		.operating_mode = TEGRA_USB_HOST,
		.power_down_on_bus_suspend = 1,
	},
	[1] = {
		.phy_config = &ulpi_phy_config,
		.operating_mode = TEGRA_USB_HOST,
		.power_down_on_bus_suspend = 1,
	},
	[2] = {
		.phy_config = &utmi_phy_config[1],
		.operating_mode = TEGRA_USB_HOST,
		.power_down_on_bus_suspend = 1,
	},
};

void __init shuttle_usb_initialize_vars(void)
{
	tegra_ehci1_device.dev.platform_data = &tegra_ehci_pdata[0];
	tegra_ehci2_device.dev.platform_data = &tegra_ehci_pdata[1];
	tegra_ehci3_device.dev.platform_data = &tegra_ehci_pdata[2];
}

static struct platform_device *shuttle_usb_devices[] __initdata = {
	&tegra_ehci1_device,
	&tegra_ehci2_device,
	&tegra_ehci3_device,
	&androidusb_device,
};

int __init shuttle_usb_register_devices(void)
{
	return platform_add_devices(shuttle_usb_devices, ARRAY_SIZE(shuttle_usb_devices));
}
