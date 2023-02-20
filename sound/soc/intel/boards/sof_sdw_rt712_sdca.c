// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2020 Intel Corporation

/*
 *  sof_sdw_rt712_sdca - Helpers to handle RT712-SDCA from generic machine driver
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
#include <sound/jack.h>
#include "sof_sdw_common.h"

/*
 * Note this MUST be called before snd_soc_register_card(), so that the props
 * are in place before the codec component driver's probe function parses them.
 */
static int rt712_sdca_add_codec_device_props(struct device *sdw_dev)
{
	struct property_entry props[MAX_NO_PROPS] = {};
	struct fwnode_handle *fwnode;
	int ret;

	if (!SOF_RT711_JDSRC(sof_sdw_quirk))
		return 0;

	props[0] = PROPERTY_ENTRY_U32("realtek,jd-src", SOF_RT711_JDSRC(sof_sdw_quirk));

	fwnode = fwnode_create_software_node(props, NULL);
	if (IS_ERR(fwnode))
		return PTR_ERR(fwnode);

	ret = device_add_software_node(sdw_dev, to_software_node(fwnode));

	fwnode_handle_put(fwnode);

	return ret;
}

static const struct snd_soc_dapm_widget rt712_sdca_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("DMic", NULL),
};

static const struct snd_soc_dapm_route rt712_sdca_map[] = {
	/* Headphones */
	{ "Headphone", NULL, "rt712 HP" },
	{ "rt712 MIC2", NULL, "Headset Mic" },
};

static const struct snd_kcontrol_new rt712_sdca_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static struct snd_soc_jack_pin rt712_sdca_jack_pins[] = {
	{
		.pin    = "Headphone",
		.mask   = SND_JACK_HEADPHONE,
	},
	{
		.pin    = "Headset Mic",
		.mask   = SND_JACK_MICROPHONE,
	},
};

static int rt712_sdca_rtd_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct mc_private *ctx = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct snd_soc_component *component = codec_dai->component;
	struct snd_soc_jack *jack;
	int ret;

	card->components = devm_kasprintf(card->dev, GFP_KERNEL,
					  "%s hs:rt712-sdca",
					  card->components);
	if (!card->components)
		return -ENOMEM;

	ret = snd_soc_add_card_controls(card, rt712_sdca_controls,
					ARRAY_SIZE(rt712_sdca_controls));
	if (ret) {
		dev_err(card->dev, "rt712-sdca controls addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dapm_new_controls(&card->dapm, rt712_sdca_widgets,
					ARRAY_SIZE(rt712_sdca_widgets));
	if (ret) {
		dev_err(card->dev, "rt712-sdca widgets addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dapm_add_routes(&card->dapm, rt712_sdca_map,
				      ARRAY_SIZE(rt712_sdca_map));

	if (ret) {
		dev_err(card->dev, "rt712-sdca map addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_card_jack_new_pins(rtd->card, "Headset Jack",
					 SND_JACK_HEADSET | SND_JACK_BTN_0 |
					 SND_JACK_BTN_1 | SND_JACK_BTN_2 |
					 SND_JACK_BTN_3,
					 &ctx->sdw_headset,
					 rt712_sdca_jack_pins,
					 ARRAY_SIZE(rt712_sdca_jack_pins));
	if (ret) {
		dev_err(rtd->card->dev, "Headset Jack creation failed: %d\n",
			ret);
		return ret;
	}

	jack = &ctx->sdw_headset;

	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_PLAYPAUSE);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);

	ret = snd_soc_component_set_jack(component, jack, NULL);

	if (ret)
		dev_err(rtd->card->dev, "Headset Jack call-back failed: %d\n",
			ret);

	return ret;
}

int sof_sdw_rt712_sdca_exit(struct snd_soc_card *card, struct snd_soc_dai_link *dai_link)
{
	struct mc_private *ctx = snd_soc_card_get_drvdata(card);

	if (!ctx->headset_codec_dev)
		return 0;

	if (!SOF_RT711_JDSRC(sof_sdw_quirk))
		return 0;

	device_remove_software_node(ctx->headset_codec_dev);
	put_device(ctx->headset_codec_dev);

	return 0;
}

int sof_sdw_rt712_sdca_init(struct snd_soc_card *card,
			    const struct snd_soc_acpi_link_adr *link,
			    struct snd_soc_dai_link *dai_links,
			    struct sof_sdw_codec_info *info,
			    bool playback)
{
	struct mc_private *ctx = snd_soc_card_get_drvdata(card);
	struct device *sdw_dev;
	int ret;

	/*
	 * headset should be initialized once.
	 * Do it with dai link for playback.
	 */
	if (!playback)
		return 0;

	sdw_dev = bus_find_device_by_name(&sdw_bus_type, NULL, dai_links->codecs[0].name);
	if (!sdw_dev)
		return -EPROBE_DEFER;

	ret = rt712_sdca_add_codec_device_props(sdw_dev);
	if (ret < 0) {
		put_device(sdw_dev);
		return ret;
	}
	ctx->headset_codec_dev = sdw_dev;

	dai_links->init = rt712_sdca_rtd_init;

	return 0;
}

static const struct snd_soc_dapm_widget rt712_spk_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
};

/*
 * dapm routes for rt712 spk will be registered dynamically according
 * to the number of rt712 spk used. The first two entries will be registered
 * for one codec case, and the last two entries are also registered
 * if two 1316s are used.
 */
static const struct snd_soc_dapm_route rt712_spk_map[] = {
	{ "Speaker", NULL, "rt712 SPOL" },
	{ "Speaker", NULL, "rt712 SPOR" },
};

static const struct snd_kcontrol_new rt712_spk_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
};

static int first_spk_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	int ret;

	card->components = devm_kasprintf(card->dev, GFP_KERNEL,
					  "%s spk:rt712",
					  card->components);
	if (!card->components)
		return -ENOMEM;

	ret = snd_soc_add_card_controls(card, rt712_spk_controls,
					ARRAY_SIZE(rt712_spk_controls));
	if (ret) {
		dev_err(card->dev, "rt712 spk controls addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dapm_new_controls(&card->dapm, rt712_spk_widgets,
					ARRAY_SIZE(rt712_spk_widgets));
	if (ret) {
		dev_err(card->dev, "rt712 spk widgets addition failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dapm_add_routes(&card->dapm, rt712_spk_map, 2);
	if (ret)
		dev_err(rtd->dev, "failed to add first SPK map: %d\n", ret);

	return ret;
}

static int second_spk_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	int ret;

	ret = snd_soc_dapm_add_routes(&card->dapm, rt712_spk_map + 2, 2);
	if (ret)
		dev_err(rtd->dev, "failed to add second SPK map: %d\n", ret);

	return ret;
}

static int all_spk_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;

	ret = first_spk_init(rtd);
	if (ret)
		return ret;

	return second_spk_init(rtd);
}

int sof_sdw_rt712_spk_init(struct snd_soc_card *card,
			   const struct snd_soc_acpi_link_adr *link,
			   struct snd_soc_dai_link *dai_links,
			   struct sof_sdw_codec_info *info,
			   bool playback)
{
	/* Count amp number and do init on playback link only. */
	if (!playback)
		return 0;

	info->amp_num++;
	if (info->amp_num == 1)
		dai_links->init = first_spk_init;

	if (info->amp_num == 2) {
		/*
		 * if two 1316s are in one dai link, the init function
		 * in this dai link will be first set for the first speaker,
		 * and it should be reset to initialize all speakers when
		 * the second speaker is found.
		 */
		if (dai_links->init)
			dai_links->init = all_spk_init;
		else
			dai_links->init = second_spk_init;
	}

	return 0;
}

static int rt712_sdca_dmic_rtd_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;

	card->components = devm_kasprintf(card->dev, GFP_KERNEL,
					  "%s mic:rt712-sdca-dmic",
					  card->components);
	if (!card->components)
		return -ENOMEM;

	return 0;
}

int sof_sdw_rt712_sdca_dmic_init(struct snd_soc_card *card,
				 const struct snd_soc_acpi_link_adr *link,
				 struct snd_soc_dai_link *dai_links,
				 struct sof_sdw_codec_info *info,
				 bool playback)
{
	dai_links->init = rt712_sdca_dmic_rtd_init;

	return 0;
}
