// SPDX-License-Identifier: GPL-2.0-only
//
// rt711-sdw-sdca.c -- rt711 SDCA ALSA SoC audio driver
//
// Copyright(c) 2020 Realtek Semiconductor Corp.
//
//

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/soundwire/sdw_registers.h>
#include <linux/module.h>

#include "rt711-sdca.h"
#include "rt711-sdca-sdw.h"

static bool rt711_sdca_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SDW_DP0_INT:
	case SDW_SCP_SDCA_INT1 ... SDW_SCP_SDCA_INTMASK4:
	case 0x201a ... 0x2027:
	case 0x2029 ... 0x202a:
	case 0x202d ... 0x2034:
	case 0x2200 ... 0x2204:
	case 0x2206 ... 0x2212:
	case 0x2220 ... 0x2223:
	case 0x2230 ... 0x2239:
	case 0x2f01 ... 0x2f0f:
	case 0x2f30 ... 0x2f36:
	case 0x2f50 ... 0x2f5a:
	case 0x2f60:
	case 0x3200 ... 0x3212:
	case 0x2000000 ... 0x20000ff:
	case 0x2002000 ... 0x20020ff:
	case 0x5600000 ... 0x56000ff:
	case 0x5602000 ... 0x56020ff:
	case 0x5700000 ... 0x57000ff:
	case 0x5702000 ... 0x57020ff:
	case 0x5800000 ... 0x58000ff:
	case 0x5802000 ... 0x58020ff:
	case 0x5900000 ... 0x59000ff:
	case 0x5902000 ... 0x59020ff:
	case 0x5b00000 ... 0x5b000ff:
	case 0x5b02000 ... 0x5b020ff:
	case 0x5f00000 ... 0x5f000ff:
	case 0x5f02000 ... 0x5f020ff:
	case 0x6100000 ... 0x61000ff:
	case 0x6102000 ... 0x61020ff:
	case 0x40600488 ... 0x40600490:
	case 0x40c80080 ... 0x40c80098:
	case 0x44030000 ... 0x44030017:
		return true;
	default:
		return false;
	}
}

static bool rt711_sdca_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SDW_DP0_INT:
	case SDW_SCP_SDCA_INT1 ... SDW_SCP_SDCA_INT4:
	case 0x201b:
	case 0x201c:
	case 0x201d:
	case 0x201f:
	case 0x2021:
	case 0x2023:
	case 0x2230:
	case 0x202d ... 0x202f: /* BRA */
	case 0x2200 ... 0x2212: /* i2c debug */
	case RT711_RC_CAL_STATUS:
	case 0x200001a:
	case 0x200201a:
	case 0x2000046:
	case 0x2002046:
	case 0x2000080:
	case 0x2002080:
	case 0x2000081:
	case 0x2002081:
	case 0x2000083:
	case 0x2002083:
	case 0x5800000:
	case 0x5802000:
	case 0x5800001:
	case 0x5802001:
	case 0x5f00001:
	case 0x5f02001:
	case 0x40600490:
	case 0x40c80080 ... 0x40c80098:
	case 0x44030000 ... 0x44030017:
		return true;
	default:
		return false;
	}
}

static int rt711_sdca_sdw_read(void *context, unsigned int reg, unsigned int *val)
{
	struct device *dev = context;
	struct rt711_sdca_priv *rt711 = dev_get_drvdata(dev);
	unsigned int sdw_data_1, sdw_data_0;
	int ret;

	switch (reg) {
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_USER_FU05, RT711_SDCA_CTL_FU_VOLUME, CH_L):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_USER_FU05, RT711_SDCA_CTL_FU_VOLUME, CH_R):
	case SDW_SDCA_CTL(FUN_MIC_ARRAY, RT711_SDCA_ENT_USER_FU1E, RT711_SDCA_CTL_FU_VOLUME, CH_L):
	case SDW_SDCA_CTL(FUN_MIC_ARRAY, RT711_SDCA_ENT_USER_FU1E, RT711_SDCA_CTL_FU_VOLUME, CH_R):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_USER_FU0F, RT711_SDCA_CTL_FU_VOLUME, CH_L):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_USER_FU0F, RT711_SDCA_CTL_FU_VOLUME, CH_R):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_PLATFORM_FU44, RT711_SDCA_CTL_FU_CH_GAIN, CH_L):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_PLATFORM_FU44, RT711_SDCA_CTL_FU_CH_GAIN, CH_R):
	case SDW_SDCA_CTL(FUN_MIC_ARRAY, RT711_SDCA_ENT_PLATFORM_FU15, RT711_SDCA_CTL_FU_CH_GAIN, CH_L):
	case SDW_SDCA_CTL(FUN_MIC_ARRAY, RT711_SDCA_ENT_PLATFORM_FU15, RT711_SDCA_CTL_FU_CH_GAIN, CH_R):
		ret = regmap_read(rt711->sdw_regmap, reg, &sdw_data_0);
		if (ret < 0)
			return ret;
		ret = regmap_read(rt711->sdw_regmap, SDW_SDCA_MBQ_CTL(reg), &sdw_data_1);
		if (ret < 0)
			return ret;
		*val = ((sdw_data_1 & 0xff) << 8) | (sdw_data_0 & 0xff);
		break;
	default:
		/* SDCA mapping case or normal address */
		if (reg & 0x40000000 || reg < 0xffff) {
			ret = regmap_read(rt711->sdw_regmap, reg, val);
			if (ret < 0)
				return ret;
		}

		/* vendor registers case */
		if (reg < 0x40000000 && reg > 0xffff) {
			ret = regmap_read(rt711->sdw_regmap, reg, &sdw_data_0);
			if (ret < 0)
				return ret;
			ret = regmap_read(rt711->sdw_regmap,
				SDW_SDCA_MBQ_CTL(reg), &sdw_data_1);
			if (ret < 0)
				return ret;
			*val = ((sdw_data_1 & 0xff) << 8) | (sdw_data_0 & 0xff);
		}
		break;
	}

	dev_dbg(dev, "[%s] %04x => %08x\n", __func__, reg, *val);
	return 0;
}

static int rt711_sdca_sdw_write(void *context, unsigned int reg, unsigned int val)
{
	struct device *dev = context;
	struct rt711_sdca_priv *rt711 = dev_get_drvdata(dev);

	switch (reg) {
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_USER_FU05, RT711_SDCA_CTL_FU_VOLUME, CH_L):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_USER_FU05, RT711_SDCA_CTL_FU_VOLUME, CH_R):
	case SDW_SDCA_CTL(FUN_MIC_ARRAY, RT711_SDCA_ENT_USER_FU1E, RT711_SDCA_CTL_FU_VOLUME, CH_L):
	case SDW_SDCA_CTL(FUN_MIC_ARRAY, RT711_SDCA_ENT_USER_FU1E, RT711_SDCA_CTL_FU_VOLUME, CH_R):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_USER_FU0F, RT711_SDCA_CTL_FU_VOLUME, CH_L):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_USER_FU0F, RT711_SDCA_CTL_FU_VOLUME, CH_R):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_PLATFORM_FU44, RT711_SDCA_CTL_FU_CH_GAIN, CH_L):
	case SDW_SDCA_CTL(FUN_JACK_CODEC, RT711_SDCA_ENT_PLATFORM_FU44, RT711_SDCA_CTL_FU_CH_GAIN, CH_R):
	case SDW_SDCA_CTL(FUN_MIC_ARRAY, RT711_SDCA_ENT_PLATFORM_FU15, RT711_SDCA_CTL_FU_CH_GAIN, CH_L):
	case SDW_SDCA_CTL(FUN_MIC_ARRAY, RT711_SDCA_ENT_PLATFORM_FU15, RT711_SDCA_CTL_FU_CH_GAIN, CH_R):
		regmap_write(rt711->sdw_regmap, SDW_SDCA_MBQ_CTL(reg), ((val >> 8) & 0xff));
		regmap_write(rt711->sdw_regmap, reg, (val & 0xff));
		break;
	default:
		/* SDCA mapping case or normal address */
		if (reg & 0x40000000 || reg < 0xffff)
			regmap_write(rt711->sdw_regmap, reg, val);

		/* vendor registers case */
		if (reg < 0x40000000 && reg > 0xffff) {
			regmap_write(rt711->sdw_regmap,
				SDW_SDCA_MBQ_CTL(reg), ((val >> 8) & 0xff));
			regmap_write(rt711->sdw_regmap, reg, (val & 0xff));
		}
		break;
	}

	dev_dbg(dev, "[%s] %04x <= %04x\n", __func__, reg, val);
	return 0;
}

static const struct regmap_config rt711_sdca_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.readable_reg = rt711_sdca_readable_register,
	.volatile_reg = rt711_sdca_volatile_register,
	.max_register = 0x44ffffff,
	.reg_defaults = rt711_sdca_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(rt711_sdca_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
	.use_single_read = true,
	.use_single_write = true,
	.reg_read = rt711_sdca_sdw_read,
	.reg_write = rt711_sdca_sdw_write,
};

static const struct regmap_config rt711_sdca_sdw_regmap = {
	.name = "sdw",
	.reg_bits = 32,
	.val_bits = 8,
	.readable_reg = rt711_sdca_readable_register,
	.max_register = 0x44ffffff,
	.cache_type = REGCACHE_NONE,
	.use_single_read = true,
	.use_single_write = true,
};

static int rt711_sdca_update_status(struct sdw_slave *slave,
				enum sdw_slave_status status)
{
	struct rt711_sdca_priv *rt711 = dev_get_drvdata(&slave->dev);

	/* Update the status */
	rt711->status = status;

	if (status == SDW_SLAVE_UNATTACHED)
		rt711->hw_init = false;

	/*
	 * Perform initialization only if slave status is present and
	 * hw_init flag is false
	 */
	if (rt711->hw_init || rt711->status != SDW_SLAVE_ATTACHED)
		return 0;

	/* perform I/O transfers required for Slave initialization */
	return rt711_sdca_io_init(&slave->dev, slave);
}

static int rt711_sdca_read_prop(struct sdw_slave *slave)
{
	struct sdw_slave_prop *prop = &slave->prop;
	int nval, i;
	u32 bit;
	unsigned long addr;
	struct sdw_dpn_prop *dpn;

	prop->scp_int1_mask = SDW_SCP_INT1_BUS_CLASH | SDW_SCP_INT1_PARITY;
	prop->quirks = SDW_SLAVE_QUIRKS_INVALID_INITIAL_PARITY;
	prop->is_sdca = true;

	prop->paging_support = true;

	/* first we need to allocate memory for set bits in port lists */
	prop->source_ports = 0x14; /* BITMAP: 00010100 */
	prop->sink_ports = 0x8; /* BITMAP:  00001000 */

	nval = hweight32(prop->source_ports);
	prop->src_dpn_prop = devm_kcalloc(&slave->dev, nval,
		sizeof(*prop->src_dpn_prop), GFP_KERNEL);
	if (!prop->src_dpn_prop)
		return -ENOMEM;

	i = 0;
	dpn = prop->src_dpn_prop;
	addr = prop->source_ports;
	for_each_set_bit(bit, &addr, 32) {
		dpn[i].num = bit;
		dpn[i].type = SDW_DPN_FULL;
		dpn[i].simple_ch_prep_sm = true;
		dpn[i].ch_prep_timeout = 10;
		i++;
	}

	/* do this again for sink now */
	nval = hweight32(prop->sink_ports);
	prop->sink_dpn_prop = devm_kcalloc(&slave->dev, nval,
		sizeof(*prop->sink_dpn_prop), GFP_KERNEL);
	if (!prop->sink_dpn_prop)
		return -ENOMEM;

	i = 0;
	dpn = prop->sink_dpn_prop;
	addr = prop->sink_ports;
	for_each_set_bit(bit, &addr, 32) {
		dpn[i].num = bit;
		dpn[i].type = SDW_DPN_FULL;
		dpn[i].simple_ch_prep_sm = true;
		dpn[i].ch_prep_timeout = 10;
		i++;
	}

	/* set the timeout values */
	prop->clk_stop_timeout = 20;

	/* wake-up event */
	prop->wake_capable = 1;

	return 0;
}

static int rt711_sdca_interrupt_callback(struct sdw_slave *slave,
					struct sdw_slave_intr_status *status)
{
	struct rt711_sdca_priv *rt711 = dev_get_drvdata(&slave->dev);
	int ret = 0, stat, count = 0, retry = 10;
	unsigned int sdca_cascade, scp_sdca_stat1, scp_sdca_stat2;
	unsigned int buf;

	dev_dbg(&slave->dev,
		"%s control_port_stat=%x, sdca_cascade=%x", __func__,
		status->control_port, status->sdca_cascade);

	ret = regmap_read(rt711->regmap, SDW_SCP_SDCA_INT1, &rt711->scp_sdca_stat1);
	if (ret < 0)
		goto io_error;
	ret = regmap_read(rt711->regmap, SDW_SCP_SDCA_INT2, &rt711->scp_sdca_stat2);
	if (ret < 0)
		goto io_error;

	do {
		/* clear flag */
		ret = regmap_read(rt711->regmap, SDW_SCP_SDCA_INT1, &buf);
		if (ret < 0)
			goto io_error;
		if (buf & SDW_SCP_SDCA_INTMASK_SDCA_0) {
			ret = regmap_write(rt711->regmap, SDW_SCP_SDCA_INT1, SDW_SCP_SDCA_INTMASK_SDCA_0);
			if (ret < 0)
				goto io_error;
		}
		ret = regmap_read(rt711->regmap, SDW_SCP_SDCA_INT2, &buf);
		if (ret < 0)
			goto io_error;
		if (buf & SDW_SCP_SDCA_INTMASK_SDCA_8) {
			ret = regmap_write(rt711->regmap, SDW_SCP_SDCA_INT2, SDW_SCP_SDCA_INTMASK_SDCA_8);
			if (ret < 0)
				goto io_error;
		}

		/* check if flag clear or not */
		ret = regmap_read(rt711->regmap, SDW_DP0_INT, &buf);
		if (ret < 0)
			goto io_error;
		sdca_cascade = buf & SDW_DP0_SDCA_CASCADE;

		ret = regmap_read(rt711->regmap, SDW_SCP_SDCA_INT1, &buf);
		if (ret < 0)
			goto io_error;
		scp_sdca_stat1 = buf & SDW_SCP_SDCA_INTMASK_SDCA_0;

		ret = regmap_read(rt711->regmap, SDW_SCP_SDCA_INT2, &buf);
		if (ret < 0)
			goto io_error;
		scp_sdca_stat2 = buf & SDW_SCP_SDCA_INTMASK_SDCA_8;

		stat = scp_sdca_stat1 || scp_sdca_stat2 || sdca_cascade;

		count++;
	} while (stat != 0 && count < retry);

	if (count == retry) {
		dev_warn(&slave->dev, "Reached MAX_RETRY on SDCA_Cascade clear\n");
		dev_warn(&slave->dev,
			"%s scp_sdca_stat1=0x%x, scp_sdca_stat2=0x%x\n", __func__,
			rt711->scp_sdca_stat1, rt711->scp_sdca_stat2);
	}

	if (status->sdca_cascade)
		mod_delayed_work(system_power_efficient_wq,
			&rt711->jack_detect_work, msecs_to_jiffies(250));

	return 0;

io_error:
	pr_err_ratelimited("IO error in %s, ret %d\n", __func__, ret);
	return ret;
}

static struct sdw_slave_ops rt711_sdca_slave_ops = {
	.read_prop = rt711_sdca_read_prop,
	.interrupt_callback = rt711_sdca_interrupt_callback,
	.update_status = rt711_sdca_update_status,
};

static int rt711_sdca_sdw_probe(struct sdw_slave *slave,
				const struct sdw_device_id *id)
{
	struct regmap *sdw_regmap, *regmap;
	int ret;

	/* Regmap Initialization */
	sdw_regmap = devm_regmap_init_sdw(slave, &rt711_sdca_sdw_regmap);
	if (!sdw_regmap)
		return -EINVAL;

	regmap = devm_regmap_init(&slave->dev, NULL,
		&slave->dev, &rt711_sdca_regmap);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = rt711_sdca_init(&slave->dev, sdw_regmap, regmap, slave);

	return ret;
}

static int rt711_sdca_sdw_remove(struct sdw_slave *slave)
{
	struct rt711_sdca_priv *rt711 = dev_get_drvdata(&slave->dev);

	if (rt711 && rt711->hw_init)
		cancel_delayed_work(&rt711->jack_detect_work);

	return 0;
}

static const struct sdw_device_id rt711_sdca_id[] = {
	SDW_SLAVE_ENTRY_EXT(0x025d, 0x711, 0x3, 0x1, 0),
	{},
};
MODULE_DEVICE_TABLE(sdw, rt711_sdca_id);

static int __maybe_unused rt711_sdca_dev_suspend(struct device *dev)
{
	struct rt711_sdca_priv *rt711 = dev_get_drvdata(dev);

	if (!rt711->hw_init)
		return 0;

	regcache_cache_only(rt711->regmap, true);

	return 0;
}

#define RT711_PROBE_TIMEOUT 2000

static int __maybe_unused rt711_sdca_dev_resume(struct device *dev)
{
	struct sdw_slave *slave = dev_to_sdw_dev(dev);
	struct rt711_sdca_priv *rt711 = dev_get_drvdata(dev);
	unsigned long time;

	if (!rt711->hw_init)
		return 0;

	if (!slave->unattach_request)
		goto regmap_sync;

	time = wait_for_completion_timeout(&slave->initialization_complete,
				msecs_to_jiffies(RT711_PROBE_TIMEOUT));
	if (!time) {
		dev_err(&slave->dev, "Initialization not complete, timed out\n");
		return -ETIMEDOUT;
	}

regmap_sync:
	slave->unattach_request = 0;
	regcache_cache_only(rt711->regmap, false);
	regcache_sync(rt711->regmap);
	return 0;
}

static const struct dev_pm_ops rt711_sdca_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(rt711_sdca_dev_suspend, rt711_sdca_dev_resume)
	SET_RUNTIME_PM_OPS(rt711_sdca_dev_suspend, rt711_sdca_dev_resume, NULL)
};

static struct sdw_driver rt711_sdca_sdw_driver = {
	.driver = {
		.name = "rt711-sdca",
		.owner = THIS_MODULE,
		.pm = &rt711_sdca_pm,
	},
	.probe = rt711_sdca_sdw_probe,
	.remove = rt711_sdca_sdw_remove,
	.ops = &rt711_sdca_slave_ops,
	.id_table = rt711_sdca_id,
};
module_sdw_driver(rt711_sdca_sdw_driver);

MODULE_DESCRIPTION("ASoC RT711 SDCA SDW driver");
MODULE_AUTHOR("Shuming Fan <shumingf@realtek.com>");
MODULE_LICENSE("GPL");
