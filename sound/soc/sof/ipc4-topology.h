/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __INCLUDE_SOUND_SOF_TOPOLOGY2_H__
#define __INCLUDE_SOUND_SOF_TOPOLOGY2_H__

#include <sound/sof/ipc4/header.h>

#define SOF_IPC4_NODE_INDEX(x)	(x)
#define SOF_IPC4_NODE_TYPE(x)  ((x) << 8)

struct sof_copier_gateway_cfg {
	uint32_t node_id; /**< ID of Gateway Node */

	uint32_t dma_buffer_size; /**< Preferred Gateway DMA buffer size (in bytes) */

	/* Length of gateway node configuration blob specified in #config_data */
	uint32_t config_length;

	uint32_t config_data[0]; /**< Gateway node configuration blob */
};

struct sof_ipc4_module_copier {
	struct sof_ipc4_base_module_cfg base_config; /**< common config for all comps */
	struct sof_ipc4_audio_format out_format;
	uint32_t copier_feature_mask;
	struct sof_copier_gateway_cfg gtw_cfg;
};

struct sof_ipc4_pipeline {
	struct sof_ipc_pipe_new pipe_new;
	uint32_t lp_mode;	/**< low power mode */
	uint32_t mem_usage;
	int state;
};

struct sof_ipc4_available_audio_format {
	struct sof_ipc4_base_module_cfg *base_config;
	struct sof_ipc4_audio_format *out_audio_fmt;
	struct sof_ipc4_audio_format *ref_audio_fmt;
	int audio_fmt_num;
};

struct sof_ipc4_host {
	struct snd_soc_component *scomp;
	struct sof_ipc4_module_copier copier;
	u32 *copier_config;
	uint32_t ipc_config_size;
	void *ipc_config_data;
	struct sof_ipc4_available_audio_format available_fmt;
};

struct sof_ipc4_dai {
	struct sof_ipc4_module_copier copier;
	uint32_t *copier_config;
	uint32_t ipc_config_size;
	void *ipc_config_data;
	struct sof_ipc4_available_audio_format available_fmt;
};

struct sof_ipc4_gain_data {
	uint32_t channels;
	uint32_t init_val;
	uint32_t curve_type;
	uint32_t reserved;
	uint32_t curve_duration;
} __aligned(8);

struct sof_ipc4_gain {
	struct sof_ipc4_base_module_cfg base_config;
	struct sof_ipc4_gain_data data;
	struct sof_ipc4_available_audio_format available_fmt;
};

enum sof_ipc4_mixer_type {
	sof_ipc4_mix_in,
	sof_ipc4_mix_out
};

struct sof_ipc4_mixer {
	struct sof_ipc4_base_module_cfg base_config;
	enum sof_ipc4_mixer_type type;
	struct sof_ipc4_available_audio_format available_fmt;
};

void sof_ipc4_dbg_audio_format(struct device *dev, struct sof_ipc4_audio_format *format,
			       size_t object_size, int num_format, const char *widget_name,
			       const char *function_name);

int snd_sof_load_topology2(struct snd_soc_component *scomp, const char *file);
#endif
