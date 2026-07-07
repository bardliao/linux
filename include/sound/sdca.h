/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */
/*
 * The MIPI SDCA specification is available for public downloads at
 * https://www.mipi.org/mipi-sdca-v1-0-download
 *
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef __SDCA_H__
#define __SDCA_H__

#include <linux/types.h>
#include <linux/kconfig.h>

struct acpi_table_swft;
struct fwnode_handle;
struct sdw_slave;
struct sdca_dev;
struct sdca_entity_iot;

#define SDCA_MAX_FUNCTION_COUNT 8

/**
 * struct sdca_function_desc - short descriptor for an SDCA Function
 * @node: firmware node for the Function.
 * @func_dev: pointer to SDCA function device.
 * @name: Human-readable string.
 * @type: Function topology type.
 * @adr: ACPI address (used for SDCA register access).
 * @duplicate: Internal flag to indicate if other functions of the same type
 * exist.
 */
struct sdca_function_desc {
	struct fwnode_handle *node;
	struct sdca_dev *func_dev;
	const char *name;
	u32 type;
	u8 adr;

	bool duplicate;
};

/**
 * struct sdca_device_data - structure containing all SDCA related information
 * @interface_revision: Value read from _DSD property, mainly to check
 * for changes between silicon versions.
 * @num_functions: Total number of supported SDCA functions. Invalid/unsupported
 * functions will be skipped.
 * @terminal_iot: Dynamically allocated array with parsed Terminal Entity
 * properties (struct sdca_entity_iot) gathered before device registration.
 * @num_terminal_iot: Number of entries in @terminal_iot.
 * @terminal_transducer_count: Maximum transducer count found in terminal
 * entities across all SDCA functions.
 * @function: Array of function descriptors.
 * @swft: Pointer to the SWFT table, if available.
 */
struct sdca_device_data {
	u32 interface_revision;
	int num_functions;
	struct sdca_entity_iot *terminal_iot;
	int num_terminal_iot;
	u32 terminal_transducer_count;
	struct sdca_function_desc function[SDCA_MAX_FUNCTION_COUNT];
	struct acpi_table_swft *swft;
};

enum sdca_quirk {
	SDCA_QUIRKS_RT712_VB,
	SDCA_QUIRKS_SKIP_FUNC_TYPE_PATCHING,
};

#if IS_ENABLED(CONFIG_ACPI) && IS_ENABLED(CONFIG_SND_SOC_SDCA)

void sdca_lookup_functions(struct sdw_slave *slave);
int sdca_get_mic_count(struct sdw_slave *slave, struct sdca_function_desc *function);
void sdca_lookup_terminal_iot(struct sdw_slave *slave);
void sdca_lookup_swft(struct sdw_slave *slave);
void sdca_lookup_interface_revision(struct sdw_slave *slave);
bool sdca_device_quirk_match(struct sdw_slave *slave, enum sdca_quirk quirk);
int sdca_dev_register_functions(struct sdw_slave *slave);
void sdca_dev_unregister_functions(struct sdw_slave *slave);

#else

static inline void sdca_lookup_functions(struct sdw_slave *slave) {}
static inline void sdca_lookup_function_entities(struct sdw_slave *slave,
					 struct sdca_function_desc *function) {}
static inline void sdca_lookup_terminal_iot(struct sdw_slave *slave) {}
static inline void sdca_lookup_swft(struct sdw_slave *slave) {}
static inline void sdca_lookup_interface_revision(struct sdw_slave *slave) {}
static inline bool sdca_device_quirk_match(struct sdw_slave *slave, enum sdca_quirk quirk)
{
	return false;
}

static inline int sdca_dev_register_functions(struct sdw_slave *slave)
{
	return 0;
}

static inline void sdca_dev_unregister_functions(struct sdw_slave *slave) {}

#endif

#endif
