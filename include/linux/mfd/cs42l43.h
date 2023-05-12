/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CS42L43 core driver external data
 *
 * Copyright (C) 2022-2023 Cirrus Logic, Inc. and
 *                         Cirrus Logic International Semiconductor Ltd.
 */

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/soundwire/sdw.h>
#include <linux/workqueue.h>

#ifndef CS42L43_CORE_EXT_H
#define CS42L43_CORE_EXT_H

#define CS42L43_N_SUPPLIES		3

struct cs42l43 {
	struct device *dev;
	struct regmap *regmap;
	struct sdw_slave *sdw;

	struct regulator *vdd_p;
	struct regulator *vdd_d;
	struct regulator_bulk_data core_supplies[CS42L43_N_SUPPLIES];

	struct gpio_desc *reset;
	int irq;

	struct work_struct boot_work;
	struct completion device_attach;
	struct completion device_detach;
	struct completion firmware_download;
	int firmware_error;

	unsigned int sdw_freq;
	// Lock to gate control of the PLL and its sources
	struct mutex pll_lock;

	bool sdw_pll_active;
	bool attached;
	bool hw_lock;
};

#endif /* CS42L43_CORE_EXT_H */
