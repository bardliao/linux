// SPDX-License-Identifier: GPL-2.0
//
// CS42L43 core driver
//
// Copyright (C) 2022-2023 Cirrus Logic, Inc. and
//                         Cirrus Logic International Semiconductor Ltd.

#include <linux/bitops.h>
#include <linux/build_bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/jiffies.h>
#include <linux/mfd/core.h>
#include <linux/mfd/cs42l43-regs.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/soundwire/sdw.h>

#include "cs42l43.h"

static const struct reg_sequence cs42l43_reva_patch[] = {
	{ 0x4000,					0x00000055 },
	{ 0x4000,					0x000000AA },
	{ 0x10084,					0x00000000 },
	{ 0x1741C,					0x00CD2000 },
	{ 0x1718C,					0x00000003 },
	{ 0x4000,					0x00000000 },
	{ CS42L43_CCM_BLK_CLK_CONTROL,			0x00000002 },
	{ CS42L43_HPPATHVOL,				0x011B011B },
	{ CS42L43_OSC_DIV_SEL,				0x00000001 },
	{ CS42L43_DACCNFG2,				0x00000005 },
	{ CS42L43_MIC_DETECT_CONTROL_ANDROID,		0x80790079 },
	{ CS42L43_RELID,				0x0000000F },
};

static const struct reg_default cs42l43_reg_default[] = {
	{ CS42L43_DRV_CTRL1,				0x000186C0 },
	{ CS42L43_DRV_CTRL3,				0x286DB018 },
	{ CS42L43_DRV_CTRL4,				0x000006D8 },
	{ CS42L43_DRV_CTRL_5,				0x136C00C0 },
	{ CS42L43_GPIO_CTRL1,				0x00000707 },
	{ CS42L43_GPIO_CTRL2,				0x00000000 },
	{ CS42L43_GPIO_FN_SEL,				0x00000000 },
	{ CS42L43_MCLK_SRC_SEL,				0x00000000 },
	{ CS42L43_SAMPLE_RATE1,				0x00000003 },
	{ CS42L43_SAMPLE_RATE2,				0x00000003 },
	{ CS42L43_SAMPLE_RATE3,				0x00000003 },
	{ CS42L43_SAMPLE_RATE4,				0x00000003 },
	{ CS42L43_PLL_CONTROL,				0x00000000 },
	{ CS42L43_FS_SELECT1,				0x00000000 },
	{ CS42L43_FS_SELECT2,				0x00000000 },
	{ CS42L43_FS_SELECT3,				0x00000000 },
	{ CS42L43_FS_SELECT4,				0x00000000 },
	{ CS42L43_PDM_CONTROL,				0x00000000 },
	{ CS42L43_ASP_CLK_CONFIG1,			0x00010001 },
	{ CS42L43_ASP_CLK_CONFIG2,			0x00000000 },
	{ CS42L43_OSC_DIV_SEL,				0x00000001 },
	{ CS42L43_ADC_B_CTRL1,				0x00000000 },
	{ CS42L43_ADC_B_CTRL2,				0x00000000 },
	{ CS42L43_DECIM_HPF_WNF_CTRL1,			0x00000001 },
	{ CS42L43_DECIM_HPF_WNF_CTRL2,			0x00000001 },
	{ CS42L43_DECIM_HPF_WNF_CTRL3,			0x00000001 },
	{ CS42L43_DECIM_HPF_WNF_CTRL4,			0x00000001 },
	{ CS42L43_DMIC_PDM_CTRL,			0x00000000 },
	{ CS42L43_DECIM_VOL_CTRL_CH1_CH2,		0x20122012 },
	{ CS42L43_DECIM_VOL_CTRL_CH3_CH4,		0x20122012 },
	{ CS42L43_INTP_VOLUME_CTRL1,			0x00000180 },
	{ CS42L43_INTP_VOLUME_CTRL2,			0x00000180 },
	{ CS42L43_AMP1_2_VOL_RAMP,			0x00000022 },
	{ CS42L43_ASP_CTRL,				0x00000004 },
	{ CS42L43_ASP_FSYNC_CTRL1,			0x000000FA },
	{ CS42L43_ASP_FSYNC_CTRL2,			0x00000001 },
	{ CS42L43_ASP_FSYNC_CTRL3,			0x00000000 },
	{ CS42L43_ASP_FSYNC_CTRL4,			0x000001F4 },
	{ CS42L43_ASP_DATA_CTRL,			0x0000003A },
	{ CS42L43_ASP_RX_EN,				0x00000000 },
	{ CS42L43_ASP_TX_EN,				0x00000000 },
	{ CS42L43_ASP_RX_CH1_CTRL,			0x00170001 },
	{ CS42L43_ASP_RX_CH2_CTRL,			0x00170031 },
	{ CS42L43_ASP_RX_CH3_CTRL,			0x00170061 },
	{ CS42L43_ASP_RX_CH4_CTRL,			0x00170091 },
	{ CS42L43_ASP_RX_CH5_CTRL,			0x001700C1 },
	{ CS42L43_ASP_RX_CH6_CTRL,			0x001700F1 },
	{ CS42L43_ASP_TX_CH1_CTRL,			0x00170001 },
	{ CS42L43_ASP_TX_CH2_CTRL,			0x00170031 },
	{ CS42L43_ASP_TX_CH3_CTRL,			0x00170061 },
	{ CS42L43_ASP_TX_CH4_CTRL,			0x00170091 },
	{ CS42L43_ASP_TX_CH5_CTRL,			0x001700C1 },
	{ CS42L43_ASP_TX_CH6_CTRL,			0x001700F1 },
	{ CS42L43_ASPTX1_INPUT,				0x00800000 },
	{ CS42L43_ASPTX2_INPUT,				0x00800000 },
	{ CS42L43_ASPTX3_INPUT,				0x00800000 },
	{ CS42L43_ASPTX4_INPUT,				0x00800000 },
	{ CS42L43_ASPTX5_INPUT,				0x00800000 },
	{ CS42L43_ASPTX6_INPUT,				0x00800000 },
	{ CS42L43_SWIRE_DP1_CH1_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP1_CH2_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP1_CH3_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP1_CH4_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP2_CH1_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP2_CH2_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP3_CH1_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP3_CH2_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP4_CH1_INPUT,			0x00800000 },
	{ CS42L43_SWIRE_DP4_CH2_INPUT,			0x00800000 },
	{ CS42L43_ASRC_INT1_INPUT1,			0x00800000 },
	{ CS42L43_ASRC_INT2_INPUT1,			0x00800000 },
	{ CS42L43_ASRC_INT3_INPUT1,			0x00800000 },
	{ CS42L43_ASRC_INT4_INPUT1,			0x00800000 },
	{ CS42L43_ASRC_DEC1_INPUT1,			0x00800000 },
	{ CS42L43_ASRC_DEC2_INPUT1,			0x00800000 },
	{ CS42L43_ASRC_DEC3_INPUT1,			0x00800000 },
	{ CS42L43_ASRC_DEC4_INPUT1,			0x00800000 },
	{ CS42L43_ISRC1INT1_INPUT1,			0x00800000 },
	{ CS42L43_ISRC1INT2_INPUT1,			0x00800000 },
	{ CS42L43_ISRC1DEC1_INPUT1,			0x00800000 },
	{ CS42L43_ISRC1DEC2_INPUT1,			0x00800000 },
	{ CS42L43_ISRC2INT1_INPUT1,			0x00800000 },
	{ CS42L43_ISRC2INT2_INPUT1,			0x00800000 },
	{ CS42L43_ISRC2DEC1_INPUT1,			0x00800000 },
	{ CS42L43_ISRC2DEC2_INPUT1,			0x00800000 },
	{ CS42L43_EQ1MIX_INPUT1,			0x00800000 },
	{ CS42L43_EQ1MIX_INPUT2,			0x00800000 },
	{ CS42L43_EQ1MIX_INPUT3,			0x00800000 },
	{ CS42L43_EQ1MIX_INPUT4,			0x00800000 },
	{ CS42L43_EQ2MIX_INPUT1,			0x00800000 },
	{ CS42L43_EQ2MIX_INPUT2,			0x00800000 },
	{ CS42L43_EQ2MIX_INPUT3,			0x00800000 },
	{ CS42L43_EQ2MIX_INPUT4,			0x00800000 },
	{ CS42L43_SPDIF1_INPUT1,			0x00800000 },
	{ CS42L43_SPDIF2_INPUT1,			0x00800000 },
	{ CS42L43_AMP1MIX_INPUT1,			0x00800000 },
	{ CS42L43_AMP1MIX_INPUT2,			0x00800000 },
	{ CS42L43_AMP1MIX_INPUT3,			0x00800000 },
	{ CS42L43_AMP1MIX_INPUT4,			0x00800000 },
	{ CS42L43_AMP2MIX_INPUT1,			0x00800000 },
	{ CS42L43_AMP2MIX_INPUT2,			0x00800000 },
	{ CS42L43_AMP2MIX_INPUT3,			0x00800000 },
	{ CS42L43_AMP2MIX_INPUT4,			0x00800000 },
	{ CS42L43_AMP3MIX_INPUT1,			0x00800000 },
	{ CS42L43_AMP3MIX_INPUT2,			0x00800000 },
	{ CS42L43_AMP3MIX_INPUT3,			0x00800000 },
	{ CS42L43_AMP3MIX_INPUT4,			0x00800000 },
	{ CS42L43_AMP4MIX_INPUT1,			0x00800000 },
	{ CS42L43_AMP4MIX_INPUT2,			0x00800000 },
	{ CS42L43_AMP4MIX_INPUT3,			0x00800000 },
	{ CS42L43_AMP4MIX_INPUT4,			0x00800000 },
	{ CS42L43_ASRC_INT_ENABLES,			0x00000100 },
	{ CS42L43_ASRC_DEC_ENABLES,			0x00000100 },
	{ CS42L43_PDNCNTL,				0x00000000 },
	{ CS42L43_RINGSENSE_DEB_CTRL,			0x0000001B },
	{ CS42L43_TIPSENSE_DEB_CTRL,			0x0000001B },
	{ CS42L43_HS2,					0x050106F3 },
	{ CS42L43_STEREO_MIC_CTRL,			0x00000000 },
	{ CS42L43_STEREO_MIC_CLAMP_CTRL,		0x00000001 },
	{ CS42L43_BLOCK_EN2,				0x00000000 },
	{ CS42L43_BLOCK_EN3,				0x00000000 },
	{ CS42L43_BLOCK_EN4,				0x00000000 },
	{ CS42L43_BLOCK_EN5,				0x00000000 },
	{ CS42L43_BLOCK_EN6,				0x00000000 },
	{ CS42L43_BLOCK_EN7,				0x00000000 },
	{ CS42L43_BLOCK_EN8,				0x00000000 },
	{ CS42L43_BLOCK_EN9,				0x00000000 },
	{ CS42L43_BLOCK_EN10,				0x00000000 },
	{ CS42L43_BLOCK_EN11,				0x00000000 },
	{ CS42L43_TONE_CH1_CTRL,			0x00000000 },
	{ CS42L43_TONE_CH2_CTRL,			0x00000000 },
	{ CS42L43_MIC_DETECT_CONTROL_1,			0x00000003 },
	{ CS42L43_HS_BIAS_SENSE_AND_CLAMP_AUTOCONTROL,	0x02000003 },
	{ CS42L43_MIC_DETECT_CONTROL_ANDROID,		0x80790079 },
	{ CS42L43_ISRC1_CTRL,				0x00000000 },
	{ CS42L43_ISRC2_CTRL,				0x00000000 },
	{ CS42L43_CTRL_REG,				0x00000006 },
	{ CS42L43_FDIV_FRAC,				0x40000000 },
	{ CS42L43_CAL_RATIO,				0x00000080 },
	{ CS42L43_SPI_CLK_CONFIG1,			0x00000000 },
	{ CS42L43_SPI_CONFIG1,				0x00000000 },
	{ CS42L43_SPI_CONFIG2,				0x00000000 },
	{ CS42L43_SPI_CONFIG3,				0x00000001 },
	{ CS42L43_SPI_CONFIG4,				0x00000000 },
	{ CS42L43_TRAN_CONFIG3,				0x00000000 },
	{ CS42L43_TRAN_CONFIG4,				0x00000000 },
	{ CS42L43_TRAN_CONFIG5,				0x00000000 },
	{ CS42L43_TRAN_CONFIG6,				0x00000000 },
	{ CS42L43_TRAN_CONFIG7,				0x00000000 },
	{ CS42L43_TRAN_CONFIG8,				0x00000000 },
	{ CS42L43_DACCNFG1,				0x00000008 },
	{ CS42L43_DACCNFG2,				0x00000005 },
	{ CS42L43_HPPATHVOL,				0x011B011B },
	{ CS42L43_PGAVOL,				0x00003470 },
	{ CS42L43_LOADDETENA,				0x00000000 },
	{ CS42L43_CTRL,					0x00000037 },
	{ CS42L43_COEFF_DATA_IN0,			0x00000000 },
	{ CS42L43_COEFF_RD_WR0,				0x00000000 },
	{ CS42L43_START_EQZ0,				0x00000000 },
	{ CS42L43_MUTE_EQ_IN0,				0x00000000 },
	{ CS42L43_DECIM_MASK,				0x0000000F },
	{ CS42L43_EQ_MIX_MASK,				0x0000000F },
	{ CS42L43_ASP_MASK,				0x000000FF },
	{ CS42L43_PLL_MASK,				0x00000003 },
	{ CS42L43_SOFT_MASK,				0x0000FFFF },
	{ CS42L43_SWIRE_MASK,				0x00007FFF },
	{ CS42L43_MSM_MASK,				0x00000FFF },
	{ CS42L43_ACC_DET_MASK,				0x00000FFF },
	{ CS42L43_I2C_TGT_MASK,				0x00000003 },
	{ CS42L43_SPI_MSTR_MASK,			0x00000007 },
	{ CS42L43_SW_TO_SPI_BRIDGE_MASK,		0x00000001 },
	{ CS42L43_OTP_MASK,				0x00000007 },
	{ CS42L43_CLASS_D_AMP_MASK,			0x00003FFF },
	{ CS42L43_GPIO_INT_MASK,			0x0000003F },
	{ CS42L43_ASRC_MASK,				0x0000000F },
	{ CS42L43_HPOUT_MASK,				0x00000003 },
};

static bool cs42l43_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS42L43_DEVID:
	case CS42L43_REVID:
	case CS42L43_RELID:
	case CS42L43_SFT_RESET:
	case CS42L43_DRV_CTRL1:
	case CS42L43_DRV_CTRL3:
	case CS42L43_DRV_CTRL4:
	case CS42L43_DRV_CTRL_5:
	case CS42L43_GPIO_CTRL1:
	case CS42L43_GPIO_CTRL2:
	case CS42L43_GPIO_STS:
	case CS42L43_GPIO_FN_SEL:
	case CS42L43_MCLK_SRC_SEL:
	case CS42L43_SAMPLE_RATE1 ... CS42L43_SAMPLE_RATE4:
	case CS42L43_PLL_CONTROL:
	case CS42L43_FS_SELECT1 ... CS42L43_FS_SELECT4:
	case CS42L43_PDM_CONTROL:
	case CS42L43_ASP_CLK_CONFIG1 ... CS42L43_ASP_CLK_CONFIG2:
	case CS42L43_OSC_DIV_SEL:
	case CS42L43_ADC_B_CTRL1 ...  CS42L43_ADC_B_CTRL2:
	case CS42L43_DECIM_HPF_WNF_CTRL1 ... CS42L43_DECIM_HPF_WNF_CTRL4:
	case CS42L43_DMIC_PDM_CTRL:
	case CS42L43_DECIM_VOL_CTRL_CH1_CH2 ... CS42L43_DECIM_VOL_CTRL_CH3_CH4:
	case CS42L43_INTP_VOLUME_CTRL1 ... CS42L43_INTP_VOLUME_CTRL2:
	case CS42L43_AMP1_2_VOL_RAMP:
	case CS42L43_ASP_CTRL:
	case CS42L43_ASP_FSYNC_CTRL1 ... CS42L43_ASP_FSYNC_CTRL4:
	case CS42L43_ASP_DATA_CTRL:
	case CS42L43_ASP_RX_EN ... CS42L43_ASP_TX_EN:
	case CS42L43_ASP_RX_CH1_CTRL ... CS42L43_ASP_RX_CH6_CTRL:
	case CS42L43_ASP_TX_CH1_CTRL ... CS42L43_ASP_TX_CH6_CTRL:
	case CS42L43_OTP_REVISION_ID:
	case CS42L43_ASPTX1_INPUT:
	case CS42L43_ASPTX2_INPUT:
	case CS42L43_ASPTX3_INPUT:
	case CS42L43_ASPTX4_INPUT:
	case CS42L43_ASPTX5_INPUT:
	case CS42L43_ASPTX6_INPUT:
	case CS42L43_SWIRE_DP1_CH1_INPUT:
	case CS42L43_SWIRE_DP1_CH2_INPUT:
	case CS42L43_SWIRE_DP1_CH3_INPUT:
	case CS42L43_SWIRE_DP1_CH4_INPUT:
	case CS42L43_SWIRE_DP2_CH1_INPUT:
	case CS42L43_SWIRE_DP2_CH2_INPUT:
	case CS42L43_SWIRE_DP3_CH1_INPUT:
	case CS42L43_SWIRE_DP3_CH2_INPUT:
	case CS42L43_SWIRE_DP4_CH1_INPUT:
	case CS42L43_SWIRE_DP4_CH2_INPUT:
	case CS42L43_ASRC_INT1_INPUT1:
	case CS42L43_ASRC_INT2_INPUT1:
	case CS42L43_ASRC_INT3_INPUT1:
	case CS42L43_ASRC_INT4_INPUT1:
	case CS42L43_ASRC_DEC1_INPUT1:
	case CS42L43_ASRC_DEC2_INPUT1:
	case CS42L43_ASRC_DEC3_INPUT1:
	case CS42L43_ASRC_DEC4_INPUT1:
	case CS42L43_ISRC1INT1_INPUT1:
	case CS42L43_ISRC1INT2_INPUT1:
	case CS42L43_ISRC1DEC1_INPUT1:
	case CS42L43_ISRC1DEC2_INPUT1:
	case CS42L43_ISRC2INT1_INPUT1:
	case CS42L43_ISRC2INT2_INPUT1:
	case CS42L43_ISRC2DEC1_INPUT1:
	case CS42L43_ISRC2DEC2_INPUT1:
	case CS42L43_EQ1MIX_INPUT1 ... CS42L43_EQ1MIX_INPUT4:
	case CS42L43_EQ2MIX_INPUT1 ... CS42L43_EQ2MIX_INPUT4:
	case CS42L43_SPDIF1_INPUT1:
	case CS42L43_SPDIF2_INPUT1:
	case CS42L43_AMP1MIX_INPUT1 ... CS42L43_AMP1MIX_INPUT4:
	case CS42L43_AMP2MIX_INPUT1 ... CS42L43_AMP2MIX_INPUT4:
	case CS42L43_AMP3MIX_INPUT1 ... CS42L43_AMP3MIX_INPUT4:
	case CS42L43_AMP4MIX_INPUT1 ... CS42L43_AMP4MIX_INPUT4:
	case CS42L43_ASRC_INT_ENABLES ... CS42L43_ASRC_DEC_ENABLES:
	case CS42L43_PDNCNTL:
	case CS42L43_RINGSENSE_DEB_CTRL:
	case CS42L43_TIPSENSE_DEB_CTRL:
	case CS42L43_TIP_RING_SENSE_INTERRUPT_STATUS:
	case CS42L43_HS2:
	case CS42L43_HS_STAT:
	case CS42L43_MCU_SW_INTERRUPT:
	case CS42L43_STEREO_MIC_CTRL:
	case CS42L43_STEREO_MIC_CLAMP_CTRL:
	case CS42L43_BLOCK_EN2 ... CS42L43_BLOCK_EN11:
	case CS42L43_TONE_CH1_CTRL ... CS42L43_TONE_CH2_CTRL:
	case CS42L43_MIC_DETECT_CONTROL_1:
	case CS42L43_DETECT_STATUS_1:
	case CS42L43_HS_BIAS_SENSE_AND_CLAMP_AUTOCONTROL:
	case CS42L43_MIC_DETECT_CONTROL_ANDROID:
	case CS42L43_ISRC1_CTRL:
	case CS42L43_ISRC2_CTRL:
	case CS42L43_CTRL_REG:
	case CS42L43_FDIV_FRAC:
	case CS42L43_CAL_RATIO:
	case CS42L43_SPI_CLK_CONFIG1:
	case CS42L43_SPI_CONFIG1 ... CS42L43_SPI_CONFIG4:
	case CS42L43_SPI_STATUS1 ... CS42L43_SPI_STATUS2:
	case CS42L43_TRAN_CONFIG1 ... CS42L43_TRAN_CONFIG8:
	case CS42L43_TRAN_STATUS1 ... CS42L43_TRAN_STATUS3:
	case CS42L43_TX_DATA:
	case CS42L43_RX_DATA:
	case CS42L43_DACCNFG1 ... CS42L43_DACCNFG2:
	case CS42L43_HPPATHVOL:
	case CS42L43_PGAVOL:
	case CS42L43_LOADDETRESULTS:
	case CS42L43_LOADDETENA:
	case CS42L43_CTRL:
	case CS42L43_COEFF_DATA_IN0:
	case CS42L43_COEFF_RD_WR0:
	case CS42L43_INIT_DONE0:
	case CS42L43_START_EQZ0:
	case CS42L43_MUTE_EQ_IN0:
	case CS42L43_DECIM_INT ... CS42L43_HPOUT_INT:
	case CS42L43_DECIM_MASK ... CS42L43_HPOUT_MASK:
	case CS42L43_DECIM_INT_SHADOW ... CS42L43_HP_OUT_SHADOW:
	case CS42L43_BOOT_CONTROL:
	case CS42L43_BLOCK_EN:
	case CS42L43_SHUTTER_CONTROL:
	case CS42L43_MCU_SW_REV ... CS42L43_MCU_RAM_MAX:
		return true;
	default:
		return false;
	}
}

static bool cs42l43_precious_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS42L43_SFT_RESET:
	case CS42L43_TX_DATA:
	case CS42L43_RX_DATA:
	case CS42L43_DECIM_INT ... CS42L43_HPOUT_INT:
	case CS42L43_MCU_SW_REV ... CS42L43_MCU_RAM_MAX:
		return true;
	default:
		return false;
	}
}

static bool cs42l43_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS42L43_DEVID:
	case CS42L43_REVID:
	case CS42L43_RELID:
	case CS42L43_GPIO_STS:
	case CS42L43_OTP_REVISION_ID:
	case CS42L43_TIP_RING_SENSE_INTERRUPT_STATUS:
	case CS42L43_HS_STAT:
	case CS42L43_MCU_SW_INTERRUPT:
	case CS42L43_DETECT_STATUS_1:
	case CS42L43_SPI_STATUS1 ... CS42L43_SPI_STATUS2:
	case CS42L43_TRAN_CONFIG1 ... CS42L43_TRAN_CONFIG2:
	case CS42L43_TRAN_CONFIG8:
	case CS42L43_TRAN_STATUS1 ... CS42L43_TRAN_STATUS3:
	case CS42L43_LOADDETRESULTS:
	case CS42L43_INIT_DONE0:
	case CS42L43_DECIM_INT_SHADOW ... CS42L43_HP_OUT_SHADOW:
	case CS42L43_BOOT_CONTROL:
	case CS42L43_BLOCK_EN:
		return true;
	default:
		return cs42l43_precious_register(dev, reg);
	}
}

#if IS_ENABLED(CONFIG_MFD_CS42L43_I2C)
const struct regmap_config cs42l43_i2c_regmap = {
	.reg_bits		= 32,
	.reg_stride		= 4,
	.val_bits		= 32,
	.reg_format_endian	= REGMAP_ENDIAN_BIG,
	.val_format_endian	= REGMAP_ENDIAN_BIG,

	.max_register		= CS42L43_MCU_RAM_MAX,
	.readable_reg		= cs42l43_readable_register,
	.volatile_reg		= cs42l43_volatile_register,
	.precious_reg		= cs42l43_precious_register,

	.cache_type		= REGCACHE_RBTREE,
	.reg_defaults		= cs42l43_reg_default,
	.num_reg_defaults	= ARRAY_SIZE(cs42l43_reg_default),
};
EXPORT_SYMBOL_NS_GPL(cs42l43_i2c_regmap, MFD_CS42L43);
#endif

#if IS_ENABLED(CONFIG_MFD_CS42L43_SDW)
const struct regmap_config cs42l43_sdw_regmap = {
	.reg_bits		= 32,
	.reg_stride		= 4,
	.val_bits		= 32,
	.reg_format_endian	= REGMAP_ENDIAN_LITTLE,
	.val_format_endian	= REGMAP_ENDIAN_LITTLE,

	.max_register		= CS42L43_MCU_RAM_MAX,
	.readable_reg		= cs42l43_readable_register,
	.volatile_reg		= cs42l43_volatile_register,
	.precious_reg		= cs42l43_precious_register,

	.cache_type		= REGCACHE_RBTREE,
	.reg_defaults		= cs42l43_reg_default,
	.num_reg_defaults	= ARRAY_SIZE(cs42l43_reg_default),
};
EXPORT_SYMBOL_NS_GPL(cs42l43_sdw_regmap, MFD_CS42L43);
#endif

static const char * const cs42l43_core_supplies[] = {
	"VDD_A", "VDD_IO", "VDD_CP",
};

static const char * const cs42l43_parent_supplies[] = { "VDD_AMP" };

static const struct mfd_cell cs42l43_devs[] = {
	{ .name = "cs42l43-pinctrl", },
	{ .name = "cs42l43-irq", },
	{ .name = "cs42l43-spi", },
	{
		.name = "cs42l43-codec",
		.parent_supplies = cs42l43_parent_supplies,
		.num_parent_supplies = ARRAY_SIZE(cs42l43_parent_supplies),
	},
};

static int cs42l43_soft_reset(struct cs42l43 *cs42l43)
{
	static const struct reg_sequence reset[] = {
		{ CS42L43_SFT_RESET, 0x5A000000 },
	};
	unsigned long time;

	dev_dbg(cs42l43->dev, "Soft resetting\n");

	reinit_completion(&cs42l43->device_detach);

	/* apply cache only as the device will also fall off the soundwire bus */
	regcache_cache_only(cs42l43->regmap, true);
	regmap_multi_reg_write_bypassed(cs42l43->regmap, reset, ARRAY_SIZE(reset));

	msleep(20);

	if (cs42l43->sdw) {
		time = wait_for_completion_timeout(&cs42l43->device_detach,
						   msecs_to_jiffies(100));
		if (!time) {
			dev_err(cs42l43->dev, "Timed out waiting for device detach\n");
			return -ETIMEDOUT;
		}
	}

	return -EAGAIN;
}

static int cs42l43_wait_for_attach(struct cs42l43 *cs42l43)
{
	unsigned long time;

	if (!cs42l43->attached) {
		time = wait_for_completion_timeout(&cs42l43->device_attach,
						   msecs_to_jiffies(500));
		if (!time) {
			dev_err(cs42l43->dev, "Timed out waiting for device re-attach\n");
			return -ETIMEDOUT;
		}
	}

	regcache_cache_only(cs42l43->regmap, false);

	// Must enable OSC_DIV before doing any SoundWire reads
	if (cs42l43->sdw)
		regmap_write(cs42l43->regmap, CS42L43_OSC_DIV_SEL, 0x1);

	return 0;
}

static int cs42l43_mcu_stage_2_3(struct cs42l43 *cs42l43, bool shadow)
{
	unsigned int need_reg = CS42L43_NEED_CONFIGS;
	unsigned int val;
	int ret;

	dev_dbg(cs42l43->dev, "Moving firmware to stage 3\n");

	if (shadow)
		need_reg = CS42L43_FW_SH_BOOT_CFG_NEED_CONFIGS;

	regmap_write(cs42l43->regmap, need_reg, 0x0);

	ret = regmap_read_poll_timeout(cs42l43->regmap, CS42L43_BOOT_STATUS,
				       val, (val == 3), 5000, 20000);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to move to stage 3: %d, 0x%x\n", ret, val);
		return ret;
	}

	return -EAGAIN;
}

static int cs42l43_mcu_stage_3_2(struct cs42l43 *cs42l43)
{
	dev_dbg(cs42l43->dev, "Returning firmware to stage 2\n");

	regmap_write(cs42l43->regmap, CS42L43_FW_CTRL_NEED_CONFIGS,
		     CS42L43_FW_PATCH_NEED_CFG_MASK);
	regmap_write(cs42l43->regmap, CS42L43_FW_CTRL_HAVE_CONFIGS, 0x0);

	return cs42l43_soft_reset(cs42l43);
}

static int cs42l43_mcu_disable(struct cs42l43 *cs42l43)
{
	unsigned int val;
	int ret;

	dev_dbg(cs42l43->dev, "Disabling firmware\n");

	regmap_write(cs42l43->regmap, CS42L43_FW_CTRL_MM_MCU_CFG_REG, 0xF05AA50F);
	regmap_write(cs42l43->regmap, CS42L43_FW_CTRL_MM_CTRL_SELECTION, 0x1);
	regmap_write(cs42l43->regmap, CS42L43_MCU_SW_INTERRUPT, CS42L43_CONTROL_IND_MASK);
	regmap_write(cs42l43->regmap, CS42L43_MCU_SW_INTERRUPT, 0);

	ret = regmap_read_poll_timeout(cs42l43->regmap, CS42L43_SOFT_INT_SHADOW, val,
				       (val & CS42L43_CONTROL_APPLIED_INT_MASK),
				       5000, 20000);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to disable firmware: %d, 0x%x\n", ret, val);
		return ret;
	}

	/* Soft reset to clear any register state the firmware left behind */
	return cs42l43_soft_reset(cs42l43);
}

struct cs42l43_patch_header {
	__le16 version;
	__le16 size;
	u8 reserved;
	u8 secure;
	__le16 bss_size;
	__le32 apply_addr;
	__le32 checksum;
	__le32 sha;
	__le16 swrev;
	__le16 patchid;
	__le16 ipxid;
	__le16 romver;
	__le32 load_addr;
} __packed;

static void cs42l43_mcu_load_firmware(const struct firmware *firmware, void *context)
{
	struct cs42l43 *cs42l43 = context;
	struct cs42l43_patch_header *hdr;
	unsigned int loadaddr, val;
	int ret;

	if (!firmware) {
		dev_err(cs42l43->dev, "Failed to load firmware\n");
		cs42l43->firmware_error = -ENODEV;
		goto err;
	}

	dev_dbg(cs42l43->dev, "Updating firmware\n");

	hdr = (void *)&firmware->data[0];
	loadaddr = le32_to_cpu(hdr->load_addr);

	if (le16_to_cpu(hdr->version) != 0x3) {
		dev_err(cs42l43->dev, "Bad firmware file format: %d\n", hdr->version);
		cs42l43->firmware_error = -EINVAL;
		goto err_release;
	}

	regmap_write(cs42l43->regmap, CS42L43_PATCH_START_ADDR, loadaddr);
	regmap_bulk_write(cs42l43->regmap, loadaddr + 0x100000,
			  &firmware->data[0], firmware->size / sizeof(u32));

	regmap_write(cs42l43->regmap, CS42L43_MCU_SW_INTERRUPT, CS42L43_PATCH_IND_MASK);
	regmap_write(cs42l43->regmap, CS42L43_MCU_SW_INTERRUPT, 0);

	ret = regmap_read_poll_timeout(cs42l43->regmap, CS42L43_SOFT_INT_SHADOW, val,
				       (val & CS42L43_PATCH_APPLIED_INT_MASK),
				       5000, 500000);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to update firmware: %d, 0x%x\n", ret, val);
		cs42l43->firmware_error = ret;
		goto err_release;
	}

err_release:
	release_firmware(firmware);
err:
	complete(&cs42l43->firmware_download);
}

static int cs42l43_mcu_update_step(struct cs42l43 *cs42l43)
{
	unsigned int mcu_rev, bios_rev, boot_status, secure_cfg;
	bool patched, shadow;
	int ret;

	// Clear any stale software interrupt bits
	regmap_read(cs42l43->regmap, CS42L43_SOFT_INT, &mcu_rev);

	ret = regmap_read(cs42l43->regmap, CS42L43_BOOT_STATUS, &boot_status);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to read boot status: %d\n", ret);
		return ret;
	}

	ret = regmap_read(cs42l43->regmap, CS42L43_MCU_SW_REV, &mcu_rev);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to read firmware revision: %d\n", ret);
		return ret;
	}

	bios_rev = ((mcu_rev & CS42L43_BIOS_MAJOR_REV_MASK) << 12) |
		   ((mcu_rev & CS42L43_BIOS_MINOR_REV_MASK) << 4) |
		   ((mcu_rev & CS42L43_BIOS_SUBMINOR_REV_MASK) >> 8);
	mcu_rev = ((mcu_rev & CS42L43_FW_MAJOR_REV_MASK) << 12) |
		  ((mcu_rev & CS42L43_FW_MINOR_REV_MASK) << 4) |
		  ((mcu_rev & CS42L43_FW_SUBMINOR_REV_MASK) >> 8);

	patched = mcu_rev >= 0x2105 || bios_rev > 0x0000;
	shadow = mcu_rev >= 0x2200;

	ret = regmap_read(cs42l43->regmap, CS42L43_BOOT_CONTROL, &secure_cfg);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to read security settings: %d\n", ret);
		return ret;
	}

	cs42l43->hw_lock = secure_cfg & CS42L43_LOCK_HW_STS_MASK;

	if (!patched && cs42l43->hw_lock) {
		dev_err(cs42l43->dev, "Unpatched secure device\n");
		return -EPERM;
	}

	dev_dbg(cs42l43->dev, "Firmware(0x%x) in boot stage %d\n", mcu_rev, boot_status);

	switch (boot_status) {
	case 2:
		if (!patched) {
			ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_UEVENT,
						      "cs42l43.bin", cs42l43->dev,
						      GFP_KERNEL, cs42l43,
						      cs42l43_mcu_load_firmware);
			if (ret) {
				dev_err(cs42l43->dev, "Failed to request firmware: %d\n", ret);
				return ret;
			}

			wait_for_completion(&cs42l43->firmware_download);

			if (cs42l43->firmware_error)
				return cs42l43->firmware_error;

			return -EAGAIN;
		} else {
			return cs42l43_mcu_stage_2_3(cs42l43, shadow);
		}
	case 3:
		if (patched)
			return cs42l43_mcu_disable(cs42l43);
		else
			return cs42l43_mcu_stage_3_2(cs42l43);
	case 4:
		return 0;
	default:
		dev_err(cs42l43->dev, "Invalid boot status: %d\n", boot_status);
		return -EINVAL;
	}
}

static int cs42l43_mcu_update(struct cs42l43 *cs42l43)
{
	const int update_retries = 5;
	int i, ret;

	for (i = 0; i < update_retries; i++) {
		ret = cs42l43_mcu_update_step(cs42l43);
		if (ret != -EAGAIN)
			return ret;

		ret = cs42l43_wait_for_attach(cs42l43);
		if (ret)
			return ret;
	}

	dev_err(cs42l43->dev, "Failed retrying update\n");
	return -ETIMEDOUT;
}

static void cs42l43_boot_work(struct work_struct *work)
{
	struct cs42l43 *cs42l43 = container_of(work, struct cs42l43, boot_work);
	unsigned int devid, revid, otp;
	int ret;

	dev_dbg(cs42l43->dev, "Boot work running\n");

	ret = cs42l43_wait_for_attach(cs42l43);
	if (ret)
		goto err;

	if (cs42l43->sdw)
		cs42l43->irq = cs42l43->sdw->irq;

	ret = regmap_read(cs42l43->regmap, CS42L43_DEVID, &devid);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to read devid: %d\n", ret);
		goto err;
	}

	switch (devid) {
	case 0x42a43:
		break;
	default:
		dev_err(cs42l43->dev, "Unrecognised devid: 0x%06x\n", devid);
		goto err;
	}

	ret = regmap_read(cs42l43->regmap, CS42L43_REVID, &revid);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to read rev: %d\n", ret);
		goto err;
	}

	ret = regmap_read(cs42l43->regmap, CS42L43_OTP_REVISION_ID, &otp);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to read otp rev: %d\n", ret);
		goto err;
	}

	dev_info(cs42l43->dev,
		 "devid: 0x%06x, rev: 0x%02x, otp: 0x%02x\n", devid, revid, otp);

	ret = cs42l43_mcu_update(cs42l43);
	if (ret)
		goto err;

	ret = regmap_register_patch(cs42l43->regmap, cs42l43_reva_patch,
				    ARRAY_SIZE(cs42l43_reva_patch));
	if (ret) {
		dev_err(cs42l43->dev, "Failed to apply register patch: %d\n", ret);
		goto err;
	}

	pm_runtime_mark_last_busy(cs42l43->dev);
	pm_runtime_put_autosuspend(cs42l43->dev);

	ret = devm_mfd_add_devices(cs42l43->dev, PLATFORM_DEVID_NONE,
				   cs42l43_devs, ARRAY_SIZE(cs42l43_devs),
				   NULL, 0, NULL);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to add subdevices: %d\n", ret);
		goto err;
	}

	dev_dbg(cs42l43->dev, "Successfully initialised\n");

	return;

err:
	pm_runtime_put_sync(cs42l43->dev);
	cs42l43_dev_remove(cs42l43);
}

static int cs42l43_power_up(struct cs42l43 *cs42l43)
{
	int ret;

	ret = regulator_enable(cs42l43->vdd_p);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to enable VDD_P: %d\n", ret);
		return ret;
	}

	usleep_range(50, 100); /* VDD_P must be on for 50uS before any other supply */

	gpiod_set_value_cansleep(cs42l43->reset, 1);

	ret = regulator_bulk_enable(CS42L43_N_SUPPLIES, cs42l43->core_supplies);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to enable core supplies: %d\n", ret);
		goto err_reset;
	}

	ret = regulator_enable(cs42l43->vdd_d);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to enable VDD_D: %d\n", ret);
		goto err_core_supplies;
	}

	usleep_range(1000, 2000);

	dev_dbg(cs42l43->dev, "Powered up\n");

	return 0;

err_core_supplies:
	regulator_bulk_disable(CS42L43_N_SUPPLIES, cs42l43->core_supplies);
err_reset:
	gpiod_set_value_cansleep(cs42l43->reset, 0);
	regulator_disable(cs42l43->vdd_p);

	return ret;
}

static int cs42l43_power_down(struct cs42l43 *cs42l43)
{
	int ret;

	ret = regulator_disable(cs42l43->vdd_d);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to disable VDD_D: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_disable(CS42L43_N_SUPPLIES, cs42l43->core_supplies);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to disable core supplies: %d\n", ret);
		return ret;
	}

	gpiod_set_value_cansleep(cs42l43->reset, 0);

	ret = regulator_disable(cs42l43->vdd_p);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to disable VDD_P: %d\n", ret);
		return ret;
	}

	dev_dbg(cs42l43->dev, "Powered down\n");

	return 0;
}

int cs42l43_dev_probe(struct cs42l43 *cs42l43)
{
	int i, ret;

	dev_set_drvdata(cs42l43->dev, cs42l43);

	mutex_init(&cs42l43->pll_lock);
	init_completion(&cs42l43->device_attach);
	init_completion(&cs42l43->device_detach);
	init_completion(&cs42l43->firmware_download);
	INIT_WORK(&cs42l43->boot_work, cs42l43_boot_work);

	regcache_cache_only(cs42l43->regmap, true);

	cs42l43->reset = devm_gpiod_get_optional(cs42l43->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(cs42l43->reset)) {
		ret = PTR_ERR(cs42l43->reset);
		dev_err(cs42l43->dev, "Failed to get reset: %d\n", ret);
		return ret;
	}

	cs42l43->vdd_p = devm_regulator_get(cs42l43->dev, "VDD_P");
	if (IS_ERR(cs42l43->vdd_p)) {
		ret = PTR_ERR(cs42l43->vdd_p);
		dev_err(cs42l43->dev, "Failed to get VDD_P: %d\n", ret);
		return ret;
	}

	cs42l43->vdd_d = devm_regulator_get(cs42l43->dev, "VDD_D");
	if (IS_ERR(cs42l43->vdd_d)) {
		ret = PTR_ERR(cs42l43->vdd_d);
		dev_err(cs42l43->dev, "Failed to get VDD_D: %d\n", ret);
		return ret;
	}

	BUILD_BUG_ON(ARRAY_SIZE(cs42l43_core_supplies) != CS42L43_N_SUPPLIES);

	for (i = 0; i < CS42L43_N_SUPPLIES; i++)
		cs42l43->core_supplies[i].supply = cs42l43_core_supplies[i];

	ret = devm_regulator_bulk_get(cs42l43->dev, CS42L43_N_SUPPLIES,
				      cs42l43->core_supplies);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to get core supplies: %d\n", ret);
		return ret;
	}

	ret = cs42l43_power_up(cs42l43);
	if (ret)
		return ret;

	pm_runtime_set_autosuspend_delay(cs42l43->dev, 250);
	pm_runtime_use_autosuspend(cs42l43->dev);
	pm_runtime_set_active(cs42l43->dev);
	pm_runtime_get_noresume(cs42l43->dev);
	pm_runtime_enable(cs42l43->dev);

	queue_work(system_long_wq, &cs42l43->boot_work);

	return 0;
}
EXPORT_SYMBOL_NS_GPL(cs42l43_dev_probe, MFD_CS42L43);

void cs42l43_dev_remove(struct cs42l43 *cs42l43)
{
	pm_runtime_disable(cs42l43->dev);
	cs42l43_power_down(cs42l43);
}
EXPORT_SYMBOL_NS_GPL(cs42l43_dev_remove, MFD_CS42L43);

static int __maybe_unused cs42l43_suspend(struct device *dev)
{
	struct cs42l43 *cs42l43 = dev_get_drvdata(dev);
	int ret;

	dev_dbg(cs42l43->dev, "System suspend\n");

	/*
	 * Don't care about being resumed here, but we do want force_resume to
	 * always trigger an actual resume, so that register state for the
	 * MCU/GPIOs is returned as soon as possible after system resume
	 */
	pm_runtime_get_noresume(dev);

	ret = pm_runtime_force_suspend(dev);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to force suspend: %d\n", ret);
		return ret;
	}

	pm_runtime_put_noidle(dev);

	ret = cs42l43_power_down(cs42l43);
	if (ret)
		return ret;

	return 0;
}

static int __maybe_unused cs42l43_resume(struct device *dev)
{
	struct cs42l43 *cs42l43 = dev_get_drvdata(dev);
	int ret;

	dev_dbg(cs42l43->dev, "System resume\n");

	ret = cs42l43_power_up(cs42l43);
	if (ret)
		return ret;

	ret = pm_runtime_force_resume(dev);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to force resume: %d\n", ret);
		return ret;
	}

	return 0;
}

static int __maybe_unused cs42l43_runtime_suspend(struct device *dev)
{
	struct cs42l43 *cs42l43 = dev_get_drvdata(dev);

	dev_dbg(cs42l43->dev, "Runtime suspend\n");

	/*
	 * Whilst we don't power the chip down here, going into runtime
	 * suspend lets the SoundWire bus power down, which means we can't
	 * communicate with the device any more.
	 */
	regcache_cache_only(cs42l43->regmap, true);

	return 0;
}

static int __maybe_unused cs42l43_runtime_resume(struct device *dev)
{
	struct cs42l43 *cs42l43 = dev_get_drvdata(dev);
	unsigned int reset_canary;
	int ret;

	dev_dbg(cs42l43->dev, "Runtime resume\n");

	ret = cs42l43_wait_for_attach(cs42l43);
	if (ret)
		return ret;

	ret = regmap_read(cs42l43->regmap, CS42L43_RELID, &reset_canary);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to check reset canary: %d\n", ret);
		goto err;
	}

	if (!reset_canary) {
		/*
		 * If the canary has cleared the chip has reset, re-handle the
		 * MCU and mark the cache as dirty to indicate the chip reset.
		 */
		ret = cs42l43_mcu_update(cs42l43);
		if (ret)
			goto err;

		regcache_mark_dirty(cs42l43->regmap);
	}

	ret = regcache_sync(cs42l43->regmap);
	if (ret) {
		dev_err(cs42l43->dev, "Failed to restore register cache: %d\n", ret);
		goto err;
	}

	return 0;

err:
	regcache_cache_only(cs42l43->regmap, true);

	return ret;
}

const struct dev_pm_ops cs42l43_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cs42l43_suspend, cs42l43_resume)
	SET_RUNTIME_PM_OPS(cs42l43_runtime_suspend, cs42l43_runtime_resume, NULL)
};
EXPORT_SYMBOL_NS_GPL(cs42l43_pm_ops, MFD_CS42L43);

MODULE_DESCRIPTION("CS42L43 Core Driver");
MODULE_AUTHOR("Charles Keepax <ckeepax@opensource.cirrus.com>");
MODULE_LICENSE("GPL");
