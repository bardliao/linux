// SPDX-License-Identifier: GPL-2.0
//
// CS42L43 SoundWire driver
//
// Copyright (C) 2022-2023 Cirrus Logic, Inc. and
//                         Cirrus Logic International Semiconductor Ltd.

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mfd/cs42l43-regs.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_registers.h>
#include <linux/soundwire/sdw_type.h>

#include "cs42l43.h"

enum cs42l43_sdw_ports {
	CS42L43_DMIC_DEC_ASP_PORT = 1,
	CS42L43_SPK_TX_PORT,
	CS42L43_SPDIF_HP_PORT,
	CS42L43_SPK_RX_PORT,
	CS42L43_ASP_PORT,
};

static int cs42l43_read_prop(struct sdw_slave *sdw)
{
	struct sdw_slave_prop *prop = &sdw->prop;
	struct device *dev = &sdw->dev;
	struct sdw_dpn_prop *dpn;
	unsigned long addr;
	int nval;
	int i;
	u32 bit;

	prop->irq = true;
	prop->paging_support = true;
	prop->wake_capable = true;
	prop->source_ports = BIT(CS42L43_DMIC_DEC_ASP_PORT) | BIT(CS42L43_SPK_TX_PORT);
	prop->sink_ports = BIT(CS42L43_SPDIF_HP_PORT) |
			   BIT(CS42L43_SPK_RX_PORT) | BIT(CS42L43_ASP_PORT);
	prop->quirks = SDW_SLAVE_QUIRKS_INVALID_INITIAL_PARITY;
	prop->scp_int1_mask = SDW_SCP_INT1_BUS_CLASH | SDW_SCP_INT1_PARITY | SDW_SCP_INT1_IMPL_DEF;

	nval = hweight32(prop->source_ports);
	prop->src_dpn_prop = devm_kcalloc(dev, nval, sizeof(*prop->src_dpn_prop), GFP_KERNEL);
	if (!prop->src_dpn_prop)
		return -ENOMEM;

	i = 0;
	dpn = prop->src_dpn_prop;
	addr = prop->source_ports;
	for_each_set_bit(bit, &addr, 32) {
		dpn[i].num = bit;
		dpn[i].max_ch = 2;
		dpn[i].type = SDW_DPN_FULL;
		dpn[i].max_word = 24;
		i++;
	}
	/* All ports are 2 channels max, except the first one, CS42L43_DMIC_DEC_ASP_PORT */
	dpn[CS42L43_DMIC_DEC_ASP_PORT].max_ch = 4;

	nval = hweight32(prop->sink_ports);
	prop->sink_dpn_prop = devm_kcalloc(dev, nval, sizeof(*prop->sink_dpn_prop), GFP_KERNEL);
	if (!prop->sink_dpn_prop)
		return -ENOMEM;

	i = 0;
	dpn = prop->sink_dpn_prop;
	addr = prop->sink_ports;
	for_each_set_bit(bit, &addr, 32) {
		dpn[i].num = bit;
		dpn[i].max_ch = 2;
		dpn[i].type = SDW_DPN_FULL;
		dpn[i].max_word = 24;
		i++;
	}

	return 0;
}

static int cs42l43_sdw_update_status(struct sdw_slave *sdw, enum sdw_slave_status status)
{
	struct cs42l43 *cs42l43 = dev_get_drvdata(&sdw->dev);
	int ret;

	switch (status) {
	case SDW_SLAVE_ATTACHED:
		dev_dbg(cs42l43->dev, "Device attach at power status: %d\n",
			cs42l43->power_status);

		switch (cs42l43->power_status) {
		case CS42L43_POWER_APPLIED:
			cs42l43->irq = sdw->irq;

			ret = cs42l43_dev_identify(cs42l43);
			if (ret)
				goto err;
			fallthrough;
		case CS42L43_POWER_IDENTIFIED:
			ret = cs42l43_dev_update(cs42l43);
			if (ret == -EAGAIN)
				break;
			else if (ret)
				goto err;
			fallthrough;
		case CS42L43_POWER_UPDATED:
			ret = cs42l43_dev_init(cs42l43);
			if (ret)
				goto err;
			fallthrough;
		case CS42L43_POWER_READY:
			sdw_write_no_pm(cs42l43->sdw, CS42L43_GEN_INT_MASK_1,
					CS42L43_INT_STAT_GEN1_MASK);
			break;
		case CS42L43_POWER_SUSPENDED:
			complete(&cs42l43->resume_renum);
			break;
		default:
			dev_err(cs42l43->dev, "Invalid power status: %d\n",
				cs42l43->power_status);
			return -EINVAL;
		}
		break;
	default:
		dev_dbg(cs42l43->dev, "status: %d\n", status);
		break;
	}

	return 0;

err:
	cs42l43_dev_abort(cs42l43);

	return ret;
}

static int cs42l43_sdw_interrupt(struct sdw_slave *sdw,
				 struct sdw_slave_intr_status *status)
{
	sdw_read_no_pm(sdw, CS42L43_GEN_INT_STAT_1);
	sdw_write_no_pm(sdw, CS42L43_GEN_INT_STAT_1, 1);

	return 0;
}

static const struct sdw_slave_ops cs42l43_sdw_ops = {
	.read_prop		= cs42l43_read_prop,
	.update_status		= cs42l43_sdw_update_status,
	.interrupt_callback	= cs42l43_sdw_interrupt,
};

static int cs42l43_sdw_probe(struct sdw_slave *sdw, const struct sdw_device_id *id)
{
	struct cs42l43 *cs42l43;
	struct device *dev = &sdw->dev;

	pr_err("bard: %s\n", __func__);
	cs42l43 = devm_kzalloc(dev, sizeof(*cs42l43), GFP_KERNEL);
	if (!cs42l43)
		return -ENOMEM;

	cs42l43->dev = dev;
	cs42l43->sdw = sdw;

	cs42l43->regmap = devm_regmap_init_sdw(sdw, &cs42l43_sdw_regmap);
	if (IS_ERR(cs42l43->regmap)) {
		dev_err(cs42l43->dev, "Failed to allocate regmap: %ld\n",
			PTR_ERR(cs42l43->regmap));
		return PTR_ERR(cs42l43->regmap);
	}

	return cs42l43_dev_probe(cs42l43);
}

static int cs42l43_sdw_remove(struct sdw_slave *sdw)
{
	struct cs42l43 *cs42l43 = dev_get_drvdata(&sdw->dev);

	cs42l43_dev_remove(cs42l43);

	return 0;
}

static const struct sdw_device_id cs42l43_sdw_id[] = {
	SDW_SLAVE_ENTRY(0x01FA, 0x4243, 0),
	{}
};
MODULE_DEVICE_TABLE(sdw, cs42l43_sdw_id);

static struct sdw_driver cs42l43_sdw_driver = {
	.driver = {
		.name	= "cs42l43",
		.pm	= &cs42l43_pm_ops,
	},

	.probe		= cs42l43_sdw_probe,
	.remove		= cs42l43_sdw_remove,
	.ops		= &cs42l43_sdw_ops,
	.id_table	= cs42l43_sdw_id,
};
module_sdw_driver(cs42l43_sdw_driver);

MODULE_IMPORT_NS(MFD_CS42L43);

MODULE_DESCRIPTION("CS42L43 SoundWire Driver");
MODULE_AUTHOR("Lucas Tanure <tanureal@opensource.cirrus.com>");
MODULE_LICENSE("GPL");
