/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/* Copyright(c) 2015-17 Intel Corporation. */

#ifndef __SDW_INTEL_H
#define __SDW_INTEL_H

/**
 * struct sdw_intel_ops: Intel audio driver callback ops
 *
 * @config_stream: configure the stream with the hw_params
 * the first argument containing the context is mandatory
 */
struct sdw_intel_ops {
	int (*config_stream)(void *arg, void *substream,
			     void *dai, void *hw_params,
			     int link_id, int alh_stream_id);
	int (*free_stream)(void *arg, void *substream, void *dai, int link_id);
};

/**
 * struct sdw_intel_res - Soundwire Intel resource structure
 * @mmio_base: mmio base of SoundWire registers
 * @irq: interrupt number
 * @handle: ACPI parent handle
 * @parent: parent device
 * @ops: callback ops
 * @arg: callback arg
 * @link_mask: bit-wise mask indicating which link is enabled
 *
 * All fields are set prior to calling sdw_intel_init(), except for
 * link_mask which is set by sdw_intel_init()
 */
struct sdw_intel_res {
	void __iomem *mmio_base;
	int irq;
	acpi_handle handle;
	struct device *parent;
	const struct sdw_intel_ops *ops;
	void *arg;
	int link_mask;
	spinlock_t irq_lock;
};

void *sdw_intel_init(acpi_handle *parent_handle, struct sdw_intel_res *res);
int sdw_intel_enable(void *arg);
void sdw_intel_exit(void *arg);

#endif
