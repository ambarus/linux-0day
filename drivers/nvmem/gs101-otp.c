// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2025 Linaro Ltd.
 *
 * Google GS101 OTP driver.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mod_devicetable.h>

struct gs101_otp {
	struct clk *pclk;
	struct regmap *regmap;
};

static int gs101_otp_read(void *context, unsigned int offset, void *val,
			  size_t bytes)
{
	struct gs101_otp *gotp = context;

	return regmap_bulk_read(gotp->regmap, offset, val, bytes / 4);
}

static struct nvmem_config gs101_otp_nvmem_config = {
	.name = "gs101-otp",
	.add_legacy_fixed_of_cells = true,
	.reg_read = gs101_otp_read,
	.word_size = 4,
	.stride = 4,
};

static int gs101_otp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvmem_device *nvmem;
	struct gs101_otp *gotp;
	struct resource *res;
	void __iomem *base;

	gotp = devm_kzalloc(dev, sizeof(*gotp), GFP_KERNEL);
	if (!gotp)
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

	gotp->regmap = devm_regmap_init_mmio(dev, base, &reg_config);
	if (IS_ERR(gotp->regmap))
		return PTR_ERR(gotp->regmap);

	gotp->pclk = devm_clk_get_enabled(dev, "pclk");
	if (IS_ERR(gotp->pclk))
		return dev_err_probe(dev, PTR_ERR(gotp->pclk),
				     "Could not get pclk\n");

	gs101_otp_nvmem_config.size = resource_size(res);
	gs101_otp_nvmem_config.dev = dev;
	gs101_otp_nvmem_config.priv = gotp;

	nvmem = devm_nvmem_register(dev, &gs101_otp_nvmem_config);

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
