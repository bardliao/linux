// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
// Copyright(c) 2024 Intel Corporation.

/*
 * Soundwire SDCA UAJ Function Driver
 */

#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/auxiliary_bus.h>
#include <sound/sdca_function.h>
#include <linux/bitops.h>
#include <sound/core.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <sound/initval.h>
#include <sound/jack.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/pm_runtime.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/soundwire/sdw_registers.h>
#include <linux/slab.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/soc-component.h>
#include "sdca_function_device.h"

struct sdca_uaj_priv {
	struct sdca_dev *sdev;
};


#define SDCA_STEREO_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_192000)
#define SDCA_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_driver sdca_uaj_dai[] = {
	{
		.name = "sdca_uaj-aif1",
		.playback = {
			.stream_name = "DP1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SDCA_STEREO_RATES,
			.formats = SDCA_FORMATS,
		},
		.capture = {
			.stream_name = "DP2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SDCA_STEREO_RATES,
			.formats = SDCA_FORMATS,
		},
	},
};

static const struct snd_soc_dapm_widget sdca_uaj_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DP1RX", "DP1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("DP2TX", "DP2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_OUTPUT("HP"),
};

static const struct snd_soc_dapm_route sdca_uaj_audio_map[] = {
	{ "HP", NULL, "DP1RX" },
	{ "DP2TX", NULL, "MIC" },
};


static bool is_route_exist(struct snd_soc_dapm_route *routes, int num_routes,
			   const char *source, const char *sink)
{
	int i;

	for (i = 0; i < num_routes; i++) {
		if (!strcmp(routes[i].source, source) &&
		    !strcmp(routes[i].sink, sink))
			return true;
	}

	return false;
}

static int sdca_uaj_component_probe(struct snd_soc_component *component) {
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);
	struct sdca_uaj_priv *uaj_priv = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_widget *dapm_widgets;
	struct snd_soc_dapm_route *dapm_routes;
	struct sdca_dev *sdev = uaj_priv->sdev;
	int entity_id;
	int route_id;
	int dapm_id;
	int ret;
	int i;
	int j;

	dev_info(component->dev, "bard: %s num_entities %d\n", __func__, sdev->function.num_entities);
	dapm_widgets = devm_kmalloc_array(component->dev, sdev->function.num_entities,
					  sizeof(*dapm_widgets), GFP_KERNEL);
	if (!dapm_widgets)
		return -ENOMEM;

	/* TODO: use a suitable number of routes */
	dapm_routes = devm_kmalloc_array(component->dev, 128,
					  sizeof(*dapm_routes), GFP_KERNEL);
	if (!dapm_routes)
		return -ENOMEM;

	dapm_id = 0;
	route_id = 0;
	for (i = 0; i < sdev->function.num_entities; i++) {
		dev_info(component->dev, "bard: %s: entities[%d] id %d type %#x label %s\n", __func__, i,
			 sdev->function.entities[i].id,
			 sdev->function.entities[i].entity_type,
			 sdev->function.entities[i].label);
		for (j = 0; j < sdev->function.entities[i].source_count; j++) {
			entity_id = sdca_find_entity_by_id(&sdev->function, sdev->function.entities[i].sources[j]);
			if (entity_id < 0) {
				dev_err(component->dev, "%s: source entity %d not found\n", __func__,
					sdev->function.entities[i].sources[j]);
				return entity_id;
			}
			if (!is_route_exist(dapm_routes, route_id,
				       sdev->function.entities[entity_id].label,
				       sdev->function.entities[i].label)) {
				dapm_routes[route_id++] = (struct snd_soc_dapm_route) {
					.source = sdev->function.entities[entity_id].label,
					.sink = sdev->function.entities[i].label,
				};
				dev_info(component->dev, "bard: %s: add route %s -> %s \n", __func__,
					 sdev->function.entities[entity_id].label,
					 sdev->function.entities[i].label);
			}
		}
		for (j = 0; j < sdev->function.entities[i].sink_count; j++) {
			entity_id = sdca_find_entity_by_id(&sdev->function, sdev->function.entities[i].sinks[j]);
			if (entity_id < 0) {
				dev_err(component->dev, "%s: sink entity %d not found\n", __func__,
					sdev->function.entities[i].sinks[j]);
				return entity_id;
			}
			if (!is_route_exist(dapm_routes, route_id,
				       sdev->function.entities[i].label,
				       sdev->function.entities[entity_id].label)) {
				dapm_routes[route_id++] = (struct snd_soc_dapm_route) {
					.source = sdev->function.entities[i].label,
					.sink = sdev->function.entities[entity_id].label,
				};
				dev_info(component->dev, "bard: %s: add route %s -> %s \n", __func__,
					 sdev->function.entities[i].label,
					 sdev->function.entities[entity_id].label);
			}
		}

		switch (sdev->function.entities[i].entity_type) {
		case SDCA_ENTITY_TYPE_IT:
		case SDCA_ENTITY_TYPE_TG:	/* Tone Generator */
			dapm_widgets[dapm_id++] = SND_SOC_DAPM_INPUT(sdev->function.entities[i].label);
			break;
		case SDCA_ENTITY_TYPE_OT:
			dapm_widgets[dapm_id++] = SND_SOC_DAPM_OUTPUT(sdev->function.entities[i].label);
			break;
		case SDCA_ENTITY_TYPE_PDE:
			dapm_widgets[dapm_id++] = SND_SOC_DAPM_SUPPLY(sdev->function.entities[i].label, SND_SOC_NOPM, 0, 0, NULL, 0);
			break;
		case SDCA_ENTITY_TYPE_MU:	/* Mixer Unit */
			dapm_widgets[dapm_id++] = SND_SOC_DAPM_MIXER(sdev->function.entities[i].label, SND_SOC_NOPM, 0, 0, NULL, 0);
			break;
		case SDCA_ENTITY_TYPE_SU:		/* Selector Unit */
			//dapm_widgets[dapm_id++] = SND_SOC_DAPM_MUX(sdev->function.entities[i].label, SND_SOC_NOPM, 0, 0, NULL);
			/* FIXME: Use PGA until we know how to set MUX controls */
			dapm_widgets[dapm_id++] = SND_SOC_DAPM_PGA(sdev->function.entities[i].label, SND_SOC_NOPM, 0, 0, NULL, 0);
			break;
		case SDCA_ENTITY_TYPE_XU:	/* Extension Unit */
		case SDCA_ENTITY_TYPE_MFPU:		/* Multi-Function Processing Unit */
		case SDCA_ENTITY_TYPE_SMPU:		/* Smart Mic Processing Unit */
		case SDCA_ENTITY_TYPE_SAPU:		/* Smart Amp Processing Unit */
		case SDCA_ENTITY_TYPE_CRU:	/* Channel Remapping Unit */
		case SDCA_ENTITY_TYPE_UDMPU:		/* UpDownMixerUnit */
		case SDCA_ENTITY_TYPE_FU:		/* Feature Unit */
			dapm_widgets[dapm_id++] = SND_SOC_DAPM_PGA(sdev->function.entities[i].label, SND_SOC_NOPM, 0, 0, NULL, 0);
			break;
		case SDCA_ENTITY_TYPE_PCE:		/* Privacy Control Entity */
		case SDCA_ENTITY_TYPE_CX:		/* Clock selector */
		case SDCA_ENTITY_TYPE_CS:		/* Clock Source */
		case SDCA_ENTITY_TYPE_GE:		/* Group Entity */
		case SDCA_ENTITY_TYPE_HIDE:		/* HID Entity */
		default:
			break;
		}
	}

	dev_info(component->dev, "bard: %s: dapm_id %d\n", __func__, dapm_id);

	ret = snd_soc_dapm_new_controls(dapm,
					dapm_widgets, dapm_id);

	if (ret < 0)
		return ret;

	ret = snd_soc_dapm_add_routes(dapm,
				      dapm_routes, route_id);
	return ret;
}

struct snd_soc_component_driver soc_sdca_uaj_dev = {
	.probe = sdca_uaj_component_probe,
	.dapm_widgets = sdca_uaj_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sdca_uaj_dapm_widgets),
	.dapm_routes = sdca_uaj_audio_map,
	.num_dapm_routes = ARRAY_SIZE(sdca_uaj_audio_map),
	.endianness = 1,
};

/*
 * probe and init (aux_dev_id argument is required by function prototype but not used)
 */
static int sdca_uaj_probe(struct auxiliary_device *auxdev,
			  const struct auxiliary_device_id *aux_dev_id)
{
	struct sdca_dev *sdev = auxiliary_dev_to_sdca_dev(auxdev);
	struct sdca_uaj_priv *uaj_priv;
	struct device *dev = &auxdev->dev;
	int ret;

	uaj_priv = devm_kzalloc(dev, sizeof(*uaj_priv), GFP_KERNEL);
	if (!uaj_priv)
		return -ENOMEM;

	dev_set_drvdata(dev, uaj_priv);
	ret =  sdca_parse_function(dev,
				   auxdev->dev.fwnode,
				   &sdev->function);
	if (ret < 0)
		dev_err(dev, "%s: %pfwP: probe failed: %d\n",
			__func__, auxdev->dev.fwnode, ret);

	uaj_priv->sdev = sdev;
	ret =  devm_snd_soc_register_component(dev,
			&soc_sdca_uaj_dev, sdca_uaj_dai, ARRAY_SIZE(sdca_uaj_dai));


	if (ret < 0)
		return ret;

	dev_info(dev, "bard: %s ret %d\n", __func__, ret);
	return ret;
}

static const struct auxiliary_device_id sdca_uaj_id_table[] = {
	{ .name = "snd_soc_sdca." SDCA_FUNCTION_TYPE_UAJ_NAME },
	{},
};
MODULE_DEVICE_TABLE(auxiliary, sdca_uaj_id_table);

static struct auxiliary_driver sdca_uaj_drv = {
	.probe = sdca_uaj_probe,
	.id_table = sdca_uaj_id_table
};
module_auxiliary_driver(sdca_uaj_drv);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("SDCA UAJ driver");
MODULE_IMPORT_NS(SND_SOC_SDCA);
