// SPDX-License-Identifier: GPL-2.0-only
// Based on sof_sdw_rt5682.c
// Copyright (c) 2020 Intel Corporation

/*
 *  sof_sdw_cs42l43 - Helpers to handle CS42L43 from generic machine driver
 */
#define DEBUG
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <sound/jack.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_type.h>
#include <sound/cs42l43.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/soc-acpi.h>
#include <sound/soc-dapm.h>
#include "sof_sdw_common.h"

static const struct snd_soc_dapm_widget sof_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_LINE("Monitor", NULL),
};

static const struct snd_soc_dapm_route sof_map[] = {
	{ "Speaker", NULL, "cs42l43 AMP1_OUT_P" },
};

static struct cs42l43_jack_pdata cohen_pdata = {
	.detect_us = 1000,
	.bias_ramp_ms = 10,
	.tip = {
		.pullup = true,
		.fall_db_ms = 500,
		.rise_db_ms = 500,
	},
	.ring = {
		.pullup = true,
		.fall_db_ms = 500,
		.rise_db_ms = 500,
	},
	.use_ring_sense = true,
	.buttons = {
		{ .threshold = 10, .button = SND_JACK_BTN_0, },
		{ .threshold = 200, .button = SND_JACK_BTN_1, },
	},
};

static int cs42l43_rtd_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = asoc_rtd_to_codec(rtd, 0)->component;
	struct mc_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_jack *jack = &ctx->sdw_headset;
	struct snd_soc_card *card = rtd->card;
	int ret;

	dev_dbg(card->dev, "%s():%d\n", __func__, __LINE__);

	card->components = devm_kasprintf(card->dev, GFP_KERNEL, "%s hs:cs42l43", card->components);
	if (!card->components)
		return -ENOMEM;

	ret = snd_soc_dapm_new_controls(&card->dapm, sof_widgets, ARRAY_SIZE(sof_widgets));
	if (ret) {
		dev_err(card->dev, "widgets addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dapm_add_routes(&card->dapm, sof_map, ARRAY_SIZE(sof_map));
	if (ret) {
		dev_err(card->dev, "map addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_card_jack_new(card, "Headphone Jack",
				    SND_JACK_MECHANICAL | SND_JACK_AVOUT |
				    SND_JACK_HEADSET | SND_JACK_LINEOUT |
				    SND_JACK_BTN_0 | SND_JACK_BTN_1 |
				    SND_JACK_BTN_2 | SND_JACK_BTN_3 |
				    SND_JACK_BTN_4 | SND_JACK_BTN_5,
				    jack);
	if (ret) {
		dev_err(card->dev, "Failed to create jack: %d\n", ret);
		return ret;
	}

	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_PLAYPAUSE);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEDOWN);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOICECOMMAND);

	ret = snd_soc_component_set_jack(component, jack, &cohen_pdata);
	if (ret < 0) {
		dev_err(card->dev, "Failed to register jack: %d\n", ret);
		return ret;
	}

	ret = snd_soc_component_set_sysclk(component, CS42L43_SYSCLK, CS42L43_SYSCLK_SDW,
					   4800000, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set sysclk: %d\n", ret);
		return ret;
	}

	return 0;
}

int sof_sdw_cs42l43_init(struct snd_soc_card *card, const struct snd_soc_acpi_link_adr *link,
			 struct snd_soc_dai_link *dai_links, struct sof_sdw_codec_info *info,
			 bool playback)
{
	/* Only initialize once. Do it with dai link for playback. */
	if (!playback)
		return 0;

	dai_links->init = cs42l43_rtd_init;

	return 0;
}
