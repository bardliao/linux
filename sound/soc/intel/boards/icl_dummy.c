// SPDX-License-Identifier: GPL-2.0
// Copyright(c) 2018 Intel Corporation.

/*
 * Intel Icelake I2S Machine Driver with dummy Codecs
 *
 * Modified from:
 *   Intel Geminilake I2S Machine driver
 */

#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../../codecs/hdac_hdmi.h"

#define GLK_PLAT_CLK_FREQ 19200000

#define DUAL_CHANNEL 2
#define QUAD_CHANNEL 4
#define NAME_SIZE 32

static struct snd_soc_jack icelake_hdmi[3];

struct icl_hdmi_pcm {
	struct list_head head;
	struct snd_soc_dai *codec_dai;
	int device;
};

struct icl_card_private {
	struct list_head hdmi_pcm_list;
};

static int icelake_ssp_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	/* The ADSP will convert the FE rate to 48k, stereo */
	rate->min = rate->max = 48000;
	channels->min = channels->max = DUAL_CHANNEL;

	/* set SSP to 24 bit */
	snd_mask_none(fmt);
	snd_mask_set(fmt, SNDRV_PCM_FORMAT_S24_LE);

	return 0;
}

static int icelake_hdmi_init(struct snd_soc_pcm_runtime *rtd)
{
	struct icl_card_private *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *dai = rtd->codec_dai;
	struct icl_hdmi_pcm *pcm;

	pcm = devm_kzalloc(rtd->card->dev, sizeof(*pcm), GFP_KERNEL);
	if (!pcm)
		return -ENOMEM;

	pcm->device = dai->id;
	pcm->codec_dai = dai;

	list_add_tail(&pcm->head, &ctx->hdmi_pcm_list);

	return 0;
}

static const unsigned int rates[] = {
	48000,
};

static const struct snd_pcm_hw_constraint_list constraints_rates = {
	.count = ARRAY_SIZE(rates),
	.list  = rates,
	.mask = 0,
};

static const unsigned int channels[] = {
	DUAL_CHANNEL,
};

static const struct snd_pcm_hw_constraint_list constraints_channels = {
	.count = ARRAY_SIZE(channels),
	.list = channels,
	.mask = 0,
};

static unsigned int channels_quad[] = {
	QUAD_CHANNEL,
};

static struct snd_pcm_hw_constraint_list constraints_channels_quad = {
	.count = ARRAY_SIZE(channels_quad),
	.list = channels_quad,
	.mask = 0,
};

static int icelake_dmic_fixup(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
				SNDRV_PCM_HW_PARAM_CHANNELS);

	/*
	 * set BE channel constraint as user FE channels
	 */
	channels->min = channels->max = 4;

	return 0;
}

/* icelake digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link icelake_dais[] = {
	/* Back End DAI links */
	{
		/* SSP0 - Codec */
		.name = "SSP0-Codec",
		.id = 0,
		.cpu_dai_name = "SSP0 Pin",
		.platform_name = "sof-audio",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = icelake_ssp_fixup,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "dmic01",
		.id = 1,
		.cpu_dai_name = "DMIC01 Pin",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "0000:00:0e.0",
		.ignore_suspend = 1,
		.dpcm_capture = 1,
		.no_pcm = 1,
	},
	{
		.name = "iDisp1",
		.id = 2,
		.cpu_dai_name = "iDisp1 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi1",
		.platform_name = "0000:00:05.0",
		.init = icelake_hdmi_init,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		.name = "iDisp2",
		.id = 3,
		.cpu_dai_name = "iDisp2 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi2",
		.platform_name = "0000:00:05.0",
		.init = icelake_hdmi_init,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
	{
		.name = "iDisp3",
		.id = 4,
		.cpu_dai_name = "iDisp3 Pin",
		.codec_name = "ehdaudio0D2",
		.codec_dai_name = "intel-hdmi-hifi3",
		.platform_name = "0000:00:05.0",
		.init = icelake_hdmi_init,
		.dpcm_playback = 1,
		.no_pcm = 1,
	},
};

static int icl_card_late_probe(struct snd_soc_card *card)
{
	struct icl_card_private *ctx = snd_soc_card_get_drvdata(card);
	struct snd_soc_component *component = NULL;
	char jack_name[NAME_SIZE];
	struct icl_hdmi_pcm *pcm;
	int err = 0;
	int i = 0;

	list_for_each_entry(pcm, &ctx->hdmi_pcm_list, head) {
		component = pcm->codec_dai->component;
		snprintf(jack_name, sizeof(jack_name),
			"HDMI/DP, pcm=%d Jack", pcm->device);
		err = snd_soc_card_jack_new(card, jack_name,
					SND_JACK_AVOUT, &icelake_hdmi[i],
					NULL, 0);

		if (err)
			return err;

		err = hdac_hdmi_jack_init(pcm->codec_dai, pcm->device,
						&icelake_hdmi[i]);
		if (err < 0)
			return err;

		i++;
	}

	if (!component)
		return -EINVAL;

	return hdac_hdmi_jack_port_init(component, &card->dapm);
}

/* icelake audio machine driver for summy codec */
static struct snd_soc_card icl_audio_card_dummy = {
	.name = "icl_dummy",
	.owner = THIS_MODULE,
	.dai_link = icelake_dais,
	.num_links = ARRAY_SIZE(icelake_dais),
#if 0
	.controls = icelake_controls,
	.num_controls = ARRAY_SIZE(icelake_controls),
	.dapm_widgets = icelake_widgets,
	.num_dapm_widgets = ARRAY_SIZE(icelake_widgets),
	.dapm_routes = icelake_map,
	.num_dapm_routes = ARRAY_SIZE(icelake_map),
#endif
	.fully_routed = false,
	.late_probe = icl_card_late_probe,
};

static int icelake_audio_probe(struct platform_device *pdev)
{
	struct icl_card_private *ctx;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_ATOMIC);
	if (!ctx)
		return -ENOMEM;

	INIT_LIST_HEAD(&ctx->hdmi_pcm_list);

	icl_audio_card_dummy.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&icl_audio_card_dummy, ctx);

	return devm_snd_soc_register_card(&pdev->dev,
					&icl_audio_card_dummy);
}

static const struct platform_device_id icl_board_ids[] = {
	{
		.name = "icl_dummy",
		.driver_data =
			(kernel_ulong_t)&icl_audio_card_dummy,
	},
	{ }
};

static struct platform_driver icelake_audio = {
	.probe = icelake_audio_probe,
	.driver = {
		.name = "icl_dummy",
		.pm = &snd_soc_pm_ops,
	},
	.id_table = icl_board_ids,
};
module_platform_driver(icelake_audio)

/* Module information */
MODULE_DESCRIPTION("Icelake Audio Machine driver in I2S mode");
MODULE_AUTHOR("Bard Liao <bard.liao@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:icl_dummy");
