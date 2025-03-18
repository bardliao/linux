// SPDX-License-Identifier: GPL-2.0
/*
 * SDCA IRQ handler library
 *
 * Copyright (C) 2025 Cirrus Logic, Inc. and
 *                         Cirrus Logic International Semiconductor Ltd.
 *
 * The MIPI SDCA specification is available for public downloads at
 * https://www.mipi.org/mipi-sdca-v1-0-download
 */
#include <linux/device.h>
#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_registers.h>
#include <sound/sdca.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <sound/sdca_interrupts.h>
#include <sound/sdca_function.h>

static irqreturn_t sdca_base_irq_handler(int irq, void *data)
{
	struct sdca_interrupt *priv = data;

	return IRQ_HANDLED;
}

#define IRQ_SDCA_OFFSET(reg_number) ((SDW_SCP_SDCA_INT##reg_number) - SDW_SCP_SDCA_INT1)
#define IRQ_SDCA(number, reg_number) REGMAP_IRQ_REG(number, \
						    IRQ_SDCA_OFFSET(reg_number), \
						    SDW_SCP_SDCA_INTMASK_SDCA_##number)

static const struct regmap_irq regmap_irqs[] = {
	IRQ_SDCA(0, 1),
	IRQ_SDCA(1, 1),
	IRQ_SDCA(2, 1),
	IRQ_SDCA(3, 1),
	IRQ_SDCA(4, 1),
	IRQ_SDCA(5, 1),
	IRQ_SDCA(6, 1),
	IRQ_SDCA(7, 1),
	IRQ_SDCA(8, 2),
	IRQ_SDCA(9, 2),
	IRQ_SDCA(10, 2),
	IRQ_SDCA(11, 2),
	IRQ_SDCA(12, 2),
	IRQ_SDCA(13, 2),
	IRQ_SDCA(14, 2),
	IRQ_SDCA(15, 2),
	IRQ_SDCA(16, 3),
	IRQ_SDCA(17, 3),
	IRQ_SDCA(18, 3),
	IRQ_SDCA(19, 3),
	IRQ_SDCA(20, 3),
	IRQ_SDCA(21, 3),
	IRQ_SDCA(22, 3),
	IRQ_SDCA(23, 3),
	IRQ_SDCA(24, 4),
	IRQ_SDCA(25, 4),
	IRQ_SDCA(26, 4),
	IRQ_SDCA(27, 4),
	IRQ_SDCA(28, 4),
	IRQ_SDCA(29, 4),
	IRQ_SDCA(30, 4),
};

static const struct regmap_irq_chip sdca_irq_chip = {
	.name = "sdca_irq",

	.status_base = SDW_SCP_SDCA_INT1,
	.unmask_base = SDW_SCP_SDCA_INTMASK1,
	.ack_base = SDW_SCP_SDCA_INT1,
	.num_regs = 4,

	.irqs = regmap_irqs,
	.num_irqs = SDCA_MAX_INTERRUPTS,

	.runtime_pm = true,
};

int sdca_request_irq(struct sdca_interrupt_info *interrupt_info, struct sdca_interrupt *irq)
{
	char *name = irq->name;
	irq_handler_t thread_fn = irq->callback;
	int ret;

	ret = irq_create_mapping(interrupt_info->irq_dom, irq->num);

	if (ret < 0) {
		dev_err(interrupt_info->dev, "Failed to map IRQ %s\n", name);
		return 1;
	}

	dev_dbg(interrupt_info->dev, "Request IRQ %d for %s\n", ret, name);

	ret = devm_request_threaded_irq(interrupt_info->dev, ret, NULL, thread_fn,
					IRQF_ONESHOT, name, irq);
	if (ret) {
		dev_err(interrupt_info->dev, "Failed to request IRQ %s\n", name);
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sdca_request_irq);

static int sdca_process_controls_with_interrupts(struct sdca_interrupt_info *interrupt_info,
						 struct sdca_function_data *functions,
						 int num_functions)
{
	struct sdca_function_data *function;
	struct sdca_control *control;
	struct sdca_interrupt *interrupt;
	int total_controls = 0;
	int local_controls = 0;
	int i, j, k;

	for (i = 0; i < num_functions; i++) {
		function = &functions[i];
		for (j = 0; j < function->num_entities; j++) {
			for (k = 0; k < function->entities[j].num_controls; k++) {
				control = &function->entities[j].controls[k];
				if (control->interrupt_position > 0) {
					interrupt = &function->interrupt[local_controls++];
					interrupt->num = control->interrupt_position;
					interrupt->name = devm_kasprintf(interrupt_info->dev,
							GFP_KERNEL, "%s: %s", function->desc->name,
							control->label);
					interrupt->callback = &sdca_base_irq_handler;
					interrupt->regmap = interrupt_info->regmap;
					interrupt->control = control;
					interrupt->control_reg = SDW_SDCA_CTL(function->desc->adr,
									      function->entities[j].id,
									      control->sel, 0);

					total_controls++;
				}
			}
		}
		function->interrupt_num = local_controls;
		local_controls = 0;
	}

	return total_controls;
}

int sdca_irq_config(struct sdca_interrupt_info *interrupt_info, unsigned int irq,
		    struct sdca_function_data *functions, int num_functions)
{
	struct irq_data *irq_data;
	unsigned long irq_flags;
	struct irq_domain *dom;
	int ret;
	int control_count;

	control_count = sdca_process_controls_with_interrupts(interrupt_info, functions,
							      num_functions);
	if (control_count <= 0) {
		dev_info(interrupt_info->dev, "No interrupts to act on\n");
		return 0;
	} else if (control_count >= SDCA_MAX_INTERRUPTS) {
		dev_err(interrupt_info->dev, "Too many interrupts: %d\n", total_controls);
		return -EINVAL;
	}

	interrupt_info->irq_chip = sdca_irq_chip;
	interrupt_info->irq_chip.irq_drv_data = interrupt_info->drv_data;

	irq_data = irq_get_irq_data(irq);
	if (!irq_data) {
		dev_err(interrupt_info->dev, "Invalid IRQ: %d\n", irq);
		return -EINVAL;
	}

	irq_flags = irqd_get_trigger_type(irq_data);
	switch (irq_flags) {
	case IRQF_TRIGGER_LOW:
	case IRQF_TRIGGER_HIGH:
	case IRQF_TRIGGER_RISING:
	case IRQF_TRIGGER_FALLING:
		break;
	case IRQ_TYPE_NONE:
	default:
		irq_flags = IRQF_TRIGGER_LOW;
		break;
	}

	irq_flags |= IRQF_ONESHOT;

	ret = devm_regmap_add_irq_chip(interrupt_info->dev, interrupt_info->regmap, irq, irq_flags,
				       0, &interrupt_info->irq_chip, &interrupt_info->irq_data);
	if (ret) {
		dev_err(interrupt_info->dev, "Failed to add IRQ chip: %d\n", ret);
		return ret;
	}
	dev_dbg(interrupt_info->dev, "Configured IRQ %d with flags 0x%lx\n", irq, irq_flags);

	interrupt_info->irq_dom = regmap_irq_get_domain(interrupt_info->irq_data);
	if (!dom)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_NS(sdca_irq_config, "SND_SOC_SDCA_IRQ_HANDLER");

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SDCA IRQ handler library");
