/*
 * arch/arm/mach-tegra/board-shuttle-audio.c
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

/* All configurations related to audio */
 
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/i2c.h>
#include <linux/version.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>
#include <asm/io.h>

#include <mach/io.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/gpio.h>
#include <mach/i2s.h>
#include <mach/system.h>
#include <mach/shuttle_audio.h>

#include "board.h"
#include "board-shuttle.h"
#include "gpio-names.h"
#include "devices.h"

/* For Shuttle, 
	Codec is ALC5624
	Codec I2C Address = 0x30(includes R/W bit), PMU i2c segment 0 
	Codec MCLK = APxx DAP_MCLK1
*/

static struct i2c_board_info __initdata shuttle_i2c_bus0_board_info[] = {
	{
		I2C_BOARD_INFO("alc5624", 0x18),
	},
};

static struct shuttle_audio_platform_data shuttle_audio_pdata = {
	.gpio_hp_det = SHUTTLE_HP_DETECT,
}; 

static struct platform_device shuttle_audio_device = {
	.name = "tegra-snd-shuttle",
	.id   = 0,
	.dev = {
		.platform_data = &shuttle_audio_pdata,
	}, 
};

static struct platform_device spdif_dit_device = {
	.name   = "spdif-dit",
	.id     = -1,
}; 

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
/* Linux Kernel 2.6.36 didn't support an standard linux interfase ... */
/* This is the Stereo DAC interface. */
static struct tegra_audio_platform_data tegra_audio_pdata = {
	.i2s_master		= false, /* Stereo DAC */
	.dsp_master		= false, /* Don't care */
	.dma_on			= true,  /* use dma by default */
	.i2s_clk_rate	= 240000000,
	.dap_clk		= "clk_dev1",
	.audio_sync_clk = "audio_2x",
	.mode			= I2S_BIT_FORMAT_I2S,
	.fifo_fmt		= I2S_FIFO_PACKED,
	.bit_size		= I2S_BIT_SIZE_16,
	.i2s_bus_width 	= 32, /* Using Packed 16 bit data, the dma is 32 bit. */
	.dsp_bus_width 	= 16, /* When using DSP mode (unused), this should be 16 bit. */
	.mask			= TEGRA_AUDIO_ENABLE_TX,
};

/* Bluetooth Audio. */
static struct tegra_audio_platform_data tegra_audio2_pdata = {
	.i2s_master		= false, 		/* Bluetooth audio */
	.dsp_master		= true,  		/* Bluetooth */
	.dsp_master_clk = 8000, 	/* Bluetooth audio speed */
	.dma_on			= true,  		/* use dma by default */
	.i2s_clk_rate	= 2000000, 	/* BCM4329 max bitclock is 2048000 Hz */
	.dap_clk		= "clk_dev1",
	.audio_sync_clk = "audio_2x",
	.mode			= I2S_BIT_FORMAT_DSP, /* Using CODEC in network mode */
	.fifo_fmt		= I2S_FIFO_16_LSB,
	.bit_size		= I2S_BIT_SIZE_16,
	.i2s_bus_width 	= 16, /* Capturing a single timeslot, mono 16 bits */
	.dsp_bus_width 	= 16,
	.mask			= TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX,
};

static struct tegra_audio_platform_data tegra_spdif_pdata = {
	.dma_on			= true,  /* use dma by default */
	.i2s_clk_rate	= 5644800,
	.mode			= SPDIF_BIT_MODE_MODE16BIT,
	.fifo_fmt		= 1,
};
#endif

/* Default music path: I2S1(DAC1)<->Dap1<->HifiCodec
   Bluetooth to codec: I2S2(DAC2)<->Dap4<->Bluetooth
*/
static inline void das_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_ADDRESS(TEGRA_APB_MISC_BASE) + offset);
}

#define APB_MISC_DAS_DAP_CTRL_SEL_0             0xc00
#define APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0   0xc40

static void init_dac1(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)	
	bool master = tegra_audio_pdata.i2s_master;
#else
	bool master = false;
#endif
	/* DAC1 -> DAP1 */
	das_writel((!master)<<31, APB_MISC_DAS_DAP_CTRL_SEL_0);
	das_writel(0, APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0);
}

static void init_dac2(void)
{
	/* DAC2 -> DAP4 for Bluetooth Voice */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)		
	bool master = tegra_audio2_pdata.dsp_master;
#else
	bool master = false;
#endif
	das_writel((!master)<<31 | 1, APB_MISC_DAS_DAP_CTRL_SEL_0 + 12);
	das_writel(3<<28 | 3<<24 | 3,
			APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0 + 4);
}

void __init shuttle_audio_initialize_vars(void)
{
	/* Init audio connections */
	init_dac1();
	init_dac2();

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)	
	/* Patch in the platform data */
	tegra_i2s_device1.dev.platform_data = &tegra_audio_pdata;
	tegra_i2s_device2.dev.platform_data = &tegra_audio2_pdata;
	tegra_spdif_device.dev.platform_data = &tegra_spdif_pdata;
#endif
}

static struct platform_device *shuttle_i2s_devices[] __initdata = {
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&shuttle_audio_device,
	&spdif_dit_device,
	&tegra_spdif_device,
	&tegra_das_device,
	&tegra_pcm_device,
};

int __init shuttle_audio_register_devices(void)
{
	int ret = i2c_register_board_info(0, shuttle_i2c_bus0_board_info, 
		ARRAY_SIZE(shuttle_i2c_bus0_board_info)); 
	if (ret)
		return ret;
	return platform_add_devices(shuttle_i2s_devices, ARRAY_SIZE(shuttle_i2s_devices));
}
