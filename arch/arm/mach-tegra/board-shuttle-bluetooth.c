/*
 * Bluetooth Broadcomm  and low power control via GPIO
 *
 * Copyright (C) 2011 Eduardo José Tagle <ejtagle@tutopia.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/jiffies.h>
#include <linux/rfkill.h>
#include <linux/slab.h>

#include "board-shuttle.h"
#include "gpio-names.h"

#include <mach/hardware.h>
#include <asm/mach-types.h>


#define SHUTTLE_BT_RESET TEGRA_GPIO_PU0

struct shuttle_pm_bt_data {
	struct regulator *regulator;
	struct rfkill *rfkill;
	int pre_resume_state;
	int state;
};

/* Power control */
static void __shuttle_pm_bt_toggle_radio(struct device *dev, unsigned int on)
{
	struct shuttle_pm_bt_data *bt_data = dev_get_drvdata(dev);

	dev_info(dev, "__shuttle_pm_bt_toggle_radio %d\n", on);

	/* Avoid turning it on if already on */
	if  (bt_data->state == on)
		return;
	
	if (on) {
	
		regulator_enable(bt_data->regulator);
	
		/* Bluetooth power on sequence */
		gpio_set_value(SHUTTLE_BT_RESET, 0); /* Assert reset */
		msleep(2);
		gpio_set_value(SHUTTLE_BT_RESET, 1); /* Deassert reset */
		msleep(2);

	} else {
	
		gpio_set_value(SHUTTLE_BT_RESET, 0); /* Assert reset */
		
		regulator_disable(bt_data->regulator);
	}
	
	/* store new state */
	bt_data->state = on;
}

static int bt_rfkill_set_block(void *data, bool blocked)
{
	struct device *dev = data;
	dev_dbg(dev, "blocked %d\n", blocked);

	__shuttle_pm_bt_toggle_radio(dev, !blocked);

	return 0;
}

static const struct rfkill_ops shuttle_bt_rfkill_ops = {
       .set_block = bt_rfkill_set_block,
};

static ssize_t bt_read(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int ret = 0;
	struct shuttle_pm_bt_data *bt_data = dev_get_drvdata(dev);
	
	if (!strcmp(attr->attr.name, "power_on")) {
		if (bt_data->state)
			ret = 1;
	} else if (!strcmp(attr->attr.name, "reset")) {
		if (bt_data->state == 0)
			ret = 1;
	}

	if (!ret) {
		return strlcpy(buf, "0\n", 3);
	} else {
		return strlcpy(buf, "1\n", 3);
	}
}

static ssize_t bt_write(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long on = simple_strtoul(buf, NULL, 10);
	struct shuttle_pm_bt_data *bt_data = dev_get_drvdata(dev);

	if (!strcmp(attr->attr.name, "power_on")) {
		rfkill_set_sw_state(bt_data->rfkill, on ? 1 : 0);
		__shuttle_pm_bt_toggle_radio(dev, on);
	} else if (!strcmp(attr->attr.name, "reset")) {
		/* reset is low-active, so we need to invert */
		__shuttle_pm_bt_toggle_radio(dev, !on);
	}

	return count;
}

static DEVICE_ATTR(power_on, 0644, bt_read, bt_write);
static DEVICE_ATTR(reset, 0644, bt_read, bt_write);

#ifdef CONFIG_PM
static int shuttle_bt_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct shuttle_pm_bt_data *bt_data = dev_get_drvdata(&pdev->dev);

	dev_dbg(&pdev->dev, "suspending\n");

	bt_data->pre_resume_state = bt_data->state;
	__shuttle_pm_bt_toggle_radio(&pdev->dev, 0);

	return 0;
}

static int shuttle_bt_resume(struct platform_device *pdev)
{
	struct shuttle_pm_bt_data *bt_data = dev_get_drvdata(&pdev->dev);
	dev_dbg(&pdev->dev, "resuming\n");

	__shuttle_pm_bt_toggle_radio(&pdev->dev, bt_data->pre_resume_state);
	return 0;
}
#else
#define shuttle_bt_suspend	NULL
#define shuttle_bt_resume		NULL
#endif

static struct attribute *shuttle_bt_sysfs_entries[] = {
	&dev_attr_power_on.attr,
	&dev_attr_reset.attr,
	NULL
};

static struct attribute_group shuttle_bt_attr_group = {
	.name	= NULL,
	.attrs	= shuttle_bt_sysfs_entries,
};

static int __init shuttle_bt_probe(struct platform_device *pdev)
{
	struct rfkill *rfkill;
	struct regulator *regulator;
	struct shuttle_pm_bt_data *bt_data;
	int ret;

	dev_info(&pdev->dev, "starting\n");

	bt_data = kzalloc(sizeof(*bt_data), GFP_KERNEL);
	dev_set_drvdata(&pdev->dev, bt_data);

	regulator = regulator_get(&pdev->dev, "vddhostif_bt");
	if (IS_ERR(regulator))
		return -ENODEV;

	bt_data->regulator = regulator;

	/* Init io pins */
	tegra_gpio_enable(SHUTTLE_BT_RESET);
	gpio_request(SHUTTLE_BT_RESET, "bluetooth_reset");
	gpio_direction_output(SHUTTLE_BT_RESET, 0);
	
	rfkill = rfkill_alloc(pdev->name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
                            &shuttle_bt_rfkill_ops, &pdev->dev);

	if (!rfkill) {
		dev_err(&pdev->dev, "Failed to allocate rfkill\n");
		return -ENOMEM;
	}
	bt_data->rfkill = rfkill;

	/* Disable bluetooth */
	__shuttle_pm_bt_toggle_radio(&pdev->dev,0);
    rfkill_init_sw_state(rfkill, 0);

	ret = rfkill_register(rfkill);
	if (ret) {
		rfkill_destroy(rfkill);
		dev_err(&pdev->dev, "Failed to register rfkill\n");
		return ret;
	}
	
	return sysfs_create_group(&pdev->dev.kobj, &shuttle_bt_attr_group);
}

static int shuttle_bt_remove(struct platform_device *pdev)
{
	struct shuttle_pm_bt_data *bt_data = dev_get_drvdata(&pdev->dev);

	sysfs_remove_group(&pdev->dev.kobj, &shuttle_bt_attr_group);

	if (bt_data->rfkill) {
		rfkill_destroy(bt_data->rfkill);
	}

	if (!bt_data || !bt_data->regulator)
		return 0;

	__shuttle_pm_bt_toggle_radio(&pdev->dev, 0);

	regulator_put(bt_data->regulator);

	kfree(bt_data);

	return 0;
}
static struct platform_driver shuttle_bt_driver = {
	.probe		= shuttle_bt_probe,
	.remove		= shuttle_bt_remove,
	.suspend	= shuttle_bt_suspend,
	.resume		= shuttle_bt_resume,
	.driver		= {
		.name		= "shuttle-pm-bt",
	},
};

static int __devinit shuttle_bt_init(void)
{
	return platform_driver_register(&shuttle_bt_driver);
}

static void shuttle_bt_exit(void)
{
	platform_driver_unregister(&shuttle_bt_driver);
}

module_init(shuttle_bt_init);
module_exit(shuttle_bt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eduardo José Tagle <ejtagle@tutopia.com>");
MODULE_DESCRIPTION("Shuttle Bluetooth power management");


 