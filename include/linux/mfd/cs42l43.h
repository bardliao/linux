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
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/soundwire/sdw.h>

#ifndef CS42L43_CORE_EXT_H
#define CS42L43_CORE_EXT_H

#define CS42L43_N_SUPPLIES		3
#define CS42L43_UPDATE_RETRIES		5

enum cs42l43_power_status {
	CS42L43_POWER_OFF = 0,
	CS42L43_POWER_APPLIED,
	CS42L43_POWER_IDENTIFIED,
	CS42L43_POWER_UPDATED,
	CS42L43_POWER_READY,
	CS42L43_POWER_SUSPENDED,
};

struct cs42l43 {
	struct device *dev;
	struct regmap *regmap;
	struct sdw_slave *sdw;

	struct regulator *vdd_p;
	struct regulator *vdd_d;
	struct regulator_bulk_data core_supplies[CS42L43_N_SUPPLIES];

	struct gpio_desc *reset;
	int irq;

	enum cs42l43_power_status power_status;
	struct completion resume_renum;

	bool hw_lock;
};

#endif /* CS42L43_CORE_EXT_H */
