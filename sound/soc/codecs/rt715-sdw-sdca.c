/* SPDX-License-Identifier: GPL-2.0-only */
/*
* rt715-sdca.c -- rt715 ALSA SoC audio driver
*
* Copyright(c) 2019 Realtek Semiconductor Corp.
*
* ALC715 ASoC Codec Driver based Intel Dummy SdW codec driver
*
*/
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_type.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include "rt715-sdca.h"
#include "rt715-sdw-sdca.h"

static bool rt715_readable_register(struct device *dev, unsigned int reg)
{

	switch (reg) {
	case 0x004d:
	case 0x005a:
	case 0x005e:
	case 0x0062:
	case 0x0072:
	case 0x201a ... 0x2027:
	case 0x2029 ... 0x202a:
	case 0x202d ... 0x2034:
	case 0x2200 ... 0x2204:
	case 0x2206 ... 0x2212:
	case 0x2230 ... 0x2239:
	case 0x2f5b:
	case 0x200002b:
	case 0x200202b:
	case 0x2000036:
	case 0x2002036:
	case 0x2000037:
	case 0x2002037:
	case 0x2000039:
	case 0x2002039:
	case 0x6100000:
	case 0x6102000:
	case 0x40880288:
	case 0x40882288:
		return true;
	default:
		return false;
	}
}

static bool rt715_volatile_register(struct device *dev, unsigned int reg)
{

	switch (reg) {
	case 0x005a:
	case 0x201b:
	case 0x201c:
	case 0x201d:
	case 0x201f:
	case 0x2230:
	case 0x202d ... 0x202f: /* BRA */
	case 0x2201 ... 0x2212: /* i2c debug */
	case 0x40880288: /* VAD Searching status */
		return true;
	default:
		return false;
	}
}

static int rt715_sdw_read_sdca(void *context, unsigned int reg, unsigned int *val)
{
	struct device *dev = context;
	struct rt715_sdca_priv *rt715 = dev_get_drvdata(dev);
	unsigned int sdw_data_1, sdw_data_0;
	int ret;

	switch (reg) {
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_SMPU_TRIG_ST_EN,
		RT715_SDCA_SMPU_TRIG_EN_CTRL, CH_00):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_SMPU_TRIG_ST_EN,
		RT715_SDCA_SMPU_TRIG_ST_CTRL, CH_00):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC7_27_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC7_27_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC7_27_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC7_27_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_05):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_06):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_07):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_08):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_05):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_06):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_07):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_08):
		ret = regmap_read(rt715->sdw_regmap,
			reg, &sdw_data_0);
		if (ret < 0)
			return ret;
		ret = regmap_read(rt715->sdw_regmap, reg | BIT(13), &sdw_data_1);
		if (ret < 0)
			return ret;
		*val = ((sdw_data_1 & 0xff) << 8) | (sdw_data_0 & 0xff);
		goto _done_;
	default:
		break;
	}
	/* SDCA mapping case or normal address */
	if (reg & 0x40000000 || reg < 0xffff) {
		ret = regmap_read(rt715->sdw_regmap,
			reg, val);
		if (ret < 0)
			return ret;
	}

	/* vendor registers case */
	if (reg < 0x40000000 && reg > 0xffff) {
		ret = regmap_read(rt715->sdw_regmap, reg, &sdw_data_0);
		if (ret < 0)
			return ret;
		ret = regmap_read(rt715->sdw_regmap,
			reg | BIT(13), &sdw_data_1);
		if (ret < 0)
			return ret;
		*val = ((sdw_data_1 & 0xff) << 8) | (sdw_data_0 & 0xff);
	}

_done_:
	dev_dbg(dev, "[%s] %04x => %08x\n", __func__, reg, *val);

	return 0;
}

static int rt715_sdw_write_sdca(void *context, unsigned int reg, unsigned int val)
{
	struct device *dev = context;
	struct rt715_sdca_priv *rt715 = dev_get_drvdata(dev);

	switch (reg) {
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC8_9_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_SMPU_TRIG_ST_EN,
		RT715_SDCA_SMPU_TRIG_EN_CTRL, CH_00):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_SMPU_TRIG_ST_EN,
		RT715_SDCA_SMPU_TRIG_ST_CTRL, CH_00):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC10_11_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC7_27_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC7_27_VOL,
		RT715_SDCA_FU_MUTE_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC7_27_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_ADC7_27_VOL,
		RT715_SDCA_FU_VOL_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_05):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_06):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_07):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_AMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_08):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_01):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_02):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_03):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_04):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_05):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_06):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_07):
	case RT715_SDCA_ADDR(FUN_MIC_ARRAY, RT715_SDCA_FU_DMIC_GAIN_EN,
		RT715_SDCA_FU_DMIC_GAIN_CTRL, CH_08):
		regmap_write(rt715->sdw_regmap,
			reg | BIT(13), (val >> 8) & 0xff);
		regmap_write(rt715->sdw_regmap, reg, val & 0xff);
		goto _done_;
	default:
		break;
	}

	/* SDCA mapping case or normal address */
	if (reg & 0x40000000 || reg < 0xffff)
		regmap_write(rt715->sdw_regmap, reg, val);

	/* vendor registers case */
	if (reg < 0x40000000 && reg > 0xffff) {
		regmap_write(rt715->sdw_regmap,
			reg | BIT(13), (val >> 8) & 0xff);
		regmap_write(rt715->sdw_regmap, reg, val & 0xff);
	}

_done_:
	dev_dbg(dev, "[%s] %04x <= %04x\n", __func__, reg, val);

	return 0;
}

static const struct regmap_config rt715_sdca_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.readable_reg = rt715_readable_register,
	.volatile_reg = rt715_volatile_register,
	.max_register = 0x43ffffff,
	.reg_defaults = rt715_reg_defaults_sdca,
	.num_reg_defaults = ARRAY_SIZE(rt715_reg_defaults_sdca),
	.cache_type = REGCACHE_RBTREE,
	.use_single_read = true,
	.use_single_write = true,
	.reg_read = rt715_sdw_read_sdca,
	.reg_write = rt715_sdw_write_sdca,
};

static const struct regmap_config rt715_sdca_sdw_regmap = {
	.name = "sdw",
	.reg_bits = 32,
	.val_bits = 8,
	.readable_reg = rt715_readable_register,
	.max_register = 0x43ffffff,
	.cache_type = REGCACHE_NONE,
	.use_single_read = true,
	.use_single_write = true,
};

static int rt715_update_status(struct sdw_slave *slave,
				enum sdw_slave_status status)
{
	struct rt715_sdca_priv *rt715 = dev_get_drvdata(&slave->dev);

	/* Update the status */
	rt715->status = status;

	/*
	 * Perform initialization only if slave status is present and
	 * hw_init flag is false
	 */
	if (rt715->hw_init || rt715->status != SDW_SLAVE_ATTACHED)
		return 0;

	/* perform I/O transfers required for Slave initialization */
	return rt715_io_init(&slave->dev, slave);
}

static int rt715_read_prop(struct sdw_slave *slave)
{
	struct sdw_slave_prop *prop = &slave->prop;
	int nval, i;
	u32 bit;
	unsigned long addr;
	struct sdw_dpn_prop *dpn;

	prop->paging_support = true;

	/* first we need to allocate memory for set bits in port lists */
	prop->source_ports = 0x50;/* BITMAP: 01010000 */
	prop->sink_ports = 0x0;	/* BITMAP:  00000000 */

	nval = hweight32(prop->source_ports);
	prop->src_dpn_prop = devm_kcalloc(&slave->dev, nval,
					sizeof(*prop->src_dpn_prop),
					GFP_KERNEL);
	if (!prop->src_dpn_prop)
		return -ENOMEM;

	dpn = prop->src_dpn_prop;
	i = 0;
	addr = prop->source_ports;
	for_each_set_bit(bit, &addr, 32) {
		dpn[i].num = bit;
		dpn[i].simple_ch_prep_sm = true;
		dpn[i].ch_prep_timeout = 10;
		i++;
	}

	/* do this again for sink now */
	nval = hweight32(prop->sink_ports);

	prop->sink_dpn_prop = devm_kcalloc(&slave->dev, nval,
					sizeof(*prop->sink_dpn_prop),
					GFP_KERNEL);
	if (!prop->sink_dpn_prop)
		return -ENOMEM;

	dpn = prop->sink_dpn_prop;
	i = 0;
	addr = prop->sink_ports;
	for_each_set_bit(bit, &addr, 32) {
		dpn[i].num = bit;
		dpn[i].simple_ch_prep_sm = true;
		dpn[i].ch_prep_timeout = 10;
		i++;
	}

	/* set the timeout values */
	prop->clk_stop_timeout = 20;

	return 0;
}

static struct sdw_slave_ops rt715_slave_ops = {
	.read_prop = rt715_read_prop,
	.update_status = rt715_update_status,
};

static int rt715_sdw_probe(struct sdw_slave *slave,
			   const struct sdw_device_id *id)
{
	struct regmap *sdw_regmap, *regmap;

	/* Assign ops */
	slave->ops = &rt715_slave_ops;

	/* Regmap Initialization */
	sdw_regmap = devm_regmap_init_sdw(slave, &rt715_sdca_sdw_regmap);

	if (!sdw_regmap)
		return -EINVAL;

	regmap = devm_regmap_init(&slave->dev, NULL,
		&slave->dev, &rt715_sdca_regmap);

	if (!regmap)
		return -EINVAL;

	return rt715_init(&slave->dev, sdw_regmap, regmap, slave);
}

static const struct sdw_device_id rt715_id[] = {
	SDW_SLAVE_ENTRY_EXT(0x025d, 0x715, 0x3, 0x1, 0),
	{},
};
MODULE_DEVICE_TABLE(sdw, rt715_id);

static int rt715_dev_suspend(struct device *dev)
{
	struct rt715_sdca_priv *rt715 = dev_get_drvdata(dev);

	if (!rt715->hw_init)
		return 0;

	regcache_cache_only(rt715->regmap, true);

	return 0;
}

#define RT715_PROBE_TIMEOUT 2000

static int rt715_dev_resume(struct device *dev)
{
	struct sdw_slave *slave = dev_to_sdw_dev(dev);
	struct rt715_sdca_priv *rt715 = dev_get_drvdata(dev);
	unsigned long time;
	int ret = 0;

	if (!rt715->hw_init)
		return 0;

	time = wait_for_completion_timeout(&slave->enumeration_complete,
					   msecs_to_jiffies(RT715_PROBE_TIMEOUT));
	if (!time) {
		dev_err(&slave->dev, "Enumeration not complete, timed out\n");
		return -ETIMEDOUT;
	}
	regcache_cache_only(rt715->regmap, false);

	ret = regcache_sync_region(rt715->regmap, 0x2000000, 0x61020ff);
	ret = regcache_sync_region(rt715->regmap, 0x40400000, 0x40880f80);

	return 0;
}

static const struct dev_pm_ops rt715_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(rt715_dev_suspend, rt715_dev_resume)
	SET_RUNTIME_PM_OPS(rt715_dev_suspend, rt715_dev_resume, NULL)
};

static struct sdw_driver rt715_sdw_driver = {
	.driver = {
		.name = "rt715",
		.owner = THIS_MODULE,
		.pm = &rt715_pm,
	},
	.probe = rt715_sdw_probe,
	.ops = &rt715_slave_ops,
	.id_table = rt715_id,
};
module_sdw_driver(rt715_sdw_driver);

MODULE_DESCRIPTION("ASoC RT715 driver SDW");
MODULE_AUTHOR("Jack Yu <jack.yu@realtek.com>");
MODULE_LICENSE("GPL v2");
