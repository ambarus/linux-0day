// SPDX-License-Identifier: GPL-2.0-only
/*
 * Samsung Exynos ACPM protocol based clock driver.
 *
 * Copyright 2025 Linaro Ltd.
 */

#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/firmware/samsung/exynos-acpm-protocol.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <dt-bindings/clock/google,gs101.h>

struct acpm_clk {
	u32 id;
	struct clk_hw hw;
	unsigned int acpm_chan_id;
	const struct acpm_handle *handle;
};

#define to_acpm_clk(clk) container_of(clk, struct acpm_clk, hw)

struct acpm_clk_variant {
	unsigned int id;
	const char *name;
};

struct acpm_clk_match_data {
	const struct acpm_clk_variant *clks;
	unsigned int nr_clks;
	unsigned int acpm_chan_id;
};

static unsigned long acpm_clk_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct acpm_clk *clk = to_acpm_clk(hw);

	return clk->handle->ops.dvfs_ops.get_rate(clk->handle,
					clk->acpm_chan_id, clk->id, 0);
}

static long acpm_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	/*
	 * We can't figure out what rate it will be, so just return the
	 * rate back to the caller. acpm_clk_recalc_rate() will be called
	 * after the rate is set and we'll know what rate the clock is
	 * running at then.
	 */
	return rate;
}

static int acpm_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct acpm_clk *clk = to_acpm_clk(hw);

	return clk->handle->ops.dvfs_ops.set_rate(clk->handle,
					clk->acpm_chan_id, clk->id, rate);
}

static const struct clk_ops acpm_clk_ops = {
	.recalc_rate = acpm_clk_recalc_rate,
	.round_rate = acpm_clk_round_rate,
	.set_rate = acpm_clk_set_rate,
};

static int __init acpm_clk_ops_init(struct device *dev, struct acpm_clk *aclk,
				    const char *name)
{
	struct clk_init_data init = {};

	init.name = name;
	init.ops = &acpm_clk_ops;
	aclk->hw.init = &init;

	return devm_clk_hw_register(dev, &aclk->hw);
}

static int __init acpm_clk_probe(struct platform_device *pdev)
{
	const struct acpm_clk_match_data *match_data;
	const struct acpm_handle *acpm_handle;
	struct clk_hw_onecell_data *clk_data;
	struct clk_hw **hws;
	struct device *dev = &pdev->dev;
	struct acpm_clk *aclks;
	unsigned int acpm_chan_id;
	int i, err, count;

	acpm_handle = devm_acpm_get_by_node(dev, dev->parent->of_node);
	if (IS_ERR(acpm_handle))
		return dev_err_probe(dev, PTR_ERR(acpm_handle),
				     "Failed to get acpm handle.\n");

	match_data = of_device_get_match_data(dev);
	if (!match_data)
		return dev_err_probe(dev, -EINVAL,
				     "Failed to get match data.\n");

	count = match_data->nr_clks;
	acpm_chan_id = match_data->acpm_chan_id;

	clk_data = devm_kzalloc(dev, struct_size(clk_data, hws, count),
				GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->num = count;
	hws = clk_data->hws;

	aclks = devm_kcalloc(dev, count, sizeof(*aclks), GFP_KERNEL);
	if (!aclks)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		const struct acpm_clk_variant *variant = &match_data->clks[i];
		struct acpm_clk *aclk = &aclks[i];

		hws[i] = &aclk->hw;

		aclk->id = variant->id;
		aclk->handle = acpm_handle;
		aclk->acpm_chan_id = acpm_chan_id;

		err = acpm_clk_ops_init(dev, aclk, variant->name);
		if (err)
			return dev_err_probe(dev, err,
					     "Failed to register clock.\n");
	}

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   clk_data);
}

#define ACPM_CLK(_id, cname)					\
	{							\
		.id		= _id,				\
		.name		= cname,			\
	}

static const struct acpm_clk_variant gs101_acpm_clks[] __initconst = {
	ACPM_CLK(CLK_ACPM_DVFS_MIF, "mif"),
	ACPM_CLK(CLK_ACPM_DVFS_INT, "int"),
	ACPM_CLK(CLK_ACPM_DVFS_CPUCL0, "cpucl0"),
	ACPM_CLK(CLK_ACPM_DVFS_CPUCL1, "cpucl1"),
	ACPM_CLK(CLK_ACPM_DVFS_CPUCL2, "cpucl2"),
	ACPM_CLK(CLK_ACPM_DVFS_G3D, "g3d"),
	ACPM_CLK(CLK_ACPM_DVFS_G3DL2, "g3dl2"),
	ACPM_CLK(CLK_ACPM_DVFS_TPU, "tpu"),
	ACPM_CLK(CLK_ACPM_DVFS_INTCAM, "intcam"),
	ACPM_CLK(CLK_ACPM_DVFS_TNR, "tnr"),
	ACPM_CLK(CLK_ACPM_DVFS_CAM, "cam"),
	ACPM_CLK(CLK_ACPM_DVFS_MFC, "mfc"),
	ACPM_CLK(CLK_ACPM_DVFS_DISP, "disp"),
	ACPM_CLK(CLK_ACPM_DVFS_BO, "b0"),
};

static const struct acpm_clk_match_data acpm_clk_gs101 __initconst = {
	.clks = gs101_acpm_clks,
	.nr_clks = ARRAY_SIZE(gs101_acpm_clks),
	.acpm_chan_id = 0,
};

static const struct of_device_id acpm_clk_ids[] __initconst = {
	{
		.compatible = "google,gs101-acpm-dvfs-clocks",
		.data =  &acpm_clk_gs101,
	},
	{}
};
MODULE_DEVICE_TABLE(of, acpm_clk_ids);

static struct platform_driver acpm_clk_driver __refdata = {
	.driver	= {
		.name = "acpm-clocks",
		.of_match_table = acpm_clk_ids,
	},
	.probe = acpm_clk_probe,
};
module_platform_driver(acpm_clk_driver);

MODULE_AUTHOR("Tudor Ambarus <tudor.ambarus@linaro.org>");
MODULE_DESCRIPTION("Samsung Exynos ACPM clock driver");
MODULE_LICENSE("GPL");
