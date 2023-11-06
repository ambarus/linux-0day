#include <linux/completion.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/hyperbus.h>
#include <linux/mtd/mtd.h>
#include <linux/mux/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sched/task_stack.h>
#include <linux/types.h>

static struct hyperbus_ctlr ctlr;

static int ifx_hyperbus_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hyperbus_device *hbdev;
	struct resource *res;

	hbdev = devm_kzalloc(dev, sizeof(*hbdev), GFP_KERNEL);
	if (!hbdev)
		return -ENOMEM;

	platform_set_drvdata(pdev, hbdev);

	hbdev->np = of_get_next_child(pdev->dev.of_node, NULL);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	hbdev->map.size = resource_size(res);
	hbdev->map.virt = devm_ioremap_resource(dev, res);
	if (IS_ERR(hbdev->map.virt))
		return PTR_ERR(hbdev->map.virt);

	ctlr.dev = dev;
	hbdev->ctlr = &ctlr;

	return hyperbus_register_device(hbdev);
}

static int ifx_hyperbus_remove(struct platform_device *pdev)
{
	struct hyperbus_device *hbdev = platform_get_drvdata(pdev);

	hyperbus_unregister_device(hbdev);
	return 0;
}

static const struct of_device_id ifx_hyperbus_dt_ids[] = {
	{
		.compatible = "infineon,ifx-hyperbus",
	},
	{ /* end of table */ }
};

MODULE_DEVICE_TABLE(of, ifx_hyperbus_dt_ids);

static struct platform_driver ifx_hyperbus_platform_driver = {
	.probe = ifx_hyperbus_probe,
	.remove = ifx_hyperbus_remove,
	.driver = {
		.name = "ifx-hyperbus",
		.of_match_table = ifx_hyperbus_dt_ids,
	},
};

module_platform_driver(ifx_hyperbus_platform_driver);

MODULE_AUTHOR("Takahiro Kuwano <Takahiro.Kuwano@infineon.com>");
MODULE_DESCRIPTION("HYPERBUS driver");
MODULE_LICENSE("GPL v2");
