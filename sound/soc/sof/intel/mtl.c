// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Authors: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//

/*
 * Hardware interface for audio DSP on Meteorlake.
 */

#include <linux/firmware.h>
#include <sound/sof/ipc4/header.h>
#include "../ipc4-priv.h"
#include "../ops.h"
#include "hda.h"
#include "hda-ipc.h"
#include "../sof-audio.h"
#include "mtl.h"

static const struct snd_sof_debugfs_map mtl_dsp_debugfs[] = {
	{"hda", HDA_DSP_HDA_BAR, 0, 0x4000, SOF_DEBUGFS_ACCESS_ALWAYS},
	{"pp", HDA_DSP_PP_BAR,  0, 0x1000, SOF_DEBUGFS_ACCESS_ALWAYS},
	{"dsp", HDA_DSP_BAR,  0, 0x10000, SOF_DEBUGFS_ACCESS_ALWAYS},
};

static void mtl_ipc_host_done(struct snd_sof_dev *sdev);
static void mtl_ipc_dsp_done(struct snd_sof_dev *sdev);

/* Check if an IPC IRQ occurred */
static bool mtl_dsp_check_ipc_irq(struct snd_sof_dev *sdev)
{
	bool ret = false;
	u32 irq_status;
	u32 hfintipptr;

	/* read Interrupt IP Pointer */
	hfintipptr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfINTIPPTR) & MTL_HfINTIPPTR_PTR_MASK;
	irq_status = snd_sof_dsp_read(sdev, HDA_DSP_BAR, hfintipptr + MTL_DSP_IRQSTS);

	dev_vdbg(sdev->dev, "irq handler: irq_status:0x%x\n", irq_status);

	/* invalid message ? */
	if (irq_status == 0xffffffff)
		goto out;

	/* IPC message ? */
	if (irq_status & MTL_DSP_IRQSTS_IPC)
		ret = true;

out:
	return ret;
}

/* Check if an SDW IRQ occurred */
static bool mtl_dsp_check_sdw_irq(struct snd_sof_dev *sdev)
{
	bool ret = false;
	u32 irq_status;
	u32 hfintipptr;

	/* read Interrupt IP Pointer */
	hfintipptr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfINTIPPTR) & MTL_HfINTIPPTR_PTR_MASK;
	irq_status = snd_sof_dsp_read(sdev, HDA_DSP_BAR, hfintipptr + MTL_DSP_IRQSTS);

	dev_vdbg(sdev->dev, "irq handler: irq_status:0x%x\n", irq_status);

	/* invalid message ? */
	if (irq_status == 0xffffffff)
		goto out;

	/* IPC message ? */
	if (irq_status & MTL_DSP_IRQSTS_SDW)
		ret = true;

out:
	return ret;
}

static int mtl_ipc_send_msg(struct snd_sof_dev *sdev, struct snd_sof_ipc_msg *msg)
{
	struct sof_ipc4_msg *msg_data = msg->msg_data;

	/* send the message via mailbox */
	if (msg_data->data_size)
		sof_mailbox_write(sdev, sdev->host_box.offset, msg_data->data_ptr,
				  msg_data->data_size);

	snd_sof_dsp_write(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxIDDy,
			  msg_data->extension);
	snd_sof_dsp_write(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxIDR,
			  msg_data->primary | MTL_DSP_REG_HfIPCxIDR_BUSY);

	return 0;
}

static void mtl_enable_ipc_interrupts(struct snd_sof_dev *sdev)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;

	/* enable IPC DONE and BUSY interrupts */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, chip->ipc_ctl,
				MTL_DSP_REG_HfIPCxCTL_BUSY | MTL_DSP_REG_HfIPCxCTL_DONE,
				MTL_DSP_REG_HfIPCxCTL_BUSY | MTL_DSP_REG_HfIPCxCTL_DONE);
}

static void mtl_disable_ipc_interrupts(struct snd_sof_dev *sdev)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;

	/* disable IPC DONE and BUSY interrupts */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, chip->ipc_ctl,
				MTL_DSP_REG_HfIPCxCTL_BUSY | MTL_DSP_REG_HfIPCxCTL_DONE, 0);
}

static int mtl_enable_interrupts(struct snd_sof_dev *sdev)
{
	u32 hfintipptr, irqinten, host_ipc;
	u32 hipcie;
	int ret;

	/* read Interrupt IP Pointer */
	hfintipptr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfINTIPPTR) & MTL_HfINTIPPTR_PTR_MASK;

	/* Enable Host IPC and SOUNDWIRE */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, hfintipptr,
				MTL_IRQ_INTEN_L_HOST_IPC_MASK | MTL_IRQ_INTEN_L_SOUNDWIRE_MASK,
				MTL_IRQ_INTEN_L_HOST_IPC_MASK | MTL_IRQ_INTEN_L_SOUNDWIRE_MASK);

	/* check if operation was successful */
	host_ipc = MTL_IRQ_INTEN_L_HOST_IPC_MASK | MTL_IRQ_INTEN_L_SOUNDWIRE_MASK;
	irqinten = snd_sof_dsp_read(sdev, HDA_DSP_BAR, hfintipptr);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, hfintipptr, irqinten,
					    (irqinten & host_ipc) == host_ipc,
					    HDA_DSP_REG_POLL_INTERVAL_US, HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to enable Host IPC and/or SOUNDWIRE\n");
		return ret;
	}

	/* Set Host IPC interrupt enable */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfHIPCIE,
				MTL_DSP_REG_HfHIPCIE_IE_MASK, MTL_DSP_REG_HfHIPCIE_IE_MASK);

	/* check if operation was successful */
	host_ipc = MTL_DSP_REG_HfHIPCIE_IE_MASK;
	hipcie = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfHIPCIE);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfHIPCIE, hipcie,
					    (hipcie & host_ipc) == host_ipc,
					    HDA_DSP_REG_POLL_INTERVAL_US, HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to set Host IPC interrupt enable\n");
		return ret;
	}

	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfSNDWIE,
				MTL_DSP_REG_HfSNDWIE_IE_MASK, MTL_DSP_REG_HfSNDWIE_IE_MASK);
	host_ipc = MTL_DSP_REG_HfSNDWIE_IE_MASK;
	hipcie = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfSNDWIE);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfSNDWIE, hipcie,
					    (hipcie & host_ipc) == host_ipc,
					    HDA_DSP_REG_POLL_INTERVAL_US, HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0)
		dev_err(sdev->dev, "error: failed to set SoundWire IPC interrupt enable\n");

	return ret;
}

static int mtl_disable_interrupts(struct snd_sof_dev *sdev)
{
	u32 hfintipptr, irqinten, host_ipc;
	u32 hipcie;
	int ret;

	/* read Interrupt IP Pointer */
	hfintipptr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfINTIPPTR) & MTL_HfINTIPPTR_PTR_MASK;

	/* Disable Host IPC and SOUNDWIRE */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, hfintipptr,
				MTL_IRQ_INTEN_L_HOST_IPC_MASK | MTL_IRQ_INTEN_L_SOUNDWIRE_MASK, 0);

	/* check if operation was successful */
	host_ipc = MTL_IRQ_INTEN_L_HOST_IPC_MASK | MTL_IRQ_INTEN_L_SOUNDWIRE_MASK;
	irqinten = snd_sof_dsp_read(sdev, HDA_DSP_BAR, hfintipptr);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, hfintipptr, irqinten,
					    (irqinten & host_ipc) == 0,
					    HDA_DSP_REG_POLL_INTERVAL_US, HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to disable Host IPC and SoundWire\n");
		return ret;
	}

	/* Set Host IPC interrupt disable */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfHIPCIE,
				MTL_DSP_REG_HfHIPCIE_IE_MASK, 0);

	/* check if operation was successful */
	host_ipc = MTL_DSP_REG_HfHIPCIE_IE_MASK;
	hipcie = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfHIPCIE);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfHIPCIE, hipcie,
					    (hipcie & host_ipc) == 0,
					    HDA_DSP_REG_POLL_INTERVAL_US, HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to set Host IPC interrupt disable\n");
		return ret;
	}

	/* Set SoundWire IPC interrupt disable */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfSNDWIE,
				MTL_DSP_REG_HfSNDWIE_IE_MASK, 0);
	host_ipc = MTL_DSP_REG_HfSNDWIE_IE_MASK;
	hipcie = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfSNDWIE);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfSNDWIE, hipcie,
					    (hipcie & host_ipc) == 0,
					    HDA_DSP_REG_POLL_INTERVAL_US, HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0)
		dev_err(sdev->dev, "error: failed to set SoundWire IPC interrupt disable\n");

	return ret;
}

/* pre fw run operations */
static int mtl_dsp_pre_fw_run(struct snd_sof_dev *sdev)
{
	u32 dsphfdsscs, dsphfpwrsts;
	u32 cpa, pgs;
	int ret;

	/* Set the DSP subsystem power on */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_HfDSSCS,
				MTL_HfDSSCS_SPA_MASK, MTL_HfDSSCS_SPA_MASK);

	/* Wait for unstable CPA read (1 then 0 then 1) just after setting SPA bit */
	usleep_range(1000, 1010);

	/* poll with timeout to check if operation successful */
	cpa = MTL_HfDSSCS_CPA_MASK;
	dsphfdsscs = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfDSSCS);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_HfDSSCS, dsphfdsscs,
					    (dsphfdsscs & cpa) == cpa, HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		dev_err(sdev->dev, "error: failed to enable DSP subsystem\n");
		return ret;
	}

	/* Power up gated-DSP-0 domain in order to access the DSP shim register block. */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_HfPWRCTL,
				MTL_HfPWRCTL_WPDSPHPxPG, MTL_HfPWRCTL_WPDSPHPxPG);

	usleep_range(1000, 1010);

	/* poll with timeout to check if operation successful */
	pgs = MTL_HfPWRSTS_DSPHPxPGS_MASK;
	dsphfpwrsts = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfPWRSTS);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_HfPWRSTS, dsphfpwrsts,
					    (dsphfpwrsts & pgs) == pgs,
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0)
		dev_err(sdev->dev, "error: failed to power up gated DSP domain\n");

	return ret;
}

static int mtl_dsp_post_fw_run(struct snd_sof_dev *sdev)
{
	int ret;

	if (sdev->first_boot) {
		ret = hda_sdw_startup(sdev);
		if (ret < 0) {
			dev_err(sdev->dev,
				"error: could not startup SoundWire links\n");
			return ret;
		}
	}

	hda_sdw_int_enable(sdev, true);
	return 0;
}

static void mtl_dsp_dump(struct snd_sof_dev *sdev, u32 flags)
{
	u32 fwsts, fwlec, romdbgsts, romdbgerr;

	fwsts = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_ROM_STS);
	fwlec = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_ROM_ERROR);
	romdbgsts = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfFLGPxQWy);
	romdbgerr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfFLGPxQWy_ERROR);

	dev_err(sdev->dev, "error: ROM status: 0x%x, ROM error: 0x%x\n", fwsts, fwlec);
	dev_err(sdev->dev, "error: ROM debug status: 0x%x, ROM debug error: 0x%x\n", romdbgsts,
		romdbgerr);
	romdbgsts = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfFLGPxQWy + 0x8 * 3);
	dev_err(sdev->dev, "ROM feature bit%s enabled\n", romdbgsts & BIT(24) ? "" : " not");
}

static int mtl_dsp_core_power_up(struct snd_sof_dev *sdev, int core_mask)
{
	unsigned int cpa;
	u32 dspcxctl;
	int ret;

	/* Only the primary core can be powered up by the host */
	if (core_mask != BIT(0))
		return 0;

	/* Program the owner of the IP & shim registers (10: Host CPU) */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP2CxCTL_PRIMARY_CORE,
				MTL_DSP2CxCTL_PRIMARY_CORE_OSEL,
				0x2 << MTL_DSP2CxCTL_PRIMARY_CORE_OSEL_SHIFT);

	/* enable SPA bit */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP2CxCTL_PRIMARY_CORE,
				MTL_DSP2CxCTL_PRIMARY_CORE_SPA_MASK,
				MTL_DSP2CxCTL_PRIMARY_CORE_SPA_MASK);

	/* Wait for unstable CPA read (1 then 0 then 1) just after setting SPA bit */
	usleep_range(1000, 1010);

	/* poll with timeout to check if operation successful */
	cpa = MTL_DSP2CxCTL_PRIMARY_CORE_CPA_MASK;
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_DSP2CxCTL_PRIMARY_CORE, dspcxctl,
					    (dspcxctl & cpa) == cpa, HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0) {
		dev_err(sdev->dev, "error: %s: timeout on MTL_DSP2CxCTL_PRIMARY_CORE read\n",
			__func__);
		return ret;
	}

	/* did core power up ? */
	dspcxctl = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP2CxCTL_PRIMARY_CORE);
	if ((dspcxctl & MTL_DSP2CxCTL_PRIMARY_CORE_CPA_MASK)
		!= MTL_DSP2CxCTL_PRIMARY_CORE_CPA_MASK) {
		dev_err(sdev->dev, "error: power up core failed core_mask %xadspcs 0x%x\n",
			core_mask, dspcxctl);
		ret = -EIO;
	}

	return ret;
}

static int mtl_dsp_core_power_down(struct snd_sof_dev *sdev, int core_mask)
{
	u32 dspcxctl;
	int ret;

	/* Only the primary core can be powered down by the host */
	if (core_mask != BIT(0))
		return 0;

	/* disable SPA bit */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP2CxCTL_PRIMARY_CORE,
				MTL_DSP2CxCTL_PRIMARY_CORE_SPA_MASK, 0);

	/* Wait for unstable CPA read (1 then 0 then 1) just after setting SPA bit */
	usleep_range(1000, 1010);

	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_DSP2CxCTL_PRIMARY_CORE, dspcxctl,
					    !(dspcxctl & MTL_DSP2CxCTL_PRIMARY_CORE_CPA_MASK),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_PD_TIMEOUT * USEC_PER_MSEC);
	if (ret < 0)
		dev_err(sdev->dev, "error: failed to power down primary core\n");

	return ret;
}

static int mtl_dsp_cl_init(struct snd_sof_dev *sdev, int stream_tag, bool imr_boot)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	const struct sof_intel_dsp_desc *chip = hda->desc;
	unsigned int status;
	u32 ipc_hdr;
	int ret;

	/* step 1: purge FW request */
	ipc_hdr = chip->ipc_req_mask | HDA_DSP_ROM_IPC_CONTROL;
	if (!imr_boot)
		ipc_hdr |= HDA_DSP_ROM_IPC_PURGE_FW | ((stream_tag - 1) << 9);

	snd_sof_dsp_write(sdev, HDA_DSP_BAR, chip->ipc_req, ipc_hdr);

	/* step 2: power up primary core */
	ret = mtl_dsp_core_power_up(sdev, chip->host_managed_cores_mask);
	if (ret < 0) {
		if (hda->boot_iteration == HDA_FW_BOOT_ATTEMPTS)
			dev_err(sdev->dev, "error: dsp core 0/1 power up failed\n");
		goto err;
	}

	dev_dbg(sdev->dev, "Primary core power up successful\n");

	/* TODO: set SSP slave mode */

	/* step 4: wait for IPC DONE bit from ROM */
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, chip->ipc_ack, status,
					    ((status & chip->ipc_ack_mask) == chip->ipc_ack_mask),
					    HDA_DSP_REG_POLL_INTERVAL_US, MTL_DSP_PURGE_TIMEOUT_US);
	if (ret < 0) {
		if (hda->boot_iteration == HDA_FW_BOOT_ATTEMPTS)
			dev_err(sdev->dev, "error: timeout waiting for purge IPC done\n");
		goto err;
	}

	/* set DONE bit to clear the reply IPC message */
	snd_sof_dsp_update_bits_forced(sdev, HDA_DSP_BAR, chip->ipc_ack, chip->ipc_ack_mask,
				       chip->ipc_ack_mask);

	/* step 5: enable interrupts */
	ret = mtl_enable_interrupts(sdev);
	if (ret < 0) {
		if (hda->boot_iteration == HDA_FW_BOOT_ATTEMPTS)
			dev_err(sdev->dev, "error: %s: failed to enable interrupts\n", __func__);
		return ret;
	}

	mtl_enable_ipc_interrupts(sdev);

	/* step 6: wait for ROM_INIT_DONE */
	/*
	 * ACE workaround as in Windows driver.
	 * "ACE platform cannot catch FSR_ROM_INIT_DONE, because this status is setting by short
	 * time. This "for" loop is redundant for us and we get timeout almost everytime by this
	 * condition."
	 * So dont wait for ROM INIT.
	 */

	return 0;

err:
	snd_sof_dsp_dbg_dump(sdev, "MTL DSP init fail", 0);
	mtl_dsp_core_power_down(sdev, chip->host_managed_cores_mask);
	return ret;
}

static int mtl_dsp_cl_boot_firmware(struct snd_sof_dev *sdev)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct snd_sof_pdata *plat_data = sdev->pdata;
	const struct sof_dev_desc *desc = plat_data->desc;
	const struct sof_intel_dsp_desc *chip_info;
	struct hdac_ext_stream *stream;
	struct firmware stripped_firmware;
	struct snd_dma_buffer dmab;
	int ret, ret1, i;

	chip_info = desc->chip_info;

	if (plat_data->fw->size <= plat_data->fw_offset) {
		dev_err(sdev->dev, "error: firmware size must be greater than firmware offset\n");
		return -EINVAL;
	}

	stripped_firmware.data = plat_data->fw->data + plat_data->fw_offset;
	stripped_firmware.size = plat_data->fw->size - plat_data->fw_offset;

	/* init for booting wait */
	init_waitqueue_head(&sdev->boot_wait);

	/* prepare DMA for code loader stream */
	stream = hda_cl_stream_prepare(sdev, HDA_CL_STREAM_FORMAT, stripped_firmware.size,
				       &dmab, SNDRV_PCM_STREAM_PLAYBACK);
	if (IS_ERR(stream)) {
		dev_err(sdev->dev, "error: dma prepare for fw loading failed\n");
		return PTR_ERR(stream);
	}

	memcpy(dmab.area, stripped_firmware.data,
	       stripped_firmware.size);

	/* try ROM init a few times before giving up */
	for (i = 0; i < HDA_FW_BOOT_ATTEMPTS; i++) {
		dev_dbg(sdev->dev,
			"Attempting iteration %d of Core En/ROM load...\n", i);

		hda->boot_iteration = i + 1;
		ret = mtl_dsp_cl_init(sdev, stream->hstream.stream_tag, false);

		/* don't retry anymore if successful */
		if (!ret)
			break;
	}

	if (i == HDA_FW_BOOT_ATTEMPTS) {
		dev_err(sdev->dev, "error: dsp init failed after %d attempts with err: %d\n",
			i, ret);
		goto cleanup;
	}

	/*
	 * When a SoundWire link is in clock stop state, a Slave
	 * device may trigger in-band wakes for events such as jack
	 * insertion or acoustic event detection. This event will lead
	 * to a WAKEEN interrupt, handled by the PCI device and routed
	 * to PME if the PCI device is in D3. The resume function in
	 * audio PCI driver will be invoked by ACPI for PME event and
	 * initialize the device and process WAKEEN interrupt.
	 *
	 * The WAKEEN interrupt should be processed ASAP to prevent an
	 * interrupt flood, otherwise other interrupts, such IPC,
	 * cannot work normally.  The WAKEEN is handled after the ROM
	 * is initialized successfully, which ensures power rails are
	 * enabled before accessing the SoundWire SHIM registers
	 */
	if (!sdev->first_boot)
		hda_sdw_process_wakeen(sdev);

	/*
	 * at this point DSP ROM has been initialized and
	 * should be ready for code loading and firmware boot
	 */
	ret = hda_cl_copy_fw(sdev, stream);
	if (!ret) {
		dev_dbg(sdev->dev, "Firmware download successful, booting...\n");
	} else {
		snd_sof_dsp_dbg_dump(sdev, "load fw failed", SOF_DBG_DUMP_REGS | SOF_DBG_DUMP_PCI |
				     SOF_DBG_DUMP_MBOX);
		dev_err(sdev->dev, "error: load fw failed ret: %d\n", ret);
	}

cleanup:
	/*
	 * Perform codeloader stream cleanup.
	 * This should be done even if firmware loading fails.
	 * If the cleanup also fails, we return the initial error
	 */
	ret1 = hda_cl_cleanup(sdev, &dmab, stream);
	if (ret1 < 0) {
		dev_err(sdev->dev, "error: Code loader DSP cleanup failed\n");

		/* set return value to indicate cleanup failure */
		if (!ret)
			ret = ret1;
	}

	/*
	 * return primary core id if both fw copy
	 * and stream clean up are successful
	 */
	if (!ret)
		return chip_info->init_core_mask;

	/* disable DSP */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_PP_BAR,
				SOF_HDA_REG_PP_PPCTL,
				SOF_HDA_PPCTL_GPROCEN, 0);
	return ret;
}

static irqreturn_t mtl_ipc_irq_thread(int irq, void *context)
{
	struct snd_sof_dev *sdev = context;
	bool ipc_irq = false;
	u32 hipcida, hipctdr;

	hipcida = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxIDA);

	/* reply message from DSP */
	if (hipcida & MTL_DSP_REG_HfIPCxIDA_DONE) {
		/* DSP received the message */
		snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxCTL,
					MTL_DSP_REG_HfIPCxCTL_DONE, 0);

		mtl_ipc_dsp_done(sdev);

		ipc_irq = true;
	}

	hipctdr = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxTDR);
	if (hipctdr & MTL_DSP_REG_HfIPCxTDR_BUSY) {
		/* Message from DSP (reply or notification) */
		u32 hipctdd = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxTDDy);
		u32 primary = hipctdr & MTL_DSP_REG_HfIPCxTDR_MSG_MASK;
		u32 extension = hipctdd;

		/*
		 * ace fw sends a new fw ipc message to host to
		 * notify the status of the last host ipc message
		 */
		if (primary & SOF_IPC4_MSG_DIR_MASK) {
			/* Reply received */
			struct sof_ipc4_msg *data = sdev->ipc->msg.reply_data;

			data->primary = primary;
			data->extension = extension;

			spin_lock_irq(&sdev->ipc_lock);

			snd_sof_ipc_get_reply(sdev);
			snd_sof_ipc_reply(sdev, data->primary);

			mtl_ipc_host_done(sdev);

			spin_unlock_irq(&sdev->ipc_lock);
		} else {
			/* Notification received */
			struct sof_ipc4_msg data = {{ 0 }};

			data.primary = primary;
			data.extension = extension;

			sdev->ipc->msg.rx_data = &data;
			snd_sof_ipc_msgs_rx(sdev);
			sdev->ipc->msg.rx_data = NULL;
		}

		mtl_ipc_host_done(sdev);

		ipc_irq = true;
	}

	if (!ipc_irq) {
		/* This interrupt is not shared so no need to return IRQ_NONE. */
		dev_dbg_ratelimited(sdev->dev, "nothing to do in IPC IRQ thread\n");
	}

	return IRQ_HANDLED;
}

static void mtl_ipc_host_done(struct snd_sof_dev *sdev)
{
	/*
	 * clear busy interrupt to tell dsp controller this interrupt has been accepted,
	 * not trigger it again
	 */
	snd_sof_dsp_update_bits_forced(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxTDR,
				       MTL_DSP_REG_HfIPCxTDR_BUSY, MTL_DSP_REG_HfIPCxTDR_BUSY);
	/*
	 * clear busy bit to ack dsp the msg has been processed and send reply msg to dsp
	 */
	snd_sof_dsp_update_bits_forced(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxTDA,
				       MTL_DSP_REG_HfIPCxTDA_BUSY, 0);
}

static void mtl_ipc_dsp_done(struct snd_sof_dev *sdev)
{
	/*
	 * set DONE bit - tell DSP we have received the reply msg from DSP, and processed it,
	 * don't send more reply to host
	 */
	snd_sof_dsp_update_bits_forced(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxIDA,
				       MTL_DSP_REG_HfIPCxIDA_DONE, MTL_DSP_REG_HfIPCxIDA_DONE);

	/* unmask Done interrupt */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_DSP_REG_HfIPCxCTL,
				MTL_DSP_REG_HfIPCxCTL_DONE, MTL_DSP_REG_HfIPCxCTL_DONE);
}

static int mtl_dsp_ipc_get_mailbox_offset(struct snd_sof_dev *sdev)
{
	return MTL_DSP_MBOX_UPLINK_OFFSET;
}

static int mtl_dsp_ipc_get_window_offset(struct snd_sof_dev *sdev, u32 id)
{
	return MTL_SRAM_WINDOW_OFFSET(id);
}

static int mtl_suspend(struct snd_sof_dev *sdev, bool runtime_suspend)
{
#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA)
	struct hdac_bus *bus = sof_to_bus(sdev);
#endif
	u32 dsphfdsscs;
	u32 cpa;
	int ret;

	mtl_disable_ipc_interrupts(sdev);
	ret = mtl_disable_interrupts(sdev);
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA)
	hda_codec_jack_wake_enable(sdev, runtime_suspend);
	/* power down all hda link */
	snd_hdac_ext_bus_link_power_down_all(bus);
#endif
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_HfPWRCTL,
				MTL_HfPWRCTL_WPDSPHPxPG, 0);

	/* Set the DSP subsystem power down */
	snd_sof_dsp_update_bits(sdev, HDA_DSP_BAR, MTL_HfDSSCS,
				MTL_HfDSSCS_SPA_MASK, 0);

	/* Wait for unstable CPA read (1 then 0 then 1) just after setting SPA bit */
	usleep_range(1000, 1010);

	/* poll with timeout to check if operation successful */
	cpa = MTL_HfDSSCS_CPA_MASK;
	dsphfdsscs = snd_sof_dsp_read(sdev, HDA_DSP_BAR, MTL_HfDSSCS);
	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_BAR, MTL_HfDSSCS, dsphfdsscs,
					    (dsphfdsscs & cpa) == 0, HDA_DSP_REG_POLL_INTERVAL_US,
					HDA_DSP_RESET_TIMEOUT_US);
	if (ret < 0)
		dev_err(sdev->dev, "error: failed to disable DSP subsystem\n");

	return ret;
}

static int mtl_dsp_suspend(struct snd_sof_dev *sdev, u32 target_state)
{
	const struct sof_dsp_power_state target_dsp_state = {
		.state = target_state,
		.substate = target_state == SOF_DSP_PM_D0 ?
				SOF_HDA_DSP_PM_D0I3 : 0,
	};
	int ret;

	ret = mtl_suspend(sdev, false);
	if (ret < 0)
		return ret;

	return snd_sof_dsp_set_power_state(sdev, &target_dsp_state);
}

static int mtl_dsp_runtime_suspend(struct snd_sof_dev *sdev)
{
	const struct sof_dsp_power_state target_state = {
		.state = SOF_DSP_PM_D3,
	};
	int ret;

	ret = mtl_suspend(sdev, true);
	if (ret < 0)
		return ret;

	return snd_sof_dsp_set_power_state(sdev, &target_state);
}

static int mtl_resume(struct snd_sof_dev *sdev, bool runtime_resume)
{
#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA)
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_ext_link *hlink = NULL;
#endif
	int ret = 0;

#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA)
	/* check jack status */
	if (runtime_resume) {
		hda_codec_jack_wake_enable(sdev, false);
		if (sdev->system_suspend_target == SOF_SUSPEND_NONE)
			hda_codec_jack_check(sdev);
	}

	/* turn off the links that were off before suspend */
	list_for_each_entry(hlink, &bus->hlink_list, list) {
		if (!hlink->ref_count)
			snd_hdac_ext_bus_link_power_down(hlink);
	}

	/* check dma status and clean up CORB/RIRB buffers */
	if (!bus->cmd_dma_state)
		snd_hdac_bus_stop_cmd_io(bus);
#endif

	return ret;
}

static int mtl_dsp_resume(struct snd_sof_dev *sdev)
{
	const struct sof_dsp_power_state target_state = {
		.state = SOF_DSP_PM_D0,
		.substate = SOF_HDA_DSP_PM_D0I0,
	};
	int ret;

	ret = mtl_resume(sdev, false);
	if (ret < 0)
		return ret;

	return snd_sof_dsp_set_power_state(sdev, &target_state);
}

static int mtl_dsp_runtime_resume(struct snd_sof_dev *sdev)
{
	const struct sof_dsp_power_state target_state = {
		.state = SOF_DSP_PM_D0,
	};
	int ret;

	ret = mtl_resume(sdev, true);
	if (ret < 0)
		return ret;

	return snd_sof_dsp_set_power_state(sdev, &target_state);
}

/* Meteorlake ops */
struct snd_sof_dsp_ops sof_mtl_ops = {
	/* probe and remove */
	.probe		= hda_dsp_probe,
	.remove		= hda_dsp_remove,
	.shutdown	= hda_dsp_shutdown,

	/* Register IO */
	.write		= sof_io_write,
	.read		= sof_io_read,
	.write64	= sof_io_write64,
	.read64		= sof_io_read64,

	/* Block IO */
	.block_read	= sof_block_read,
	.block_write	= sof_block_write,

	/* doorbell */
	.irq_thread	= mtl_ipc_irq_thread,

	/* ipc */
	.send_msg	= mtl_ipc_send_msg,
	.get_mailbox_offset = mtl_dsp_ipc_get_mailbox_offset,
	.get_window_offset = mtl_dsp_ipc_get_window_offset,

	.ipc_msg_data	= hda_ipc_msg_data,
	.set_stream_data_offset = hda_set_stream_data_offset,

	/* machine driver */
	.machine_select = hda_machine_select,
	.machine_register = sof_machine_register,
	.machine_unregister = sof_machine_unregister,
	.set_mach_params = hda_set_mach_params,

	/* debug */
	.debug_map	= mtl_dsp_debugfs,
	.debug_map_count	= ARRAY_SIZE(mtl_dsp_debugfs),
	.dbg_dump	= mtl_dsp_dump,
	.ipc_dump	= cnl_ipc_dump,

	/* stream callbacks */
	.pcm_open	= hda_dsp_pcm_open,
	.pcm_close	= hda_dsp_pcm_close,
	.pcm_hw_params	= hda_dsp_pcm_hw_params,
	.pcm_hw_free	= hda_dsp_stream_hw_free,
	.pcm_trigger	= hda_dsp_pcm_trigger,
	.pcm_pointer	= hda_dsp_pcm_pointer,

	/* firmware loading */
	.load_firmware = snd_sof_load_firmware_raw,

	/* pre/post fw run */
	.pre_fw_run = mtl_dsp_pre_fw_run,
	.post_fw_run = mtl_dsp_post_fw_run,

	/* firmware run */
	.run = mtl_dsp_cl_boot_firmware,

	/* dsp core get/put */
	.core_get = mtl_dsp_core_power_up,
	.core_put = mtl_dsp_core_power_down,

	/* trace callback */
	.trace_init = hda_dsp_trace_init,
	.trace_release = hda_dsp_trace_release,
	.trace_trigger = hda_dsp_trace_trigger,

	/* DAI drivers */
	.drv		= skl_dai,
	.num_drv	= SOF_SKL_NUM_DAIS,

	/* PM */
	.suspend		= mtl_dsp_suspend,
	.resume			= mtl_dsp_resume,
	.runtime_suspend	= mtl_dsp_runtime_suspend,
	.runtime_resume		= mtl_dsp_runtime_resume,
	.runtime_idle		= hda_dsp_runtime_idle,
	.set_hw_params_upon_resume = hda_dsp_set_hw_params_upon_resume,
	.set_power_state	= hda_dsp_set_power_state,

	/* ALSA HW info flags */
	.hw_info =	SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,

	.dsp_arch_ops = &sof_xtensa_arch_ops,
};
EXPORT_SYMBOL_NS(sof_mtl_ops, SND_SOC_SOF_INTEL_HDA_COMMON);

int sof_mtl_ops_init(struct snd_sof_dev *sdev)
{
	struct sof_ipc4_fw_data *ipc4_data;

	sdev->private = devm_kzalloc(sdev->dev, sizeof(struct sof_ipc4_fw_data), GFP_KERNEL);
	if (!sdev->private)
		return -ENOMEM;

	ipc4_data = sdev->private;
	ipc4_data->manifest_fw_hdr_offset = SOF_MAN4_FW_HDR_OFFSET;

	/* set DAI ops */
	hda_set_dai_drv_ops(sdev, &sof_mtl_ops);

	return 0;
};
EXPORT_SYMBOL_NS(sof_mtl_ops_init, SND_SOC_SOF_INTEL_HDA_COMMON);

const struct sof_intel_dsp_desc mtl_chip_info = {
	.cores_num = 3,
	.init_core_mask = 1,
	.host_managed_cores_mask = BIT(0),
	.ipc_req = MTL_DSP_REG_HfIPCxIDR,
	.ipc_req_mask = MTL_DSP_REG_HfIPCxIDR_BUSY,
	.ipc_ack = MTL_DSP_REG_HfIPCxIDA,
	.ipc_ack_mask = MTL_DSP_REG_HfIPCxIDA_DONE,
	.ipc_ctl = MTL_DSP_REG_HfIPCxCTL,
	.rom_status_reg = MTL_DSP_ROM_STS,
	.rom_init_timeout	= 300,
	.ssp_count = ICL_SSP_COUNT,
	.ssp_base_offset = CNL_SSP_BASE_OFFSET,
	.sdw_shim_base = SDW_SHIM_BASE_ACE,
	.sdw_alh_base = SDW_ALH_BASE_ACE,
	.check_sdw_irq = mtl_dsp_check_sdw_irq,
	.check_ipc_irq = mtl_dsp_check_ipc_irq,
	.hw_ip_version = SOF_INTEL_ACE_1_0,
};
EXPORT_SYMBOL_NS(mtl_chip_info, SND_SOC_SOF_INTEL_HDA_COMMON);
