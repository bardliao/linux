// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
//
#define DEBUG
#include <uapi/sound/sof/tokens.h>
#include <sound/pcm_params.h>
#include "sof-priv.h"
#include "sof-audio.h"
#include "ipc3-ops.h"
#include "ipc4-topology.h"
#include "ops.h"

/* Full volume for default values */
#define VOL_ZERO_DB	BIT(VOLUME_FWL)

struct sof_widget_data {
	int ctrl_type;
	int ipc_cmd;
	struct sof_abi_hdr *pdata;
	struct snd_sof_control *control;
};

static const struct sof_topology_token ipc4_sched_tokens[] = {
	{SOF_TKN_SCHED_LP_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_pipeline, lp_mode)}
};

/* DMIC */
static const struct sof_topology_token dmic_tokens[] = {
	{SOF_TKN_INTEL_DMIC_DRIVER_VERSION,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_dmic_params, driver_ipc_version)},
	{SOF_TKN_INTEL_DMIC_CLK_MIN,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_dmic_params, pdmclk_min)},
	{SOF_TKN_INTEL_DMIC_CLK_MAX,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_dmic_params, pdmclk_max)},
	{SOF_TKN_INTEL_DMIC_SAMPLE_RATE,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_dmic_params, fifo_fs)},
	{SOF_TKN_INTEL_DMIC_DUTY_MIN,
		SND_SOC_TPLG_TUPLE_TYPE_SHORT, get_token_u16,
		offsetof(struct sof_ipc_dai_dmic_params, duty_min)},
	{SOF_TKN_INTEL_DMIC_DUTY_MAX,
		SND_SOC_TPLG_TUPLE_TYPE_SHORT, get_token_u16,
		offsetof(struct sof_ipc_dai_dmic_params, duty_max)},
	{SOF_TKN_INTEL_DMIC_NUM_PDM_ACTIVE,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_dmic_params,
			 num_pdm_active)},
	{SOF_TKN_INTEL_DMIC_FIFO_WORD_LENGTH,
		SND_SOC_TPLG_TUPLE_TYPE_SHORT, get_token_u16,
		offsetof(struct sof_ipc_dai_dmic_params, fifo_bits)},
	{SOF_TKN_INTEL_DMIC_UNMUTE_RAMP_TIME_MS,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_dmic_params, unmute_ramp_time)},
};

/* ALH */
static const struct sof_topology_token alh_tokens[] = {
	{SOF_TKN_INTEL_ALH_RATE,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_alh_params, rate)},
	{SOF_TKN_INTEL_ALH_CH,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_alh_params, channels)},
};

/* gain */
static const struct sof_topology_token gain_tokens[] = {
	{SOF_TKN_GAIN_RAMP_TYPE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_u32, offsetof(struct sof_ipc4_gain_data, curve_type)},
	{SOF_TKN_GAIN_RAMP_DURATION,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_gain_data, curve_duration)},
	{SOF_TKN_GAIN_VAL, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_u32, offsetof(struct sof_ipc4_gain_data, init_val)},
};

/* Generic components */
static const struct sof_topology_token ipc4_comp_tokens[] = {
	{SOF_TKN_COMP_CPC, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_base_module_cfg, cpc)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IBS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_base_module_cfg, ibs)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OBS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_base_module_cfg, obs)},
	{SOF_TKN_COMP_IS_PAGES, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_base_module_cfg, is_pages)},
};

static const struct sof_topology_token ipc4_mixer_tokens[] = {
	{SOF_TKN_MIXER_TYPE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_mixer, type)},
};

/* cavs audio format */
static const struct sof_topology_token ipc4_in_audio_format_tokens[] = {
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_RATE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, sampling_frequency)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_BIT_DEPTH, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, bit_depth)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_CH_MAP, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, ch_map)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_CH_CFG, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, ch_cfg)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_INTERLEAVING_STYLE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, interleaving_style)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_IN_FMT_CFG, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, fmt_cfg)},
};

static const struct sof_topology_token ipc4_out_audio_format_tokens[] = {
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_RATE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, sampling_frequency)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_BIT_DEPTH, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, bit_depth)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_CH_MAP, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, ch_map)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_CH_CFG, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, ch_cfg)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_INTERLEAVING_STYLE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, interleaving_style)},
	{SOF_TKN_CAVS_AUDIO_FORMAT_OUT_FMT_CFG, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc4_audio_format, fmt_cfg)},
};

static const struct sof_topology_token ipc4_copier_gateway_cfg_tokens[] = {
	{SOF_TKN_CAVS_AUDIO_FORMAT_DMA_BUFFER_SIZE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_copier_gateway_cfg, dma_buffer_size)},
};

static const struct sof_topology_token ipc4_copier_tokens[] = {
	{SOF_TKN_INTEL_COPIER_NODE_TYPE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		0},
};

static const struct sof_topology_token ipc4_audio_fmt_num_tokens[] = {
	{SOF_TKN_COMP_NUM_AUDIO_FORMATS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		0},
};

/* DAI */
static const struct sof_topology_token dai_tokens[] = {
	{SOF_TKN_DAI_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
		offsetof(struct sof_ipc_comp_dai, type)},
	{SOF_TKN_DAI_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_dai, dai_index)},
	{SOF_TKN_DAI_DIRECTION, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_comp_dai, direction)},
};

/* BE DAI link */
static const struct sof_topology_token dai_link_tokens[] = {
	{SOF_TKN_DAI_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
		offsetof(struct sof_ipc_dai_config, type)},
	{SOF_TKN_DAI_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_config, dai_index)},
};

/* SSP */
static const struct sof_topology_token ssp_tokens[] = {
	{SOF_TKN_INTEL_SSP_CLKS_CONTROL, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_ssp_params, clks_control)},
	{SOF_TKN_INTEL_SSP_MCLK_ID, SND_SOC_TPLG_TUPLE_TYPE_SHORT, get_token_u16,
		offsetof(struct sof_ipc_dai_ssp_params, mclk_id)},
	{SOF_TKN_INTEL_SSP_SAMPLE_BITS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_ssp_params, sample_valid_bits)},
	{SOF_TKN_INTEL_SSP_FRAME_PULSE_WIDTH, SND_SOC_TPLG_TUPLE_TYPE_SHORT,	get_token_u16,
		offsetof(struct sof_ipc_dai_ssp_params, frame_pulse_width)},
	{SOF_TKN_INTEL_SSP_QUIRKS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_ssp_params, quirks)},
	{SOF_TKN_INTEL_SSP_TDM_PADDING_PER_SLOT, SND_SOC_TPLG_TUPLE_TYPE_BOOL, get_token_u16,
		offsetof(struct sof_ipc_dai_ssp_params, tdm_per_slot_padding_flag)},
	{SOF_TKN_INTEL_SSP_BCLK_DELAY, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_ssp_params, bclk_delay)},
};

/* HDA */
static const struct sof_topology_token hda_tokens[] = {
	{SOF_TKN_INTEL_HDA_RATE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_hda_params, rate)},
	{SOF_TKN_INTEL_HDA_CH, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_u32,
		offsetof(struct sof_ipc_dai_hda_params, channels)},
};

static const struct sof_token_info ipc4_token_list[SOF_TOKEN_COUNT] = {
	[SOF_DAI_TOKENS] = {"DAI tokens", dai_tokens, ARRAY_SIZE(dai_tokens)},
	[SOF_DAI_LINK_TOKENS] = {"DAI link tokens", dai_link_tokens, ARRAY_SIZE(dai_link_tokens)},
	[SOF_HDA_TOKENS] = {"HDA tokens", hda_tokens, ARRAY_SIZE(hda_tokens)},
	[SOF_SSP_TOKENS] = {"SSP tokens", ssp_tokens, ARRAY_SIZE(ssp_tokens)},
	[SOF_ALH_TOKENS] = {"ALH tokens", alh_tokens, ARRAY_SIZE(alh_tokens)},
	[SOF_DMIC_TOKENS] = {"DMIC tokens", dmic_tokens, ARRAY_SIZE(dmic_tokens)},
	[SOF_SCHED_TOKENS] = {"Scheduler tokens", ipc4_sched_tokens, ARRAY_SIZE(ipc4_sched_tokens)},
	[SOF_GAIN_TOKENS] = {"Gain tokens", gain_tokens, ARRAY_SIZE(gain_tokens)},
	[SOF_IPC4_COMP_TOKENS] = {"IPC4 Component tokens", ipc4_comp_tokens, ARRAY_SIZE(ipc4_comp_tokens)},
	[SOF_IPC4_MIXER_TOKENS] = {"IPC4 Mixer tokens", ipc4_mixer_tokens, ARRAY_SIZE(ipc4_mixer_tokens)},
	[SOF_IPC4_IN_AUDIO_FORMAT_TOKENS] = {"IPC4 Input Audio format tokens", ipc4_in_audio_format_tokens, ARRAY_SIZE(ipc4_in_audio_format_tokens)},
	[SOF_IPC4_OUT_AUDIO_FORMAT_TOKENS] = {"IPC4 Output Audio format tokens", ipc4_out_audio_format_tokens, ARRAY_SIZE(ipc4_out_audio_format_tokens)},
	[SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS] = {"IPC4 Cpoier gateway config tokens", ipc4_copier_gateway_cfg_tokens, ARRAY_SIZE(ipc4_copier_gateway_cfg_tokens)},
	[SOF_IPC4_COPIER_TOKENS] = {"IPC4 Cpoier tokens", ipc4_copier_tokens, ARRAY_SIZE(ipc4_copier_tokens)},
	[SOF_IPC4_AUDIO_FMT_NUM_TOKENS] = {"IPC4 Audio format number tokens", ipc4_audio_fmt_num_tokens, ARRAY_SIZE(ipc4_audio_fmt_num_tokens)},
};
#if 0
static int sof_ipc4_assign_module_id(struct snd_sof_widget *swidget)
{
	struct snd_soc_component *scomp = swidget->scomp;
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct sof_ipc4_fw_modules *fw_modules = (struct sof_ipc4_fw_modules *)sdev->fw_modules;
	u8 nil_uuid[SOF_UUID_SIZE] = {0};
	int i;

	/* UUID is required */
	if (!memcmp(&swidget->uuid, nil_uuid, SOF_UUID_SIZE))
		return -EINVAL;

	/* set module ID for widget */
	for (i = 0; i < sdev->fw_module_num; i++) {
		if (!memcmp(swidget->uuid, fw_modules[i].man4_module_entry.uuid,
			    UUID_SIZE)) {
			swidget->module_id = i;
			break;
		}
	}

	if (i == sdev->fw_module_num) {
		dev_err(sdev->dev, "Failed to find module ID for widget %s with uuid %pUL",
			swidget->widget->name, swidget->uuid);
		return -EINVAL;
	}

	return 0;
}
#endif
/**
 * sof_ipc4_comp_alloc - allocate and initialize buffer for a new component
 * @swidget: pointer to struct snd_sof_widget containing extended data
 * @ipc_size: IPC payload size that will be updated depending on valid
 *  extended data.
 * @index: ID of the pipeline the component belongs to
 *
 * Return: The pointer to the new allocated component, NULL if failed.
 */
static void *sof_ipc4_comp_alloc(struct snd_sof_widget *swidget,
				 size_t *ipc_size, int index)
{
	struct sof_ipc_comp *comp;
	size_t total_size = *ipc_size;
	size_t ext_size = sizeof(swidget->uuid);
//	int ret;

//	ret = sof_ipc4_assign_module_id(swidget);
//	if (ret < 0)
//		return NULL;

	comp = kzalloc(total_size, GFP_KERNEL);
	if (!comp)
		return NULL;

	comp->id = swidget->comp_id;
	comp->pipeline_id = index;
	comp->core = swidget->core;

	/* handle the extended data if needed */
	if (total_size > *ipc_size) {
		/* append extended data to the end of the component */
		memcpy((u8 *)comp + *ipc_size, swidget->uuid, ext_size);
		comp->ext_data_length = ext_size;
	}

	/* update ipc_size and return */
	*ipc_size = total_size;
	return comp;
}

void sof_ipc4_dbg_audio_format(struct device *dev, struct sof_ipc4_audio_format *format,
			       size_t object_size, int num_format, const char *widget_name,
			       const char *function_name)
{
	struct sof_ipc4_audio_format *fmt;
	void *ptr = format;
	int i;

	for (i = 0; i < num_format; i++, ptr = (u8 *)ptr + object_size) {
		fmt = ptr;
		dev_err(dev, "bard: %s i %d %s rate %u, bit_depth %u ch_map %u ch_cfg %u interleaving_style %u fmt_cfg %u\n",
			 function_name, i, widget_name,
			 fmt->sampling_frequency,
			 fmt->bit_depth,
			 fmt->ch_map,
			 fmt->ch_cfg,
			 fmt->interleaving_style,
			 fmt->fmt_cfg);
	}
}

static int sof_ipc4_get_audio_fmt(struct snd_soc_component *scomp,
				  struct snd_sof_widget *swidget,
				  struct sof_ipc4_available_audio_format *available_fmt,
				  bool has_out_format)
{
	struct sof_ipc4_base_module_cfg *base_config;
	struct sof_ipc4_audio_format *out_format;
	int audio_fmt_num = 0;
	int ret;

	ret = sof_update_ipc_object(scomp, &audio_fmt_num,
				    SOF_IPC4_AUDIO_FMT_NUM_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(audio_fmt_num), 1);

	if (ret != 0 || audio_fmt_num <= 0) {
		dev_err(scomp->dev, "Invalid number of audio formats: %d\n", audio_fmt_num);
		return -EINVAL;
	}
	pr_err("bard: %s %s audio_fmt_num %d\n", __func__, swidget->widget->name, audio_fmt_num);
	available_fmt->audio_fmt_num = audio_fmt_num;

	base_config = kcalloc(available_fmt->audio_fmt_num, sizeof(*base_config), GFP_KERNEL);
	if (!base_config)
		return -ENOMEM;

	ret = sof_update_ipc_object(scomp, base_config,
				    SOF_IPC4_COMP_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(*base_config),
				    available_fmt->audio_fmt_num);

	if (ret != 0) {
		dev_err(scomp->dev, "parse base_config tokens failed %d\n", ret);
		goto err_in;
	}

	ret = sof_update_ipc_object(scomp, &base_config->audio_fmt,
				    SOF_IPC4_IN_AUDIO_FORMAT_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(*base_config),
				    available_fmt->audio_fmt_num);

	if (ret != 0) {
		dev_err(scomp->dev, "parse base_config audio_fmt tokens failed %d\n", ret);
		goto err_in;
	}

	sof_ipc4_dbg_audio_format(scomp->dev, &base_config->audio_fmt,
				  sizeof(*base_config),
				  available_fmt->audio_fmt_num,
				  swidget->widget->name, __func__);

	available_fmt->base_config = base_config;

	if (!has_out_format)
		goto finish;

	out_format = kcalloc(available_fmt->audio_fmt_num, sizeof(*out_format), GFP_KERNEL);
	if (!out_format) {
		ret = -ENOMEM;
		goto err_in;
	}

	ret = sof_update_ipc_object(scomp, out_format,
				    SOF_IPC4_OUT_AUDIO_FORMAT_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(*out_format),
				    available_fmt->audio_fmt_num);

	if (ret != 0) {
		dev_err(scomp->dev, "parse output audio_fmt tokens failed\n");
		goto err_out;
	}

	available_fmt->out_audio_fmt = out_format;
	sof_ipc4_dbg_audio_format(scomp->dev, out_format, sizeof(*out_format),
				  available_fmt->audio_fmt_num,
				  swidget->widget->name, __func__);

finish:
	return 0;

err_out:
	kfree(out_format);
err_in:
	kfree(base_config);

	return ret;
}


static void sof_ipc4_widget_free_comp(struct snd_sof_widget *swidget)
{
	kfree(swidget->private);
}

static int sof_ipc4_widget_setup_pcm(struct snd_sof_widget *swidget)
{
	struct snd_soc_component *scomp = swidget->scomp;
	struct sof_ipc4_host *host;
	size_t size = sizeof(*host);
	int node_type = 0;
	int ret;

	pr_err("bard: %s\n", __func__);
	host = (struct sof_ipc4_host *)
	       sof_ipc4_comp_alloc(swidget, &size, swidget->pipeline_id);
	if (!host)
		return -ENOMEM;

	swidget->private = host;

	ret = sof_ipc4_get_audio_fmt(scomp, swidget, &host->available_fmt, true);
	if (ret != 0)
		goto err;

	ret = sof_update_ipc_object(scomp, &host->copier.gtw_cfg,
				    SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(host->copier.gtw_cfg), 1);

	if (ret != 0 || !host->copier.gtw_cfg.dma_buffer_size) {
		dev_err(scomp->dev, "Invalid host dma_buffer_size %d\n",
			host->copier.gtw_cfg.dma_buffer_size);
		goto err;
	}
	dev_dbg(scomp->dev, "host %s gtw_cfg dma_buffer_size %u\n",
		swidget->widget->name, host->copier.gtw_cfg.dma_buffer_size);

	ret = sof_update_ipc_object(scomp, &node_type,
				    SOF_IPC4_COPIER_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(node_type), 1);

	if (ret != 0) {
		dev_err(scomp->dev, "parse host copier_node tokens failed %d\n",
			ret);
		goto err;
	}
	dev_dbg(scomp->dev, "host %s node_type %u\n",
		swidget->widget->name, node_type);

	host->copier.gtw_cfg.node_id = SOF_IPC4_NODE_TYPE(node_type);	

	return 0;
err:
	kfree(host);
	return ret;
}

static int sof_ipc4_widget_setup_comp_dai(struct snd_sof_widget *swidget)
{
	struct snd_soc_component *scomp = swidget->scomp;
	struct snd_sof_dai *dai = swidget->private;
	struct sof_ipc4_dai *ipc4_dai;
	int node_type = 0;
	int ret;

	pr_err("bard: %s\n", __func__);

	ipc4_dai = kzalloc(sizeof(*ipc4_dai), GFP_KERNEL);
	if (!ipc4_dai)
		return -ENOMEM;

	ret = sof_ipc4_get_audio_fmt(scomp, swidget, &ipc4_dai->available_fmt, true);
	if (ret != 0)
		goto finish;

	ret = sof_update_ipc_object(scomp, &ipc4_dai->copier.gtw_cfg,
				    SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(ipc4_dai->copier.gtw_cfg), 1);

	if (ret != 0 || !ipc4_dai->copier.gtw_cfg.dma_buffer_size) {
		dev_err(scomp->dev, "Invalid dai dma_buffer_size %d\n",
			ipc4_dai->copier.gtw_cfg.dma_buffer_size);
		ret = -EINVAL;
		goto finish;
	}
	dev_dbg(scomp->dev, "dai %s gtw_cfg dma_buffer_size %u\n",
		swidget->widget->name, ipc4_dai->copier.gtw_cfg.dma_buffer_size);

	ret = sof_update_ipc_object(scomp, &node_type,
				    SOF_IPC4_COPIER_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(node_type), 1);

	if (ret != 0) {
		dev_err(scomp->dev, "parse dai copier_node tokens failed %d\n",
			ret);
		goto finish;
	}
	dev_dbg(scomp->dev, "dai %s node_type %u\n",
		swidget->widget->name, node_type);

	ipc4_dai->copier.gtw_cfg.node_id = SOF_IPC4_NODE_TYPE(node_type);

	dai->scomp = scomp;

	dev_dbg(scomp->dev, "dai %s cpc %d\n", swidget->widget->name,
		ipc4_dai->copier.base_config.cpc);

finish:
	dai->private = ipc4_dai;
	return ret;
}

static int sof_ipc4_widget_setup_comp_pipeline(struct snd_sof_widget *swidget)
{
	struct snd_soc_component *scomp = swidget->scomp;
	struct sof_ipc4_pipeline *pipeline;
	int ret;

	pr_err("bard: %s\n", __func__);
	pipeline = kzalloc(sizeof(*pipeline), GFP_KERNEL);
	if (!pipeline)
		return -ENOMEM;

	pipeline->pipe_new.pipeline_id = swidget->pipeline_id;
#if 0
	ret = sof_update_ipc_object(scomp, pipeline->pipe_new, SOF_SCHED_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(*pipeline), 1); // Is it necessary?
	if (ret != 0)
		goto err;
#endif

	ret = sof_update_ipc_object(scomp, pipeline, SOF_SCHED_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(*pipeline), 1);
	if (ret != 0) {
		dev_err(scomp->dev, "error: parse pipeline tokens failed\n");
		goto err;
	}

	dev_dbg(scomp->dev, "pipeline %s: id %d pri %d core %d lp mode %d\n",
		swidget->widget->name, pipeline->pipe_new.pipeline_id,
		pipeline->pipe_new.priority, pipeline->pipe_new.core,
		pipeline->lp_mode);

	swidget->private = pipeline;

	pr_err("bard: %s return 0\n", __func__);
	return 0;
err:
	kfree(pipeline);
	return ret;
}

/*
 * PGA Topology
 */

static int sof_ipc4_widget_setup_comp_pga(struct snd_sof_widget *swidget)
{
	struct snd_soc_component *scomp = swidget->scomp;
	struct sof_ipc4_gain *gain;
	size_t ipc_size = sizeof(*gain);
	int ret;

	pr_err("bard: %s\n", __func__);
#if 0
	/* set module ID for pga widget */
	ret = sof_ipc4_assign_module_id(swidget);
	if (ret < 0)
		return ret;
	/* Why not sof_ipc4_comp_alloc */
	gain = kzalloc(sizeof(*gain), GFP_KERNEL);
#else
	gain = sof_ipc4_comp_alloc(swidget, &ipc_size, swidget->pipeline_id);
#endif
	if (!gain)
		return -ENOMEM;

	/*
	 * available_fmt->out_audio_fmt will be ignored since there is no
	 * out_audio_fmt in gain.
	 */
	ret = sof_ipc4_get_audio_fmt(scomp, swidget, &gain->available_fmt, false);
	if (ret != 0)
		goto err;

	ret = sof_update_ipc_object(scomp, &gain->data, SOF_GAIN_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(gain->data), 1);
	if (ret != 0) {
		dev_err(scomp->dev, "error: parse gain tokens failed\n");
		goto err;
	}

	dev_dbg(scomp->dev, "tplg2: ready widget %s, ramp_type %d, duration %d, val %d  cpc %d",
		swidget->widget->name, gain->data.curve_type, gain->data.curve_duration,
		gain->data.init_val, gain->base_config.cpc);

	swidget->private = gain;

	return 0;
err:
	kfree(gain);
	return ret;
}

static int sof_ipc4_widget_setup_comp_mixer(struct snd_sof_widget *swidget)
{
#if 1
	struct snd_soc_component *scomp = swidget->scomp;
	struct sof_ipc4_mixer *mixer;
	size_t ipc_size = sizeof(*mixer);
	int ret;

	pr_err("bard: %s\n", __func__);

	mixer = sof_ipc4_comp_alloc(swidget, &ipc_size, swidget->pipeline_id);
	if (!mixer)
		return -ENOMEM;

	/*
	 * available_fmt->out_audio_fmt will be ignored since there is no
	 * out_audio_fmt in gain.
	 */
	ret = sof_ipc4_get_audio_fmt(scomp, swidget, &mixer->available_fmt, false);
	if (ret != 0)
		goto err;

	ret = sof_update_ipc_object(scomp, &mixer, SOF_IPC4_MIXER_TOKENS, swidget->tuples,
				    swidget->num_tuples, sizeof(*mixer), 1);
	if (ret != 0) {
		dev_err(scomp->dev, "error: parse mixer tokens failed\n");
		goto err;
	}

	dev_dbg(scomp->dev, "mixer type %d", mixer->type);

	swidget->private = mixer;

	return 0;
err:
	kfree(mixer);
	return ret;
#else
	return 0;
#endif
}
#if 0
static int sof_link_hda_load(struct snd_soc_component *scomp, struct snd_sof_dai_link *slink,
			     struct sof_ipc_dai_config *config, struct snd_sof_dai *dai)
{
	struct sof_dai_private_data *private = dai->private;
	u32 size = sizeof(*config);
	int ret;

	/* init IPC */
	memset(&config->hda, 0, sizeof(config->hda));
	config->hdr.size = size;

	/* parse one set of HDA tokens */
	ret = sof_update_ipc_object(scomp, &config->hda, SOF_HDA_TOKENS, slink->tuples,
				    slink->num_tuples, size, 1);
	if (ret < 0)
		return ret;

	dev_dbg(scomp->dev, "HDA config rate %d channels %d\n",
		config->hda.rate, config->hda.channels);

	config->hda.link_dma_ch = DMA_CHAN_INVALID;

	dai->number_configs = 1;
	dai->current_config = 0;
	private->dai_config = kmemdup(config, size, GFP_KERNEL);
	if (!private->dai_config)
		return -ENOMEM;

	return 0;
}

static void sof_dai_set_format(struct snd_soc_tplg_hw_config *hw_config,
			       struct sof_ipc_dai_config *config)
{
	/* clock directions wrt codec */
	config->format &= ~SOF_DAI_FMT_CLOCK_PROVIDER_MASK;
	if (hw_config->bclk_provider == SND_SOC_TPLG_BCLK_CP) {
		/* codec is bclk provider */
		if (hw_config->fsync_provider == SND_SOC_TPLG_FSYNC_CP)
			config->format |= SOF_DAI_FMT_CBP_CFP;
		else
			config->format |= SOF_DAI_FMT_CBP_CFC;
	} else {
		/* codec is bclk consumer */
		if (hw_config->fsync_provider == SND_SOC_TPLG_FSYNC_CP)
			config->format |= SOF_DAI_FMT_CBC_CFP;
		else
			config->format |= SOF_DAI_FMT_CBC_CFC;
	}

	/* inverted clocks ? */
	config->format &= ~SOF_DAI_FMT_INV_MASK;
	if (hw_config->invert_bclk) {
		if (hw_config->invert_fsync)
			config->format |= SOF_DAI_FMT_IB_IF;
		else
			config->format |= SOF_DAI_FMT_IB_NF;
	} else {
		if (hw_config->invert_fsync)
			config->format |= SOF_DAI_FMT_NB_IF;
		else
			config->format |= SOF_DAI_FMT_NB_NF;
	}
}

static int sof_link_ssp_load(struct snd_soc_component *scomp, struct snd_sof_dai_link *slink,
			     struct sof_ipc_dai_config *config, struct snd_sof_dai *dai)
{
	struct snd_soc_tplg_hw_config *hw_config = slink->hw_configs;
	struct sof_dai_private_data *private = dai->private;
	u32 size = sizeof(*config);
	int current_config;
	int i, ret;

	/*
	 * Parse common data, we should have 1 common data per hw_config.
	 */
	ret = sof_update_ipc_object(scomp, &config->ssp, SOF_SSP_TOKENS, slink->tuples,
				    slink->num_tuples, size, slink->num_hw_configs);
	if (ret < 0)
		return ret;

	/* process all possible hw configs */
	for (i = 0; i < slink->num_hw_configs; i++) {
		if (le32_to_cpu(hw_config[i].id) == slink->default_hw_cfg_id)
			current_config = i;

		/* handle master/slave and inverted clocks */
		sof_dai_set_format(&hw_config[i], &config[i]);

		config[i].hdr.size = size;

		/* copy differentiating hw configs to ipc structs */
		config[i].ssp.mclk_rate = le32_to_cpu(hw_config[i].mclk_rate);
		config[i].ssp.bclk_rate = le32_to_cpu(hw_config[i].bclk_rate);
		config[i].ssp.fsync_rate = le32_to_cpu(hw_config[i].fsync_rate);
		config[i].ssp.tdm_slots = le32_to_cpu(hw_config[i].tdm_slots);
		config[i].ssp.tdm_slot_width = le32_to_cpu(hw_config[i].tdm_slot_width);
		config[i].ssp.mclk_direction = hw_config[i].mclk_direction;
		config[i].ssp.rx_slots = le32_to_cpu(hw_config[i].rx_slots);
		config[i].ssp.tx_slots = le32_to_cpu(hw_config[i].tx_slots);

		dev_dbg(scomp->dev, "tplg: config SSP%d fmt %#x mclk %d bclk %d fclk %d width (%d)%d slots %d mclk id %d quirks %d clks_control %#x\n",
			config[i].dai_index, config[i].format,
			config[i].ssp.mclk_rate, config[i].ssp.bclk_rate,
			config[i].ssp.fsync_rate, config[i].ssp.sample_valid_bits,
			config[i].ssp.tdm_slot_width, config[i].ssp.tdm_slots,
			config[i].ssp.mclk_id, config[i].ssp.quirks, config[i].ssp.clks_control);

		/* validate SSP fsync rate and channel count */
		if (config[i].ssp.fsync_rate < 8000 || config[i].ssp.fsync_rate > 192000) {
			dev_err(scomp->dev, "Invalid fsync rate for SSP%d\n", config[i].dai_index);
			return -EINVAL;
		}

		if (config[i].ssp.tdm_slots < 1 || config[i].ssp.tdm_slots > 8) {
			dev_err(scomp->dev, "Invalid channel count for SSP%d\n",
				config[i].dai_index);
			return -EINVAL;
		}
	}

	dai->number_configs = slink->num_hw_configs;
	dai->current_config = current_config;
	private->dai_config = kmemdup(config, size * slink->num_hw_configs, GFP_KERNEL);
	if (!private->dai_config)
		return -ENOMEM;

	return 0;
}

static int sof_link_dmic_load(struct snd_soc_component *scomp, struct snd_sof_dai_link *slink,
			      struct sof_ipc_dai_config *config, struct snd_sof_dai *dai)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct sof_dai_private_data *private = dai->private;
	struct sof_ipc_fw_ready *ready = &sdev->fw_ready;
	struct sof_ipc_fw_version *v = &ready->version;
	size_t size = sizeof(*config);
	int i, ret;

	/* Ensure the entire DMIC config struct is zeros */
	memset(&config->dmic, 0, sizeof(config->dmic));

	/* parse the required set of DMIC tokens based on num_hw_cfgs */
	ret = sof_update_ipc_object(scomp, &config->dmic, SOF_DMIC_TOKENS, slink->tuples,
				    slink->num_tuples, size, slink->num_hw_configs);
	if (ret < 0)
		return ret;

	/* parse the required set of DMIC PDM tokens based on number of active PDM's */
	ret = sof_update_ipc_object(scomp, &config->dmic.pdm[0], SOF_DMIC_PDM_TOKENS,
				    slink->tuples, slink->num_tuples,
				    sizeof(struct sof_ipc_dai_dmic_pdm_ctrl),
				    config->dmic.num_pdm_active);
	if (ret < 0)
		return ret;

	/* set IPC header size */
	config->hdr.size = size;

	/* debug messages */
	dev_dbg(scomp->dev, "tplg: config DMIC%d driver version %d\n",
		config->dai_index, config->dmic.driver_ipc_version);
	dev_dbg(scomp->dev, "pdmclk_min %d pdm_clkmax %d duty_min %d\n",
		config->dmic.pdmclk_min, config->dmic.pdmclk_max,
		config->dmic.duty_min);
	dev_dbg(scomp->dev, "duty_max %d fifo_fs %d num_pdms active %d\n",
		config->dmic.duty_max, config->dmic.fifo_fs,
		config->dmic.num_pdm_active);
	dev_dbg(scomp->dev, "fifo word length %d\n", config->dmic.fifo_bits);

	for (i = 0; i < config->dmic.num_pdm_active; i++) {
		dev_dbg(scomp->dev, "pdm %d mic a %d mic b %d\n",
			config->dmic.pdm[i].id,
			config->dmic.pdm[i].enable_mic_a,
			config->dmic.pdm[i].enable_mic_b);
		dev_dbg(scomp->dev, "pdm %d polarity a %d polarity b %d\n",
			config->dmic.pdm[i].id,
			config->dmic.pdm[i].polarity_mic_a,
			config->dmic.pdm[i].polarity_mic_b);
		dev_dbg(scomp->dev, "pdm %d clk_edge %d skew %d\n",
			config->dmic.pdm[i].id,
			config->dmic.pdm[i].clk_edge,
			config->dmic.pdm[i].skew);
	}

	/*
	 * this takes care of backwards compatible handling of fifo_bits_b.
	 * It is deprecated since firmware ABI version 3.0.1.
	 */
	if (SOF_ABI_VER(v->major, v->minor, v->micro) < SOF_ABI_VER(3, 0, 1))
		config->dmic.fifo_bits_b = config->dmic.fifo_bits;

	dai->number_configs = 1;
	dai->current_config = 0;
	private->dai_config = kmemdup(config, size, GFP_KERNEL);
	if (!private->dai_config)
		return -ENOMEM;

	return 0;
}

static int sof_ipc4_link_alh_load(struct snd_soc_component *scomp, int index,
				  struct snd_soc_dai_link *link,
				  struct snd_soc_tplg_link_config *cfg,
				  struct snd_soc_tplg_hw_config *hw_config,
				  struct sof_ipc_dai_config *config)
{
	struct snd_sof_dev *sdev = snd_soc_component_get_drvdata(scomp);
	struct snd_soc_tplg_private *private = &cfg->priv;
	size_t size = sizeof(*config);
	int ret;

	/* Ensure the entire DMIC config struct is zeros */
	memset(&config->alh, 0, sizeof(struct sof_ipc_dai_alh_params));

	/* get DMIC tokens */
	ret = sof_parse_tokens(scomp, &config->alh, alh_tokens,
			       ARRAY_SIZE(alh_tokens), private->array,
			       le32_to_cpu(private->size));
	if (ret != 0) {
		dev_err(scomp->dev, "error: parse alh tokens failed %d\n",
			le32_to_cpu(private->size));
		return ret;
	}

	/* debug messages */
	dev_dbg(scomp->dev, "tplg: config ALH rate %d channels %d\n",
		config->alh.rate, config->alh.channels);

	ret = sof_ipc4_set_dai_config(sdev, size, link, config);
	if (ret < 0)
		dev_err(scomp->dev, "error: failed to process hda dai link %s", __func__);

	return ret;
}

static int sof_link_alh_load(struct snd_soc_component *scomp, struct snd_sof_dai_link *slink,
			     struct sof_ipc_dai_config *config, struct snd_sof_dai *dai)
{
	struct sof_dai_private_data *private = dai->private;
	u32 size = sizeof(*config);
	int ret;

	/* Ensure the entire ALH config struct is zeros */
	memset(&config->alh, 0, sizeof(struct sof_ipc_dai_alh_params));
	
	/* parse the required set of ALH tokens based on num_hw_cfgs */
	ret = sof_update_ipc_object(scomp, &config->alh, SOF_ALH_TOKENS, slink->tuples,
				    slink->num_tuples, size, slink->num_hw_configs);
	if (ret < 0)
		return ret;

	/* debug messages */
	dev_dbg(scomp->dev, "tplg: config ALH rate %d channels %d\n",
		config->alh.rate, config->alh.channels);

	/* set config for all DAI's with name matching the link name */
	dai->number_configs = 1;
	dai->current_config = 0;
	private->dai_config = kmemdup(config, size, GFP_KERNEL);
	if (!private->dai_config)
		return -ENOMEM;

	return 0;
}
#endif

static void sof_ipc4_widget_free_comp_dai(struct snd_sof_widget *swidget)
{
	switch (swidget->id) {
	case snd_soc_dapm_dai_in:
	case snd_soc_dapm_dai_out:
	{
		struct snd_sof_dai *dai = swidget->private;
		struct sof_ipc4_dai *ipc4_dai;

		if (!dai)
			return;

		ipc4_dai = dai->private;
		if (ipc4_dai)
			kfree(ipc4_dai);

		kfree(dai);
		break;
	}
	default:
		break;
	}
}

static int sof_ipc4_widget_free(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget)
{
#if 0
	struct sof_ipc4_pipeline *pipeline;
	int ret;

	/* decrement usecount and update config for static DAI widgets */
	if (--swidget->use_count > 0)
		return 0;

	/* only free when refcount is 0 */
	if (!swidget->private)
		return 0;

	switch (swidget->id) {
	case snd_soc_dapm_scheduler:
		pipeline = (struct sof_ipc4_pipeline *)swidget->private;

		ret = sof_ipc4_delete_pipeline(sdev, swidget->pipeline_id);
		if (ret < 0)
			goto error;

		pipeline->mem_usage = 0;
		break;
	default:
		ret = sof_ipc4_reset_route_setup_status(sdev, swidget);
		if (ret < 0)
			goto error;

		ret = sof_ipc4_release_module(sdev, swidget->module_id, swidget->instance_id);
		if (ret < 0)
			goto error;

		break;
	}

	swidget->complete = 0;
	dev_dbg(sdev->dev, "widget %s instance %d freed\n", swidget->widget->name,
		swidget->instance_id);

	return 0;

error:
	dev_err(sdev->dev, "error: failed to free widget %s\n", swidget->widget->name);
	swidget->use_count++;
	return ret;
#else
	swidget->use_count--;
		return 0;
#endif
}

/* token list for each topology object */
static enum sof_tokens host_token_list[] = {
	SOF_IPC4_COMP_TOKENS,
	SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS,
	SOF_IPC4_IN_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_OUT_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS,
	SOF_IPC4_COPIER_TOKENS,
	SOF_IPC4_AUDIO_FMT_NUM_TOKENS,
	SOF_IPC4_IN_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_OUT_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS,
	SOF_IPC4_COPIER_TOKENS,
};

static enum sof_tokens mixer_token_list[] = {
	SOF_IPC4_COMP_TOKENS,
	SOF_IPC4_MIXER_TOKENS,
	SOF_IPC4_IN_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS,
	SOF_IPC4_COPIER_TOKENS,
	SOF_IPC4_AUDIO_FMT_NUM_TOKENS,
};

static enum sof_tokens pipeline_token_list[] = {
	SOF_IPC4_COMP_TOKENS,
	SOF_IPC4_IN_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_OUT_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS,
	SOF_IPC4_COPIER_TOKENS,
	SOF_IPC4_AUDIO_FMT_NUM_TOKENS,
};

static enum sof_tokens pga_token_list[] = {
	SOF_IPC4_COMP_TOKENS,
	SOF_GAIN_TOKENS,
	SOF_IPC4_IN_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS,
	SOF_IPC4_COPIER_TOKENS,
	SOF_IPC4_AUDIO_FMT_NUM_TOKENS,
};

static enum sof_tokens dai_token_list[] = {
	SOF_IPC4_COMP_TOKENS,
	SOF_IPC4_IN_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_OUT_AUDIO_FORMAT_TOKENS,
	SOF_IPC4_COPIER_GATEWAY_CFG_TOKENS,
	SOF_IPC4_COPIER_TOKENS,
	SOF_IPC4_AUDIO_FMT_NUM_TOKENS,
};

static const struct ipc_tplg_widget_ops tplg_ipc4_widget_ops[SND_SOC_DAPM_TYPE_COUNT] = {
	[snd_soc_dapm_aif_in] =  {sof_ipc4_widget_setup_pcm, sof_ipc4_widget_free_comp,
				  host_token_list, ARRAY_SIZE(host_token_list), NULL},
	[snd_soc_dapm_aif_out] = {sof_ipc4_widget_setup_pcm, sof_ipc4_widget_free_comp,
				  host_token_list, ARRAY_SIZE(host_token_list), NULL},

	[snd_soc_dapm_dai_in] = {sof_ipc4_widget_setup_comp_dai, sof_ipc4_widget_free_comp_dai,
				 dai_token_list, ARRAY_SIZE(dai_token_list), NULL},
	[snd_soc_dapm_dai_out] = {sof_ipc4_widget_setup_comp_dai, sof_ipc4_widget_free_comp_dai,
				  dai_token_list, ARRAY_SIZE(dai_token_list), NULL},
	[snd_soc_dapm_mixer] = {sof_ipc4_widget_setup_comp_mixer, sof_ipc4_widget_free_comp,
				mixer_token_list, ARRAY_SIZE(mixer_token_list),
				NULL},
	[snd_soc_dapm_scheduler] = {sof_ipc4_widget_setup_comp_pipeline, sof_ipc4_widget_free_comp,
				    pipeline_token_list, ARRAY_SIZE(pipeline_token_list), NULL},
	[snd_soc_dapm_pga] = {sof_ipc4_widget_setup_comp_pga, sof_ipc4_widget_free_comp,
			      pga_token_list, ARRAY_SIZE(pga_token_list), NULL},
};

const struct ipc_tplg_ops ipc4_tplg_ops = {
	.widget = tplg_ipc4_widget_ops,
	.control = &tplg_ipc4_control_ops,
//	.route_setup = sof_ipc4_route_setup,
//	.control_setup = sof_ipc4_control_setup,
//	.control_free = sof_ipc4_control_free,
//	.pipeline_complete = sof_ipc4_complete_pipeline,
	.token_list = ipc4_token_list,
	.widget_free = sof_ipc4_widget_free,
//	.widget_setup = sof_ipc4_widget_setup,
//	.dai_config = sof_ipc4_dai_config, //No IPC for dai_config
//	.set_up_all_pipelines = sof_ipc4_set_up_all_pipelines,
//	.tear_down_all_pipelines = sof_ipc4_tear_down_all_pipelines,
};
