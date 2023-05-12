// SPDX-License-Identifier: GPL-2.0
//
// CS42L43 IRQ driver
//
// Copyright (C) 2022-2023 Cirrus Logic, Inc. and
//                         Cirrus Logic International Semiconductor Ltd.

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip/cs42l43.h>
#include <linux/mfd/cs42l43.h>
#include <linux/mfd/cs42l43-regs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#define CS42L43_IRQ_OFFSET(reg) ((CS42L43_##reg##_INT) - CS42L43_DECIM_INT)

#define CS42L43_IRQ_REG(name, reg) REGMAP_IRQ_REG(CS42L43_##name, \
						  CS42L43_IRQ_OFFSET(reg), \
						  CS42L43_##name##_INT_MASK)

static const struct regmap_irq cs42l43_regmap_irqs[] = {
	CS42L43_IRQ_REG(PLL_LOST_LOCK,				PLL),
	CS42L43_IRQ_REG(PLL_READY,				PLL),

	CS42L43_IRQ_REG(HP_STARTUP_DONE,			MSM),
	CS42L43_IRQ_REG(HP_SHUTDOWN_DONE,			MSM),
	CS42L43_IRQ_REG(HSDET_DONE,				MSM),
	CS42L43_IRQ_REG(TIPSENSE_UNPLUG_DB,			MSM),
	CS42L43_IRQ_REG(TIPSENSE_PLUG_DB,			MSM),
	CS42L43_IRQ_REG(RINGSENSE_UNPLUG_DB,			MSM),
	CS42L43_IRQ_REG(RINGSENSE_PLUG_DB,			MSM),
	CS42L43_IRQ_REG(TIPSENSE_UNPLUG_PDET,			MSM),
	CS42L43_IRQ_REG(TIPSENSE_PLUG_PDET,			MSM),
	CS42L43_IRQ_REG(RINGSENSE_UNPLUG_PDET,			MSM),
	CS42L43_IRQ_REG(RINGSENSE_PLUG_PDET,			MSM),

	CS42L43_IRQ_REG(HS2_BIAS_SENSE,				ACC_DET),
	CS42L43_IRQ_REG(HS1_BIAS_SENSE,				ACC_DET),
	CS42L43_IRQ_REG(DC_DETECT1_FALSE,			ACC_DET),
	CS42L43_IRQ_REG(DC_DETECT1_TRUE,			ACC_DET),
	CS42L43_IRQ_REG(HSBIAS_CLAMPED,				ACC_DET),
	CS42L43_IRQ_REG(HS3_4_BIAS_SENSE,			ACC_DET),

	CS42L43_IRQ_REG(AMP2_CLK_STOP_FAULT,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP1_CLK_STOP_FAULT,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP2_VDDSPK_FAULT,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP1_VDDSPK_FAULT,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP2_SHUTDOWN_DONE,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP1_SHUTDOWN_DONE,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP2_STARTUP_DONE,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP1_STARTUP_DONE,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP2_THERM_SHDN,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP1_THERM_SHDN,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP2_THERM_WARN,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP1_THERM_WARN,			CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP2_SCDET,				CLASS_D_AMP),
	CS42L43_IRQ_REG(AMP1_SCDET,				CLASS_D_AMP),

	CS42L43_IRQ_REG(GPIO3_FALL,				GPIO),
	CS42L43_IRQ_REG(GPIO3_RISE,				GPIO),
	CS42L43_IRQ_REG(GPIO2_FALL,				GPIO),
	CS42L43_IRQ_REG(GPIO2_RISE,				GPIO),
	CS42L43_IRQ_REG(GPIO1_FALL,				GPIO),
	CS42L43_IRQ_REG(GPIO1_RISE,				GPIO),

	CS42L43_IRQ_REG(HP_ILIMIT,				HPOUT),
	CS42L43_IRQ_REG(HP_LOADDET_DONE,			HPOUT),
};

static const struct regmap_irq_chip cs42l43_irq_chip = {
	.name = "cs42l43",

	.status_base = CS42L43_DECIM_INT,
	.mask_base = CS42L43_DECIM_MASK,
	.num_regs = 16,

	.irqs = cs42l43_regmap_irqs,
	.num_irqs = ARRAY_SIZE(cs42l43_regmap_irqs),

	.runtime_pm = true,
};

struct cs42l43_irq {
	struct device *dev;

	struct regmap_irq_chip irq_chip;
	struct regmap_irq_chip_data *irq_data;
};

static int cs42l43_irq_probe(struct platform_device *pdev)
{
	struct cs42l43 *cs42l43 = dev_get_drvdata(pdev->dev.parent);
	struct cs42l43_irq *priv;
	struct irq_data *irq_data;
	unsigned long irq_flags;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	priv->irq_chip = cs42l43_irq_chip;
	priv->irq_chip.irq_drv_data = priv;

	irq_data = irq_get_irq_data(cs42l43->irq);
	if (!irq_data) {
		dev_err(priv->dev, "Invalid IRQ: %d\n", cs42l43->irq);
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

	pm_runtime_enable(priv->dev);
	pm_runtime_idle(priv->dev);

	ret = devm_regmap_add_irq_chip(priv->dev, cs42l43->regmap,
				       cs42l43->irq, irq_flags, 0,
				       &priv->irq_chip, &priv->irq_data);
	if (ret) {
		dev_err(priv->dev, "Failed to add IRQ chip: %d\n", ret);
		pm_runtime_disable(priv->dev);
		return ret;
	}

	dev_dbg(priv->dev, "Configured IRQ %d with flags 0x%lx\n",
		cs42l43->irq, irq_flags);

	return 0;
}

static int cs42l43_irq_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static struct platform_driver cs42l43_irq_driver = {
	.driver = {
		.name	= "cs42l43-irq",
	},

	.probe		= cs42l43_irq_probe,
	.remove		= cs42l43_irq_remove,
};
module_platform_driver(cs42l43_irq_driver);

MODULE_DESCRIPTION("CS42L43 IRQ Driver");
MODULE_AUTHOR("Charles Keepax <ckeepax@opensource.cirrus.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:cs42l43-irq");
