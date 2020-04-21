// SPDX-License-Identifier: GPL-2.0-only
// Copyright(c) 2019-2020 Intel Corporation.

#include <linux/device.h>
#include <linux/acpi.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_type.h>
#include "bus.h"

static void sdw_master_device_release(struct device *dev)
{
	struct sdw_master_device *md = dev_to_sdw_master_device(dev);

	kfree(md);
}

struct device_type sdw_master_type = {
	.name =		"soundwire_master",
	.release =	sdw_master_device_release,
};

/**
 * sdw_master_device_add() - create a Linux Master Device representation.
 * @parent: the parent Linux device (e.g. a PCI device)
 * @fwnode: the parent fwnode (e.g. an ACPI companion device to the parent)
 * @link_id: link index as defined by MIPI DisCo specification
 *
 */
struct sdw_master_device*
sdw_master_device_add(struct device *parent,
		      struct fwnode_handle *fwnode,
		      int link_id)
{
	struct sdw_master_device *md;
	int ret;

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return ERR_PTR(-ENOMEM);

	md->link_id = link_id;

	md->dev.parent = parent;
	md->dev.fwnode = fwnode;
	md->dev.bus = &sdw_bus_type;
	md->dev.type = &sdw_master_type;
	md->dev.dma_mask = md->dev.parent->dma_mask;
	dev_set_name(&md->dev, "sdw-master-%d", md->link_id);

	ret = device_register(&md->dev);
	if (ret) {
		dev_err(parent, "Failed to add master: ret %d\n", ret);
		/*
		 * On err, don't free but drop ref as this will be freed
		 * when release method is invoked.
		 */
		put_device(&md->dev);
		return ERR_PTR(ret);
	}

	return md;
}

