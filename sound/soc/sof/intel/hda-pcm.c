// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Authors: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//	    Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//	    Rander Wang <rander.wang@intel.com>
//          Keyon Jie <yang.jie@linux.intel.com>
//

/*
 * Hardware interface for generic Intel audio DSP HDA IP
 */

#include <sound/hda_register.h>
#include <sound/pcm_params.h>
#include <linux/soundwire/sdw.h>
#include "../ops.h"
#include "hda.h"

#define SDnFMT_BASE(x)	((x) << 14)
#define SDnFMT_MULT(x)	(((x) - 1) << 11)
#define SDnFMT_DIV(x)	(((x) - 1) << 8)
#define SDnFMT_BITS(x)	((x) << 4)
#define SDnFMT_CHAN(x)	((x) << 0)

static inline u32 get_mult_div(struct snd_sof_dev *sdev, int rate)
{
	switch (rate) {
	case 8000:
		return SDnFMT_DIV(6);
	case 9600:
		return SDnFMT_DIV(5);
	case 11025:
		return SDnFMT_BASE(1) | SDnFMT_DIV(4);
	case 16000:
		return SDnFMT_DIV(3);
	case 22050:
		return SDnFMT_BASE(1) | SDnFMT_DIV(2);
	case 32000:
		return SDnFMT_DIV(3) | SDnFMT_MULT(2);
	case 44100:
		return SDnFMT_BASE(1);
	case 48000:
		return 0;
	case 88200:
		return SDnFMT_BASE(1) | SDnFMT_MULT(2);
	case 96000:
		return SDnFMT_MULT(2);
	case 176400:
		return SDnFMT_BASE(1) | SDnFMT_MULT(4);
	case 192000:
		return SDnFMT_MULT(4);
	default:
		dev_warn(sdev->dev, "can't find div rate %d using 48kHz\n",
			 rate);
		return 0; /* use 48KHz if not found */
	}
};

static inline u32 get_bits(struct snd_sof_dev *sdev, int sample_bits)
{
	switch (sample_bits) {
	case 8:
		return SDnFMT_BITS(0);
	case 16:
		return SDnFMT_BITS(1);
	case 20:
		return SDnFMT_BITS(2);
	case 24:
		return SDnFMT_BITS(3);
	case 32:
		return SDnFMT_BITS(4);
	default:
		dev_warn(sdev->dev, "can't find %d bits using 16bit\n",
			 sample_bits);
		return SDnFMT_BITS(1); /* use 16bits format if not found */
	}
};

int hda_dsp_pcm_hw_params(struct snd_sof_dev *sdev,
			  struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct sof_ipc_stream_params *ipc_params)
{
	struct hdac_stream *hstream = substream->runtime->private_data;
	struct hdac_ext_stream *stream = stream_to_hdac_ext_stream(hstream);
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct snd_dma_buffer *dmab;
	int ret;
	u32 size, rate, bits;

	pr_err("plb: in %s\n", __func__);
	
	size = params_buffer_bytes(params);
	rate = get_mult_div(sdev, params_rate(params));
	bits = get_bits(sdev, params_width(params));

	hstream->substream = substream;

	dmab = substream->runtime->dma_buffer_p;

	hstream->format_val = rate | bits | (params_channels(params) - 1);
	hstream->bufsize = size;
	hstream->period_bytes = params_period_bytes(params);
	hstream->no_period_wakeup  =
			(params->info & SNDRV_PCM_INFO_NO_PERIOD_WAKEUP) &&
			(params->flags & SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP);

	ret = hda_dsp_stream_hw_params(sdev, stream, dmab, params);
	if (ret < 0) {
		dev_err(sdev->dev, "error: hdac prepare failed: %x\n", ret);
		return ret;
	}

	/* disable SPIB, to enable buffer wrap for stream */
	hda_dsp_stream_spib_config(sdev, stream, HDA_DSP_SPIB_DISABLE, 0);

	/* set host_period_bytes to 0 if no IPC position */
	if (hda && hda->no_ipc_position)
		ipc_params->host_period_bytes = 0;

	ipc_params->stream_tag = hstream->stream_tag;

	return 0;
}

static int sof_sdw_stream_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	//struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct sdw_stream_runtime *sdw_stream = runtime->private_data;
	int ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:

		/*
		 * Workaround to fix first playback/capture noise issue
		 * TODO: Remove this when fix is done in firmware.
		 */
#if 0
		if (dma->stream_type == SDW_STREAM_PCM) {
			for (i = 0; i < dma->nr_ports; i++) {
				port = dma->port[i];
				intel_pdi_alh_configure(sdw, port->pdi);
			}
		}
#endif
		ret = sdw_enable_stream(sdw_stream);
		if (ret) {
			dev_err(rtd->cpu_dai->dev,
				"sdw_enable_stream: %s failed: %d",
				sdw_stream->name, ret);
			return ret;
		}
		break;

	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		ret = sdw_disable_stream(sdw_stream);
		if (ret) {
			dev_err(rtd->cpu_dai->dev,
				"sdw_disable_stream: %s failed: %d",
				sdw_stream->name, ret);
			return ret;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int hda_dsp_pcm_trigger(struct snd_sof_dev *sdev,
			struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct hdac_stream *hstream = substream->runtime->private_data;
	struct hdac_ext_stream *stream = stream_to_hdac_ext_stream(hstream);

	if (rtd->dai_link->no_pcm)
		return sof_sdw_stream_trigger(substream, cmd);

	return hda_dsp_stream_trigger(sdev, stream, cmd);
}

int sdw_pcm_prepare(struct snd_sof_dev *sdev,
		    struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sdw_stream_runtime *sdw_stream = runtime->private_data;
	struct device *dev;
	int ret = 0;

	if (rtd->cpu_dai->id >= SDW_DAI_ID_RANGE_START &&
	    rtd->cpu_dai->id <= SDW_DAI_ID_RANGE_END) {

		if (!sdw_stream)
			return -EINVAL;

		dev = rtd->cpu_dai->dev;
		ret = sdw_prepare_stream(sdw_stream);
		if (ret)
			dev_err(dev, "sdw_prepare_stream: %s failed: %d",
				sdw_stream->name, ret);
	}

	return ret;
}

snd_pcm_uframes_t hda_dsp_pcm_pointer(struct snd_sof_dev *sdev,
				      struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct hdac_stream *hstream = substream->runtime->private_data;
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct snd_sof_pcm *spcm;
	snd_pcm_uframes_t pos;

	spcm = snd_sof_find_spcm_dai(sdev, rtd);
	if (!spcm) {
		dev_warn_ratelimited(sdev->dev, "warn: can't find PCM with DAI ID %d\n",
				     rtd->dai_link->id);
		return 0;
	}

	if (hda && !hda->no_ipc_position) {
		/* read position from IPC position */
		pos = spcm->stream[substream->stream].posn.host_posn;
		goto found;
	}

	/*
	 * DPIB/posbuf position mode:
	 * For Playback, Use DPIB register from HDA space which
	 * reflects the actual data transferred.
	 * For Capture, Use the position buffer for pointer, as DPIB
	 * is not accurate enough, its update may be completed
	 * earlier than the data written to DDR.
	 */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pos = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR,
				       AZX_REG_VS_SDXDPIB_XBASE +
				       (AZX_REG_VS_SDXDPIB_XINTERVAL *
					hstream->index));
	} else {
		/*
		 * For capture stream, we need more workaround to fix the
		 * position incorrect issue:
		 *
		 * 1. Wait at least 20us before reading position buffer after
		 * the interrupt generated(IOC), to make sure position update
		 * happens on frame boundary i.e. 20.833uSec for 48KHz.
		 * 2. Perform a dummy Read to DPIB register to flush DMA
		 * position value.
		 * 3. Read the DMA Position from posbuf. Now the readback
		 * value should be >= period boundary.
		 */
		usleep_range(20, 21);
		snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR,
				 AZX_REG_VS_SDXDPIB_XBASE +
				 (AZX_REG_VS_SDXDPIB_XINTERVAL *
				  hstream->index));
		pos = snd_hdac_stream_get_pos_posbuf(hstream);
	}

	if (pos >= hstream->bufsize)
		pos = 0;

found:
	pos = bytes_to_frames(substream->runtime, pos);

	dev_vdbg(sdev->dev, "PCM: stream %d dir %d position %lu\n",
		 hstream->index, substream->stream, pos);
	return pos;
}

int hda_dsp_pcm_open(struct snd_sof_dev *sdev,
		     struct snd_pcm_substream *substream)
{
	struct hdac_ext_stream *dsp_stream;
	int direction = substream->stream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sdw_stream_runtime *sdw_stream = NULL;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	int i, ret = 0;
	char *name;

	pr_err("plb: in %s dai_link->name %s dai_link->cpus->dai_name %s no pcm %d\n", __func__, dai_link->name, dai_link->cpus->dai_name, dai_link->no_pcm);
	
	if (rtd->cpu_dai->id < SDW_DAI_ID_RANGE_START) {

		pr_err("plb: in hdaudio handling\n");
		
		dsp_stream = hda_dsp_stream_get(sdev, direction);
	
		if (!dsp_stream) {
			dev_err(sdev->dev, "error: no stream available\n");
			return -ENODEV;
		}

		/* binding pcm substream to hda stream */
		substream->runtime->private_data = &dsp_stream->hstream;
		return 0;
	}

	/* SoundWire handling: THIS IS A HACK */    
	if (rtd->cpu_dai->id >= SDW_DAI_ID_RANGE_START &&
	    rtd->cpu_dai->id <= SDW_DAI_ID_RANGE_END) {
		
		if (runtime->private_data) {
			dev_warn(rtd->cpu_dai->dev, " In %s: private data already defined\n", __func__);
			return 0;
		}
		pr_err("plb: in %s sdw handling cpu_dai->id %d\n", __func__, rtd->cpu_dai->id);
		
		name = kzalloc(32, GFP_KERNEL);
		if (!name)
			return -ENOMEM;

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snprintf(name, 32, "%s-Playback", rtd->cpu_dai->name);
		else
			snprintf(name, 32, "%s-Capture", rtd->cpu_dai->name);

		sdw_stream = sdw_alloc_stream(name);
		if (!sdw_stream) {
			dev_err(rtd->cpu_dai->dev,
				"alloc stream failed for DAI %s: %d",
				rtd->cpu_dai->name, ret);
			ret = -ENOMEM;
			goto error;
		}

		/* Set stream pointer on CPU DAI */
		snd_soc_dai_set_sdw_stream(rtd->cpu_dai,
					   sdw_stream,
					   substream->stream);

		/* Set stream pointer on all CODEC DAIs */
		for (i = 0; i < rtd->num_codecs; i++)
			snd_soc_dai_set_sdw_stream(rtd->codec_dais[i],
						   sdw_stream,
						   substream->stream);
	}

	runtime->private_data = sdw_stream;
	return 0;

error:
	kfree(name);
	sdw_release_stream(sdw_stream);
	return ret;
}

int hda_dsp_pcm_close(struct snd_sof_dev *sdev,
		      struct snd_pcm_substream *substream)
{
	struct hdac_stream *hstream = NULL;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sdw_stream_runtime *sdw_stream = NULL; runtime->private_data;
	int direction = substream->stream;
	int ret;

	if (rtd->dai_link->no_pcm)
		goto be;

	hstream = runtime->private_data;
	ret = hda_dsp_stream_put(sdev, direction, hstream->stream_tag);

	if (ret) {
		dev_dbg(sdev->dev, "stream %s not opened!\n", substream->name);
		return -ENODEV;
	}

	/* unbinding pcm substream to hda stream */
	substream->runtime->private_data = NULL;

	return 0;
be:
	sdw_stream = runtime->private_data;
	if (!sdw_stream)
		return -EINVAL;

	sdw_release_stream(sdw_stream);

	return 0;
}
