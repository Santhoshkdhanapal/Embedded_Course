#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>

MODULE_LICENSE("GPL");

#define DRIVER_NAME "sensor_platform_drv"

static int my_platform_probe(struct platform_device *pdev)
{
    pr_info("platform_engine: Probe triggered for device: %s\n", pdev->name);
    // Typical tasks here: parse Device Tree nodes, map MMIO registers, register ISRs
    return 0;
}

static void my_platform_remove(struct platform_device *pdev)
{
    pr_info("platform_engine: Device removed: %s\n", pdev->name);
}

static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,embedded-sensor" },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_of_match);

static struct platform_driver my_driver = {
    .probe = my_platform_probe,
    .remove = my_platform_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = my_of_match,
        .owner = THIS_MODULE,
    },
};

// Create a mock platform device to test binding without editing device trees
static struct platform_device *mock_pdev;

static int __init my_plat_init(void)
{
    int ret;

    ret = platform_driver_register(&my_driver);
    if (ret)
        return ret;

    mock_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);
    if (IS_ERR(mock_pdev)) {
        platform_driver_unregister(&my_driver);
        return PTR_ERR(mock_pdev);
    }

    pr_info("platform_engine: Driver and mock platform device loaded\n");
    return 0;
}

static void __exit my_plat_exit(void)
{
    platform_device_unregister(mock_pdev);
    platform_driver_unregister(&my_driver);
    pr_info("platform_engine: Driver unregistered\n");
}

module_init(my_plat_init);
module_exit(my_plat_exit);
