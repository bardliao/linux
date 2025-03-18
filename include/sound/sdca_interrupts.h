/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The MIPI SDCA specification is available for public downloads at
 * https://www.mipi.org/mipi-sdca-v1-0-download
 *
 * Copyright (C) 2025 Cirrus Logic, Inc. and
 *                         Cirrus Logic International Semiconductor Ltd.
 */

#ifndef __SDCA_INTERRUPTS_H__
#define __SDCA_INTERRUPTS_H__

#include <linux/types.h>
#include <linux/kconfig.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>

struct sdca_function_data;

#define SDCA_MAX_INTERRUPTS 31 /* the last bit is reserved for future extensions */

/**
 * struct sdca_interrupt_info - structure containing SDCA interrupt related information
 * @drv_data: Driver specific IRQ data.
 * @irq_chip: regmap irq chip structure.
 * @irq_data: regmap irq chip data structure.
 * @irq_dom: Pointer to the irq domain.
 * @irqs_lock: Mutex to protect SDCA interrupts.
 * @sdca_interrupt_register_list: Bitmask of SDCA interrupt registers.
 */
struct sdca_interrupt_info {
	void *drv_data;
	struct device *dev;
	struct regmap *regmap;

	struct regmap_irq_chip irq_chip;
	struct regmap_irq_chip_data *irq_data;
	struct irq_domain *irq_dom;

	struct mutex irqs_lock; /* protects SDCA interrupts */
};

/**
 * struct sdca_interrupt - struct containing information about an SDCA interrupt
 * @regmap: Pointer to the regmap for the device.
 * @num: The interrupt number.
 * @control: Pointer to the control that the interrupt is associated with.
 * @control_reg: The register that the interrupt is associated with.
 * @callback: The callback function to be called when the interrupt is triggered.
 * @name: The name of the interrupt.
 */
struct sdca_interrupt {
	struct regmap *regmap;

	int num;
	struct sdca_control *control;
	unsigned int control_reg;

	irqreturn_t (*callback)(int irq, void *context);
	char *name;
};

/**
 * sdca_request_irq - request an SDCA interrupt
 * @interrupt_info: Pointer to the interrupt information structure.
 * @irq: Pointer to the interrupt structure.
 */
int sdca_request_irq(struct sdca_interrupt_info *interrupt_info, struct sdca_interrupt *irq);

/**
 * sdca_irq_config - configure SDCA interrupts
 * @interrupt_info: Pointer to the interrupt information structure.
 * @irq: The interrupt number.
 * @functions: Pointer to the function data structure.
 * @num_functions: The number of functions.
 */
int sdca_irq_config(struct sdca_interrupt_info *interrupt_info, unsigned int irq,
		    struct sdca_function_data *functions, int num_functions);

#endif
