// SPDX-License-Identifier: GPL-2.0
//
// CS42L43 I2C driver
//
// Copyright (C) 2022-2023 Cirrus Logic, Inc. and
//                         Cirrus Logic International Semiconductor Ltd.

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/module.h>

#include "cs42l43.h"

static int cs42l43_i2c_probe(struct i2c_client *i2c)
{
	struct cs42l43 *cs42l43;
	int ret;

	cs42l43 = devm_kzalloc(&i2c->dev, sizeof(*cs42l43), GFP_KERNEL);
	if (!cs42l43)
		return -ENOMEM;

	cs42l43->dev = &i2c->dev;
	cs42l43->irq = i2c->irq;
	// I2C is always attached by definition
	cs42l43->attached = true;

	cs42l43->regmap = devm_regmap_init_i2c(i2c, &cs42l43_i2c_regmap);
	if (IS_ERR(cs42l43->regmap)) {
		ret = PTR_ERR(cs42l43->regmap);
		dev_err(cs42l43->dev, "Failed to allocate regmap: %d\n", ret);
		return ret;
	}

	return cs42l43_dev_probe(cs42l43);
}

static void cs42l43_i2c_remove(struct i2c_client *i2c)
{
	struct cs42l43 *cs42l43 = dev_get_drvdata(&i2c->dev);

	cs42l43_dev_remove(cs42l43);
}

static struct i2c_device_id cs42l43_i2c_id[] = {
	{ "cs42l43", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, cs42l43_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id cs42l43_of_match[] = {
	{ .compatible = "cirrus,cs42l43", },
	{}
};
MODULE_DEVICE_TABLE(of, cs42l43_of_match);
#endif

#if IS_ENABLED(CONFIG_ACPI)
static const struct acpi_device_id cs42l43_acpi_match[] = {
	{ "CSC4243", 0 },
	{}
};
MODULE_DEVICE_TABLE(acpi, cs42l43_acpi_match);
#endif

static struct i2c_driver cs42l43_i2c_driver = {
	.driver = {
		.name			= "cs42l43",
		.pm			= &cs42l43_pm_ops,
		.of_match_table		= of_match_ptr(cs42l43_of_match),
		.acpi_match_table	= ACPI_PTR(cs42l43_acpi_match),
	},

	.probe_new	= cs42l43_i2c_probe,
	.remove		= cs42l43_i2c_remove,
	.id_table	= cs42l43_i2c_id,
};
module_i2c_driver(cs42l43_i2c_driver);

MODULE_IMPORT_NS(MFD_CS42L43);

MODULE_DESCRIPTION("CS42L43 I2C Driver");
MODULE_AUTHOR("Charles Keepax <ckeepax@opensource.cirrus.com>");
MODULE_LICENSE("GPL");
