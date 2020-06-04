// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2020 Intel Corporation

/*
 *  sof_sdw_rt1316 - Helpers to handle RT1316 from generic machine driver
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <sound/soc.h>
#include <sound/soc-acpi.h>
#include "sof_sdw_common.h"
//#include "../../codecs/rt1316-sdw.h"

static const struct snd_soc_dapm_widget rt1316_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
};

/*
 * dapm routes for rt1316 will be registered dynamically according
 * to the number of rt1316 used. The first two entries will be registered
 * for one codec case, and the last two entries are also registered
 * if two 1316s are used.
 */
static const struct snd_soc_dapm_route rt1316_map[] = {
	{ "Speaker", NULL, "rt1316 SPOL" },
	{ "Speaker", NULL, "rt1316 SPOR" },
};

static const struct snd_kcontrol_new rt1316_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
};

static int rt1316_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	int ret;

	card->components = devm_kasprintf(card->dev, GFP_KERNEL,
					  "%s spk:rt1316",
					  card->components);
	if (!card->components)
		return -ENOMEM;

	ret = snd_soc_add_card_controls(card, rt1316_controls,
					ARRAY_SIZE(rt1316_controls));
	if (ret) {
		dev_err(card->dev, "rt1316 controls addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dapm_new_controls(&card->dapm, rt1316_widgets,
					ARRAY_SIZE(rt1316_widgets));
	if (ret) {
		dev_err(card->dev, "rt1316 widgets addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dapm_add_routes(&card->dapm, rt1316_map, 2);
	if (ret)
		dev_err(rtd->dev, "failed to add first SPK map: %d\n", ret);

	return ret;
}

int sof_sdw_rt1316_init(const struct snd_soc_acpi_link_adr *link,
			struct snd_soc_dai_link *dai_links,
			struct sof_sdw_codec_info *info,
			bool playback)
{
	info->amp_num++;
	if (info->amp_num == 1)
		dai_links->init = rt1316_init;

	return 0;
}
