/*
 * arch/arm/mach-tegra/board-shuttle.c
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

#include <linux/usb/android_composite.h>
#include <linux/usb/f_accessory.h>

#include "board.h"
#include "board-shuttle.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"
#include "wakeups-t2.h"


static atomic_t shuttle_3g_gps_powered = ATOMIC_INIT(0);
void shuttle_3g_gps_poweron(void)
{
	if (atomic_inc_return(&shuttle_3g_gps_powered) == 1) {
		pr_debug("enabling 3G/GPS module\n");
		/* 3G/GPS power on sequence */
		gpio_set_value(SHUTTLE_3GGPS_DISABLE, 0); /* Enable power */
		msleep(2);
	}
}
EXPORT_SYMBOL_GPL(shuttle_3g_gps_poweron);

void shuttle_3g_gps_poweroff(void)
{
	if (atomic_dec_return(&shuttle_3g_gps_powered) == 0) {
		pr_debug("disabling 3G/GPS module\n");
		/* 3G/GPS power on sequence */
		gpio_set_value(SHUTTLE_3GGPS_DISABLE, 1); /* Disable power */
		msleep(2);
	}
}
EXPORT_SYMBOL_GPL(shuttle_3g_gps_poweroff);

static atomic_t shuttle_3g_gps_inited = ATOMIC_INIT(0);
void shuttle_3g_gps_init(void)
{
	if (atomic_inc_return(&shuttle_3g_gps_inited) == 1) {
		gpio_request(SHUTTLE_3GGPS_DISABLE, "gps_disable");
		gpio_direction_output(SHUTTLE_3GGPS_DISABLE, 1);
	}
}
EXPORT_SYMBOL_GPL(shuttle_3g_gps_init);

void shuttle_3g_gps_deinit(void)
{
	atomic_dec(&shuttle_3g_gps_inited);
}
EXPORT_SYMBOL_GPL(shuttle_3g_gps_deinit);

static struct tegra_suspend_platform_data shuttle_suspend = {
	.cpu_timer = 5000,
	.cpu_off_timer = 5000,
	.core_timer = 0x7e7e,
	.core_off_timer = 0x7f,
	.separate_req = true,
    .corereq_high = false,
	.sysclkreq_high = true,
	.suspend_mode = TEGRA_SUSPEND_LP0,
	.wake_enb = SHUTTLE_WAKE_KEY_POWER | 
				SHUTTLE_WAKE_KEY_RESUME | 
				TEGRA_WAKE_RTC_ALARM,
	.wake_high = TEGRA_WAKE_RTC_ALARM,
	.wake_low = SHUTTLE_WAKE_KEY_POWER | 
				SHUTTLE_WAKE_KEY_RESUME,
	.wake_any = 0,
};

static void __init tegra_shuttle_init(void)
{
	struct clk *clk;

	/* force consoles to stay enabled across suspend/resume */
	console_suspend_enabled = 0;

	/* Init the suspend information */
	tegra_init_suspend(&shuttle_suspend);
	
	/* Init the external memory controller and memory frequency scaling */
	shuttle_init_emc();

	/* Set the SDMMC2 (wifi) tap delay to 6.  This value is determined
	 * based on propagation delay on the PCB traces. */
	clk = clk_get_sys("sdhci-tegra.1", NULL);
	if (!IS_ERR(clk)) {
		tegra_sdmmc_tap_delay(clk, 6);
		clk_put(clk);
	} else {
		pr_err("Failed to set wifi sdmmc tap delay\n");
	}

	/* Initialize the pinmux */
	shuttle_pinmux_init();

	/* Initialize the clocks */
	shuttle_clks_init();

	/* Initialize audio and bluetooth voice variables */
	shuttle_audio_initialize_vars();

	/* Initialize USB variables */
	shuttle_usb_initialize_vars();
	
	/* Initialize SDHCI variables */
	shuttle_sdhci_initialize_vars();
	
	/* Init memory areas */
	shuttle_gpu_initialize_areas();

	/* Register UART devices */
	shuttle_uart_register_devices();
	
	/* Register SPI devices */
	shuttle_spi_register_devices();
	
	/* Register GPU devices */
	shuttle_gpu_register_devices();
	
	/* Register Audio devices */
	shuttle_audio_register_devices();
	
	/* Register AES encryption devices */
	shuttle_aes_register_devices();

	/* Register Watchdog devices */
	shuttle_wdt_register_devices();

	/* Init power management of Tegra */
	shuttle_pmu_register_devices();

	/* Register i2c devices */
	shuttle_i2c_register_devices();
	
	/* Register the power subsystem - Including the poweroff handler */
	shuttle_power_register_devices();
	
	/* Register all the keyboard devices */
	shuttle_keyboard_register_devices();
	
	/* Register touchscreen devices */
	shuttle_touch_register_devices();
	
	/* Register SDHCI devices */
	shuttle_sdhci_register_devices();

	/* Register accelerometer device */
	shuttle_sensors_register_devices();
	
	/* Register wlan powermanagement devices */
	shuttle_wlan_pm_register_devices();
	
	/* Register gps powermanagement devices */
	shuttle_gps_pm_register_devices();

	/* Register gsm powermanagement devices */
	shuttle_gsm_pm_register_devices();
	
	/* Register Bluetooth powermanagement devices */
	shuttle_bt_pm_register_devices();

	/* Register Camera powermanagement devices */
	shuttle_camera_pm_register_devices();

	/* Register NAND flash devices */
	shuttle_nand_register_devices();
	
	/* Register the USB device */
	shuttle_usb_register_devices();
}


void __init shuttle_reserve(void)
{
	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	/* Reserve the graphics memory */
	tegra_reserve(SHUTTLE_GPU_MEM_SIZE, SHUTTLE_FB1_MEM_SIZE, SHUTTLE_FB2_MEM_SIZE);
}

static void __init tegra_shuttle_fixup(struct machine_desc *desc,
	struct tag *tags, char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 1;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = (SHUTTLE_MEM_SIZE - SHUTTLE_GPU_MEM_SIZE);
} 

MACHINE_START(STINGRAY, "Shuttle")
	.boot_params	= 0x00000100,
	.map_io         = tegra_map_common_io,
	.init_early     = tegra_init_early,
	.init_irq       = tegra_init_irq,
	.timer          = &tegra_timer, 	
	.init_machine	= tegra_shuttle_init,
	.reserve		= shuttle_reserve,
	.fixup			= tegra_shuttle_fixup,
MACHINE_END

#if 0
#define PMC_WAKE_STATUS 0x14

static int shuttle_wakeup_key(void)
{
	unsigned long status = 
		readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);
	return status & TEGRA_WAKE_GPIO_PV2 ? KEY_POWER : KEY_RESERVED;
}
#endif


