// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2025 Linaro Ltd.
 *
 * Google GS101 OTP driver.
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>

struct gs101_otp_priv {
	struct regmap *regmap;
};

static int gs101_otp_read(void *context, unsigned int offset, void *val,
			  size_t bytes)
{
	struct gs101_otp_priv *priv = context;

	return regmap_bulk_read(priv->regmap, offset, val, bytes / 4);
}

static struct nvmem_config gs101_otp_nvmem_config = {
	.name = "gs101-otp",
	.reg_read = gs101_otp_read,
	.word_size = 4,
	.stride = 4,
};

static int gs101_otp_probe(struct platform_device *pdev)
{
	struct gs101_otp_priv *priv;
	struct nvmem_device *nvmem;
	struct resource *res;
	void __iomem *base;

	printk("tudor: otp enter: %s\n", __func__);
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	const struct regmap_config reg_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.use_relaxed_mmio = true,
		.max_register = (resource_size(res) - reg_config.reg_stride),
	};

	priv->regmap = devm_regmap_init_mmio(&pdev->dev, base, &reg_config);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	gs101_otp_nvmem_config.size = resource_size(res);
	gs101_otp_nvmem_config.dev = &pdev->dev;
	gs101_otp_nvmem_config.priv = priv;

	nvmem = devm_nvmem_register(&pdev->dev, &gs101_otp_nvmem_config);

	printk("tudor: otp: %s\n", __func__);
	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id gs101_otp_dt_ids[] = {
	{ .compatible = "google,gs101-otp" },
	{},
};
MODULE_DEVICE_TABLE(of, gs101_otp_dt_ids);

static struct platform_driver gs101_otp_driver = {
	.probe	= gs101_otp_probe,
	.driver = {
		.name	= "gs101_otp",
		.of_match_table = gs101_otp_dt_ids,
	},
};
module_platform_driver(gs101_otp_driver);

MODULE_AUTHOR("Tudor Ambarus <tudor.ambarus@linaro.org>");
MODULE_DESCRIPTION("Google GS101 OTP driver");
MODULE_LICENSE("GPL");
