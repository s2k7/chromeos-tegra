/*
 * arch/arm/mach-tegra/board-shuttle-clocks.c
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
#include <linux/dma-mapping.h>
#include <linux/fsl_devices.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/pda_power.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/i2c-tegra.h>
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
#include <mach/sdhci.h>
#include <mach/gpio.h>
#include <mach/clk.h>
#include <mach/usb_phy.h>
#include <mach/i2s.h>
#include <mach/suspend.h>
#include <mach/system.h>
#include <mach/nvmap.h>

#include "board.h"
#include "board-shuttle.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"

static __initdata struct tegra_clk_init_table shuttle_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "clk_dev1",	NULL,		26000000,	true},
	{ "clk_m",	NULL,		12000000,	true},
	{ "3d",		"pll_m",	266400000,	true},
	{ "2d",		"pll_m",	266400000,	true},
	{ "vi",		"pll_m",	50000000,	true},
	{ "vi_sensor",	"pll_m",	111000000,	false},
	{ "epp",	"pll_m",	266400000,	true},
	{ "mpe",	"pll_m",	111000000,	false},
	{ "emc",	"pll_m",	666000000,	true},
	{ "pll_c",	"clk_m",	600000000,	true},
	{ "pll_c_out1",	"pll_c",	240000000,	true},
	{ "vde",	"pll_c",	240000000,	false},
	{ "pll_p",	"clk_m",	216000000,	true},
	{ "pll_p_out1",	"pll_p",	28800000,	true},
	{ "pll_a",	"pll_p_out1",	56448000,	true},
	{ "pll_a_out0",	"pll_a",	11289600,	true},
	{ "cdev1",	"pll_a_out0",	11289600,	true},
	{ "i2s1",	"pll_a_out0",	11289600,	false},
	{ "audio",	"pll_a_out0",	11289600,	false},
	{ "audio_2x",	"audio",	22579200,	false},
	{ "pll_p_out2",	"pll_p",	48000000,	true},
	{ "pll_p_out3",	"pll_p",	72000000,	true},
	{ "i2c1_i2c",	"pll_p_out3",	72000000,	true},
	{ "i2c2_i2c",	"pll_p_out3",	72000000,	true},
	{ "i2c3_i2c",	"pll_p_out3",	72000000,	true},
	{ "dvc_i2c",	"pll_p_out3",	72000000,	true},
	{ "csi",	"pll_p_out3",	72000000,	false},
	{ "pll_p_out4",	"pll_p",	108000000,	true},
	{ "sclk",	"pll_p_out4",	108000000,	true},
	{ "hclk",	"sclk",		108000000,	true},
	{ "pclk",	"hclk",		54000000,	true},
	{ "apbdma",	"hclk",		54000000,	true},
	{ "spdif_in",	"pll_p",	36000000,	false},
	{ "csite",	"pll_p",	144000000,	true},
	{ "uartd",	"pll_p",	216000000,	true},
	{ "host1x",	"pll_p",	144000000,	true},
	{ "disp1",	"pll_p",	216000000,	true},
	{ "pll_d",	"clk_m",	1000000,	false},
	{ "pll_d_out0",	"pll_d",	500000,		false},
	{ "dsi",	"pll_d",	1000000,	false},
	{ "pll_u",	"clk_m",	480000000,	true},
	{ "clk_d",	"clk_m",	24000000,	true},
	{ "timer",	"clk_m",	12000000,	true},
	{ "i2s2",	"clk_m",	11289600,	false},
	{ "spdif_out",	"clk_m",	12000000,	false},
	{ "spi",	"clk_m",	12000000,	false},
	{ "xio",	"clk_m",	12000000,	false},
	{ "twc",	"clk_m",	12000000,	false},
	{ "sbc1",	"clk_m",	12000000,	false},
	{ "sbc2",	"clk_m",	12000000,	false},
	{ "sbc3",	"clk_m",	12000000,	false},
	{ "sbc4",	"clk_m",	12000000,	false},
	{ "ide",	"clk_m",	12000000,	false},
	{ "ndflash",	"clk_m",	108000000,	true},
	{ "vfir",	"clk_m",	12000000,	false},
	{ "sdmmc1",	"clk_m",	48000000,	true},
	{ "sdmmc2",	"clk_m",	48000000,	true},
	{ "sdmmc3",	"clk_m",	48000000,	false},
	{ "sdmmc4",	"clk_m",	48000000,	true},
	{ "la",		"clk_m",	12000000,	false},
	{ "owr",	"clk_m",	12000000,	false},
	{ "nor",	"clk_m",	12000000,	false},
	{ "mipi",	"clk_m",	12000000,	false},
	{ "i2c1",	"clk_m",	3000000,	false},
	{ "i2c2",	"clk_m",	3000000,	false},
	{ "i2c3",	"clk_m",	3000000,	false},
	{ "dvc",	"clk_m",	3000000,	false},
	{ "uarta",	"clk_m",	12000000,	false},
	{ "uartb",	"clk_m",	12000000,	false},
	{ "uartc",	"clk_m",	12000000,	false},
	{ "uarte",	"clk_m",	12000000,	false},
	{ "cve",	"clk_m",	12000000,	false},
	{ "tvo",	"clk_m",	12000000,	false},
	{ "hdmi",	"clk_m",	12000000,	false},
	{ "tvdac",	"clk_m",	12000000,	false},
	{ "disp2",	"clk_m",	12000000,	false},
	{ "usbd",	"clk_m",	12000000,	false},
	{ "usb2",	"clk_m",	12000000,	false},
	{ "usb3",	"clk_m",	12000000,	true},
	{ "isp",	"clk_m",	12000000,	false},
	{ "csus",	"clk_m",	12000000,	false},
	{ "pwm",	"clk_32k",	32768,		false},
	{ "clk_32k",	NULL,		32768,		true},
	{ "pll_s",	"clk_32k",	32768,		false},
	{ "rtc",	"clk_32k",	32768,		true},
	{ "kbc",	"clk_32k",	32768,		true},
	{ NULL,		NULL,		0,		0},
};

void __init shuttle_clks_init(void)
{
	tegra_clk_init_from_table(shuttle_clk_init_table);
}