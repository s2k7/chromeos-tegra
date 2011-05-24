/*
 * arch/arm/mach-tegra/board-shuttle-uart.c
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

#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/memblock.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/io.h>
#include <mach/w1.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/iomap.h>

#include "board.h"
#include "board-shuttle.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

/* Shuttle uses UARTB for the debug port. */
static struct plat_serial8250_port debug_uart_platform_data[] = {
	{
		.membase 	= IO_ADDRESS(TEGRA_UARTB_BASE),
		.mapbase 	= TEGRA_UARTB_BASE,
		.irq 		= INT_UARTB,
		.flags		= UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= 216000000,
	}, {
		.flags		= 0,
	}
};

static struct platform_device debug_uart = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uart_platform_data,
	},
}; 

static struct platform_device *shuttle_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&debug_uart,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

int __init shuttle_uart_register_devices(void)
{
	return platform_add_devices(shuttle_uart_devices, ARRAY_SIZE(shuttle_uart_devices));
}