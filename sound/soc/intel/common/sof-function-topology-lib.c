// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2025 Intel Corporation.
//

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <sound/soc.h>
#include <sound/soc-acpi.h>
#include "sof-function-topology-lib.h"

enum tplg_device_id {
	TPLG_DEVICE_SDCA_JACK,
	TPLG_DEVICE_SDCA_AMP,
	TPLG_DEVICE_SDCA_MIC,
	TPLG_DEVICE_INTEL_PCH_DMIC,
	TPLG_DEVICE_HDMI,
	TPLG_DEVICE_SSP_JACK,
	TPLG_DEVICE_SSP_AMP,
	TPLG_DEVICE_SSP_BT,
	TPLG_DEVICE_SSP_HDMI_IN,
	TPLG_DEVICE_MAX
};

#define SDCA_DEVICE_MASK (BIT(TPLG_DEVICE_SDCA_JACK) | BIT(TPLG_DEVICE_SDCA_AMP) | \
			  BIT(TPLG_DEVICE_SDCA_MIC))

#define SOF_INTEL_PLATFORM_NAME_MAX 4

static int get_platform_name(struct snd_soc_card *card,
			     const struct snd_soc_acpi_mach *mach, char *platform)
{
	int ret;

	ret = sscanf(mach->sof_tplg_filename, "sof-%3s-*.tplg", platform);
	if (ret != 1) {
		dev_err(card->dev, "Invalid platform name %s of tplg %s\n",
			platform, mach->sof_tplg_filename);
		return -EINVAL;
	}

	return 0;
}

static bool all_tplg_files_exist(struct device *dev, const char ***tplg_files, int tplg_num)
{
	const struct firmware *fw;
	int ret;
	int i;

	for (i = 0; i < tplg_num; i++) {
		ret = firmware_request_nowarn(&fw, (*tplg_files)[i], dev);
		if (!ret) {
			release_firmware(fw);
		} else {
			dev_warn(dev,
				 "Failed to open topology file: %s, you might need to\n",
				 (*tplg_files)[i]);
			dev_warn(dev,
				 "download it from https://github.com/thesofproject/sof-bin/\n");
			return false;
		}
	}

	return true;
}

static char *get_tplg_filename(struct device *dev, const char *prefix,
			       const char *platform, const char *tplg_dev_name,
			       int dai_link_id, int tplg_dev)
{
	char *filename = NULL;
	/*
	 * The tplg file naming rule is sof-<platform>-<function>-id<BE id number>.tplg
	 * where <platform> is only required for the devices that need NHLT blob like DMIC
	 * as the nhlt blob is platform dependent.
	 */
	switch (tplg_dev) {
	case TPLG_DEVICE_INTEL_PCH_DMIC:
	case TPLG_DEVICE_SSP_JACK:
	case TPLG_DEVICE_SSP_AMP:
	case TPLG_DEVICE_SSP_BT:
		filename = devm_kasprintf(dev, GFP_KERNEL, "%s/sof-%s-%s-id%d.tplg",
					  prefix, platform, tplg_dev_name, dai_link_id);
		break;
	default:
		filename = devm_kasprintf(dev, GFP_KERNEL, "%s/sof-%s-id%d.tplg",
					  prefix, tplg_dev_name, dai_link_id);
		break;
	}

	return filename;
}

int sof_sdw_get_tplg_files(struct snd_soc_card *card, const struct snd_soc_acpi_mach *mach,
			   const char *prefix, const char ***tplg_files, bool best_effort)
{
	struct snd_soc_acpi_mach_params mach_params = mach->mach_params;
	struct snd_soc_dai_link *dai_link;
	char platform[SOF_INTEL_PLATFORM_NAME_MAX];
	unsigned long tplg_mask = 0;
	u16 hdmi_in_mask = 0;
	int tplg_num = 0;
	int tplg_dev;
	int ret;
	int i;

	ret = get_platform_name(card, mach, platform);
	if (ret < 0)
		return ret;

	for_each_card_prelinks(card, i, dai_link) {
		char *tplg_dev_name;

		dev_dbg(card->dev, "dai_link %s id %d\n", dai_link->name, dai_link->id);
		if (strstr(dai_link->name, "SimpleJack")) {
			tplg_dev = TPLG_DEVICE_SDCA_JACK;
			tplg_dev_name = "sdca-jack";
		} else if (strstr(dai_link->name, "SmartAmp")) {
			tplg_dev = TPLG_DEVICE_SDCA_AMP;
			tplg_dev_name = devm_kasprintf(card->dev, GFP_KERNEL,
						       "sdca-%damp", dai_link->num_cpus);
			if (!tplg_dev_name)
				return -ENOMEM;
		} else if (strstr(dai_link->name, "SmartMic")) {
			tplg_dev = TPLG_DEVICE_SDCA_MIC;
			tplg_dev_name = "sdca-mic";
		} else if (strstr(dai_link->name, "dmic")) {
			switch (mach_params.dmic_num) {
			case 2:
				tplg_dev_name = "dmic-2ch";
				break;
			case 4:
				tplg_dev_name = "dmic-4ch";
				break;
			default:
				dev_warn(card->dev,
					 "unsupported number of dmics: %d\n",
					 mach_params.dmic_num);
				continue;
			}
			tplg_dev = TPLG_DEVICE_INTEL_PCH_DMIC;
		} else if (strstr(dai_link->name, "iDisp")) {
			tplg_dev = TPLG_DEVICE_HDMI;
			tplg_dev_name = "hdmi-pcm5";
		} else if (strstr(dai_link->name, "SSP")) {
			unsigned int ssp_port;

			if (sscanf(dai_link->name, "SSP%d", &ssp_port) != 1) {
				dev_err(card->dev, "Invalid SSP port %d\n", ssp_port);
				return -EINVAL;
			}
			if (strstr(dai_link->name, "Codec")) {
				/*
				 * Assume DAI link 0 is jack which is true in all existing
				 * machine driver
				 */
				if (dai_link->id == 0) {
					tplg_dev = TPLG_DEVICE_SSP_JACK;
					tplg_dev_name = devm_kasprintf(card->dev, GFP_KERNEL,
								       "ssp%d-jack", ssp_port);
				} else {
					tplg_dev = TPLG_DEVICE_SSP_AMP;
					tplg_dev_name = devm_kasprintf(card->dev, GFP_KERNEL,
								       "ssp%d-amp", ssp_port);
				}
			} else if (strstr(dai_link->name, "BT")) {
				tplg_dev = TPLG_DEVICE_SSP_BT;
				tplg_dev_name = devm_kasprintf(card->dev, GFP_KERNEL,
							       "ssp%d-bt", ssp_port);
			} else if (strstr(dai_link->name, "HDMI")) {
				hdmi_in_mask |= BIT(ssp_port);
				/* The number of HDMI in dai link is always 2 right now */
				if (hweight16(hdmi_in_mask) != 2)
				    continue;

				tplg_dev = TPLG_DEVICE_SSP_HDMI_IN;
				tplg_dev_name = devm_kasprintf(card->dev, GFP_KERNEL,
							       "ssp%x-hdmiin", hdmi_in_mask);
			} else {
				dev_warn(card->dev,
					 "unsupported SSP link %s\n", dai_link->name);
				continue;
			}
		} else {
			/* The dai link is not supported by separated tplg yet */
			dev_dbg(card->dev,
				"dai_link %s is not supported by separated tplg yet\n",
				dai_link->name);
			if (best_effort)
				continue;

			return 0;
		}
		if (tplg_mask & BIT(tplg_dev))
			continue;

		tplg_mask |= BIT(tplg_dev);

		(*tplg_files)[tplg_num] = get_tplg_filename(card->dev, prefix, platform,
							    tplg_dev_name, dai_link->id,
							    tplg_dev);
		if (!(*tplg_files)[tplg_num])
			return -ENOMEM;
		tplg_num++;
	}

	dev_dbg(card->dev, "tplg_mask %#lx tplg_num %d\n", tplg_mask, tplg_num);

	/* Check presence of sub-topologies */
	if (all_tplg_files_exist(card->dev, tplg_files, tplg_num))
		return tplg_num;

	/* return 0 to use monolithic topology */
	return 0;
}
EXPORT_SYMBOL_GPL(sof_sdw_get_tplg_files);
