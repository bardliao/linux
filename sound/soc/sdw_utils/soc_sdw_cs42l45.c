// SPDX-License-Identifier: GPL-2.0-only
// Based on sof_sdw_rt5682.c
// This file incorporates work covered by the following copyright notice:
// Copyright (c) 2023 Intel Corporation
// Copyright (c) 2024 Advanced Micro Devices, Inc.

/*
 *  soc_sdw_cs42l45 - Helpers to handle CS42L45 from generic machine driver
 */
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_type.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/soc-acpi.h>
#include <sound/soc-dapm.h>
#include <sound/soc_sdw_utils.h>

int asoc_sdw_cs42l45_hs_rtd_init(struct snd_soc_pcm_runtime *rtd, struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = rtd->card;

	card->components = devm_kasprintf(card->dev, GFP_KERNEL, "%s hs:cs42l45",
					  card->components);
	if (!card->components)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_NS(asoc_sdw_cs42l45_hs_rtd_init, "SND_SOC_SDW_UTILS");

int asoc_sdw_cs42l45_dmic_rtd_init(struct snd_soc_pcm_runtime *rtd, struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = rtd->card;

	card->components = devm_kasprintf(card->dev, GFP_KERNEL, "%s mic:cs42l45-dmic",
					  card->components);
	if (!card->components)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_NS(asoc_sdw_cs42l45_dmic_rtd_init, "SND_SOC_SDW_UTILS");
